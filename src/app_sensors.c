/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/byteorder.h>

#include "app_sensors.h"
#include "app_settings.h"
#include "lib/minmea/minmea.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

#define NMEA_SIZE		       128
#define OBD2_PID_REQUEST_ID	       0x7DF
#define ODB2_PID_REQUEST_DATA_LENGTH   2
#define OBD2_PID_RESPONSE_ID	       0x7E8
#define OBD2_PID_RESPONSE_DLC	       8
#define OBD2_SERVICE_SHOW_CURRENT_DATA 0x01
#define ODB2_PID_VEHICLE_SPEED	       0x0D
#define ODB2_PID_VEHICLE_SPEED_DLC     4
#define GOLIOTH_STREAM_TIMEOUT_S       2

static struct golioth_client *client;

#define UART_DEVICE_NODE DT_ALIAS(click_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define UART_SEL DT_ALIAS(gnss7_sel)
static const struct gpio_dt_spec gnss7_sel = GPIO_DT_SPEC_GET(UART_SEL, gpios);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

struct can_asset_tracker_data {
	struct minmea_sentence_rmc rmc_frame;
	int vehicle_speed;
};

K_MSGQ_DEFINE(cat_msgq, sizeof(struct can_asset_tracker_data), 64, 4);
K_MSGQ_DEFINE(rmc_msgq, sizeof(struct minmea_sentence_rmc), 2, 4);
CAN_MSGQ_DEFINE(can_msgq, 2);

#define PROCESS_CAN_FRAMES_THREAD_STACK_SIZE 2048
#define PROCESS_CAN_FRAMES_THREAD_PRIORITY   2
static k_tid_t process_can_frames_tid;
struct k_thread process_can_frames_thread_data;
K_THREAD_STACK_DEFINE(process_can_frames_thread_stack, PROCESS_CAN_FRAMES_THREAD_STACK_SIZE);

#define PROCESS_RMC_FRAMES_THREAD_STACK_SIZE 2048
#define PROCESS_RMC_FRAMES_THREAD_PRIORITY   2
static k_tid_t process_rmc_frames_tid;
struct k_thread process_rmc_frames_thread_data;
K_THREAD_STACK_DEFINE(process_rmc_frames_thread_stack, PROCESS_RMC_FRAMES_THREAD_STACK_SIZE);

/* Global state shared between threads */
#define SHARED_DATA_MUTEX_TIMEOUT 1000
K_MUTEX_DEFINE(shared_data_mutex);
static int g_vehicle_speed = -1;

/* Formatting strings for sending sensor JSON to Golioth */
/* clang-format off */
#define JSON_FMT \
"{" \
	"\"time\":\"%s\"," \
	"\"gps\":" \
	"{" \
		"\"lat\":%s," \
		"\"lon\":%s," \
		"\"fake\":%s" \
	"}," \
	"\"vehicle\":" \
	"{" \
		"\"speed\":%d" \
	"}" \
"}"
#define JSON_FMT_FAKE_GPS \
"{" \
	"\"gps\":" \
	"{" \
		"\"lat\":%s," \
		"\"lon\":%s," \
		"\"fake\":%s" \
	"}," \
	"\"vehicle\":" \
	"{" \
		"\"speed\":%d" \
	"}" \
"}"
/* clang-format on */

/**
 * Convert a floating point coordinate value to a minmea_float coordinate.
 *
 * NMEA latitude is represented as [+-]DDMM.MMMM... [-90.0, 90.0]
 * NMEA longitude is represented as [+-]DDDMM.MMMM... [-180.0, 180.0]
 *
 * For example, a float -123.456789 will be be converted to:
 *   degrees = -123
 *   minutes = -0.456789 * 60 = -27.40734
 *   NMEA = (degrees * 100) + minutes = -12327.40734
 *
 * minmea_float stores the .value as a int_least32_t with a .scale factor:
 *   .value = (int_least32_t)(NMEA * .scale)
 *
 * So, NMEA -12327.40734 will be converted to:
 *   .value = -1232740734
 *   .scale = 100000
 *
 * 100000 scaling factor provides 5 digits of precision (±2cm LSB at the
 * equator) for the minutes value.
 *
 * Note: 5 digits of precision is the max we can use. If we were to try to use
 * 6 digits of precision (a scaling factor of 1000000), NMEA -12327.40734 would
 * be converted to .value = -12327407340, which would overflow int32_t:
 *   INT32_MIN =  -2147483648
 *   .value    = -12327407340 <- overflow!
 */
static inline int coord_to_minmea(struct minmea_float *f, float coord)
{
	int32_t degrees = (int32_t)coord;
	float minutes = (coord - degrees) * 60;

	/* Convert degrees to NMEA [+-]DDDMM.MMMMM format */
	degrees *= 100;

	/**
	 * Use 100000 as the scaling factor so that we can store up to 5 decimal
	 * places of minutes in minmea_float.value
	 */
	degrees *= 100000;
	minutes *= 100000;

	/* Make sure we don't overflow int32_t */
	if (minutes < INT32_MIN || minutes > (float)(INT32_MAX - 1)) {
		return -ERANGE;
	}

	/* minmea_float uses int_least32_t internally */
	f->value = (int_least32_t)(degrees + minutes);
	f->scale = 100000;

	return 0;
}

void process_can_frames_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	int err;
	int can_filter_id;
	struct can_frame can_frame;
	const struct can_filter can_filter = {
		.flags = CAN_FILTER_DATA, .id = OBD2_PID_RESPONSE_ID, .mask = CAN_STD_ID_MASK};
	struct can_frame vehicle_speed_request = {
		.flags = 0,
		.id = OBD2_PID_REQUEST_ID,
		.dlc = 8,
		.data = {ODB2_PID_REQUEST_DATA_LENGTH, OBD2_SERVICE_SHOW_CURRENT_DATA,
			 ODB2_PID_VEHICLE_SPEED, 0xCC, /* not used (ISO 15765-2 suggests 0xCC) */
			 0xCC, 0xCC, 0xCC, 0xCC}};
	int vehicle_speed;
	uint8_t data_len;

	/* Automatically put frames matching can_filter into can_msgq */
	can_filter_id = can_add_rx_filter_msgq(can_dev, &can_msgq, &can_filter);
	if (can_filter_id == -ENOSPC) {
		LOG_ERR("No free CAN filters [%d]", can_filter_id);
		return;
	} else if (can_filter_id == -ENOTSUP) {
		LOG_ERR("CAN filter type not supported [%d]", can_filter_id);
		return;
	} else if (can_filter_id != 0) {
		LOG_ERR("Error adding a message queue for the given filter [%d]", can_filter_id);
		return;
	}
	LOG_DBG("CAN bus receive filter id: %d", can_filter_id);

	while (1) {
		vehicle_speed = -1;

		/* This sending call is blocking until the message is sent. */
		err = can_send(can_dev, &vehicle_speed_request, K_MSEC(100), NULL, NULL);
		if (err) {
			LOG_ERR("Error sending CAN frame: %d", err);
		} else {
			/* Wait up to 500ms for a response (possibly multiple responses in queue) */
			while (k_msgq_get(&can_msgq, &can_frame, K_MSEC(500)) == 0) {
				data_len = can_dlc_to_bytes(can_frame.dlc);
				if ((data_len != OBD2_PID_RESPONSE_DLC) &&
				    (data_len != ODB2_PID_VEHICLE_SPEED_DLC)) {
					LOG_ERR("Wrong CAN frame data length: %u", data_len);
					continue;
				}

				if ((can_frame.data[1] ==
				     (OBD2_SERVICE_SHOW_CURRENT_DATA + 0x40)) &&
				    (can_frame.data[2] == ODB2_PID_VEHICLE_SPEED)) {
					vehicle_speed = can_frame.data[3];
				}
			}
		}

		/* Update shared global state */
		err = k_mutex_lock(&shared_data_mutex, K_MSEC(SHARED_DATA_MUTEX_TIMEOUT));
		if (err) {
			LOG_ERR("Error locking shared data mutex (lock count: %u): %d", err,
				shared_data_mutex.lock_count);
			k_sleep(K_SECONDS(get_vehicle_speed_delay_s()));
			continue;
		}
		g_vehicle_speed = vehicle_speed;
		k_mutex_unlock(&shared_data_mutex);

		/* Log vehicle speed */
		LOG_DBG("Vehicle Speed Sensor: %d km/h", vehicle_speed);

		/* Update Ostentus slide values */
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			char vehicle_speed_str[9];

			snprintk(vehicle_speed_str, sizeof(vehicle_speed_str), "%d km/h",
				 vehicle_speed);
			slide_set(VEHICLE_SPEED, vehicle_speed_str, strlen(vehicle_speed_str));
		));

		k_sleep(K_SECONDS(get_vehicle_speed_delay_s()));
	}
}

void process_rmc_frames_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	int err;
	struct minmea_sentence_rmc rmc_frame;
	struct can_asset_tracker_data cat_frame;

	while (k_msgq_get(&rmc_msgq, &rmc_frame, K_FOREVER) == 0) {
		cat_frame.rmc_frame = rmc_frame;

		/* Use the latest vehicle speed reading received from the ECU */
		err = k_mutex_lock(&shared_data_mutex, K_MSEC(SHARED_DATA_MUTEX_TIMEOUT));
		if (err) {
			LOG_ERR("Error locking shared data mutex (lock count: %u): %d", err,
				shared_data_mutex.lock_count);
		}
		cat_frame.vehicle_speed = g_vehicle_speed;
		k_mutex_unlock(&shared_data_mutex);

		err = k_msgq_put(&cat_msgq, &cat_frame, K_NO_WAIT);
		if (err) {
			LOG_ERR("Unable to add cat_frame to cat_msgq: %d", err);
		}

		LOG_DBG("GPS Position%s: %f, %f", cat_frame.rmc_frame.valid ? "" : " (fake)",
			minmea_tocoord(&rmc_frame.latitude), minmea_tocoord(&rmc_frame.longitude));

		/* Update Ostentus slide values */
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			char lat_str[12];
			char lon_str[12];

			snprintk(lat_str, sizeof(lat_str), "%f",
				 minmea_tocoord(&rmc_frame.latitude));
			snprintk(lon_str, sizeof(lon_str), "%f",
				 minmea_tocoord(&rmc_frame.longitude));
			slide_set(LATITUDE, lat_str, strlen(lat_str));
			slide_set(LONGITUDE, lon_str, strlen(lon_str));
		));
	}
}

/* This is called from the UART irq callback to try to get out fast */
static void process_reading(char *raw_nmea)
{
	/* _last_gps timestamp records when the previous GPS value was stored */
	static uint64_t _last_gps;
	enum minmea_sentence_id sid;
	sid = minmea_sentence_id(raw_nmea, false);
	if (sid == MINMEA_SENTENCE_RMC) {
		struct minmea_sentence_rmc rmc_frame;
		bool success = minmea_parse_rmc(&rmc_frame, raw_nmea);
		if (success) {
			uint64_t wait_for = _last_gps;
			if (k_uptime_delta(&wait_for) >= ((uint64_t)get_gps_delay_s() * 1000)) {
				if (rmc_frame.valid == true) {
					/* if queue is full, message is silently dropped */
					k_msgq_put(&rmc_msgq, &rmc_frame, K_NO_WAIT);

					/*
					 * wait_for now contains the current timestamp. Store this
					 * for the next reading.
					 */
					_last_gps = wait_for;
				} else {
					if (get_fake_gps_enabled_s() == true) {
						/* use fake GPS coordinates from LightDB state */
						coord_to_minmea(&rmc_frame.latitude,
								get_fake_gps_latitude_s());
						coord_to_minmea(&rmc_frame.longitude,
								get_fake_gps_longitude_s());
						k_msgq_put(&rmc_msgq, &rmc_frame, K_NO_WAIT);

						/*
						 * wait_for now contains the current timestamp.
						 * Store this for the next reading.
						 */
						_last_gps = wait_for;
					}
				}
			} else {
				/* LOG_DBG("Ignoring reading due to gps_delay_s window"); */
			}
		}
	}
}

/* UART callback */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;
	static char rx_buf[NMEA_SIZE];
	static int rx_buf_pos;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	while (uart_irq_rx_ready(uart_dev)) {

		uart_fifo_read(uart_dev, &c, 1);

		if ((c == '\n') && rx_buf_pos > 0) {
			/* terminate string */
			if (rx_buf_pos == (NMEA_SIZE - 1)) {
				rx_buf[rx_buf_pos] = '\0';
			} else {
				rx_buf[rx_buf_pos] = '\n';
				rx_buf[rx_buf_pos + 1] = '\0';
			}

			process_reading(rx_buf);
			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

void app_sensors_init(void)
{
	int err;

	LOG_DBG("Initializing GNSS receiver");

	err = gpio_pin_configure_dt(&gnss7_sel, GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		LOG_ERR("Unable to configure GNSS SEL Pin: %d", err);
	}

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device %s not ready", uart_dev->name);
	}

	/* Configure UART interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	uart_irq_rx_enable(uart_dev);

	LOG_DBG("Initializing CAN controller");

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device %s not ready", can_dev->name);
	}

	/* Start the CAN controller */
	err = can_start(can_dev);
	if (err == -EALREADY) {
		LOG_ERR("CAN controller already started [%d]", err);
	} else if (err != 0) {
		LOG_ERR("Error starting CAN controller [%d]", err);
	}

	/* Spawn a thread to process CAN frames */
	process_can_frames_tid = k_thread_create(
		&process_can_frames_thread_data, process_can_frames_thread_stack,
		K_THREAD_STACK_SIZEOF(process_can_frames_thread_stack), process_can_frames_thread,
		NULL, NULL, NULL, PROCESS_CAN_FRAMES_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!process_can_frames_tid) {
		LOG_ERR("Error spawning CAN frame processing thread");
	}

	/* Spawn a thread to process RMC frames */
	process_rmc_frames_tid = k_thread_create(
		&process_rmc_frames_thread_data, process_rmc_frames_thread_stack,
		K_THREAD_STACK_SIZEOF(process_rmc_frames_thread_stack), process_rmc_frames_thread,
		NULL, NULL, NULL, PROCESS_RMC_FRAMES_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!process_rmc_frames_tid) {
		LOG_ERR("Error spawning RMC frame processing thread");
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_sensors_read_and_stream(void)
{
	int err;
	struct can_asset_tracker_data cached_data;
	char json_buf[256];
	char ts_str[32];
	char lat_str[12];
	char lon_str[12];

	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_and_report_battery(client);
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			slide_set(BATTERY_V, get_batt_v_str(), strlen(get_batt_v_str()));
			slide_set(BATTERY_LVL, get_batt_lvl_str(), strlen(get_batt_lvl_str()));
		));
	));

	if (golioth_client_is_connected(client)) {
		while (k_msgq_get(&cat_msgq, &cached_data, K_NO_WAIT) == 0) {
			snprintk(lat_str, sizeof(lat_str), "%f",
				 minmea_tocoord(&cached_data.rmc_frame.latitude));
			snprintk(lon_str, sizeof(lon_str), "%f",
				 minmea_tocoord(&cached_data.rmc_frame.longitude));
			snprintk(ts_str, sizeof(ts_str), "20%02d-%02d-%02dT%02d:%02d:%02d.%03dZ",
				 cached_data.rmc_frame.date.year, cached_data.rmc_frame.date.month,
				 cached_data.rmc_frame.date.day, cached_data.rmc_frame.time.hours,
				 cached_data.rmc_frame.time.minutes,
				 cached_data.rmc_frame.time.seconds,
				 cached_data.rmc_frame.time.microseconds);

			if (cached_data.rmc_frame.valid == true) {
				/*
				 * `time` will not appear in the `data` payload once received
				 * by Golioth LightDB Stream, but instead will override the
				 * `time` timestamp of the data.
				 */
				snprintk(json_buf, sizeof(json_buf), JSON_FMT, ts_str, lat_str,
					 lon_str, "false", cached_data.vehicle_speed);
			} else { /* Fake GPS data does not have a `time` field */
				snprintk(json_buf, sizeof(json_buf), JSON_FMT_FAKE_GPS, lat_str,
					 lon_str, "true", cached_data.vehicle_speed);
			}

			err = golioth_stream_set_sync(client, "tracker", GOLIOTH_CONTENT_TYPE_JSON,
						      json_buf, strlen(json_buf),
						      GOLIOTH_STREAM_TIMEOUT_S);
			if (err)
				LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		}
	}
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
	client = sensors_client;
}
