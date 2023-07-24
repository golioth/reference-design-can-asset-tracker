/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

/**
 * Process changes received from the Golioth Settings Service and return a code
 * to Golioth to indicate the success or failure of the update.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service
 */

#include <stdint.h>
#include <net/golioth/system_client.h>

int32_t get_loop_delay_s(void);
int32_t get_gps_delay_s(void);
int32_t get_vehicle_speed_delay_s(void);
int app_settings_init(struct golioth_client *state_client);
int app_settings_observe(void);
int app_settings_register(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
