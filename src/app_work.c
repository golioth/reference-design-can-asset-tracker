/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/logging/log.h>
#include <net/golioth/system_client.h>

#include "app_work.h"
#include "app_settings.h"
#include "libostentus/libostentus.h"
#include "app_gnss.h"
#include "lib/minmea/minmea.h"
#include "app_can.h"

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

static struct golioth_client *client;

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	struct rmc_msg cached_rmc_msg;
	struct can_msg cached_can_msg;
	char json_buf[128];
	char ts_str[32];
	char lat_str[12];
	char lon_str[12];
	int can_frame_count;
	uint16_t kmh, mph, mph_max, mph_min, mph_avg;

	/* Log battery levels if possible */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (log_battery_info();));

	while (k_msgq_get(&rmc_msgq, &cached_rmc_msg, K_NO_WAIT) == 0) {
		snprintf(lat_str, sizeof(lat_str), "%f",
			minmea_tocoord(&cached_rmc_msg.frame.latitude));
		snprintf(lon_str, sizeof(lon_str), "%f",
			minmea_tocoord(&cached_rmc_msg.frame.longitude));
		snprintf(ts_str, sizeof(ts_str),
			"20%02d-%02d-%02dT%02d:%02d:%02d.%03dZ",
			cached_rmc_msg.frame.date.year,
			cached_rmc_msg.frame.date.month,
			cached_rmc_msg.frame.date.day,
			cached_rmc_msg.frame.time.hours,
			cached_rmc_msg.frame.time.minutes,
			cached_rmc_msg.frame.time.seconds,
			cached_rmc_msg.frame.time.microseconds);

		can_frame_count = 0;
		kmh = 0;
		mph = 0;
		mph_max = 0;
		mph_min = UINT16_MAX;
		mph_avg = 0;
		while (k_msgq_peek(&can_msgq, &cached_can_msg) == 0) {
			/* Check if the CAN frame was received after the RMC frame */
			if (cached_can_msg.timestamp > cached_rmc_msg.timestamp) {
				/* Stop processing CAN frames until next RMC frame */
				/* Peek'd CAN frame is left in the queue */
				break;
			}

			/* Remove the CAN frame from the queue for processing */
			err = k_msgq_get(&can_msgq, &cached_can_msg, K_NO_WAIT);
			if (err) {
				LOG_ERR("Unable to get cached CAN message: %d", err);
				/* Try to process the next frame in the queue */
				continue;
			}

			if (cached_can_msg.frame.dlc != 5U) {
				LOG_ERR("Wrong CAN frame data length: %u",
					cached_can_msg.frame.dlc);
				/* Try to process the next frame in the queue */
				continue;
			}

			kmh = sys_be16_to_cpu(UNALIGNED_GET(
				(uint16_t *)&cached_can_msg.frame.data[3])) / 100;
			mph = (uint16_t)(kmh * 0.6213751);

			mph_max = MAX(mph, mph_max);
			mph_min = MIN(mph, mph_min);
			mph_avg += mph;
			can_frame_count++;
		}
		if (can_frame_count > 1)
			mph_avg /= can_frame_count;

		snprintk(json_buf, sizeof(json_buf),
			"{\"lat\":%s,\"lon\":%s,\"time\":\"%s\",\"mph_max\":\"%u\",\"mph_min\":\"%u\",\"mph_avg\":\"%u\"}",
			lat_str,
			lon_str,
			ts_str,
			mph_max,
			mph_min,
			mph_avg);
		LOG_DBG("%s", json_buf);
		slide_set(O_LAT, lat_str, strlen(lat_str));
		slide_set(O_LON, lon_str, strlen(lon_str));

		err = golioth_stream_push(client, "position",
				GOLIOTH_CONTENT_FORMAT_APP_JSON,
				json_buf, strlen(json_buf));
		if (err) LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
}
