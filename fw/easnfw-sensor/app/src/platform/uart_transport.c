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

int uart_transport_send(const struct easnfw_frame_header *header,
	const void *payload)
{
#if DT_HAS_CHOSEN(easnfw_interconnect)
	const uint8_t *header_bytes = (const uint8_t *)header;
	const uint8_t *payload_bytes = payload;

	for (size_t i = 0U; i < sizeof(*header); ++i) {
		uart_poll_out(uart_device, header_bytes[i]);
	}
	for (size_t i = 0U; i < header->payload_length; ++i) {
		uart_poll_out(uart_device, payload_bytes[i]);
	}
	return 0;
#else
	ARG_UNUSED(header);
	ARG_UNUSED(payload);
	return -ENODEV;
#endif
}
