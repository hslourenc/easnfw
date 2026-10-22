#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/uart.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(easnfw_cloud_uart, LOG_LEVEL_INF);

static const struct device *const uart_dev =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_interfw_link_uart));
static uint8_t tx_buffer[INTERFW_LINK_UART_MAX_FRAME_SIZE];
static uint8_t rx_buffer_a[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t rx_buffer_b[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t *current_rx_buffer;
static easnfw_cloud_uart_rx_handler_t rx_handler;
static bool initialized;
static bool tx_busy;

static void uart_callback(const struct device *dev, struct uart_event *event,
			  void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type == UART_TX_DONE || event->type == UART_TX_ABORTED) {
		tx_busy = false;
	}
	if (event->type == UART_RX_RDY && rx_handler != NULL) {
		rx_handler(&event->data.rx.buf[event->data.rx.offset],
			   event->data.rx.len);
	} else if (event->type == UART_RX_BUF_REQUEST) {
		uint8_t *next = current_rx_buffer == rx_buffer_a ?
			rx_buffer_b : rx_buffer_a;
		if (uart_rx_buf_rsp(dev, next, sizeof(rx_buffer_a)) == 0) {
			current_rx_buffer = next;
		}
	}
}

int easnfw_cloud_uart_init(void)
{
	struct uart_config config = {
		.baudrate = DT_PROP(DT_CHOSEN(easnfw_interfw_link_uart),
				    current_speed),
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS,
	};
	int err;

	if (!device_is_ready(uart_dev)) {
		return -ENODEV;
	}
	err = uart_configure(uart_dev, &config);
	if (err != 0) {
		return err;
	}
	err = uart_callback_set(uart_dev, uart_callback, NULL);
	if (err != 0) {
		return err;
	}
	initialized = true;
	return 0;
}

int easnfw_cloud_uart_tx(uint8_t const *data, size_t len)
{
	if (!initialized || data == NULL || len == 0u ||
	    len > sizeof(tx_buffer) || tx_busy) {
		return -EINVAL;
	}
	memcpy(tx_buffer, data, len);
	tx_busy = true;
	if (uart_tx(uart_dev, tx_buffer, len, SYS_FOREVER_US) != 0) {
		tx_busy = false;
		return -EIO;
	}
	return 0;
}

int easnfw_cloud_uart_rx_set(easnfw_cloud_uart_rx_handler_t handler)
{
	int err;
	if (!initialized || handler == NULL) {
		return -EINVAL;
	}
	rx_handler = handler;
	current_rx_buffer = rx_buffer_a;
	err = uart_rx_enable(uart_dev, current_rx_buffer, sizeof(rx_buffer_a),
			     50000u);
	return err;
}
