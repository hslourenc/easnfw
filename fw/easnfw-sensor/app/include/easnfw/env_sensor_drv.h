#ifndef EASNFW_ENV_SENSOR_DRV_H
#define EASNFW_ENV_SENSOR_DRV_H

#include <easnfw/env_sampling.h>

/** Fetch a fresh sample from the configured BME688-compatible sensor. */
int app_env_sensor__sample(struct easnfw_environment_sample *sample);

#endif
