# SPDX-License-Identifier: Apache-2.0
set(CONF_FILE segger_rtt.conf sensors.conf shell.conf display.conf app.conf)
set(DTC_OVERLAY_FILE ssd1306_128x64.overlay)
set(BOARD thingy52_nrf52832)


cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(climate_sens)

#Add Include Directories
include_directories(
			lib/sens/
            lib/display_ctl/
			)

target_sources(app PRIVATE src/main.c
                            lib/sens/sens.c
                            lib/sens/battery.c
                            lib/display_ctl/display_ctl.c
                            )
