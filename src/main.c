
#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "sens.h"

LOG_MODULE_REGISTER(core, CONFIG_LOG_DEFAULT_LEVEL);
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
	LOG_INF("Sys thread init OK");
	return 0;
}

/*
 * Main thread, serves to blink the debug_led
 */
void main(void)
{
	LOG_DBG("Init debug leds");
	if (!device_is_ready(led1.port) || !device_is_ready(led2.port)) {
		LOG_ERR("Debug led device error");
		return;
	}

	if (gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE) || gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE)) {
		LOG_ERR("Debug led gpio error");
		return;
	}

	/* Init systhreads */
	thread_init();

	while (1) {
		if (gpio_pin_toggle_dt(&led1) || gpio_pin_toggle_dt(&led2)) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
