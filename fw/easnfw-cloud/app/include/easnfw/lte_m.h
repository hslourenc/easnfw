/**
 * @file lte_m.h
 * @brief Cellular modem attachment and network-time interface.
 */
#ifndef EASNFW_LTE_M_H
#define EASNFW_LTE_M_H

#include <stdint.h>

/** Start registration; return 0 when registered, -EAGAIN while pending. */
int lte_m_connect(void);

/**
 * @brief Obtain cellular UTC as milliseconds since the Unix epoch.
 *
 * @param unix_time_ms Receives the UTC epoch time.
 * @return 0 on success, -EAGAIN while registration or time is pending,
 *         otherwise a negative errno value.
 */
int lte_m_get_utc_time_ms(int64_t *unix_time_ms);

#endif
