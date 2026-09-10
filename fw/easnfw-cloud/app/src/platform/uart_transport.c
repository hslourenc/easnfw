#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <easnfw/shared.h>

#if DT_HAS_CHOSEN(easnfw_interconnect)
static const struct device *const uart_device =
	DEVICE_DT_GET(DT_CHOSEN(easnfw_interconnect));
#endif

int uart_transport_init(void)
{
#if DT_HAS_CHOSEN(easnfw_interconnect)
	return device_is_ready(uart_device) ? 0 : -ENODEV;
#else
	return -ENODEV;
#endif
}

int uart_transport_receive(uint8_t *buffer, size_t capacity, size_t *received)
{
#if DT_HAS_CHOSEN(easnfw_interconnect)
	if (buffer == NULL || received == NULL) {
		return -EINVAL;
	}
	*received = 0U;
	while (*received < capacity) {
		unsigned char byte;
		if (uart_poll_in(uart_device, &byte) != 0) {
			break;
		}
		buffer[(*received)++] = byte;
	}
	return 0;
#else
	ARG_UNUSED(buffer);
	ARG_UNUSED(capacity);
	ARG_UNUSED(received);
	return -ENODEV;
#endif
}
