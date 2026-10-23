#ifndef EASNFW_AUDIO_SAMPLING_H
#define EASNFW_AUDIO_SAMPLING_H

#include <stdint.h>

#include <zephyr/autoconf.h>

#define EASNFW_AUDIO_CHANNELS 1U
#define EASNFW_AUDIO_BITS_PER_SAMPLE 16U
#define EASNFW_AUDIO_BLOCK_SAMPLES CONFIG_EASNFW_AUDIO_SAMPLES_PER_BLOCK

/** One mono PCM16 block acquired from the digital microphone. */
struct easnfw_audio_block {
	uint32_t sequence;
	uint16_t sample_count;
	int16_t samples[EASNFW_AUDIO_BLOCK_SAMPLES];
};

/** Initialize and start audio capture for a track. */
int audio_sampling_start(void);

/** Stop audio capture and release the I2S peripheral. */
int audio_sampling_stop(void);

/** Acquire one full PCM16 audio block. */
int audio_sampling_acquire(struct easnfw_audio_block *block);

#endif
