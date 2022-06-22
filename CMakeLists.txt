# SPDX-License-Identifier: Apache-2.0
set(CONF_FILE segger_rtt.conf sensors.conf shell.conf display.conf)
set(DTC_OVERLAY_FILE ssd1306_128x32.overlay)


cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(climate_sens)

#Add Include Directories
include_directories(
			lib/sens/
			)

target_sources(app PRIVATE src/main.c
                            lib/sens/sens.c
                            )
