#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_cloud_tx, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(cloud_tx_stack, 1024);
static struct k_thread cloud_tx_thread_data;

static void cloud_tx_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		k_msgq_get(&assembled_payload_q, &message, K_FOREVER);
		/* EASNFW_TODO: Use LTE-M/HTTPS and apply exponential retry backoff. */
		k_msgq_put(&tx_result_q, &message, K_NO_WAIT);
	}
}

void cloud_tx_start(void)
{
	k_thread_create(&cloud_tx_thread_data, cloud_tx_stack,
		K_THREAD_STACK_SIZEOF(cloud_tx_stack), cloud_tx_thread, NULL, NULL, NULL,
		5, 0, K_NO_WAIT);
}