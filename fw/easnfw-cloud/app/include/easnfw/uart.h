#ifndef EASNFW_CLOUD_UART_H
#define EASNFW_CLOUD_UART_H

#include <stddef.h>
#include <stdint.h>

typedef void (*easnfw_cloud_uart_rx_handler_t)(uint8_t const *data, size_t len);

int easnfw_cloud_uart_init(void);
int easnfw_cloud_uart_tx(uint8_t const *data, size_t len);
int easnfw_cloud_uart_rx_set(easnfw_cloud_uart_rx_handler_t handler);

#endif
