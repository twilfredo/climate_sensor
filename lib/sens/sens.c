#include <zephyr/zephyr.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <stdio.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(climate_sens, CONFIG_LOG_DEFAULT_LEVEL);

static bool app_fw_2;

/* Process and HTS221 Sample */
static void hts221_process_sample(const struct device *dev)
{
#ifdef CONFIG_APP_OBS_NUMBER
	static unsigned int obs;
#endif
	struct sensor_value temp, hum;
	if (sensor_sample_fetch(dev) < 0) {
		LOG_ERR("hts221: sensor sample update error\n");
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
		LOG_ERR("hts221: cannot read HTS221 temperature channel\n");
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum) < 0) {
		LOG_ERR("hts221: cannot read HTS221 humidity channel\n");
		return;
	}

#ifdef CONFIG_APP_OBS_NUMBER
	++obs;
	LOG_INF("hts221: observation:%u\n", obs);
#endif

	/* display temperature */
	LOG_INF("hts221: temperature:%.1f C\n", sensor_value_to_double(&temp));

	/* display humidity */
	LOG_INF("hts221: relative Humidity:%.1f%%\n",
		sensor_value_to_double(&hum));
}

static void lps22hb_process_sample(const struct device *dev)
{
#ifdef CONFIG_APP_OBS_NUMBER
	static unsigned int obs;
#endif
	struct sensor_value pressure, temp;

	if (sensor_sample_fetch(dev) < 0) {
		LOG_ERR("lps22hb: sensor sample update error\n");
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_PRESS, &pressure) < 0) {
		LOG_ERR("lps22hb: cannot read LPS22HB pressure channel\n");
		return;
	}

	if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp) < 0) {
		LOG_ERR("lps22hb: cannot read LPS22HB temperature channel\n");
		return;
	}

#ifdef CONFIG_APP_OBS_NUMBER
	++obs;
	LOG_INF("lps22hb: observation:%u\n", obs);
#endif

	/* display pressure */
	LOG_INF("lps22hb: pressure:%.1f kPa\n", sensor_value_to_double(&pressure));

	/* display temperature */
	LOG_INF("lps22hb: temperature:%.1f C\n", sensor_value_to_double(&temp));
}

static int ccs811_process_sample(const struct device *dev)
{
	struct sensor_value co2, tvoc, voltage, current;
	int rc = 0;
#ifdef CONFIG_APP_MONITOR_BASELINE
	int baseline = -1;
	rc = ccs811_baseline_fetch(dev);
	if (rc >= 0) {
		baseline = rc;
		rc = 0;
	}
#endif
	if (rc == 0) {
		rc = sensor_sample_fetch(dev);
	}
	if (rc == 0) {
		const struct ccs811_result_type *rp = ccs811_result(dev);

		sensor_channel_get(dev, SENSOR_CHAN_CO2, &co2);
		sensor_channel_get(dev, SENSOR_CHAN_VOC, &tvoc);
		sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &voltage);
		sensor_channel_get(dev, SENSOR_CHAN_CURRENT, &current);
		LOG_INF("ccs811: %u ppm eCO2; %u ppb eTVOC\n",
		       co2.val1, tvoc.val1);
#ifdef CONFIG_CCS811_VERBOSE
		LOG_INF("ccs811: Voltage: %d.%06dV; Current: %d.%06dA\n", voltage.val1,
		       voltage.val2, current.val1, current.val2);
#endif
#ifdef CONFIG_APP_MONITOR_BASELINE
		LOG_INF("ccs811: baseline %04x\n", baseline);
#endif
		if (app_fw_2 && !(rp->status & CCS811_STATUS_DATA_READY)) {
			LOG_ERR("ccs811: stale data\n");
		}

		if (rp->status & CCS811_STATUS_ERROR) {
			LOG_ERR("ccs811: status error: %02x\n", rp->error);
		}
	}
	return rc;
}

/*
 * Sensor thread to read out all sensory data collected
 * by the onboard climate sensors.
 */
void sens_thread(void *unused1, void *unused2, void *unused3)
{
	const struct device *hts221_dev = device_get_binding("HTS221");
	const struct device *lps22hb_dev = device_get_binding(DT_LABEL(DT_INST(0, st_lps22hb_press)));
	const struct device *ccs811_dev = device_get_binding(DT_LABEL(DT_INST(0, ams_ccs811)));

	struct ccs811_configver_type cfgver;
	int rc;

	if (hts221_dev == NULL) {
		LOG_ERR("could not get HTS221 device\n");
		return;
	}

	if (lps22hb_dev == NULL) {
		LOG_ERR("could not get LPS22HB device\n");
		return;
	}

	if (ccs811_dev == NULL) {
		LOG_ERR("could not get CCS811 device\n");
		return;
	}

	rc = ccs811_configver_fetch(ccs811_dev, &cfgver);

	if (rc == 0) {
		LOG_INF("ccs811: HW %02x; FW Boot %04x App %04x ; mode %02x\n",
		       cfgver.hw_version, cfgver.fw_boot_version,
		       cfgver.fw_app_version, cfgver.mode);
		app_fw_2 = (cfgver.fw_app_version >> 8) > 0x11;
	}

#ifdef CONFIG_APP_USE_DEF_ENVDATA
	struct sensor_value temp = { CONFIG_APP_ENV_TEMPERATURE };
	struct sensor_value humidity = { CONFIG_APP_ENV_HUMIDITY };

	rc = ccs811_envdata_update(ccs811_dev, &temp, &humidity);

	LOG_INF("CCS811 Calibrated for %d Cel, %d %%RH Status %s : errno %d\n",
			temp.val1, humidity.val1, rc ? "Calibration err" : "Okay", rc);
#endif

	while(1) {
		hts221_process_sample(hts221_dev);
		lps22hb_process_sample(lps22hb_dev);

		rc = ccs811_process_sample(ccs811_dev);
		if (rc == -EAGAIN) {
			LOG_WRN("CCS811 fetch got stale data\n");
		} else if (rc != 0) {
			LOG_ERR("CCS811 fetch failed: %d\n", rc);
		}
		k_msleep(2000);
	}
}