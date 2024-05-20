/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Process changes received from the Golioth Settings Service and return a code
 * to Golioth to indicate the success or failure of the update.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <golioth/client.h>

int32_t get_loop_delay_s(void);
void app_settings_register(struct golioth_client *client);
int32_t get_gps_delay_s(void);
bool get_fake_gps_enabled_s(void);
float get_fake_gps_latitude_s(void);
float get_fake_gps_longitude_s(void);
int32_t get_vehicle_speed_delay_s(void);

#endif /* __APP_SETTINGS_H__ */
