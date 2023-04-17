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

struct asset_tracker_data {
	struct minmea_sentence_rmc frame;
};

/* Global timestamp records when the previous GPS value was stored */
uint64_t _last_gps = 0;

K_MSGQ_DEFINE(nmea_msgq, sizeof(struct asset_tracker_data), 64, 4);

static char rx_buf[NMEA_SIZE];
static int rx_buf_pos;

static struct golioth_client *client;
/* Add Sensor structs here */

/* This is called from the UART irq callback to try to get out fast */
static void process_reading(char *raw_nmea) {
	enum minmea_sentence_id sid;
	sid = minmea_sentence_id(raw_nmea, false);
	if (sid == MINMEA_SENTENCE_RMC) {
		struct asset_tracker_data at_data;
		bool success = minmea_parse_rmc(&at_data.frame, raw_nmea);
		if (success) {
			uint64_t wait_for = _last_gps;
			if (k_uptime_delta(&wait_for) >= ((uint64_t)get_gps_delay_s() * 1000)) {
				if (at_data.frame.valid == true) {
					LOG_DBG("Adding GPS frame to nmea_msgq");

					/* if queue is full, message is silently dropped */
					k_msgq_put(&nmea_msgq, &at_data, K_NO_WAIT);

					/* wait_for now contains the current timestamp. Store this
					 * for the next reading.
					 */
					_last_gps = wait_for;
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

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	struct asset_tracker_data cached_data;
	char json_buf[128];
	char ts_str[32];
	char lat_str[12];
	char lon_str[12];

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

		snprintk(json_buf, sizeof(json_buf),
				"{\"lat\":%s,\"lon\":%s,\"time\":\"%s\"}",
				lat_str,
				lon_str,
				ts_str
				);
		LOG_DBG("%s", json_buf);
		slide_set(O_LAT, lat_str, strlen(lat_str));
		slide_set(O_LON, lon_str, strlen(lon_str));

		LOG_DBG("Sending GPS data to Golioth LightDB Stream");
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
}
