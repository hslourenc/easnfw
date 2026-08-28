#include <stdint.h>

uint32_t easnfw_retry_delay_seconds(uint32_t retry_count)
{
	/* EASNFW_TODO: Enforce the configured retry limit and overflow policy. */
	return retry_count < 31U ? (1U << retry_count) : UINT32_MAX;
}