#ifndef EASNFW_PIPELINE_H
#define EASNFW_PIPELINE_H

#include <zephyr/kernel.h>
#include <stdint.h>

/** Result values carried between cloud pipeline stages. */
enum easnfw_pipeline_result {
	EASNFW_PIPELINE_PENDING = 0,
	EASNFW_PIPELINE_TRANSPORT_UNAVAILABLE,
	EASNFW_PIPELINE_REJECTED,
};

/**
 * Descriptor for one bounded, slab-owned record moving through the pipeline.
 *
 * The buffer ownership transfers with the descriptor and is returned to the
 * record pool by the Receiving stage after the final result is consumed.
 */
struct easnfw_pipeline_message {
	uint32_t sequence;
	uint32_t length;
	uint32_t checksum;
	uint8_t transfer_id;
	enum easnfw_pipeline_result result;
	uint8_t *data;
};

extern struct k_msgq rx_payload_q;
extern struct k_msgq assembled_payload_q;
extern struct k_msgq tx_result_q;

/**
 * @brief Allocate the single bounded record buffer.
 *
 * @param data Receives the allocated buffer address.
 * @return 0 on success, or a negative errno value when no buffer is available.
 */
int easnfw_record_buffer_alloc(uint8_t **data);

/**
 * @brief Return a buffer allocated by easnfw_record_buffer_alloc().
 *
 * @param data Buffer address to release.
 */
void easnfw_record_buffer_free(uint8_t *data);

#endif