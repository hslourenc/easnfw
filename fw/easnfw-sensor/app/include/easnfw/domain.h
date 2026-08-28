#ifndef EASNFW_DOMAIN_H
#define EASNFW_DOMAIN_H

#include <stdint.h>

struct easnfw_audio_block {
	uint32_t sequence;
};

struct easnfw_environment_sample {
	int32_t temperature_milli_c;
	uint32_t pressure_pa;
	uint32_t humidity_milli_pct;
};

int audio_sampling_acquire(struct easnfw_audio_block *block);
int env_sampling_acquire(struct easnfw_environment_sample *sample);
int audio_algo_process(const struct easnfw_audio_block *input,
	struct easnfw_audio_block *output);

#endif