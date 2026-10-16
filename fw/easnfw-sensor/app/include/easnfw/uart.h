#ifndef EASNFW_UART_H
#define EASNFW_UART_H

#include <stdbool.h>

/**
 * @brief Get Inter-FW Link UART initialization status.
 *
 * @return true if Inter-FW Link UART is initialized, false otherwise.
 */
bool uart__is_ifl_init(void);

/**
 * @brief Initialize Inter-FW Link UART.
 *
 * The UART is configured for 1 Mbps, 8N1, RTS/CTS hardware flow control.
 *
 * @return 0 on success, or a negative EASNFW status code on failure.
 */
int uart__ifl_init(void);

#endif /* EASNFW_UART_H */