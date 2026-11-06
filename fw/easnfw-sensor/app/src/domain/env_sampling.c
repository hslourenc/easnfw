#include <easnfw/env_sampling.h>
#include <easnfw/env_sensor_drv.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include <zephyr/logging/log.h>
#include "../verif/test_cases.h"
LOG_MODULE_REGISTER(easnfw_env_sampling, LOG_LEVEL_INF);
#endif

int env_sampling_acquire(struct easnfw_environment_sample *sample)
{
	int err = app_env_sensor__sample(sample);

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.short_sampling_cycle != 0) {
		if (err == 0) {
			LOG_INF("HIL_ENV_SAMPLE temp=%d pressure=%u humidity=%u gas=%u",
				sample->temperature_milli_c, sample->pressure_pa,
				sample->humidity_milli_pct, sample->gas_resistance_ohms);
		} else {
			LOG_ERR("HIL_ENV_SAMPLE_ERROR error=%d", err);
		}
	}
#endif
	return err;
}