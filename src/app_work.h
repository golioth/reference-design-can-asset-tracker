/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
    O_LAT,
    O_LON,
    O_TEM
} slide_key;

#define O_LABEL_LAT "Latitude"
#define O_LABEL_LON "Longitude"
#define O_LABEL_TEM "Temperature"
#define O_SUMMARY_TITLE "Cold Chain"

#endif /* __APP_WORK_H__ */
