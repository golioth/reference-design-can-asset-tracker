/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <net/golioth/system_client.h>

int32_t get_loop_delay_s(void);
int32_t get_gps_delay_s(void);
int32_t get_vehicle_speed_delay_s(void);
void app_settings_init(struct golioth_client *state_client);
void app_settings_observe(void);
int app_register_settings(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
