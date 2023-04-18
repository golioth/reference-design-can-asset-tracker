/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>

#include "app_work.h"
#include "app_settings.h"
#include "libostentus/libostentus.h"
#include "lib/minmea/minmea.h"
#include <stdio.h>

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

#define UART_DEVICE_NODE DT_ALIAS(click_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define UART_SEL DT_ALIAS(gnss7_sel)
static const struct gpio_dt_spec gnss7_sel = GPIO_DT_SPEC_GET(UART_SEL, gpios);

#define NMEA_SIZE 128

struct cold_chain_data {
	struct sensor_value tem;
	struct sensor_value pre;
	struct sensor_value hum;
	struct minmea_sentence_rmc frame;
};

struct weather_data {
	struct sensor_value tem;
	struct sensor_value pre;
	struct sensor_value hum;
};

/* Global timestamp records when the previous GPS value was stored */
uint64_t _last_gps = 0;

/* Global to hold BME280 readings; updated at 1 Hz by thread */
struct weather_data _latest_weather_data;

K_MSGQ_DEFINE(nmea_msgq, sizeof(struct cold_chain_data), 64, 4);

static char rx_buf[NMEA_SIZE];
static int rx_buf_pos;

static struct golioth_client *client;
/* Add Sensor structs here */
const struct device *weather_dev;

/* Thread reads weather sensor and provides easy access to latest data */
K_MUTEX_DEFINE(weather_mutex); /* Protect data */
K_SEM_DEFINE(bme280_initialized_sem, 0, 1); /* Wait until sensor is ready */

void weather_sensor_data_fetch(void) {
	if (!weather_dev) {
		return;
	}
	sensor_sample_fetch(weather_dev);
	if (k_mutex_lock(&weather_mutex, K_MSEC(100)) == 0) {
		sensor_channel_get(weather_dev, SENSOR_CHAN_AMBIENT_TEMP, &_latest_weather_data.tem);
		sensor_channel_get(weather_dev, SENSOR_CHAN_PRESS, &_latest_weather_data.pre);
		sensor_channel_get(weather_dev, SENSOR_CHAN_HUMIDITY, &_latest_weather_data.hum);
		k_mutex_unlock(&weather_mutex);
	} else {
		LOG_DBG("Unable to lock mutex to read weather sensor");
	}
}

#define WEATHER_STACK 1024

extern void weather_sensor_thread(void *d0, void *d1, void *d2) {
	/* Block until sensor is available */
	k_sem_take(&bme280_initialized_sem, K_FOREVER);
	while(1) {
		weather_sensor_data_fetch();
		k_sleep(K_SECONDS(1));
	}
}

K_THREAD_DEFINE(weather_sensor_tid, WEATHER_STACK,
            weather_sensor_thread, NULL, NULL, NULL,
            K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

/* This is called from the UART irq callback to try to get out fast */
static void process_reading(char *raw_nmea) {
	enum minmea_sentence_id sid;
	sid = minmea_sentence_id(raw_nmea, false);
	if (sid == MINMEA_SENTENCE_RMC) {
		struct cold_chain_data cc_data;
		bool success = minmea_parse_rmc(&cc_data.frame, raw_nmea);
		if (success) {
			uint64_t wait_for = _last_gps;
			if (k_uptime_delta(&wait_for) >= ((uint64_t)get_gps_delay_s() * 1000)) {
				if (cc_data.frame.valid == true) {
					if (k_mutex_lock(&weather_mutex, K_MSEC(1)) == 0) {
						cc_data.tem = _latest_weather_data.tem;
						cc_data.pre = _latest_weather_data.pre;
						cc_data.hum = _latest_weather_data.hum;
						k_mutex_unlock(&weather_mutex);
						/* if queue is full, message is silently dropped */
						k_msgq_put(&nmea_msgq, &cc_data, K_NO_WAIT);

						/* wait_for now contains the current timestamp. Store this
						 * for the next reading. */
						_last_gps = wait_for;
					} else {
						LOG_ERR("Couldn't read weather info, skipping this reading");
					}
				} else {
					LOG_DBG("Skipping because satellite fix not established");
				}
			} else {
				LOG_DBG("Ignoring reading due to gps_delay_s window");
			}
		}
	}
}

/* UART callback */
void serial_cb(const struct device *dev, void *user_data) {
	uint8_t c;

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

/*
 * Get a device structure from a devicetree node with compatible
 * "bosch,bme280". (If there are multiple, just pick one.)
 */
static const struct device *get_bme280_device(void)
{
	const struct device *const bme_dev = DEVICE_DT_GET_ANY(bosch_bme280);

	if (bme_dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("\nError: no device found.");
		return NULL;
	}

	if (!device_is_ready(bme_dev)) {
		LOG_ERR("Error: Device \"%s\" is not ready; "
		       "check the driver initialization logs for errors.",
		       bme_dev->name);
		return NULL;
	}

	LOG_DBG("Found device \"%s\", getting sensor data", bme_dev->name);

	/* Give semaphore to signal sensor is ready for reading */
	k_sem_give(&bme280_initialized_sem);
	return bme_dev;
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	struct cold_chain_data cached_data;
	char json_buf[128];
	char ts_str[32];
	char lat_str[12];
	char lon_str[12];
	char tem_str[12];

	/* Log battery levels if possible */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (log_battery_info();));

	while (k_msgq_get(&nmea_msgq, &cached_data, K_NO_WAIT) == 0) {

		snprintf(lat_str, sizeof(lat_str), "%f", minmea_tocoord(&cached_data.frame.latitude));
		snprintf(lon_str, sizeof(lon_str), "%f", minmea_tocoord(&cached_data.frame.longitude));
		snprintf(ts_str, sizeof(ts_str), "20%02d-%02d-%02dT%02d:%02d:%02d.%03dZ",
				cached_data.frame.date.year,
				cached_data.frame.date.month,
				cached_data.frame.date.day,
				cached_data.frame.time.hours,
				cached_data.frame.time.minutes,
				cached_data.frame.time.seconds,
				cached_data.frame.time.microseconds
				);
		snprintk(tem_str, sizeof(tem_str), "%d.%02dc", cached_data.tem.val1, cached_data.tem.val2 / 10000);

		snprintk(json_buf, sizeof(json_buf),
				"{\"lat\":%s,\"lon\":%s,\"time\":\"%s\",\"tem\":%d.%06d,\"pre\":%d.%06d,\"hum\":%d.%06d}",
				lat_str,
				lon_str,
				ts_str,
				cached_data.tem.val1, cached_data.tem.val2,
				cached_data.pre.val1, cached_data.pre.val2,
				cached_data.hum.val1, cached_data.hum.val2
				);
		LOG_DBG("%s", json_buf);
		slide_set(O_LAT, lat_str, strlen(lat_str));
		slide_set(O_LON, lon_str, strlen(lon_str));
		slide_set(O_TEM, tem_str, strlen(tem_str));

		err = golioth_stream_push(client, "gps",
				GOLIOTH_CONTENT_FORMAT_APP_JSON,
				json_buf, strlen(json_buf));
		if (err) LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}
}

void app_work_init(struct golioth_client* work_client) {
	LOG_INF("Initializing UART");
	client = work_client;

	int err = gpio_pin_configure_dt(&gnss7_sel, GPIO_OUTPUT_ACTIVE);

	if (err < 0) {
		LOG_ERR("Unable to configure GNSS SEL Pin: %d", err);
	}

	/* configure interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	uart_irq_rx_enable(uart_dev);

	weather_dev = get_bme280_device();
	weather_sensor_data_fetch();
}
