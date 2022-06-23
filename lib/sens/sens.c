#include <zephyr/zephyr.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/ccs811.h>
#include <stdio.h>
#include <zephyr/sys/util.h>
#include <math.h>

#include "sens.h"
#include "battery.h"

LOG_MODULE_REGISTER(climate_sens, CONFIG_LOG_DEFAULT_LEVEL);

/** A discharge curve specific to the power source. */
static const struct battery_level_point levels[] = {
#if DT_NODE_HAS_PROP(DT_INST(0, voltage_divider), io_channels)
	/* "Curve" here eyeballed from captured data for the [Adafruit
	 * 3.7v 2000 mAh](https://www.adafruit.com/product/2011) LIPO
	 * under full load that started with a charge of 3.96 V and
	 * dropped about linearly to 3.58 V over 15 hours.  It then
	 * dropped rapidly to 3.10 V over one hour, at which point it
	 * stopped transmitting.
	 *
	 * Based on eyeball comparisons we'll say that 15/16 of life
	 * goes between 3.95 and 3.55 V, and 1/16 goes between 3.55 V
	 * and 3.1 V.
	 */

	{ 10000, 3950 },
	{ 625, 3550 },
	{ 0, 3100 },
#else
	/* Linear from maximum voltage to minimum voltage. */
	{ 10000, 3600 },
	{ 0, 1700 },
#endif
};

static bool app_fw_2;
/* Global buffer to save fetched sample data */
static struct sens_packet sens_data = {0};
/* Define a sensor msgq */
K_MSGQ_DEFINE(sens_q, sizeof(struct sens_packet), 20, 4);

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

	/* Update data buffers */
	sens_data.hts221_temp = sensor_value_to_double(&temp);
	sens_data.hts221_rh = sensor_value_to_double(&hum);
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

	/* Update data buffers */
	sens_data.lps22hb_press = sensor_value_to_double(&pressure);
	sens_data.lps22hb_temp = sensor_value_to_double(&temp);
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
		/* Update data buffers */
		sens_data.ccs811_eco2 = co2.val1;
		sens_data.ccs811_etvoc = tvoc.val1;
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

static void lis2dh_process_sample(const struct device *sensor)
{
	static unsigned int count;
	struct sensor_value accel[3];
	const char *overrun = "";
	double rads = 0, degs = 0;
	int rc = sensor_sample_fetch(sensor);

	++count;
	if (rc == -EBADMSG) {
		/* Sample overrun.  Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}
	if (rc == 0) {
		rc = sensor_channel_get(sensor,
					SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	if (rc < 0) {
		LOG_ERR("ERROR: Update failed: %d\n", rc);
	} else {
		LOG_INF("lisdh: #%u @ %u ms: %sx %f , y %f , z %f",
		       count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
		rads = atan(sensor_value_to_double(&accel[0])/(sensor_value_to_double(&accel[1])));
		degs = rads*RAD_TO_DEG;
		sens_data.xy_angle = rads*RAD_TO_DEG;
		LOG_INF("lisdh: angle: %.2f", degs);
	}
}

void battery_process_sample(void) {
 	int batt_mV;
	unsigned int batt_pptt;

	batt_mV = battery_sample();
	batt_pptt = battery_level_pptt(batt_mV, levels);
	LOG_INF("%d mV; %u pptt\n",
		batt_mV, batt_pptt);

	sens_data.batt_mV = batt_mV;
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
	const struct device *lis2dh_dev = DEVICE_DT_GET_ANY(st_lis2dh);
	struct ccs811_configver_type cfgver;
	int rc;

	if (battery_measure_enable(true) != 0) {
		LOG_ERR("failed to setup battery meas");
	}

	if (lis2dh_dev == NULL) {
		LOG_ERR("could not get LIS2DH device\n");
		return;
	}

	if (!device_is_ready(lis2dh_dev)) {
		LOG_ERR("lis2dh: device is not ready\n");
		return;
	}

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
		lis2dh_process_sample(lis2dh_dev);
		battery_process_sample();

		rc = ccs811_process_sample(ccs811_dev);
		if (rc == -EAGAIN) {
			LOG_WRN("CCS811 fetch got stale data\n");
		} else if (rc != 0) {
			LOG_ERR("CCS811 fetch failed: %d\n", rc);
		}
		/* Collection complete (buffer update),now send data over */
		if (k_msgq_put(&sens_q, &sens_data, K_NO_WAIT) != 0) {
			/* Queue is full, lets purge it */
			LOG_WRN("sensor data queue attempted overflow -> purged");
			k_msgq_purge(&sens_q);
		}

		//memset(&sens_data, 0, sizeof(struct sens_packet));
		k_msleep(SAMPLE_UPDATE_RATE);
	}
}