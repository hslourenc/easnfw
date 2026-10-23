#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>
#include <easnfw/cloud_link.h>

LOG_MODULE_REGISTER(easnfw_cloud_link, LOG_LEVEL_INF);
#define CLOUD_LINK_STACK_SIZE 2048U

K_THREAD_STACK_DEFINE(cloud_link_stack, CLOUD_LINK_STACK_SIZE);
static struct k_thread cloud_link_thread_data;

/** Keep committed records pending until a durable cloud ACK is supported. */
static void cloud_link_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (true) {
		struct easnfw_pipeline_message *message;
		int err = k_msgq_get(&storage_tx_q, &message, K_FOREVER);

		if (err != 0) {
			LOG_ERR("Could not receive committed-record notification: %d",
				err);
			continue;
		}
		/**
		 * EASNFW_REVISIT: stream the committed file over IFL and wait for
		 * a distinct durable-cloud acknowledgement before marking it sent.
		 * The record remains in mass storage in the meantime.
		 */
		LOG_WRN("Record committed (id prefix %02x) but cloud transfer is "
			"unavailable; record retained",
			message->metadata.record_id[0]);
		easnfw_pipeline_message_free(message);
	}
}

void cloud_link_start(void)
{
	k_thread_create(&cloud_link_thread_data, cloud_link_stack,
		K_THREAD_STACK_SIZEOF(cloud_link_stack), cloud_link_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}