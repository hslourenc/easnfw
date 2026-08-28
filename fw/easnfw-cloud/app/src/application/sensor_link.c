#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_sensor_link, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(sensor_link_stack, 1024);
static struct k_thread sensor_link_thread_data;

static void sensor_link_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		/* EASNFW_TODO: Receive frames and relay acknowledgements over SPI. */
		k_msgq_get(&tx_result_q, &message, K_FOREVER);
		LOG_INF("Delivery result for sequence %u", message.sequence);
	}
}

void sensor_link_start(void)
{
	k_thread_create(&sensor_link_thread_data, sensor_link_stack,
		K_THREAD_STACK_SIZEOF(sensor_link_stack), sensor_link_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}