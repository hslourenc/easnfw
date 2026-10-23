#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <easnfw/audio_sampling.h>
#include <easnfw/audio_sensor_drv.h>

#define WARMUP_BLOCK_COUNT 2U

static bool audio_started;

/** Start microphone capture and discard the peripheral's startup transient. */
int audio_sampling_start(void)
{
	int err = app_audio_sensor__start();

	if (err != 0) {
		return err;
	}
	audio_started = true;

	for (size_t i = 0U; i < WARMUP_BLOCK_COUNT; ++i) {
		struct app_audio_raw_block raw = {0};

		err = app_audio_sensor__read(&raw);
		app_audio_sensor__release(&raw);
		if (err != 0) {
			(void)audio_sampling_stop();
			return err;
		}
	}
	return 0;
}

/** Read one 32-bit I2S block and preserve its upper 16 PCM bits. */
int audio_sampling_acquire(struct easnfw_audio_block *block)
{
	struct app_audio_raw_block raw = {0};
	const int32_t *raw_samples;
	int err;

	if (!audio_started || block == NULL) {
		return -EINVAL;
	}
	err = app_audio_sensor__read(&raw);
	if (err != 0) {
		return err;
	}
	if (raw.size != sizeof(int32_t) * EASNFW_AUDIO_BLOCK_SAMPLES) {
		app_audio_sensor__release(&raw);
		return -EMSGSIZE;
	}

	raw_samples = raw.data;
	for (size_t i = 0U; i < EASNFW_AUDIO_BLOCK_SAMPLES; ++i) {
		block->samples[i] = (int16_t)(raw_samples[i] >> 16);
	}
	block->sample_count = EASNFW_AUDIO_BLOCK_SAMPLES;
	app_audio_sensor__release(&raw);
	return 0;
}

/** Stop the microphone after a track and clear the sampling state. */
int audio_sampling_stop(void)
{
	const int err = app_audio_sensor__stop();

	audio_started = false;
	return err;
}