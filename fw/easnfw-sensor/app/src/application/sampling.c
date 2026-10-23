#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <easnfw/audio_sampling.h>
#include <easnfw/sampling.h>
#include <easnfw/env_sampling.h>
#include <easnfw/pipeline.h>
#include <easnfw/mass_storage.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>

LOG_MODULE_REGISTER(easnfw_sampling, LOG_LEVEL_INF);
#define SAMPLING_STACK_SIZE 6144U

K_THREAD_STACK_DEFINE(sampling_stack, SAMPLING_STACK_SIZE);
static struct k_thread sampling_thread_data;

/** Generate a unique ID and record environmental and sampling metadata. */
static int record_metadata_init(
	struct app_mass_storage_record_metadata *metadata,
	const struct easnfw_environment_sample *environment,
	int64_t environment_utc_ms)
{
	int err;

	for (size_t i = 0U; i < sizeof(metadata->record_id); i += sizeof(uint32_t)) {
		const uint32_t random = sys_rand32_get();

		memcpy(&metadata->record_id[i], &random, sizeof(random));
	}
	metadata->sample_rate_hz = CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ;
	metadata->track_duration_seconds = CONFIG_EASNFW_TRACK_LEN_SECONDS;
	metadata->temperature_milli_c = environment->temperature_milli_c;
	metadata->pressure_pa = environment->pressure_pa;
	metadata->humidity_milli_pct = environment->humidity_milli_pct;
	metadata->gas_resistance_ohms = environment->gas_resistance_ohms;
	err = app_utc_time__format(environment_utc_ms,
				   metadata->environment_sample_utc,
				   sizeof(metadata->environment_sample_utc));
	if (err != 0) {
		return err;
	}
	metadata->total_sample_count =
		CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ *
		CONFIG_EASNFW_TRACK_LEN_SECONDS;
	metadata->processing_algorithm_version = 1U;
	metadata->channels = EASNFW_AUDIO_CHANNELS;
	metadata->bits_per_sample = EASNFW_AUDIO_BITS_PER_SAMPLE;
	metadata->schema_version = 1U;
	metadata->timestamp_source = APP_TIMESTAMP_SOURCE_CELLULAR_NETWORK;
	return 0;
}

/** Capture a full track, queuing bounded PCM blocks for processing/storage. */
static void sampling_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (true) {
		struct easnfw_environment_sample environment;
		struct app_mass_storage_record_metadata metadata = {0};
		int64_t environment_utc_ms;
		uint64_t const required_bytes =
			(uint64_t)CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ *
			CONFIG_EASNFW_TRACK_LEN_SECONDS *
			(sizeof(int16_t)) +
			sizeof(struct app_mass_storage_record_metadata) +
			sizeof(uint32_t) + 8192U;
		uint32_t samples_left =
			CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ *
			CONFIG_EASNFW_TRACK_LEN_SECONDS;
		uint32_t sequence = 0U;
		int err;

		do {
			err = app_mass_storage__has_space(required_bytes);
			if (err == -ENOSPC) {
				LOG_INF("Waiting for SD space before next track");
				k_sleep(K_SECONDS(30));
			}
		} while (err == -ENOSPC);
		if (err != 0) {
			LOG_ERR("Cannot check storage capacity: %d", err);
			k_sleep(K_SECONDS(CONFIG_EASNFW_INTER_TRACK_INTERVAL_SECONDS));
			continue;
		}
		err = env_sampling_acquire(&environment);
		if (err == 0) {
			err = app_utc_time__now_ms(&environment_utc_ms);
		}
		if (err == 0) {
			err = record_metadata_init(&metadata, &environment,
						   environment_utc_ms);
		}
		if (err == 0) {
			err = audio_sampling_start();
		}
		if (err == 0) {
			int64_t audio_start_utc_ms;

			err = app_utc_time__now_ms(&audio_start_utc_ms);
			if (err == 0) {
				err = app_utc_time__format(
					audio_start_utc_ms,
					metadata.audio_start_utc,
					sizeof(metadata.audio_start_utc));
			}
		}
		if (err != 0) {
			(void)audio_sampling_stop();
			LOG_ERR("Track acquisition could not start: %d", err);
			k_sleep(K_SECONDS(CONFIG_EASNFW_INTER_TRACK_INTERVAL_SECONDS));
			continue;
		}

		LOG_INF("Starting track %u at %s",
			(unsigned int)metadata.record_id[0],
			metadata.audio_start_utc);
		while (samples_left > 0U) {
			struct easnfw_pipeline_message *message;

			err = easnfw_pipeline_message_alloc(&message);
			if (err != 0) {
				LOG_ERR("Audio pipeline pool exhausted; aborting track");
				break;
			}
			message->kind = EASNFW_PIPELINE_AUDIO_BLOCK;
			message->metadata = metadata;

			err = audio_sampling_acquire(&message->audio);
			if (err != 0) {
				LOG_ERR("Audio acquisition failed at block %u: %d",
					sequence, err);
				easnfw_pipeline_message_free(message);
				break;
			}
			message->sequence = sequence++;
			if (message->audio.sample_count > samples_left) {
				message->audio.sample_count = samples_left;
			}
			samples_left -= message->audio.sample_count;
			if (samples_left == 0U) {
				int64_t audio_end_utc_ms;

				err = app_utc_time__now_ms(&audio_end_utc_ms);
				if (err == 0) {
					err = app_utc_time__format(
						audio_end_utc_ms,
						message->metadata.audio_end_utc,
						sizeof(message->metadata.audio_end_utc));
				}
				if (err != 0) {
					easnfw_pipeline_message_free(message);
					break;
				}
			}
			err = k_msgq_put(&audio_block_q, &message, K_FOREVER);
			if (err != 0) {
				LOG_ERR("Audio pipeline queue failed: %d", err);
				easnfw_pipeline_message_free(message);
				break;
			}
		}
		{
			const int stop_err = audio_sampling_stop();

			if (err == 0 && stop_err != 0) {
				err = stop_err;
			}
		}
		if (err != 0) {
			LOG_ERR("Track %u is incomplete and was not committed",
				sequence);
			err = easnfw_pipeline_abort_track();
			if (err != 0) {
				LOG_ERR("Could not queue track-abort marker: %d", err);
			}
		}
		k_sleep(K_SECONDS(CONFIG_EASNFW_INTER_TRACK_INTERVAL_SECONDS));
	}
}

void sampling_start(void)
{
	k_thread_create(&sampling_thread_data, sampling_stack,
		K_THREAD_STACK_SIZEOF(sampling_stack), sampling_thread, NULL, NULL,
		NULL, 4, 0, K_NO_WAIT);
}