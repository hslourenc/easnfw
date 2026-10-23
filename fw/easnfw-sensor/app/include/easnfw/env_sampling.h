/**
 * @file env_sampling.h
 * @brief Environmental sample representation and acquisition interface.
 *
 * Values use fixed-point SI units so records can be transferred and persisted
 * without floating-point conversion in the pipeline.
 */
#ifndef EASNFW_ENV_SAMPLING_H
#define EASNFW_ENV_SAMPLING_H

#include <stdint.h>

/** Environmental values represented in fixed-point SI units. */
struct easnfw_environment_sample {
	int32_t temperature_milli_c;
	uint32_t pressure_pa;
	uint32_t humidity_milli_pct;
	uint32_t gas_resistance_ohms;
};

/** Read temperature, pressure, relative humidity, and gas resistance. */
int env_sampling_acquire(struct easnfw_environment_sample *sample);

#endif
