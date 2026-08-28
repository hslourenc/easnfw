#ifndef EASNFW_PIPELINE_H
#define EASNFW_PIPELINE_H

#include <zephyr/kernel.h>
#include <stdint.h>

struct easnfw_pipeline_message {
	uint32_t sequence;
	uint8_t kind;
};

extern struct k_msgq rx_payload_q;
extern struct k_msgq assembled_payload_q;
extern struct k_msgq tx_result_q;

void sensor_link_start(void);
void payload_assembler_start(void);
void cloud_tx_start(void);

#endif