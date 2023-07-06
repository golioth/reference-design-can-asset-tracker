/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

#define LABEL_LATITUDE	    "Latitude"
#define LABEL_LONGITUDE	    "Longitude"
#define LABEL_VEHICLE_SPEED "Speed"
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#define LABEL_BATTERY "Battery"
#endif
#define LABEL_FIRMWARE "Firmware"
#define SUMMARY_TITLE  "Asset Tracker"

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	LATITUDE,
	LONGITUDE,
	VEHICLE_SPEED,
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
	BATTERY_V,
	BATTERY_LVL,
#endif
	FIRMWARE
} slide_key;

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);

#endif /* __APP_WORK_H__ */
