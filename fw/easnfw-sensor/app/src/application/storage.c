#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <easnfw/pipeline.h>
#include <easnfw/reset.h>
#include <easnfw/storage.h>
#include <easnfw/sampling.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include "../verif/cli.h"
#include "../verif/test_cases.h"
#endif

LOG_MODULE_REGISTER(easnfw_storage, LOG_LEVEL_INF);
#define STORAGE_STACK_SIZE 6144U

K_THREAD_STACK_DEFINE(storage_stack, STORAGE_STACK_SIZE);
static struct k_thread storage_thread_data;

/** Persist processed blocks and commit each complete track as one record. */
static void storage_thread(void *a, void *b, void *c)
{
	int err;

	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	err = app_mass_storage__init();
	if (err != 0) {
		LOG_ERR("Mass storage unavailable; record pipeline will not start: %d",
			err);
		reset__diagnostic_state(err, NULL);
		return;
	}

	while (true) {
		struct easnfw_pipeline_message *message;
		err = k_msgq_get(&processed_block_q, &message, K_FOREVER);

		if (err != 0) {
			LOG_ERR("Could not receive processed block: %d", err);
			continue;
		}
		if (easnfw_pipeline_message_is_abort(message)) {
			err = app_mass_storage__record_abort();
			if (err != 0) {
				LOG_ERR("Could not close aborted record: %d", err);
				reset__diagnostic_state(err, message->metadata.record_id);
				return;
			}
			continue;
		}
		if (message->sequence == 0U) {
			err = app_mass_storage__record_begin(&message->metadata);
			if (err != 0) {
				uint8_t record_id[sizeof(message->metadata.record_id)];

				memcpy(record_id, message->metadata.record_id,
				       sizeof(record_id));
				LOG_ERR("Could not begin record: %d", err);
				easnfw_pipeline_message_free(message);
				reset__diagnostic_state(err, record_id);
				return;
			}
		}

		err = app_mass_storage__record_append(message->audio.samples,
			message->audio.sample_count *
			sizeof(message->audio.samples[0]));
		if (err != 0) {
			LOG_ERR("Could not persist block %u: %d",
				message->sequence, err);
			(void)app_mass_storage__record_abort();
			uint8_t record_id[sizeof(message->metadata.record_id)];

			memcpy(record_id, message->metadata.record_id,
			       sizeof(record_id));
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
			char record_id_hex[33];

			for (size_t i = 0U; i < sizeof(record_id); ++i) {
				snprintk(&record_id_hex[i * 2U],
					 sizeof(record_id_hex) - i * 2U,
					 "%02x", record_id[i]);
			}
			record_id_hex[sizeof(record_id_hex) - 1U] = '\0';
			int retained_err =
				app_mass_storage__verification_record_is_temporary(
					record_id);

			if (retained_err == 0) {
				LOG_INF("HIL_RUNTIME_RECORD_RETAINED id=%s",
					record_id_hex);
			} else {
				LOG_ERR("HIL_RUNTIME_RECORD_NOT_RETAINED error=%d",
					retained_err);
			}
#endif
			easnfw_pipeline_message_free(message);
			reset__diagnostic_state(err, record_id);
			return;
		}
		if (message->metadata.audio_end_utc[0] != '\0') {
			err = app_mass_storage__record_commit(&message->metadata);
			if (err != 0) {
				LOG_ERR("Could not commit record: %d", err);
				(void)app_mass_storage__record_abort();
				uint8_t record_id[sizeof(message->metadata.record_id)];

				memcpy(record_id, message->metadata.record_id,
				       sizeof(record_id));
				easnfw_pipeline_message_free(message);
				reset__diagnostic_state(err, record_id);
				return;
			}
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
			if (faults.verify_record_metadata != 0) {
				struct app_mass_storage_record_metadata persisted;

				err = app_mass_storage__verification_read_last_metadata(
					message->metadata.record_id, &persisted);
				if (err != 0) {
					LOG_ERR("HIL_RECORD_READ_FAILED error=%d", err);
				} else {
					LOG_INF("HIL_PERSISTED_ENV temp=%d pressure=%u humidity=%u gas=%u env_utc=%s",
						persisted.temperature_milli_c,
						persisted.pressure_pa,
						persisted.humidity_milli_pct,
						persisted.gas_resistance_ohms,
						persisted.environment_sample_utc);
					LOG_INF("HIL_PERSISTED_AUDIO start_utc=%s end_utc=%s",
						persisted.audio_start_utc,
						persisted.audio_end_utc);
				}
			}
#endif
			message->kind = EASNFW_PIPELINE_RECORD_READY;
			err = k_msgq_put(&storage_tx_q, &message, K_FOREVER);
			if (err != 0) {
				LOG_ERR("Committed record could not be queued for transfer: %d",
					err);
				easnfw_pipeline_message_free(message);
			}
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
			else if (faults.short_sampling_cycle != 0) {
				cli__sampling_track_committed();
			}
#endif
		} else {
			easnfw_pipeline_message_free(message);
		}
	}
}

void storage_start(void)
{
	k_thread_create(&storage_thread_data, storage_stack,
		K_THREAD_STACK_SIZEOF(storage_stack), storage_thread, NULL, NULL, NULL,
		6, 0, K_NO_WAIT);
}