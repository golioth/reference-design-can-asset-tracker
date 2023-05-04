/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <net/golioth/system_client.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/byteorder.h>

#include "app_work.h"
#include "app_settings.h"
#include "libostentus/libostentus.h"
#include "lib/minmea/minmea.h"
#include <stdio.h>

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

#define NMEA_SIZE 128
#define VEHICLE_SPEED_ID 0x244
#define VEHICLE_SPEED_DLC 5

#define UART_DEVICE_NODE DT_ALIAS(click_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define UART_SEL DT_ALIAS(gnss7_sel)
static const struct gpio_dt_spec gnss7_sel = GPIO_DT_SPEC_GET(UART_SEL, gpios);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static struct golioth_client *client;
struct can_asset_tracker_data {
	struct minmea_sentence_rmc rmc_frame;
	float mph;
	float mph_max;
	float mph_min;
	float mph_avg;
};

K_MSGQ_DEFINE(cat_msgq, sizeof(struct can_asset_tracker_data), 64, 4);
K_MSGQ_DEFINE(rmc_msgq, sizeof(struct minmea_sentence_rmc), 2, 4);
CAN_MSGQ_DEFINE(can_msgq, 2);

K_MUTEX_DEFINE(shared_data_mutex);

#define PROCESS_CAN_FRAMES_THREAD_STACK_SIZE 2048
#define PROCESS_CAN_FRAMES_THREAD_PRIORITY 2
struct k_thread process_can_frames_thread_data;
K_THREAD_STACK_DEFINE(process_can_frames_thread_stack, PROCESS_CAN_FRAMES_THREAD_STACK_SIZE);

#define PROCESS_RMC_FRAMES_THREAD_STACK_SIZE 2048
#define PROCESS_RMC_FRAMES_THREAD_PRIORITY 2
struct k_thread process_rmc_frames_thread_data;
K_THREAD_STACK_DEFINE(process_rmc_frames_thread_stack, PROCESS_RMC_FRAMES_THREAD_STACK_SIZE);

/* Global variables shared between threads */
static int g_can_frame_counter;
static float g_mph;
static float g_mph_max;
static float g_mph_min;
static float g_mph_avg;

void process_can_frames_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	int can_filter_id;
	float mph;
	uint16_t kmh;
	struct can_frame can_frame;
	const struct can_filter can_filter = {
		.flags = CAN_FILTER_DATA | CAN_FILTER_IDE,
		.id = VEHICLE_SPEED_ID,
		.mask = CAN_EXT_ID_MASK
	};

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

	/* Initialize shared global variables */
	k_mutex_lock(&shared_data_mutex, K_FOREVER);
	g_can_frame_counter = 0;
	g_mph = 0;
	g_mph_max = 0;
	g_mph_min = 0;
	g_mph_avg = 0;
	k_mutex_unlock(&shared_data_mutex);

	while (k_msgq_get(&can_msgq, &can_frame, K_FOREVER) == 0) {
		if (can_frame.dlc != VEHICLE_SPEED_DLC) {
			LOG_ERR("Wrong CAN frame data length: %u",
				can_frame.dlc);
			continue;
		}

		/* Two decimals of precision (xxx.xx km/h) stored in uint16_t */
		kmh = sys_be16_to_cpu(UNALIGNED_GET(
			(uint16_t *)&can_frame.data[3])) / 100;
		mph = kmh * 0.6213751;

		/* Update shared global variables */
		k_mutex_lock(&shared_data_mutex, K_FOREVER);
		g_mph = mph;
		if (g_can_frame_counter == 0) {
			g_mph_max = mph;
			g_mph_min = mph;
			g_mph_avg = mph;
			g_can_frame_counter++;
		} else {
			g_mph_max = MAX(mph, g_mph_max);
			g_mph_min = MIN(mph, g_mph_min);
			/* Calculate the incremental average */
			g_mph_avg += (mph - g_mph_avg) / ++g_can_frame_counter;
		}
		k_mutex_unlock(&shared_data_mutex);
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
	char lat_str[12];
	char lon_str[12];
	char mph_str[12];

	while (k_msgq_get(&rmc_msgq, &rmc_frame, K_FOREVER) == 0) {
		cat_frame.rmc_frame = rmc_frame;

		k_mutex_lock(&shared_data_mutex, K_FOREVER);
		cat_frame.mph = g_mph;
		cat_frame.mph_max = g_mph_max;
		cat_frame.mph_min = g_mph_min;
		cat_frame.mph_avg = g_mph_avg;
		g_can_frame_counter = 0;
		g_mph = 0;
		g_mph_max = 0;
		g_mph_min = 0;
		g_mph_avg = 0;
		k_mutex_unlock(&shared_data_mutex);

		err = k_msgq_put(&cat_msgq, &cat_frame, K_NO_WAIT);
		if (err) {
			LOG_ERR("Unable to add cat_frame to cat_msgq: %d", err);
		}

		/* Update Ostentus slide values */
		snprintf(lat_str, sizeof(lat_str), "%f",
			minmea_tocoord(&rmc_frame.latitude));
		snprintf(lon_str, sizeof(lon_str), "%f",
			minmea_tocoord(&rmc_frame.longitude));
		snprintf(mph_str, sizeof(mph_str), "%f", cat_frame.mph);

		slide_set(O_LAT, lat_str, strlen(lat_str));
		slide_set(O_LON, lon_str, strlen(lon_str));
		slide_set(O_MPH, mph_str, strlen(mph_str));
	}
}

/* This is called from the UART irq callback to try to get out fast */
static void process_reading(char *raw_nmea) {
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
					LOG_DBG("Skipping because satellite fix not established");
				}
			} else {
				/* LOG_DBG("Ignoring reading due to gps_delay_s window"); */
			}
		}
	}
}

/* UART callback */
void serial_cb(const struct device *dev, void *user_data) {
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
				rx_buf[rx_buf_pos+1] = '\0';
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

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	struct can_asset_tracker_data cached_data;
	char json_buf[256];
	char ts_str[32];
	char lat_str[12];
	char lon_str[12];

	/* Log battery levels if possible */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (log_battery_info();));

	while (k_msgq_get(&cat_msgq, &cached_data, K_NO_WAIT) == 0) {
		snprintf(lat_str, sizeof(lat_str), "%f",
			minmea_tocoord(&cached_data.rmc_frame.latitude));
		snprintf(lon_str, sizeof(lon_str), "%f",
			minmea_tocoord(&cached_data.rmc_frame.longitude));
		snprintf(ts_str, sizeof(ts_str), "20%02d-%02d-%02dT%02d:%02d:%02d.%03dZ",
			cached_data.rmc_frame.date.year,
			cached_data.rmc_frame.date.month,
			cached_data.rmc_frame.date.day,
			cached_data.rmc_frame.time.hours,
			cached_data.rmc_frame.time.minutes,
			cached_data.rmc_frame.time.seconds,
			cached_data.rmc_frame.time.microseconds);

		/*
		 * `time` will not appear in the `data` payload once received
		 * by Golioth LightDB Stream, but instead will override the
		 * `time` timestamp of the data.
		 */
		snprintk(json_buf, sizeof(json_buf),
			"{\"time\":\"%s\",\"gps\":{\"lat\":%s,\"lon\":%s},\"speed\":{\"mph\":%.2f,\"mph_max\":%.2f,\"mph_min\":%.2f,\"mph_avg\":%.2f}}",
			ts_str,
			lat_str,
			lon_str,
			cached_data.mph,
			cached_data.mph_max,
			cached_data.mph_min,
			cached_data.mph_avg);
		LOG_DBG("%s", json_buf);

		err = golioth_stream_push(client, "vehicle",
			GOLIOTH_CONTENT_FORMAT_APP_JSON,
			json_buf, strlen(json_buf));
		if (err) LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}
}

void app_work_init(struct golioth_client* work_client) {
	int err;
	k_tid_t process_can_frames_tid;
	k_tid_t process_rmc_frames_tid;

	client = work_client;

	LOG_INF("Initializing GNSS receiver");

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

	LOG_INF("Initializing CAN controller");

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
	process_can_frames_tid = k_thread_create(&process_can_frames_thread_data,
		process_can_frames_thread_stack,
		K_THREAD_STACK_SIZEOF(process_can_frames_thread_stack),
		process_can_frames_thread, NULL, NULL, NULL,
		PROCESS_CAN_FRAMES_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!process_can_frames_tid) {
		LOG_ERR("Error spawning CAN frame processing thread");
	}

	/* Spawn a thread to process RMC frames */
	process_rmc_frames_tid = k_thread_create(&process_rmc_frames_thread_data,
		process_rmc_frames_thread_stack,
		K_THREAD_STACK_SIZEOF(process_rmc_frames_thread_stack),
		process_rmc_frames_thread, NULL, NULL, NULL,
		PROCESS_RMC_FRAMES_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!process_rmc_frames_tid) {
		LOG_ERR("Error spawning RMC frame processing thread");
	}
}
