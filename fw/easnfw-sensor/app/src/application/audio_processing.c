#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/audio_algo.h>
#include <easnfw/audio_processing.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_audio_processing, LOG_LEVEL_INF);
#define AUDIO_PROCESSING_STACK_SIZE 4096U

K_THREAD_STACK_DEFINE(audio_processing_stack, AUDIO_PROCESSING_STACK_SIZE);
static struct k_thread audio_processing_thread_data;

/** Process one block without changing samples until the algorithm is specified. */
static void audio_processing_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	bool dropping_track = false;

	while (true) {
		struct easnfw_pipeline_message *message;
		int err = k_msgq_get(&audio_block_q, &message, K_FOREVER);

		if (err != 0) {
			LOG_ERR("Could not receive audio block: %d", err);
			continue;
		}
		if (easnfw_pipeline_message_is_abort(message)) {
			err = easnfw_pipeline_queue_abort(&processed_block_q);
			if (err != 0) {
				LOG_ERR("Could not queue track-abort marker: %d", err);
			}
			dropping_track = false;
			continue;
		}
		if (dropping_track) {
			const bool track_ended =
				message->metadata.audio_end_utc[0] != '\0';

			easnfw_pipeline_message_free(message);
			if (track_ended) {
				dropping_track = false;
				err = easnfw_pipeline_queue_abort(&processed_block_q);
				if (err != 0) {
					LOG_ERR("Could not queue track-abort marker: %d",
						err);
				}
			}
			continue;
		}

		err = audio_algo_process(&message->audio, &message->audio);
		if (err != 0) {
			const bool track_ended =
				message->metadata.audio_end_utc[0] != '\0';

			LOG_ERR("Audio processing failed for block %u: %d",
				message->sequence, err);
			easnfw_pipeline_message_free(message);
			dropping_track = !track_ended;
			if (track_ended) {
				err = easnfw_pipeline_queue_abort(&processed_block_q);
				if (err != 0) {
					LOG_ERR("Could not queue track-abort marker: %d",
						err);
				}
			}
			continue;
		}
		err = k_msgq_put(&processed_block_q, &message, K_FOREVER);
		if (err != 0) {
			const bool track_ended =
				message->metadata.audio_end_utc[0] != '\0';

			LOG_ERR("Could not queue processed block: %d", err);
			easnfw_pipeline_message_free(message);
			dropping_track = !track_ended;
			if (track_ended) {
				err = easnfw_pipeline_queue_abort(&processed_block_q);
				if (err != 0) {
					LOG_ERR("Could not queue track-abort marker: %d",
						err);
				}
			}
		}
	}
}

void audio_processing_start(void)
{
	k_thread_create(&audio_processing_thread_data, audio_processing_stack,
		K_THREAD_STACK_SIZEOF(audio_processing_stack), audio_processing_thread,
		NULL, NULL, NULL, 5, 0, K_NO_WAIT);
}