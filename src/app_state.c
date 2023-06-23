/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <stdlib.h>
#include <net/golioth/system_client.h>
#include <zephyr/data/json.h>
#include "json_helper.h"

#include "app_state.h"
#include "app_work.h"

#define LATITUDE_MIN -90.0
#define LATITUDE_MAX 90.0
#define LONGITUDE_MIN -180.0
#define LONGITUDE_MAX 180.0

#define DEVICE_STATE_FMT "{\"fake_latitude\":\"%f\",\"fake_longitude\":\"%f\"}"

float _fake_latitude = 37.789980;
float _fake_longitude = -122.400860;

static struct golioth_client *client;

static K_SEM_DEFINE(update_actual, 0, 1);

float get_fake_latitude(void)
{
	return _fake_latitude;
}

float get_fake_longitude(void)
{
	return _fake_longitude;
}

static int async_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_WRN("Failed to set state: %d", rsp->err);
		return rsp->err;
	}

	LOG_DBG("State successfully set");

	return 0;
}

void app_state_init(struct golioth_client *state_client)
{
	client = state_client;
	k_sem_give(&update_actual);
}

void app_state_update_actual(void)
{

	char sbuf[strlen(DEVICE_STATE_FMT)+24];

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, _fake_latitude, _fake_longitude);

	int err;

	err = golioth_lightdb_set_cb(client, APP_STATE_ACTUAL_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, sbuf, strlen(sbuf),
			async_handler, NULL);

	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
}

int app_state_desired_handler(struct golioth_req_rsp *rsp)
{
	char fake_latitude_str[12];
	char fake_longitude_str[12];
	struct app_state parsed_state;
	float fake_latitude;
	float fake_longitude;

	if (rsp->err) {
		LOG_ERR("Failed to receive '%s' endpoint: %d", APP_STATE_DESIRED_ENDP, rsp->err);
		return rsp->err;
	}

	LOG_HEXDUMP_DBG(rsp->data, rsp->len, APP_STATE_DESIRED_ENDP);

	parsed_state.fake_latitude = fake_latitude_str;
	parsed_state.fake_longitude = fake_longitude_str;

	int ret = json_obj_parse((char *)rsp->data, rsp->len,
			app_state_descr, ARRAY_SIZE(app_state_descr),
			&parsed_state);

	if (ret < 0) {
		LOG_ERR("Error parsing desired values: %d", ret);
		return 0;
	}

	uint8_t state_change_count = 0;

	if (ret & 1<<0) {
		/* Process fake_latitude */
		fake_latitude = strtof(parsed_state.fake_latitude, NULL);
		if ((fake_latitude <= LATITUDE_MAX) && (fake_latitude >= LATITUDE_MIN)) {
			LOG_DBG("Validated desired fake_latitude value: %f", fake_latitude);
			_fake_latitude = fake_latitude;
			++state_change_count;
		} else {
			LOG_ERR("Invalid desired fake_latitude value: %f", fake_latitude);
		}
	}
	if (ret & 1<<1) {
		/* Process fake_longitude */
		fake_longitude = strtof(parsed_state.fake_longitude, NULL);
		if ((fake_longitude <= LONGITUDE_MAX) && (fake_longitude >= LONGITUDE_MIN)) {
			LOG_DBG("Validated desired fake_longitude value: %f", fake_longitude);
			_fake_longitude = fake_longitude;
			++state_change_count;
		} else {
			LOG_ERR("Invalid desired fake_longitude value: %f", fake_longitude);
		}
	}

	if (state_change_count) {
		/* The state was changed, so update the state on the Golioth servers */
		app_state_update_actual();
	}
	return 0;
}

void app_state_observe(void)
{
	int err = golioth_lightdb_observe_cb(client, APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, app_state_desired_handler, NULL);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
	}

	/* This will only run when we first connect. It updates the actual state of
	 * the device with the Golioth servers. Future updates will be sent whenever
	 * changes occur.
	 */
	if (k_sem_take(&update_actual, K_NO_WAIT) == 0) {
		app_state_update_actual();
	}
}
