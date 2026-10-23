/**
 * @file payload_format.h
 * @brief Cloud-ingestion payload serialization interface.
 *
 * The final backend schema is not yet defined; this interface is the
 * serialization boundary to update when that contract is agreed.
 */
#ifndef EASNFW_PAYLOAD_FORMAT_H
#define EASNFW_PAYLOAD_FORMAT_H

#include <stddef.h>

/** Encode a canonical record using the cloud ingestion schema. */
int payload_format_encode(const void *record, size_t record_size,
			  void *payload, size_t payload_size);

#endif
