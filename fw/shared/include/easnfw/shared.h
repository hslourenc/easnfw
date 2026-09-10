#ifndef EASNFW_SHARED_H_
#define EASNFW_SHARED_H_

#include <stddef.h>
#include <stdint.h>

#define EASNFW_PROTOCOL_VERSION 1U
#define EASNFW_RECORD_SCHEMA_VERSION 1U

enum easnfw_message_kind {
	EASNFW_MESSAGE_RECORD = 1,
	EASNFW_MESSAGE_POWER_ON_LOG = 2,
	EASNFW_MESSAGE_ACK = 3,
};

struct easnfw_pipeline_message {
	uint32_t sequence;
	uint8_t kind;
	uint8_t reserved[3];
};

struct easnfw_frame_header {
	uint16_t protocol_version;
	uint16_t schema_version;
	uint8_t message_type;
	uint8_t reserved;
	uint32_t record_id;
	uint16_t fragment_index;
	uint16_t fragment_count;
	uint16_t payload_length;
	uint16_t checksum;
};

struct easnfw_ecoacoustic_record {
	uint32_t record_id;
	uint32_t firmware_version;
	uint32_t sample_rate_hz;
	uint32_t sample_count;
	uint32_t timestamp_start;
	uint32_t timestamp_end;
	uint8_t complete;
	uint8_t reserved[3];
};

uint16_t easnfw_checksum16(const void *data, size_t size);
int easnfw_frame_validate(const struct easnfw_frame_header *header,
	size_t payload_size);
size_t easnfw_record_size(void);
uint16_t easnfw_protocol_version(void);
uint32_t easnfw_retry_delay_seconds(uint32_t retry_count,
	uint32_t maximum_delay_seconds);

#endif
