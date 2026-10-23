#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>
#include <easnfw/storage.h>

LOG_MODULE_REGISTER(easnfw_storage, LOG_LEVEL_INF);
#define STORAGE_STACK_SIZE 6144U

K_THREAD_STACK_DEFINE(storage_stack, STORAGE_STACK_SIZE);
static struct k_thread storage_thread_data;

/** Persist processed blocks and commit each complete track as one record. */
static void storage_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	if (app_mass_storage__init() != 0) {
		LOG_ERR("Mass storage unavailable; record pipeline will not start");
		return;
	}

	bool dropping_track = false;

	while (true) {
		struct easnfw_pipeline_message *message;
		int err = k_msgq_get(&processed_block_q, &message, K_FOREVER);

		if (err != 0) {
			LOG_ERR("Could not receive processed block: %d", err);
			continue;
		}
		if (easnfw_pipeline_message_is_abort(message)) {
			(void)app_mass_storage__record_abort();
			dropping_track = false;
			continue;
		}
		if (dropping_track) {
			const bool track_ended =
				message->metadata.audio_end_utc[0] != '\0';

			easnfw_pipeline_message_free(message);
			if (track_ended) {
				(void)app_mass_storage__record_abort();
				dropping_track = false;
			}
			continue;
		}
		if (message->sequence == 0U) {
			err = app_mass_storage__record_begin(&message->metadata);
			if (err != 0) {
				LOG_ERR("Could not begin record: %d", err);
				easnfw_pipeline_message_free(message);
				dropping_track = true;
				continue;
			}
		}

		err = app_mass_storage__record_append(message->audio.samples,
			message->audio.sample_count *
			sizeof(message->audio.samples[0]));
		if (err != 0) {
			LOG_ERR("Could not persist block %u: %d",
				message->sequence, err);
			(void)app_mass_storage__record_abort();
			easnfw_pipeline_message_free(message);
			dropping_track = true;
			continue;
		}
		if (message->metadata.audio_end_utc[0] != '\0') {
			err = app_mass_storage__record_commit(&message->metadata);
			if (err != 0) {
				LOG_ERR("Could not commit record: %d", err);
				(void)app_mass_storage__record_abort();
				easnfw_pipeline_message_free(message);
				continue;
			}
			message->kind = EASNFW_PIPELINE_RECORD_READY;
			err = k_msgq_put(&storage_tx_q, &message, K_FOREVER);
			if (err != 0) {
				LOG_ERR("Committed record could not be queued for transfer: %d",
					err);
				easnfw_pipeline_message_free(message);
			}
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