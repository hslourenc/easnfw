#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>
#include <easnfw/payload_assembler.h>

LOG_MODULE_REGISTER(easnfw_payload_assembler, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(payload_assembler_stack, 1024);
static struct k_thread payload_assembler_thread_data;

/**
 * @brief Validate a complete staged transfer and forward its buffer ownership.
 *
 * The wire contract currently provides per-frame CRC and contiguous fragment
 * offsets, but does not define a canonical-record envelope or whole-record
 * checksum. Those format checks remain explicitly revisitable.
 */
static void payload_assembler_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		struct easnfw_pipeline_message message;

		if (k_msgq_get(&rx_payload_q, &message, K_FOREVER) != 0) {
			continue;
		}
		if (message.data == NULL || message.length == 0u ||
		    message.length > CONFIG_EASNFW_CLOUD_MAX_RECORD_SIZE) {
			LOG_ERR("Rejecting invalid record transfer id=%u len=%u",
				message.transfer_id, message.length);
			easnfw_record_buffer_free(message.data);
			continue;
		}

		/*
		 * EASNFW_REVISIT: canonical-record schema, record_id and
		 * whole-record integrity check must be agreed with SENSOR.
		 * Preserve the received bytes verbatim in volatile memory.
		 */
		LOG_INF("Validated transfer id=%u bytes=%u checksum=%08x",
			message.transfer_id, message.length, message.checksum);
		if (k_msgq_put(&assembled_payload_q, &message, K_NO_WAIT) != 0) {
			LOG_ERR("Cloud assembly queue full; transfer id=%u rejected",
				message.transfer_id);
			easnfw_record_buffer_free(message.data);
		}
	}
}

/**
 * @brief Start the bounded record-validation stage.
 */
void payload_assembler_start(void)
{
	k_thread_create(&payload_assembler_thread_data, payload_assembler_stack,
			K_THREAD_STACK_SIZEOF(payload_assembler_stack),
			payload_assembler_thread, NULL, NULL, NULL, 5, 0,
			K_NO_WAIT);
}
