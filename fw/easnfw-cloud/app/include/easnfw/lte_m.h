/**
 * @file lte_m.h
 * @brief Cellular modem attachment and network-time interface.
 */
#ifndef EASNFW_LTE_M_H
#define EASNFW_LTE_M_H

#include <stdint.h>

/** Initialize the modem and attach to the configured LTE network. */
int lte_m_connect(void);

/**
 * @brief Obtain cellular UTC as milliseconds since the Unix epoch.
 *
 * @param unix_time_ms Receives the UTC epoch time.
 * @return 0 on success, otherwise a negative errno value.
 */
int lte_m_get_utc_time_ms(int64_t *unix_time_ms);

#endif
