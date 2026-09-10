#include <errno.h>
#include <limits.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/audio_sensor.h>
#include <easnfw/domain.h>
#include <easnfw/mass_storage.h>

LOG_MODULE_REGISTER(easnfw_audio_sampling, LOG_LEVEL_INF);

#define SAMPLE_RATE_HZ 48000U
#define SAMPLES_PER_BLOCK 1536U
#define RAW_BLOCK_BYTES (SAMPLES_PER_BLOCK * sizeof(int32_t))
#define PCM_BLOCK_BYTES (SAMPLES_PER_BLOCK * sizeof(int16_t))

static void stats_init(struct easnfw_audio_stats *stats)
{
	stats->minimum = INT16_MAX;
	stats->maximum = INT16_MIN;
	stats->zero_samples = 0U;
	stats->clipped_samples = 0U;
	stats->sample_count = 0U;
}

static void convert_block(const int32_t *input, int16_t *output,
	struct easnfw_audio_stats *stats)
{
	for (size_t index = 0U; index < SAMPLES_PER_BLOCK; ++index) {
		int16_t sample = (int16_t)(input[index] >> 16);

		output[index] = sample;
		if (sample < stats->minimum) {
			stats->minimum = sample;
		}
		if (sample > stats->maximum) {
			stats->maximum = sample;
		}
		stats->zero_samples += sample == 0;
		stats->clipped_samples += sample == INT16_MIN || sample == INT16_MAX;
	}
	stats->sample_count += SAMPLES_PER_BLOCK;
}

int audio_sampling_acquire(struct easnfw_audio_block *block)
{
	if (block == NULL) {
		return -EINVAL;
	}
	return audio_sensor_drv_init();
}

int audio_capture_run(void)
{
	static int16_t pcm_block[SAMPLES_PER_BLOCK];
	struct easnfw_audio_stats stats;
	const uint32_t total_blocks =
		(SAMPLE_RATE_HZ * CONFIG_EASNFW_SENSOR_TRACK_SECONDS) /
		SAMPLES_PER_BLOCK;
	int ret;

#if !CONFIG_EASNFW_SENSOR_BYPASS_MASS_STORAGE
	static bool storage_ready;

	if (!storage_ready) {
		ret = mass_storage_init();
		if (ret < 0) {
			return ret;
		}
		storage_ready = true;
	}
	ret = mass_storage_begin_capture(SAMPLE_RATE_HZ, 1U, 16U);
	if (ret < 0) {
		return ret;
	}
#endif
	ret = audio_sensor_drv_init();
	if (ret < 0) {
		goto abort;
	}
	ret = audio_sensor_drv_start();
	if (ret < 0) {
		goto abort;
	}
	for (uint32_t warmup = 0U; warmup < 2U; ++warmup) {
		struct easnfw_raw_audio_block raw = { 0 };

		ret = audio_sensor_drv_read(&raw);
		audio_sensor_drv_release(&raw);
		if (ret < 0) {
			goto stop;
		}
	}
	stats_init(&stats);
	for (uint32_t sequence = 0U; sequence < total_blocks; ++sequence) {
		struct easnfw_raw_audio_block raw = { 0 };

		ret = audio_sensor_drv_read(&raw);
		if (ret < 0) {
			goto stop;
		}
		if (raw.size != RAW_BLOCK_BYTES) {
			audio_sensor_drv_release(&raw);
			ret = -EMSGSIZE;
			goto stop;
		}
		convert_block(raw.data, pcm_block, &stats);
		audio_sensor_drv_release(&raw);
#if !CONFIG_EASNFW_SENSOR_BYPASS_MASS_STORAGE
		ret = mass_storage_append_audio(pcm_block, PCM_BLOCK_BYTES);
		if (ret < 0) {
			goto stop;
		}
#endif
	}
	ret = audio_sensor_drv_stop();
	if (ret < 0) {
		goto abort;
	}
	if (stats.sample_count != SAMPLE_RATE_HZ *
	    CONFIG_EASNFW_SENSOR_TRACK_SECONDS ||
	    stats.zero_samples == stats.sample_count) {
		ret = -ENODATA;
		goto abort;
	}
#if !CONFIG_EASNFW_SENSOR_BYPASS_MASS_STORAGE
	return mass_storage_commit_capture();
#else
	LOG_INF("Audio capture passed; mass storage bypassed");
	return 0;
#endif

stop:
	(void)audio_sensor_drv_stop();
abort:
#if !CONFIG_EASNFW_SENSOR_BYPASS_MASS_STORAGE
	(void)mass_storage_abort_capture();
#endif
	return ret;
}