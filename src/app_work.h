/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

#define O_LABEL_LAT "Latitude"
#define O_LABEL_LON "Longitude"
#define O_LABEL_VEHICLE_SPEED "Speed"
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#define O_LABEL_BATTERY "Battery"
#endif
#define O_LABEL_FIRMWARE "Firmware"
#define O_SUMMARY_TITLE "Asset Tracker"

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	O_LAT,
	O_LON,
	O_VEHICLE_SPEED,
	#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
	O_BATTERY_V,
	O_BATTERY_LVL,
	#endif
	O_FIRMWARE
} slide_key;

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);

#endif /* __APP_WORK_H__ */
