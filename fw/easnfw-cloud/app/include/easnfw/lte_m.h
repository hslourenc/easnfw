#ifndef EASNFW_LTE_M_H
#define EASNFW_LTE_M_H

#include <stdint.h>

/** Initialize the modem and attach to the LTE network. */
int lte_m_connect(void);

/** Obtain cellular UTC as milliseconds since the Unix epoch. */
int lte_m_get_utc_time_ms(int64_t *unix_time_ms);

#endif
