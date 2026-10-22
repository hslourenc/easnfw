#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "../../../../shared/interfw_link_protocol.h"
#include <easnfw/uart.h>

LOG_MODULE_REGISTER(easnfw_cloud_ifl, LOG_LEVEL_INF);

static uint8_t rx_frame[INTERFW_LINK_UART_MAX_FRAME_SIZE];
static size_t rx_len;
static size_t rx_expected_len;

static uint16_t crc16(uint8_t const *data, size_t len)
{
	uint16_t crc = INTERFW_LINK_UART_CRC_INIT;
	for (size_t i = 0u; i < len; ++i) {
		crc ^= (uint16_t)data[i] << 8;
		for (uint8_t bit = 0u; bit < 8u; ++bit) {
			crc = (crc & 0x8000u) ? (crc << 1) ^ 0x1021u :
				(uint16_t)(crc << 1);
		}
	}
	return crc;
}

static void send_ack(easn_frame_hdr_t const *command, ifl_status_t status)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE + sizeof(easn_ack_payload_t) +
		      INTERFW_LINK_UART_CRC_SIZE];
	easn_frame_hdr_t const header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_ACK,
		.cmd_id = command->cmd_id,
		.seq = command->seq,
		.flags = 0u,
		.len = sizeof(easn_ack_payload_t),
	};
	easn_ack_payload_t const payload = {
		.status = status,
		.reserved = 0u,
		.next_offset = 0u,
	};
	uint16_t value;

	memcpy(frame, &header, sizeof(header));
	memcpy(&frame[INTERFW_LINK_UART_HDR_SIZE], &payload, sizeof(payload));
	value = crc16(&frame[1], INTERFW_LINK_UART_HDR_SIZE +
		      sizeof(payload) - 1u);
	frame[sizeof(frame) - 2u] = value & 0xffu;
	frame[sizeof(frame) - 1u] = value >> 8;
	(void)easnfw_cloud_uart_tx(frame, sizeof(frame));
}

static void handle_frame(void)
{
	easn_frame_hdr_t header;
	uint16_t received_crc;

	memcpy(&header, rx_frame, sizeof(header));
	received_crc = rx_frame[rx_expected_len - 2u] |
		((uint16_t)rx_frame[rx_expected_len - 1u] << 8);
	if (received_crc != crc16(&rx_frame[1], rx_expected_len - 3u) ||
	    header.type != EASN_FRAME_CMD) {
		return;
	}

	if (header.cmd_id != IFL_CMD_CLOUD__RESET ||
	    header.len != 0u) {
		send_ack(&header, header.cmd_id == IFL_CMD_CLOUD__RESET ?
			 IFL_STAT__BAD_LEN : IFL_STAT__UNKNOWN_CMD);
		return;
	}

	send_ack(&header, IFL_STAT__OK);
	k_msleep(20);
	sys_reboot(SYS_REBOOT_COLD);
}

static void rx_handler(uint8_t const *data, size_t len)
{
	for (size_t i = 0u; i < len; ++i) {
		uint8_t byte = data[i];
		if (rx_len == 0u && byte != INTERFW_LINK_UART_SOF) {
			continue;
		}
		if (rx_len >= sizeof(rx_frame)) {
			rx_len = 0u;
			rx_expected_len = 0u;
			continue;
		}
		rx_frame[rx_len++] = byte;
		if (rx_len == INTERFW_LINK_UART_HDR_SIZE) {
			uint16_t payload_len = rx_frame[5] | ((uint16_t)rx_frame[6] << 8);
			if (payload_len > INTERFW_LINK_UART_MAX_PAYLOAD) {
				rx_len = 0u;
				continue;
			}
			rx_expected_len = INTERFW_LINK_UART_HDR_SIZE + payload_len +
					  INTERFW_LINK_UART_CRC_SIZE;
		}
		if (rx_expected_len != 0u && rx_len == rx_expected_len) {
			handle_frame();
			rx_len = 0u;
			rx_expected_len = 0u;
		}
	}
}

int easnfw_cloud_ifl_init(void)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE + 3u +
		      INTERFW_LINK_UART_CRC_SIZE];
	easn_frame_hdr_t const header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_CMD,
		.cmd_id = IFL_CMD_SENSOR__CLOUD_READY,
		.seq = 0u,
		.flags = 0u,
		.len = 3u,
	};
	uint8_t const payload[] = {1u, 0u, 0u};
	uint16_t value;
	int err = easnfw_cloud_uart_init();

	if (err != 0) {
		return err;
	}
	err = easnfw_cloud_uart_rx_set(rx_handler);
	if (err != 0) {
		return err;
	}
	memcpy(frame, &header, sizeof(header));
	memcpy(&frame[INTERFW_LINK_UART_HDR_SIZE], payload, sizeof(payload));
	value = crc16(&frame[1], INTERFW_LINK_UART_HDR_SIZE +
		      sizeof(payload) - 1u);
	frame[sizeof(frame) - 2u] = value & 0xffu;
	frame[sizeof(frame) - 1u] = value >> 8;
	return easnfw_cloud_uart_tx(frame, sizeof(frame));
}
