#ifndef EASNFW_PIPELINE_H
#define EASNFW_PIPELINE_H

#include <zephyr/kernel.h>
#include <stdint.h>

#include <easnfw/audio_sampling.h>
#include <easnfw/mass_storage.h>

enum easnfw_pipeline_message_kind {
	EASNFW_PIPELINE_AUDIO_BLOCK = 1,
	EASNFW_PIPELINE_RECORD_READY,
	EASNFW_PIPELINE_RECORD_ABORT,
};

/** Bounded message exchanged by the sampling, processing, and storage stages. */
struct easnfw_pipeline_message {
	uint32_t sequence;
	enum easnfw_pipeline_message_kind kind;
	struct app_mass_storage_record_metadata metadata;
	struct easnfw_audio_block audio;
};

#define EASNFW_PIPELINE_POOL_BLOCKS 8U

extern struct k_msgq audio_block_q;
extern struct k_msgq processed_block_q;
extern struct k_msgq storage_tx_q;

/** Allocate one bounded pipeline message block without waiting. */
int easnfw_pipeline_message_alloc(struct easnfw_pipeline_message **message);

/** Return a pipeline message block to the fixed-size pool. */
void easnfw_pipeline_message_free(struct easnfw_pipeline_message *message);

/** Check whether a queued message is the static end-of-track abort marker. */
bool easnfw_pipeline_message_is_abort(
	const struct easnfw_pipeline_message *message);

/** Queue the static abort marker on a specified pipeline stage. */
int easnfw_pipeline_queue_abort(struct k_msgq *queue);

/** Queue the static marker that aborts the current track in all stages. */
int easnfw_pipeline_abort_track(void);

#endif