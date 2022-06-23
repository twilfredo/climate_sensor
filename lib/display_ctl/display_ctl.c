#include <zephyr/zephyr.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include "display_ctl.h"
#include <sens.h>

LOG_MODULE_REGISTER(disp_sens, CONFIG_LOG_DEFAULT_LEVEL);

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


/*
 * Display thread to control/log metrics on to the physical
 * display.
 */
void disp_ctl_thread(void *unused1, void *unused2, void *unused3)
{
    LOG_INF("Check display");
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    struct sens_packet sens_data = {0};

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
            disp_sens_temps(dev, &sens_data);
            /* Received non-stale air quality details */
            if (sens_data.ccs811_eco2 > 0 &&  sens_data.ccs811_etvoc > 0) {
                k_msleep(DISP_UPDATE_DELAY/2);
                disp_sens_airq(dev, &sens_data);
            }
        }

        memset(&sens_data, 0, sizeof(struct sens_packet));
        k_msleep(DISP_UPDATE_DELAY);
    }
}