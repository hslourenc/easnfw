#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/status.h>
#include <easnfw/uart.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(easnfw_ifl_uart, LOG_LEVEL_INF);

static const struct device *ifl_uart_dev =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_interfw_link_uart));
/** Maximum time a single IFL frame may take to transmit. */
#define IFL_TX_TIMEOUT_MS 200

static uint8_t ifl_tx_buffer[INTERFW_LINK_UART_MAX_FRAME_SIZE];
static uint8_t ifl_rx_buffer_a[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t ifl_rx_buffer_b[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t *ifl_current_rx_buffer;
static bool ifl_tx_busy;
static bool ifl_rx_enabled;
static uart__ifl_rx_handler_t ifl_rx_handler;

/**
 * Clear the transport busy state when the asynchronous UART transfer ends.
 * The buffer remains reserved until one of these events is received.
 */
static void ifl_uart_callback(const struct device *dev,
			      struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if ((evt->type == UART_TX_DONE) || (evt->type == UART_TX_ABORTED)) {
		ifl_tx_busy = false;
		LOG_DBG("IFL UART TX %s", evt->type == UART_TX_DONE ? "complete" : "aborted");
	}

	if (evt->type == UART_RX_RDY) {
		if ((ifl_rx_handler != NULL) && (evt->data.rx.len > 0u)) {
			ifl_rx_handler(&evt->data.rx.buf[evt->data.rx.offset],
				       evt->data.rx.len);
		}
	} else if (evt->type == UART_RX_BUF_REQUEST) {
		uint8_t *next_buffer = (ifl_current_rx_buffer == ifl_rx_buffer_a) ?
			ifl_rx_buffer_b : ifl_rx_buffer_a;

		if (uart_rx_buf_rsp(dev, next_buffer, sizeof(ifl_rx_buffer_a)) == 0) {
			ifl_current_rx_buffer = next_buffer;
			LOG_DBG("IFL UART RX buffer rotated");
		} else {
			LOG_ERR("Failed to provide next IFL UART RX buffer");
		}
	} else if (evt->type == UART_RX_DISABLED) {
		/* A line error (e.g. peer reset glitch) disables RX; restart it. */
		ifl_rx_enabled = false;
		LOG_WRN("IFL UART RX disabled, re-enabling");
		ifl_current_rx_buffer = ifl_rx_buffer_a;
		if (uart_rx_enable(dev, ifl_current_rx_buffer,
				   sizeof(ifl_rx_buffer_a),
				   CONFIG_EASNFW_IFL_UART_RX_INTERBYTE_TIMEOUT_MS *
				   1000u) == 0) {
			ifl_rx_enabled = true;
		}
	}
}

/** Indicate whether Inter-FW Link UART is initialized */
static bool uart_ifl_init = false;

bool uart__is_ifl_init(void)
{
	return uart_ifl_init;
}

int uart__ifl_init(void)
{
	/* Keep the runtime configuration aligned with the devicetree wiring. */
	const struct uart_config config = {
		.baudrate  = DT_PROP(DT_CHOSEN(easnfw_interfw_link_uart),
				    current_speed),
		.parity    = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS,
	};
	int err;

	if (!device_is_ready(ifl_uart_dev)) {
		LOG_ERR("IFL UART device is not ready");
		uart_ifl_init = false;
		return -EASNFW_STATUS__INTERFW_LINK_UART_INIT_FAILED;
	}

	LOG_INF("Configuring IFL UART at %u baud with RTS/CTS",
		config.baudrate);

	err = uart_configure(ifl_uart_dev, &config);
	if (err != 0) {
		LOG_ERR("Failed to configure IFL UART (%d)", err);
		uart_ifl_init = false;
		return -EASNFW_STATUS__INTERFW_LINK_UART_CONFIG_FAILED;
	}

	err = uart_callback_set(ifl_uart_dev, ifl_uart_callback, NULL);
	if (err != 0) {
		LOG_ERR("Failed to set IFL UART callback (%d)", err);
		return -EASNFW_STATUS__INTERFW_LINK_UART_CONFIG_FAILED;
	}

	uart_ifl_init = true;
	LOG_INF("IFL UART initialized");
	return 0;
}

int uart__ifl_tx(uint8_t const *data, size_t const len)
{
	int err;

	/* The caller must initialize the UART before requesting transmission. */
	if (!uart_ifl_init) {
		LOG_ERR("Cannot transmit: IFL UART is not initialized");
		return -EASNFW_STATUS__INTERFW_LINK_UART_INIT_FAILED;
	}

	if ((data == NULL) || (len == 0u) || (len > sizeof(ifl_tx_buffer))) {
		LOG_ERR("Invalid IFL UART TX buffer (len=%u)", (unsigned int)len);
		return -EASNFW_STATUS__INTERFW_LINK_INVALID_PAYLOAD;
	}

	/* The transport owns one buffer, so only one async TX may be active. */
	if (ifl_tx_busy) {
		LOG_WRN("IFL UART TX already in progress");
		return -EASNFW_STATUS__INTERFW_LINK_TX_BUSY;
	}

	/* Copy before starting DMA; the caller's buffer may be short-lived. */
	memcpy(ifl_tx_buffer, data, len);
	ifl_tx_busy = true;
	/*
	 * Bounded timeout: if the peer is in reset its RTS (our CTS) may stall
	 * the transfer, and the abort releases the TX buffer instead of
	 * leaving the transport busy forever.
	 */
	err = uart_tx(ifl_uart_dev, ifl_tx_buffer, len,
		      IFL_TX_TIMEOUT_MS * USEC_PER_MSEC);
	if (err != 0) {
		ifl_tx_busy = false;
		LOG_ERR("IFL UART TX failed to start (%d)", err);
		return -EASNFW_STATUS__INTERFW_LINK_TX_FAILED;
	}

	LOG_DBG("IFL UART TX started (%u bytes)", (unsigned int)len);
	return 0;
}

int uart__ifl_rx_handler_set(uart__ifl_rx_handler_t handler)
{
	int err;

	if (!uart_ifl_init || (handler == NULL)) {
		LOG_ERR("Cannot enable IFL UART RX: invalid state or handler");
		return -EASNFW_STATUS__INTERFW_LINK_RX_FAILED;
	}

	ifl_rx_handler = handler;
	if (ifl_rx_enabled) {
		LOG_DBG("IFL UART RX already enabled");
		return 0;
	}

	ifl_current_rx_buffer = ifl_rx_buffer_a;
	err = uart_rx_enable(ifl_uart_dev, ifl_current_rx_buffer,
			     sizeof(ifl_rx_buffer_a),
			     CONFIG_EASNFW_IFL_UART_RX_INTERBYTE_TIMEOUT_MS *
			     1000u);
	if (err != 0) {
		ifl_rx_handler = NULL;
		LOG_ERR("Failed to enable IFL UART RX (%d)", err);
		return -EASNFW_STATUS__INTERFW_LINK_RX_FAILED;
	}

	ifl_rx_enabled = true;
	LOG_INF("IFL UART RX enabled (%u-byte buffers)",
		(unsigned int)sizeof(ifl_rx_buffer_a));
	return 0;
}