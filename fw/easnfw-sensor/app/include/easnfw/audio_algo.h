/**
 * @file audio_algo.h
 * @brief Audio-block processing interface.
 *
 * The current baseline copies PCM input unchanged. Replace this isolated
 * operation when the scientific processing algorithm is specified.
 */
#ifndef EASNFW_AUDIO_ALGO_H
#define EASNFW_AUDIO_ALGO_H

#include <easnfw/audio_sampling.h>

/** Process one audio block; the baseline algorithm is a bit-exact pass-through. */
int audio_algo_process(const struct easnfw_audio_block *input,
		       struct easnfw_audio_block *output);

#endif
