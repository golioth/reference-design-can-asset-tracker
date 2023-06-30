/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JSON_HELPER_H_
#define __JSON_HELPER_H_

#include <zephyr/data/json.h>

struct app_state {
	bool fake_gps_enabled;
	char *fake_latitude;
	char *fake_longitude;
};

static const struct json_obj_descr app_state_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct app_state, fake_gps_enabled, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_PRIM(struct app_state, fake_latitude, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct app_state, fake_longitude, JSON_TOK_STRING)
};

#endif
