#include <errno.h>

#include <easnfw/http_client.h>

/**
 * @brief Post a payload using the not-yet-defined cloud ingestion contract.
 *
 * EASNFW_REVISIT: define HTTPS endpoint, TLS trust, device authentication,
 * request schema, retryable status codes, and the durable commit response.
 *
 * @return -ENOTSUP; no request is sent and no delivery is acknowledged.
 */
int http_client_post(const void *payload, unsigned int payload_size)
{
	(void)payload;
	(void)payload_size;
	return -ENOTSUP;
}
