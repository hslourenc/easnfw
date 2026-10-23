#include <easnfw/env_sampling.h>
#include <easnfw/env_sensor_drv.h>

int env_sampling_acquire(struct easnfw_environment_sample *sample)
{
	return app_env_sensor__sample(sample);
}