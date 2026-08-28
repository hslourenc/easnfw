#include <easnfw/domain.h>

int env_sampling_acquire(struct easnfw_environment_sample *sample)
{
	/* EASNFW_TODO: Acquire BME688 values through the environmental driver. */
	(void)sample;
	return 0;
}