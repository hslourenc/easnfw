#include <errno.h>
#include <limits.h>

#include <easnfw/shared.h>

uint16_t easnfw_checksum16(const void *data, size_t size)
{
	const uint8_t *bytes = data;
	uint32_t checksum = 0U;

	for (size_t index = 0U; index < size; ++index) {
		checksum = (checksum + bytes[index]) & UINT16_MAX;
	}
	return (uint16_t)checksum;
}

int easnfw_frame_validate(const struct easnfw_frame_header *header,
	size_t payload_size)
{
	if (header == NULL || header->protocol_version != EASNFW_PROTOCOL_VERSION ||
	    header->schema_version != EASNFW_RECORD_SCHEMA_VERSION ||
	    header->fragment_count == 0U ||
	    header->fragment_index >= header->fragment_count ||
	    header->payload_length != payload_size) {
		return -EINVAL;
	}
	return 0;
}

size_t easnfw_record_size(void)
{
	return sizeof(struct easnfw_ecoacoustic_record);
}

uint16_t easnfw_protocol_version(void)
{
	return EASNFW_PROTOCOL_VERSION;
}

uint32_t easnfw_retry_delay_seconds(uint32_t retry_count,
	uint32_t maximum_delay_seconds)
{
	uint32_t delay = 1U;

	while (retry_count > 0U && delay < maximum_delay_seconds) {
		delay = (delay > maximum_delay_seconds / 2U) ?
			maximum_delay_seconds : delay * 2U;
		--retry_count;
	}
	return delay > maximum_delay_seconds ? maximum_delay_seconds : delay;
}
