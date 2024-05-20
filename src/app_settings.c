/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include "main.h"
#include "app_settings.h"

/* How long to wait between uploading to Golioth */
static int32_t _loop_delay_s = 5;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 0

/* How long to wait between GPS readings */
static int32_t _gps_delay_s = 3;
#define GPS_DELAY_S_MAX 43200
#define GPS_DELAY_S_MIN 0

/* Fake GPS control */
static bool _fake_gps_enabled_s;

/* Fake GPS latitude value */
static float _fake_gps_latitude_s = 37.789980;
#define FAKE_GPS_LATITUDE_S_MAX 90.0
#define FAKE_GPS_LATITUDE_S_MIN -90.0

/* Fake GPS longitude value */
static float _fake_gps_longitude_s = -122.400860;
#define FAKE_GPS_LONGITUDE_S_MAX 180.0
#define FAKE_GPS_LONGITUDE_S_MIN -180.0

/* How long to wait between vehicle speed readings */
static int32_t _vehicle_speed_delay_s = 1;
#define VEHICLE_SPEED_DELAY_S_MAX 43200
#define VEHICLE_SPEED_DELAY_S_MIN 0

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

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
	_loop_delay_s = new_value;
	LOG_INF("Set loop delay to %i seconds", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_gps_delay_setting(int32_t new_value, void *arg)
{
	_gps_delay_s = new_value;
	LOG_INF("Set GPS delay to %i seconds", new_value);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_fake_gps_enabled_setting(bool new_value, void *arg)
{
	_fake_gps_enabled_s = new_value;
	LOG_INF("Enable fake GPS location: %s", new_value ? "true" : "false");
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_fake_gps_latitude_setting(float new_value, void *arg)
{
	if ((new_value < FAKE_GPS_LATITUDE_S_MIN) || (new_value > FAKE_GPS_LATITUDE_S_MAX)) {
		LOG_ERR("Invalid fake GPS latitude: %.5f", new_value);
		return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
	}
	_fake_gps_latitude_s = new_value;
	LOG_INF("Set fake GPS latitude to %.5f degrees", new_value);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_fake_gps_longitude_setting(float new_value, void *arg)
{
	if ((new_value < FAKE_GPS_LONGITUDE_S_MIN) || (new_value > FAKE_GPS_LONGITUDE_S_MAX)) {
		LOG_ERR("Invalid fake GPS longitude: %.5f", new_value);
		return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
	}
	_fake_gps_longitude_s = new_value;
	LOG_INF("Set fake GPS longitude to %.5f degrees", new_value);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_vehicle_speed_delay_setting(int32_t new_value, void *arg)
{
	_vehicle_speed_delay_s = new_value;
	LOG_INF("Set vehicle speed delay to %i seconds", new_value);
	return GOLIOTH_SETTINGS_SUCCESS;
}

void app_settings_register(struct golioth_client *client)
{
	int err;
	struct golioth_settings *settings = golioth_settings_init(client);

	err = golioth_settings_register_int_with_range(settings, "LOOP_DELAY_S", LOOP_DELAY_S_MIN,
						       LOOP_DELAY_S_MAX, on_loop_delay_setting,
						       NULL);
	if (err) {
		LOG_ERR("Failed to register on_loop_delay_setting callback: %d", err);
	}

	err = golioth_settings_register_int_with_range(settings, "GPS_DELAY_S", GPS_DELAY_S_MIN,
						       GPS_DELAY_S_MAX, on_gps_delay_setting, NULL);
	if (err) {
		LOG_ERR("Failed to register on_gps_delay_setting callback: %d", err);
	}

	err = golioth_settings_register_bool(settings, "FAKE_GPS_ENABLED",
					     on_fake_gps_enabled_setting, NULL);
	if (err) {
		LOG_ERR("Failed to register on_fake_gps_enabled_setting callback: %d", err);
	}

	err = golioth_settings_register_float(settings, "FAKE_GPS_LATITUDE",
					      on_fake_gps_latitude_setting, NULL);
	if (err) {
		LOG_ERR("Failed to register on_fake_gps_latitude_setting callback: %d", err);
	}

	err = golioth_settings_register_float(settings, "FAKE_GPS_LONGITUDE",
					      on_fake_gps_longitude_setting, NULL);
	if (err) {
		LOG_ERR("Failed to register on_fake_gps_longitude_setting callback: %d", err);
	}

	err = golioth_settings_register_int_with_range(
		settings, "VEHICLE_SPEED_DELAY_S", VEHICLE_SPEED_DELAY_S_MIN,
		VEHICLE_SPEED_DELAY_S_MAX, on_vehicle_speed_delay_setting, NULL);
	if (err) {
		LOG_ERR("Failed to register on_vehicle_speed_delay_setting callback: %d", err);
	}
}
