#ifndef EASNFW_RETRY_H
#define EASNFW_RETRY_H

#include <stdint.h>

/** @brief Get the capped exponential-backoff delay ceiling in seconds. */
uint32_t easnfw_retry_delay_seconds(uint32_t retry_count);

#endif
