#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <easnfw/pipeline.h>
#include <easnfw/retry.h>
#include <easnfw/cloud_tx.h>
#include <easnfw/http_client.h>
#include <easnfw/lte_m.h>
#include <easnfw/payload_format.h>

LOG_MODULE_REGISTER(easnfw_cloud_tx, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(cloud_tx_stack, 1024);
static struct k_thread cloud_tx_thread_data;

#define CLOUD_MAX_RETRIES 10u
static uint32_t global_retry_count;

/** @brief Identify failures eligible for bounded transient retry. */
static bool is_transient_error(int error)
{
	return error == -EAGAIN || error == -ETIMEDOUT ||
	       error == -ECONNRESET || error == -EHOSTUNREACH ||
	       error == -ENETUNREACH;
}

/** @brief Wait a uniformly selected delay in the required backoff window. */
static void wait_retry_window(uint32_t retry_count)
{
	uint32_t ceiling = easnfw_retry_delay_seconds(retry_count);
	uint32_t minimum_ms = ceiling * 750u;
	uint32_t choices_ms = ceiling * 250u + 1u;
	uint32_t limit = UINT32_MAX - (UINT32_MAX % choices_ms);
	uint32_t random;
	uint32_t delay_ms;

	do {
		random = sys_rand32_get();
	} while (random >= limit);
	delay_ms = minimum_ms + random % choices_ms;

	k_sleep(K_MSEC(delay_ms));
}

/** @brief Attempt cloud delivery without treating local acceptance as commit. */
static int transmit_record(struct easnfw_pipeline_message const *message)
{
	for (;;) {
		int err = payload_format_encode(message->data, message->length,
						NULL, 0u);

		if (err != 0) {
			return err;
		}
		err = lte_m_connect();

		if (err == 0) {
			err = http_client_post(message->data, message->length);
		}
		if (err == 0) {
			global_retry_count = 0u;
			return 0;
		}
		if (!is_transient_error(err) ||
		    global_retry_count >= CLOUD_MAX_RETRIES) {
			return err;
		}

		wait_retry_window(global_retry_count);
		if (global_retry_count < CLOUD_MAX_RETRIES) {
			++global_retry_count;
		}
	}
}

/** @brief Sole owner of the cloud transport attempt and retry state. */
static void cloud_tx_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		struct easnfw_pipeline_message message;
		int err;

		if (k_msgq_get(&assembled_payload_q, &message, K_FOREVER) != 0) {
			continue;
		}
		err = transmit_record(&message);
		if (err == -ENOTSUP) {
			message.result = EASNFW_PIPELINE_TRANSPORT_UNAVAILABLE;
			LOG_ERR("Cloud transport unavailable for transfer id=%u; "
				"no durable-delivery ACK issued", message.transfer_id);
		} else if (err == 0) {
			/*
			 * EASNFW_REVISIT: report this commit over a specified IFL
			 * command before SENSOR may remove its durable record.
			 */
			message.result = EASNFW_PIPELINE_PENDING;
			LOG_INF("Cloud commit received for transfer id=%u, but no "
				"durable-ACK command is defined", message.transfer_id);
		} else {
			message.result = EASNFW_PIPELINE_REJECTED;
			LOG_ERR("Transfer id=%u failed without durable delivery (%d)",
				message.transfer_id, err);
		}

		if (k_msgq_put(&tx_result_q, &message, K_NO_WAIT) != 0) {
			LOG_ERR("Result queue full; retaining no claim of delivery");
			easnfw_record_buffer_free(message.data);
		}
	}
}

/**
 * @brief Start the bounded cloud-transmission stage.
 */
void cloud_tx_start(void)
{
	k_thread_create(&cloud_tx_thread_data, cloud_tx_stack,
			K_THREAD_STACK_SIZEOF(cloud_tx_stack), cloud_tx_thread,
			NULL, NULL, NULL, 5, 0, K_NO_WAIT);
}
