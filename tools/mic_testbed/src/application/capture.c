#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <mic_testbed/audio_config.h>
#include <mic_testbed/audio_payload.h>
#include <mic_testbed/capture.h>
#include <mic_testbed/i2s_mic.h>
#include <mic_testbed/sd_storage.h>

LOG_MODULE_REGISTER(mic_testbed_capture, LOG_LEVEL_INF);

static int16_t pcm_block[MIC_TESTBED_SAMPLES_PER_BLOCK];

static int discard_warmup_blocks(void)
{
	for (uint32_t i = 0U; i < CONFIG_MIC_TESTBED_WARMUP_BLOCKS; ++i) {
		struct mic_testbed_raw_block block = { 0 };
		int ret = i2s_mic_read(&block);

		if (ret < 0) {
			return ret;
		}
		i2s_mic_release(&block);
	}
	return 0;
}

int mic_testbed_capture_run(void)
{
	struct mic_testbed_audio_stats stats;
	uint32_t blocks_written = 0U;
	int ret;

	LOG_INF("Capture configuration: mono PCM16, %u Hz, %u seconds",
		MIC_TESTBED_SAMPLE_RATE_HZ, CONFIG_MIC_TESTBED_TRACK_SECONDS);
	LOG_INF("Expected payload: %u samples, %u audio bytes",
		MIC_TESTBED_TOTAL_SAMPLES,
		MIC_TESTBED_TOTAL_SAMPLES * sizeof(int16_t));

	ret = sd_storage_init();
	if (ret < 0) {
		return ret;
	}
	ret = sd_storage_begin_capture(MIC_TESTBED_SAMPLE_RATE_HZ,
		MIC_TESTBED_CHANNELS, MIC_TESTBED_PCM_BITS);
	if (ret < 0) {
		return ret;
	}
	ret = i2s_mic_init();
	if (ret < 0) {
		goto abort_capture;
	}
	ret = i2s_mic_start();
	if (ret < 0) {
		goto abort_capture;
	}

	ret = discard_warmup_blocks();
	if (ret < 0) {
		goto stop_i2s;
	}
	audio_payload_stats_init(&stats);

	for (uint32_t sequence = 0U;
	     sequence < MIC_TESTBED_TOTAL_BLOCKS; ++sequence) {
		struct mic_testbed_raw_block raw_block = { 0 };
		size_t pcm_bytes;

		ret = i2s_mic_read(&raw_block);
		if (ret < 0) {
			LOG_ERR("I2S read %u failed: %d", sequence, ret);
			goto stop_i2s;
		}
		if (raw_block.size != MIC_TESTBED_RAW_BLOCK_BYTES) {
			LOG_ERR("Unexpected I2S block size: %u", raw_block.size);
			i2s_mic_release(&raw_block);
			ret = -EMSGSIZE;
			goto stop_i2s;
		}

		pcm_bytes = audio_payload_pcm32_to_pcm16(raw_block.data,
			raw_block.size, pcm_block, &stats);
		i2s_mic_release(&raw_block);

		ret = sd_storage_append_audio(pcm_block, pcm_bytes);
		if (ret < 0) {
			LOG_ERR("SD write %u failed: %d", sequence, ret);
			goto stop_i2s;
		}
		blocks_written++;

		if (((sequence + 1U) % 125U) == 0U) {
			LOG_INF("Captured %u/%u blocks", sequence + 1U,
				MIC_TESTBED_TOTAL_BLOCKS);
		}
	}

stop_i2s:
	{
		const int stop_ret = i2s_mic_stop();

		if ((ret == 0) && (stop_ret < 0)) {
			ret = stop_ret;
		}
	}
	if (ret < 0) {
		goto abort_capture;
	}
	if (blocks_written != MIC_TESTBED_TOTAL_BLOCKS) {
		ret = -EIO;
		goto abort_capture;
	}
	if (stats.sample_count != MIC_TESTBED_TOTAL_SAMPLES) {
		LOG_ERR("Sample-count test failed: %u/%u", stats.sample_count,
			MIC_TESTBED_TOTAL_SAMPLES);
		ret = -EMSGSIZE;
		goto abort_capture;
	}
	if (stats.zero_samples == stats.sample_count) {
		LOG_ERR("Signal test failed: capture contains only zero-valued samples");
		ret = -ENODATA;
		goto abort_capture;
	}
	if (audio_payload_absolute_peak(&stats) <
	    CONFIG_MIC_TESTBED_MIN_ABS_PEAK) {
		LOG_ERR("Signal test failed: absolute peak %u is below %u",
			audio_payload_absolute_peak(&stats),
			CONFIG_MIC_TESTBED_MIN_ABS_PEAK);
		ret = -ENODATA;
		goto abort_capture;
	}

	ret = sd_storage_commit_capture();
	if (ret < 0) {
		return ret;
	}

	LOG_INF("PCM range: %d to %d; absolute peak: %u",
		stats.minimum, stats.maximum,
		audio_payload_absolute_peak(&stats));
	LOG_INF("Zero samples: %u/%u; clipped samples: %u",
		stats.zero_samples, stats.sample_count, stats.clipped_samples);
	LOG_INF("PASS: format, sample count, signal activity, and committed-file tests");

	LOG_INF("Remove the SD card after power-down and inspect %s",
		sd_storage_capture_path());
	return 0;

abort_capture:
	(void)sd_storage_abort_capture();
	return ret;
}
