#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_storage, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(storage_stack, 1024);
static struct k_thread storage_thread_data;

static void storage_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		k_msgq_get(&processed_block_q, &message, K_FOREVER);
		/* EASNFW_TODO: Persist blocks, track metadata, and failure details. */
		k_msgq_put(&storage_tx_q, &message, K_NO_WAIT);
	}
}

void storage_start(void)
{
	k_thread_create(&storage_thread_data, storage_stack,
		K_THREAD_STACK_SIZEOF(storage_stack), storage_thread, NULL, NULL, NULL,
		5, 0, K_NO_WAIT);
}