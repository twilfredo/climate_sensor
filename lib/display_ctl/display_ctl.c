/**
 * @file main.c
 * @author Wilfred Mallawa
 * @brief Display control module, receives sensor data and
 *        uses the character frame buffer (cfb) to write to the
 *        interfaced ssd1306 display using Zephyr device drivers.
 * @version 0.1
 * @date 2022-06-23
 *
 */
#include <zephyr/zephyr.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include "display_ctl.h"
#include <sens.h>

LOG_MODULE_REGISTER(disp_sens, CONFIG_LOG_DEFAULT_LEVEL);

/* Push Button */
#define SW0_NODE	DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;
/* Governs the data display mode */
static uint8_t disp_mode = 0;

/* [WIP] Displays the boot splash and hw init status */
int disp_splash_screen(const struct device *dev) {
    int rc = 0;
    //! TODO: Actually check these params */
    if ((rc = cfb_print(dev,"sys_init OK sys_sens OK sys_boot OK starting ...", 0, 0)) != 0) {
        LOG_ERR("Failed to update a cfb\n");
    }
    cfb_framebuffer_finalize(dev);

    k_msleep(SPLASH_DELAY1);

    cfb_framebuffer_clear(dev, true);

    if ((rc = cfb_print(dev,"-WELCOME-", 16, 16)) != 0) {
        LOG_ERR("Failed to update a cfb\n");
    }
    cfb_framebuffer_finalize(dev);
    k_msleep(SPLASH_DELAY1);
    return rc;
}

/* returns a formatted string that contains current uptime */
static const char *now_str(void)
{
	static char buf[16]; /* ...HH:MM:SS.MMM */
	uint32_t now = k_uptime_get_32();
	unsigned int ms = now % MSEC_PER_SEC;
	unsigned int s;
	unsigned int min;
	unsigned int h;

	now /= MSEC_PER_SEC;
	s = now % 60U;
	now /= 60U;
	min = now % 60U;
	now /= 60U;
	h = now;

	snprintk(buf, sizeof(buf), "%u:%02u:%02u.%03u",
		 h, min, s, ms);
	return buf;
}

/* displays current climate date (temp/hum/pressure) */
int disp_sens_temps(const struct device *dev, struct sens_packet *data) {
    char draw_str[64];
    int rc = 0;
    /* the '\n' are for formatting on display */
    snprintk(draw_str, 64, "Temp: %.1fC RHum: %.1f%% Pressure:     %.1fkPa",
        ((data->hts221_temp + data->lps22hb_temp) / 2.0), data->hts221_rh, data->lps22hb_press);

    cfb_framebuffer_clear(dev, true);
    LOG_DBG("Displaying: [%s]", draw_str);

    if ((rc = cfb_print(dev, draw_str, 0, 0)) != 0) {
        LOG_ERR("Failed to update a cfb\n");
    }
    cfb_framebuffer_finalize(dev);
    return rc;
}

/* displays only system-stats metrics */
int disp_sys_stat(const struct device *dev, struct sens_packet *data) {
    char draw_str[64];
    int rc = 0;
    /* the '\n' are for formatting on display */
    snprintk(draw_str, 64, "Batt: %dmVUptime:     %s" , data->batt_mV, now_str());

    cfb_framebuffer_clear(dev, true);
    LOG_DBG("Displaying: [%s]", draw_str);

    if ((rc = cfb_print(dev, draw_str, 0, 0)) != 0) {
        LOG_ERR("Failed to update a cfb\n");
    }
    cfb_framebuffer_finalize(dev);
    return rc;
}

/* display only air-quality metrics */
int disp_sens_airq(const struct device *dev, struct sens_packet *data) {
    char draw_str[64];
    int rc = 0;
    /* the '\n' are for formatting on display */
    snprintk(draw_str, 64, "eCO2:\n\n\n\n\n\n\n\n\n%u ppm\n\n etVOC:\n\n\n\n\n\n\n\n%u ppb",
        data->ccs811_eco2, data->ccs811_etvoc);

    cfb_framebuffer_clear(dev, true);
    LOG_DBG("Displaying: [%s]", draw_str);

    if ((rc = cfb_print(dev, draw_str, 0, 0)) != 0) {
        LOG_ERR("Failed to update a cfb\n");
    }
    cfb_framebuffer_finalize(dev);
    return rc;
}

/* pb call back handler, to increment the display mode as
 * require to toggle the modes
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	LOG_DBG("Increment display toggle");
    disp_mode++;
    if (disp_mode > 2)
        disp_mode = 0;
}

/* Init pb gpio and cb interrupt */
int init_pb_cb(void) {
    int rc = 0;

	if (!(rc=device_is_ready(button.port))) {
		LOG_ERR("gpio: button device is not ready\n");
		return rc;
	}

	rc = gpio_pin_configure_dt(&button, GPIO_INPUT);

	if (rc != 0) {
		LOG_ERR("gpio: failed to configure pb pin\n");
		return rc;
	}

	rc = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (rc != 0) {
		LOG_ERR("gpio: failed to configure interrupt on pb\n");
		return rc;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

    return rc;
}

/*
 * Display thread to control/log metrics on to the physical
 * display.
 */
void disp_ctl_thread(void *unused1, void *unused2, void *unused3)
{
    LOG_INF("Check display");

    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct sens_packet sens_data = {0};

    if (init_pb_cb() != 0) {
        LOG_ERR("gpio: pb setup error");
    }


	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready\n");
		return;
	}

	if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
		LOG_ERR("Failed to set required pixel format\n");
		return;
	}

    LOG_INF("Initialized OK");

	if (cfb_framebuffer_init(dev)) {
		LOG_ERR("Framebuffer initialization failed!\n");
		return;
	}

	cfb_framebuffer_clear(dev, true);

	display_blanking_off(dev);

    if (disp_splash_screen(dev) != 0) {
        LOG_ERR("Splash screen display error");
    }

    k_msleep(SPLASH_DELAY);

    while(1) {
        /* Receive a sensor packet buffer */
        /* Wait here until a packet is received */
        if (k_msgq_get(&sens_q, &sens_data, K_FOREVER) == 0) {
            LOG_DBG("Updating display with new sensor data");
            if (disp_mode == MODE_TEMPS) {
                disp_sens_temps(dev, &sens_data);
            } else if (disp_mode == MODE_AIR_QUAL) {
                disp_sens_airq(dev, &sens_data);
            } else if (disp_mode == MODE_STATS) {
                disp_sys_stat(dev, &sens_data);
            }
        }

        memset(&sens_data, 0, sizeof(struct sens_packet));
        k_msleep(DISP_UPDATE_DELAY);
    }
}