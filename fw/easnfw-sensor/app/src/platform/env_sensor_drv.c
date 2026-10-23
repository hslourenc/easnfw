#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <easnfw/env_sampling.h>
#include <easnfw/env_sensor_drv.h>

LOG_MODULE_REGISTER(env_sensor_drv, LOG_LEVEL_INF);

static const struct device *const env_sensor =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_environment_sensor));

/** Read all environmental channels used by the record model. */
int app_env_sensor__sample(struct easnfw_environment_sample *sample)
{
	struct sensor_value temperature;
	struct sensor_value pressure;
	struct sensor_value humidity;
	struct sensor_value gas_resistance;
	int32_t pressure_milli_kpa;
	int err;

	if (sample == NULL) {
		return -EINVAL;
	}
	if (!device_is_ready(env_sensor)) {
		LOG_ERR("BME688-compatible environmental sensor is not ready");
		return -ENODEV;
	}

	err = sensor_sample_fetch(env_sensor);
	if (err != 0) {
		LOG_ERR("Environmental sensor sample fetch failed: %d", err);
		return err;
	}

	if ((sensor_channel_get(env_sensor, SENSOR_CHAN_AMBIENT_TEMP,
				&temperature) != 0) ||
	    (sensor_channel_get(env_sensor, SENSOR_CHAN_PRESS, &pressure) != 0) ||
	    (sensor_channel_get(env_sensor, SENSOR_CHAN_HUMIDITY,
				&humidity) != 0) ||
	    (sensor_channel_get(env_sensor, SENSOR_CHAN_GAS_RES,
				&gas_resistance) != 0)) {
		LOG_ERR("Environmental sensor did not provide every required channel");
		return -ENODATA;
	}

	pressure_milli_kpa = sensor_value_to_milli(&pressure);
	sample->temperature_milli_c = sensor_value_to_milli(&temperature);
	/* Zephyr reports pressure in kPa; milli-kPa is numerically Pa. */
	sample->pressure_pa = pressure_milli_kpa > 0 ?
		(uint32_t)pressure_milli_kpa : 0U;
	sample->humidity_milli_pct = sensor_value_to_milli(&humidity) > 0 ?
		(uint32_t)sensor_value_to_milli(&humidity) : 0U;
	sample->gas_resistance_ohms = gas_resistance.val1 > 0 ?
		(uint32_t)gas_resistance.val1 : 0U;
	return 0;
}