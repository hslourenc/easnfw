#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/atomic.h>

#include <easnfw/audio_sampling.h>
#include <easnfw/sampling.h>
#include <easnfw/env_sampling.h>
#include <easnfw/pipeline.h>
#include <easnfw/mass_storage.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>
#include <easnfw/reset.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include "../verif/test_cases.h"
#endif

LOG_MODULE_REGISTER(easnfw_sampling, LOG_LEVEL_INF);
#define SAMPLING_STACK_SIZE 6144U

K_THREAD_STACK_DEFINE(sampling_stack, SAMPLING_STACK_SIZE);
static struct k_thread sampling_thread_data;
static atomic_t sampling_stop_requested;
static atomic_t sampling_thread_started;
K_SEM_DEFINE(sampling_stop_sem, 0, 1);
K_SEM_DEFINE(sampling_stopped_sem, 0, 1);

static void sampling_thread_stopped(void)
{
	atomic_set(&sampling_thread_started, 0);
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	LOG_INF("HIL_SAMPLING_STOPPED");
#endif
	k_sem_give(&sampling_stopped_sem);
}

/** Generate a unique ID and record environmental and sampling metadata. */
static int record_metadata_init(
	struct app_mass_storage_record_metadata *metadata,
	const struct easnfw_environment_sample *environment,
	int64_t environment_utc_ms, uint32_t track_seconds)
{
	int err;

	for (size_t i = 0U; i < sizeof(metadata->record_id); i += sizeof(uint32_t)) {
		const uint32_t random = sys_rand32_get();

		memcpy(&metadata->record_id[i], &random, sizeof(random));
	}
	metadata->sample_rate_hz = CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ;
	metadata->track_duration_seconds = track_seconds;
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
		track_seconds;
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

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	unsigned int completed_tracks = 0U;
#endif
	while (atomic_get(&sampling_stop_requested) == 0) {
		struct easnfw_environment_sample environment;
		struct app_mass_storage_record_metadata metadata = {0};
		int64_t environment_utc_ms;
		uint32_t track_seconds = CONFIG_EASNFW_TRACK_LEN_SECONDS;
		uint32_t interval_seconds = CONFIG_EASNFW_INTER_TRACK_INTERVAL_SECONDS;
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
		if (faults.short_sampling_cycle != 0) {
			track_seconds = 1U;
			interval_seconds = 2U;
		}
#endif
		uint64_t const required_bytes =
			(uint64_t)CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ *
			track_seconds *
			(sizeof(int16_t)) +
			sizeof(struct app_mass_storage_record_metadata) +
			sizeof(uint32_t) + 8192U;
		uint32_t samples_left =
			CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ *
			track_seconds;
		uint32_t sequence = 0U;
		int err;

		for (;;) {
			if (atomic_get(&sampling_stop_requested) != 0) {
				sampling_thread_stopped();
				return;
			}
			err = app_mass_storage__has_space(required_bytes);
			if (err == -ENOSPC) {
				LOG_INF("Waiting for SD space before next track");
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
				if (faults.storage_space_shortage != 0) {
					LOG_INF("HIL_STORAGE_WAIT");
					(void)k_sem_take(&sampling_stop_sem, K_SECONDS(1));
				} else {
					(void)k_sem_take(&sampling_stop_sem, K_SECONDS(30));
				}
#else
				(void)k_sem_take(&sampling_stop_sem, K_SECONDS(30));
#endif
				continue;
			}
			break;
		}
		if (err != 0) {
			LOG_ERR("Cannot check storage capacity: %d", err);
			reset__diagnostic_state(err, NULL);
			return;
		}
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
		if (faults.storage_space_shortage != 0) {
			faults.storage_space_shortage = 0;
			LOG_INF("HIL_STORAGE_SPACE_AVAILABLE");
		}
#endif
		err = env_sampling_acquire(&environment);
		if (err == 0) {
			err = app_utc_time__now_ms(&environment_utc_ms);
		}
		if (err == 0) {
			err = record_metadata_init(&metadata, &environment,
						   environment_utc_ms, track_seconds);
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
			k_sleep(K_SECONDS(interval_seconds));
			continue;
		}

		LOG_INF("Starting track %u at %s",
			(unsigned int)metadata.record_id[0],
			metadata.audio_start_utc);
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
		if (faults.short_sampling_cycle != 0) {
			LOG_INF("HIL_TRACK_START uptime_ms=%lld env_utc=%s start_utc=%s",
				k_uptime_get(), metadata.environment_sample_utc,
				metadata.audio_start_utc);
		}
#endif
		while (samples_left > 0U &&
		       atomic_get(&sampling_stop_requested) == 0) {
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
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
				if (faults.short_sampling_cycle != 0) {
					LOG_INF("HIL_TRACK_END uptime_ms=%lld end_utc=%s",
						k_uptime_get(), message->metadata.audio_end_utc);
				}
#endif
			}
			for (;;) {
				err = k_msgq_put(&audio_block_q, &message,
						 K_MSEC(100));
				if (err == 0) {
					break;
				}
				if (err != -EAGAIN ||
				    atomic_get(&sampling_stop_requested) != 0) {
					LOG_ERR("Audio pipeline queue failed: %d", err);
					easnfw_pipeline_message_free(message);
					break;
				}
			}
			if (err != 0) {
				break;
			}
		}
		{
			const int stop_err = audio_sampling_stop();

			if (err == 0 && stop_err != 0) {
				err = stop_err;
			}
		}
		if (atomic_get(&sampling_stop_requested) != 0 && err == 0) {
			err = -ECANCELED;
		}
		if (err != 0) {
			LOG_ERR("Track %u is incomplete and was not committed",
				sequence);
			if (atomic_get(&sampling_stop_requested) == 0) {
				err = easnfw_pipeline_abort_track();
				if (err != 0) {
					LOG_ERR("Could not queue track-abort marker: %d",
						err);
				}
			}
		}
		if (atomic_get(&sampling_stop_requested) != 0) {
			sampling_thread_stopped();
			return;
		}
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
		else if (faults.short_sampling_cycle != 0 && ++completed_tracks == 2U) {
			(void)k_sem_take(&sampling_stop_sem, K_FOREVER);
		}
#endif
		(void)k_sem_take(&sampling_stop_sem, K_SECONDS(interval_seconds));
	}
	sampling_thread_stopped();
}

void sampling_start(void)
{
	atomic_clear(&sampling_stop_requested);
	k_sem_reset(&sampling_stop_sem);
	k_sem_reset(&sampling_stopped_sem);
	atomic_set(&sampling_thread_started, 1);
	k_thread_create(&sampling_thread_data, sampling_stack,
		K_THREAD_STACK_SIZEOF(sampling_stack), sampling_thread, NULL, NULL,
		NULL, 4, 0, K_NO_WAIT);
}

int sampling_request_stop(void)
{
	atomic_set(&sampling_stop_requested, 1);
	k_sem_give(&sampling_stop_sem);
	if (atomic_get(&sampling_thread_started) == 0) {
		return 0;
	}
	if (k_current_get() == &sampling_thread_data) {
		sampling_thread_stopped();
		return 0;
	}
	if (k_sem_take(&sampling_stopped_sem, K_SECONDS(5)) != 0) {
		return -ETIMEDOUT;
	}
	return 0;
}