#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_cloud_link, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(cloud_link_stack, 1024);
static struct k_thread cloud_link_thread_data;

static void cloud_link_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		k_msgq_get(&storage_tx_q, &message, K_FOREVER);
		/* EASNFW_TODO: Frame and send over SPI, then wait for CLOUD ack. */
		k_msgq_put(&tx_ack_q, &message, K_NO_WAIT);
	}
}

void cloud_link_start(void)
{
	k_thread_create(&cloud_link_thread_data, cloud_link_stack,
		K_THREAD_STACK_SIZEOF(cloud_link_stack), cloud_link_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}