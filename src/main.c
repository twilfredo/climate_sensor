/**
 * @file main.c
 * @author Wilfred Mallawa
 * @brief Mini application to monitor climate metrics using the
 * 		  thingy52 interfaced with an ssd1305 (128x64-OLED) display
 * 		  to display metrics in real time.
 *
 * @version 0.1
 * @date 2022-06-23
 *
 */
#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "sens.h"
#include "display_ctl.h"

LOG_MODULE_REGISTER(core, CONFIG_LOG_DEFAULT_LEVEL);

/* Thread Data */
/* Display control thread data */
struct k_thread disp_t_data = {0};
k_tid_t disp_tid = {0};
K_THREAD_STACK_DEFINE(disp_t_stack_area, DISP_T_STACK_SIZE);
/* Sensor control thread data */
struct k_thread sens_t_data = {0};
k_tid_t sens_tid = {0};
K_THREAD_STACK_DEFINE(sens_t_stack_area, SENS_T_STACK_SIZE);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "ledx" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);


int thread_init(void) {
	sens_tid = k_thread_create(&sens_t_data, sens_t_stack_area,
                                 K_THREAD_STACK_SIZEOF(sens_t_stack_area),
                                 sens_thread,
                                 NULL, NULL, NULL,
                                 SENS_T_PRIOR, 0, K_NO_WAIT);

	disp_tid = k_thread_create(&disp_t_data, disp_t_stack_area,
                                 K_THREAD_STACK_SIZEOF(disp_t_stack_area),
                                 disp_ctl_thread,
                                 NULL, NULL, NULL,
                                 DISP_T_PRIOR, 0, K_NO_WAIT);

	LOG_INF("Sys threads init OK");
	return 0;
}

/*
 * Main thread, serves to blink the debug_led
 */
void main(void)
{
	/* Init systhreads */
	thread_init();

#ifndef CONFIG_DEBUG_BLINKY
	return;
#endif

	LOG_DBG("Init debug leds");

	if (!device_is_ready(led1.port) || !device_is_ready(led2.port)) {
		LOG_ERR("Debug led device error");
		return;
	}

	if (gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE) || gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE)) {
		LOG_ERR("Debug led gpio error");
		return;
	}

	while (1) {
		if (gpio_pin_toggle_dt(&led1) || gpio_pin_toggle_dt(&led2)) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
