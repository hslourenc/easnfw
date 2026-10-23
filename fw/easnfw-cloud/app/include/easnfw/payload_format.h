#ifndef EASNFW_PAYLOAD_FORMAT_H
#define EASNFW_PAYLOAD_FORMAT_H

#include <stddef.h>

/** Encode a canonical record using the cloud ingestion schema. */
int payload_format_encode(const void *record, size_t record_size,
			  void *payload, size_t payload_size);

#endif
