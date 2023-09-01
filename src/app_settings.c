/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <net/golioth/settings.h>

#include "main.h"
#include "app_settings.h"

static struct golioth_client *client;

/* How long to wait between uploading to Golioth */
static int32_t _loop_delay_s = 5;
/* How long to wait between GPS readings */
static int32_t _gps_delay_s = 3;
/* Fake GPS control */
static bool _fake_gps_enabled_s;
/* Fake GPS latitude value */
static float _fake_gps_latitude_s = 37.789980;
/* Fake GPS longitude value */
static float _fake_gps_longitude_s = -122.400860;
/* How long to wait between vehicle speed readings */
static int32_t _vehicle_speed_delay_s = 1;

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

int32_t get_gps_delay_s(void)
{
	return _gps_delay_s;
}

bool get_fake_gps_enabled_s(void)
{
	return _fake_gps_enabled_s;
}

float get_fake_gps_latitude_s(void)
{
	return _fake_gps_latitude_s;
}

float get_fake_gps_longitude_s(void)
{
	return _fake_gps_longitude_s;
}

int32_t get_vehicle_speed_delay_s(void)
{
	return _vehicle_speed_delay_s;
}

enum golioth_settings_status on_setting(const char *key, const struct golioth_settings_value *value)
{

	LOG_DBG("Received setting: key = %s, type = %d", key, value->type);
	if (strcmp(key, "LOOP_DELAY_S") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received LOOP_DELAY_S is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 1 || value->i64 > 43200) {
			LOG_DBG("Received LOOP_DELAY_S setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_loop_delay_s == (int32_t)value->i64) {
			LOG_DBG("Received LOOP_DELAY_S already matches local value.");
		} else {
			_loop_delay_s = (int32_t)value->i64;
			LOG_INF("Set loop delay to %d seconds", _loop_delay_s);

			wake_system_thread();
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	} else if (strcmp(key, "GPS_DELAY_S") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [0, 43200] */
		if (value->i64 < 0 || value->i64 > 43200) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_gps_delay_s == (int32_t)value->i64) {
			LOG_DBG("Received GPS_DELAY_S already matches local value.");
		} else {
			_gps_delay_s = (int32_t)value->i64;
			LOG_INF("Set GPS delay to %d seconds", _gps_delay_s);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	} else if (strcmp(key, "FAKE_GPS_ENABLED") == 0) {
		/* This setting is expected to be boolean, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_BOOL) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Only update if value has changed */
		if (_fake_gps_enabled_s == (bool)value->b) {
			LOG_DBG("Received FAKE_GPS_ENABLED already matches local value.");
		} else {
			_fake_gps_enabled_s = (bool)value->b;
			LOG_INF("Fake GPS %s", _fake_gps_enabled_s ? "enabled" : "disabled");
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	} else if (strcmp(key, "FAKE_GPS_LATITUDE") == 0) {
		/* This setting is expected to be a float, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to [-90.0, 90.0] */
		if (value->f < -90.0 || value->f > 90.0) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_fake_gps_latitude_s == (float)value->f) {
			LOG_DBG("Received FAKE_GPS_LATITUDE already matches local value.");
		} else {
			_fake_gps_latitude_s = (float)value->f;
			LOG_INF("Set fake GPS latitude value to %f", _fake_gps_latitude_s);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	} else if (strcmp(key, "FAKE_GPS_LONGITUDE") == 0) {
		/* This setting is expected to be a float, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to [-180.0, 180.0] */
		if (value->f < -180.0 || value->f > 180.0) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_fake_gps_longitude_s == (float)value->f) {
			LOG_DBG("Received FAKE_GPS_LONGITUDE already matches local value.");
		} else {
			_fake_gps_longitude_s = (float)value->f;
			LOG_INF("Set fake GPS longitude value to %f", _fake_gps_longitude_s);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	} else if (strcmp(key, "VEHICLE_SPEED_DELAY_S") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [0, 43200] */
		if (value->i64 < 0 || value->i64 > 43200) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_vehicle_speed_delay_s == (int32_t)value->i64) {
			LOG_DBG("Received VEHICLE_SPEED_DELAY_S already matches local value.");
		} else {
			_vehicle_speed_delay_s = (int32_t)value->i64;
			LOG_INF("Set vehicle speed readings delay to %d seconds",
				_vehicle_speed_delay_s);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	/* If the setting is not recognized, we should return an error */
	return GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED;
}

int app_settings_init(struct golioth_client *state_client)
{
	client = state_client;
	int err = app_settings_register(client);
	return err;
}

int app_settings_observe(void)
{
	int err = golioth_settings_observe(client);
	if (err) {
		LOG_ERR("Failed to observe settings: %d", err);
	}
	return err;
}

int app_settings_register(struct golioth_client *settings_client)
{
	int err = golioth_settings_register_callback(settings_client, on_setting);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	return err;
}
