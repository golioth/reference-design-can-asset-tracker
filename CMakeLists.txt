# Copyright (c) 2022-2023 Golioth, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if((BOARD STREQUAL nrf9160dk_nrf9160) OR (BOARD STREQUAL nrf9160dk_nrf9160_ns))
  set(SHIELD "arduino_uno_click")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(can_asset_tracker)

include_directories(src/lib/inc)
add_compile_definitions(timegm=mktime)
target_sources(app PRIVATE src/lib/minmea/minmea.c)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_rpc.c)
target_sources(app PRIVATE src/app_settings.c)
target_sources(app PRIVATE src/app_state.c)
target_sources(app PRIVATE src/app_sensors.c)

add_subdirectory_ifdef(CONFIG_ALUDEL_BATTERY_MONITOR src/battery_monitor)
