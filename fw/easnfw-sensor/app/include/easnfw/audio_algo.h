#ifndef EASNFW_AUDIO_ALGO_H
#define EASNFW_AUDIO_ALGO_H

#include <easnfw/audio_sampling.h>

/** Process one audio block; the baseline algorithm is a bit-exact pass-through. */
int audio_algo_process(const struct easnfw_audio_block *input,
		       struct easnfw_audio_block *output);

#endif
