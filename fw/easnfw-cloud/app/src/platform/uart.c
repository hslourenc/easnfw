#include <string.h>

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/uart.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(easnfw_cloud_uart, LOG_LEVEL_INF);

/**
 * UART idle time after which received bytes are reported. It adds to the
 * ACK latency of every frame, so it must be much shorter than the ACK timeout.
 */
#define IFL_RX_IDLE_TIMEOUT_US 2000u

static const struct device *const uart_dev =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_interfw_link_uart));
static uint8_t tx_buffer[INTERFW_LINK_UART_MAX_FRAME_SIZE];
static uint8_t rx_buffer_a[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t rx_buffer_b[CONFIG_EASNFW_IFL_UART_RX_RING_BUF_SIZE];
static uint8_t *current_rx_buffer;
static easnfw_cloud_uart_rx_handler_t rx_handler;
static bool initialized;
static struct k_mutex tx_mutex;
static struct k_sem tx_complete;
static int tx_status;

static void uart_callback(const struct device *dev, struct uart_event *event,
			  void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type == UART_TX_DONE) {
		tx_status = 0;
		k_sem_give(&tx_complete);
	} else if (event->type == UART_TX_ABORTED) {
		tx_status = -EIO;
		k_sem_give(&tx_complete);
	}
	if (event->type == UART_RX_RDY && rx_handler != NULL) {
		rx_handler(&event->data.rx.buf[event->data.rx.offset],
			   event->data.rx.len);
	} else if (event->type == UART_RX_DISABLED && rx_handler != NULL) {
		/* A line error (e.g. peer reset glitch) disables RX; restart it. */
		current_rx_buffer = rx_buffer_a;
		(void)uart_rx_enable(dev, current_rx_buffer, sizeof(rx_buffer_a),
				     IFL_RX_IDLE_TIMEOUT_US);
	} else if (event->type == UART_RX_BUF_REQUEST) {
		uint8_t *next = current_rx_buffer == rx_buffer_a ?
			rx_buffer_b : rx_buffer_a;
		if (uart_rx_buf_rsp(dev, next, sizeof(rx_buffer_a)) == 0) {
			current_rx_buffer = next;
		}
	}
}

/**
 * @brief Configure the cloud-side asynchronous UART2 transport.
 *
 * @return 0 on success or a negative errno value.
 */
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
	k_mutex_init(&tx_mutex);
	k_sem_init(&tx_complete, 0, 1);
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

/**
 * @brief Serialize and transmit one UART buffer, waiting for completion.
 *
 * @param data Frame bytes to send.
 * @param len Number of bytes in @p data.
 * @return 0 on success or a negative errno value.
 */
int easnfw_cloud_uart_tx(uint8_t const *data, size_t len)
{
	int err;

	if (!initialized || data == NULL || len == 0u ||
	    len > sizeof(tx_buffer)) {
		return -EINVAL;
	}
	err = k_mutex_lock(&tx_mutex, K_SECONDS(2));
	if (err != 0) {
		return err;
	}
	memcpy(tx_buffer, data, len);
	tx_status = -EIO;
	k_sem_reset(&tx_complete);
	err = uart_tx(uart_dev, tx_buffer, len, SYS_FOREVER_US);
	if (err == 0) {
		err = k_sem_take(&tx_complete, K_SECONDS(2));
		if (err == 0) {
			err = tx_status;
		} else {
			err = -ETIMEDOUT;
		}
	} else {
		err = -EIO;
	}
	k_mutex_unlock(&tx_mutex);
	return err;
}

/**
 * @brief Enable asynchronous receive and install the cloud frame handler.
 *
 * @param handler Receives arbitrary byte chunks from the UART driver.
 * @return 0 on success or a negative errno value.
 */
int easnfw_cloud_uart_rx_set(easnfw_cloud_uart_rx_handler_t handler)
{
	int err;
	if (!initialized || handler == NULL) {
		return -EINVAL;
	}
	rx_handler = handler;
	current_rx_buffer = rx_buffer_a;
	err = uart_rx_enable(uart_dev, current_rx_buffer, sizeof(rx_buffer_a),
			     IFL_RX_IDLE_TIMEOUT_US);
	return err;
}
