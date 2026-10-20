#ifndef EASNFW_UART_H
#define EASNFW_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*uart__ifl_rx_handler_t)(uint8_t const *data,
				       size_t const len);

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

/**
 * @brief Transmit one complete IFL frame using the UART async API.
 *
 * @param data Frame bytes to copy into the transport-owned TX buffer.
 * @param len Number of bytes in @p data.
 *
 * @return 0 on success, or a negative EASNFW status code on failure.
 */
int uart__ifl_tx(uint8_t const *data, size_t const len);

/**
 * @brief Register the callback that receives asynchronous UART data.
 *
 * @param handler Callback invoked for each received data segment.
 *
 * @return 0 on success, or a negative EASNFW status code on failure.
 */
int uart__ifl_rx_handler_set(uart__ifl_rx_handler_t handler);

#endif /* EASNFW_UART_H */