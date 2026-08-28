#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_payload_assembler, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(payload_assembler_stack, 1024);
static struct k_thread payload_assembler_thread_data;

static void payload_assembler_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		k_msgq_get(&rx_payload_q, &message, K_FOREVER);
		/* EASNFW_TODO: Validate records and format cloud payloads. */
		k_msgq_put(&assembled_payload_q, &message, K_NO_WAIT);
	}
}

void payload_assembler_start(void)
{
	k_thread_create(&payload_assembler_thread_data, payload_assembler_stack,
		K_THREAD_STACK_SIZEOF(payload_assembler_stack), payload_assembler_thread,
		NULL, NULL, NULL, 5, 0, K_NO_WAIT);
}