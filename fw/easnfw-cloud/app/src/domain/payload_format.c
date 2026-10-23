#include <errno.h>
#include <easnfw/payload_format.h>
#include <stddef.h>

/**
 * @brief Encode a canonical record for the cloud platform.
 *
 * EASNFW_REVISIT: the record schema, envelope, encoding, and cloud endpoint
 * contract are not defined. This function must not manufacture a payload that
 * could be mistaken for a valid upload.
 *
 * @return -ENOTSUP until the cloud contract is specified and implemented.
 */
int payload_format_encode(const void *record, size_t record_size,
			  void *payload, size_t payload_size)
{
	(void)record;
	(void)record_size;
	(void)payload;
	(void)payload_size;
	return -ENOTSUP;
}
