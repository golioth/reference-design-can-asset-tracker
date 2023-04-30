/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include "app_settings.h"
#include "app_gnss.h"
#include "lib/minmea/minmea.h"

LOG_MODULE_REGISTER(app_gnss, LOG_LEVEL_DBG);

#define UART_DEVICE_NODE DT_ALIAS(click_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define UART_SEL DT_ALIAS(gnss7_sel)
static const struct gpio_dt_spec gnss7_sel = GPIO_DT_SPEC_GET(UART_SEL, gpios);

#define NMEA_SIZE 128

K_MSGQ_DEFINE(rmc_msgq, sizeof(struct rmc_msg), 64, 4);

/* This is called from the UART irq callback to try to get out fast */
static void process_reading(char *raw_nmea)
{
	static uint64_t last_gps;
	enum minmea_sentence_id sid;
	bool success;
	uint64_t wait_for;
	struct rmc_msg msg;

	sid = minmea_sentence_id(raw_nmea, false);
	if (sid == MINMEA_SENTENCE_RMC) {
		success = minmea_parse_rmc(&msg.frame, raw_nmea);
		if (success) {
			wait_for = last_gps;
			if (k_uptime_delta(&wait_for) >= ((uint64_t)get_gps_delay_s() * 1000)) {
				if (msg.frame.valid == true) {
					msg.timestamp = k_uptime_get();
					/* if queue is full, message is silently dropped */
					k_msgq_put(&rmc_msgq, &msg, K_NO_WAIT);

					/* wait_for now contains the current timestamp. Store this
					 * for the next reading.
					 */
					last_gps = wait_for;
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

int gnss_init(void)
{
	int err;

	LOG_INF("Initializing GNSS");

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device %s not ready", uart_dev->name);
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&gnss7_sel, GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		LOG_ERR("Unable to configure GNSS SEL Pin: %d", err);
		return err;
	}

	/* configure interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	uart_irq_rx_enable(uart_dev);

	return 0;
}
