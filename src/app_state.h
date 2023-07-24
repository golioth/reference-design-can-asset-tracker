/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_STATE_H__
#define __APP_STATE_H__

/** Observe and write to example endpoints for stateful data on the Golioth
 * LightDB State Service.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/light-db/
 */

#include <net/golioth/system_client.h>

#define APP_STATE_DESIRED_ENDP "desired"
#define APP_STATE_ACTUAL_ENDP  "state"

bool get_fake_gps_enabled(void);
float get_fake_latitude(void);
float get_fake_longitude(void);
void app_state_init(struct golioth_client *state_client);
int app_state_observe(void);
int app_state_update_actual(void);

#endif /* __APP_STATE_H__ */
