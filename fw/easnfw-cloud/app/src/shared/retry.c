#include <stdint.h>

/**
 * @brief Return the capped exponential-backoff ceiling in seconds.
 *
 * The cloud pipeline adds the requirement's randomization within the
 * 75%-to-100% window. Retry counts beyond the configured limit saturate.
 *
 * @param retry_count Zero-based global retry count.
 * @return A delay ceiling between one and thirty seconds.
 */
uint32_t easnfw_retry_delay_seconds(uint32_t retry_count)
{
	uint32_t delay = retry_count >= 5U ? 30U : (1U << retry_count);

	if (retry_count >= 10U || delay > 30U) {
		delay = 30U;
	}
	return delay;
}