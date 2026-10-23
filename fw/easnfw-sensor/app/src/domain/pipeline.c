#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include <easnfw/pipeline.h>

K_MSGQ_DEFINE(audio_block_q, sizeof(struct easnfw_pipeline_message *), 4,
	      __alignof__(struct easnfw_pipeline_message *));
K_MSGQ_DEFINE(processed_block_q, sizeof(struct easnfw_pipeline_message *), 4,
	      __alignof__(struct easnfw_pipeline_message *));
K_MSGQ_DEFINE(storage_tx_q, sizeof(struct easnfw_pipeline_message *), 2,
	      __alignof__(struct easnfw_pipeline_message *));
K_MEM_SLAB_DEFINE(audio_message_pool, sizeof(struct easnfw_pipeline_message),
		  EASNFW_PIPELINE_POOL_BLOCKS,
		  __alignof__(struct easnfw_pipeline_message));

static struct easnfw_pipeline_message abort_marker = {
	.kind = EASNFW_PIPELINE_RECORD_ABORT,
};

/** Allocate a bounded pipeline message block without waiting. */
int easnfw_pipeline_message_alloc(struct easnfw_pipeline_message **message)
{
	void *block;
	int err;

	if (message == NULL) {
		return -EINVAL;
	}

	err = k_mem_slab_alloc(&audio_message_pool, &block, K_NO_WAIT);
	if (err == 0) {
		memset(block, 0, sizeof(struct easnfw_pipeline_message));
		*message = block;
	}
	return err;
}

/** Return a pipeline message block to the fixed-size pool. */
void easnfw_pipeline_message_free(struct easnfw_pipeline_message *message)
{
	if (message != NULL && message != &abort_marker) {
		k_mem_slab_free(&audio_message_pool, message);
	}
}

/** Check whether a queued message is the static end-of-track abort marker. */
bool easnfw_pipeline_message_is_abort(
	const struct easnfw_pipeline_message *message)
{
	return message == &abort_marker;
}

/** Queue the static marker that aborts the current track in all stages. */
int easnfw_pipeline_queue_abort(struct k_msgq *queue)
{
	struct easnfw_pipeline_message *message = &abort_marker;

	if (queue == NULL) {
		return -EINVAL;
	}
	return k_msgq_put(queue, &message, K_FOREVER);
}

/** Queue the static marker that aborts the current track in all stages. */
int easnfw_pipeline_abort_track(void)
{
	return easnfw_pipeline_queue_abort(&audio_block_q);
}
