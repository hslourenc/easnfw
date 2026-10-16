#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <easnfw/status.h>
#include <easnfw/uart.h>

LOG_MODULE_REGISTER(easnfw_ifl_uart, LOG_LEVEL_INF);

static const struct device *ifl_uart_dev =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_interfw_link_uart));

/** Indicate whether Inter-FW Link UART is initialized */
static bool uart_ifl_init = false;

bool uart__is_ifl_init(void)
{
	return uart_ifl_init;
}

int uart__ifl_init(void)
{
	const struct uart_config config = {
		.baudrate = DT_PROP(DT_CHOSEN(easnfw_interfw_link_uart),
				    current_speed),
		.parity = UART_CFG_PARITY_NONE,
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

	err = uart_configure(ifl_uart_dev, &config);
	if (err != 0) {
		LOG_ERR("Failed to configure IFL UART (%d)", err);
		uart_ifl_init = false;
		return -EASNFW_STATUS__INTERFW_LINK_UART_CONFIG_FAILED;
	}

	uart_ifl_init = true;
	return 0;
}