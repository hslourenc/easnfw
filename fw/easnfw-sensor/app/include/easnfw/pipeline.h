#ifndef EASNFW_PIPELINE_H
#define EASNFW_PIPELINE_H

#include <zephyr/kernel.h>
#include <stdint.h>

struct easnfw_pipeline_message {
	uint32_t sequence;
	uint8_t kind;
};

extern struct k_msgq audio_block_q;
extern struct k_msgq track_meta_q;
extern struct k_msgq processed_block_q;
extern struct k_msgq storage_tx_q;
extern struct k_msgq tx_ack_q;

void sampling_start(void);
void audio_processing_start(void);
void storage_start(void);
void cloud_link_start(void);

#endif