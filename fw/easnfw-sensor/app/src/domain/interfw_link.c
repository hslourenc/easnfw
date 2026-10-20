#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/interfw_link.h>
#include <easnfw/reset.h>
#include <easnfw/status.h>
#include <easnfw/uart.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(interfw_link, LOG_LEVEL_INF);

/** Indicate whether Inter-FW Link is initialized */
static bool ifl_init = false;
/** Store Inter-FW Link state */
static struct ifl_runtime_state ifl_state;

/** easnfwtodo: this could be on interfw_link_protocol.h */
struct ifl_received_frame {
	uint16_t len;
	uint8_t data[INTERFW_LINK_UART_MAX_FRAME_SIZE];
};

/** 
 * Define a message queue to communicate with the module/thread that will
 * handle the received commands.
 */
K_MSGQ_DEFINE(ifl_rx_msgq, sizeof(struct ifl_received_frame),
	      EASNFW_SENSOR_IFL_RX_MSGQ_SIZE,
	      __alignof__(struct ifl_received_frame));

/** Hold the IFL frame currently being received */
static uint8_t ifl_rx_frame[INTERFW_LINK_UART_MAX_FRAME_SIZE];

/** Tracks how many bytes of the current IFL frame have already been received. */
static size_t ifl_rx_frame_len;

/** Expected number of bytes to be received for the current IFL frame. */
static size_t ifl_rx_frame_expected_len;

/**
 * Semaphore to synchronize command transmission and ACK reception. Semaphore
 * is taken after transmitting a command and given back when a corresponding
 * ACK is received. If semaphore is taken successfully after command
 * transmission, it means that an ACK was received (including non-OK ACK
 * statuses); if it times out, it means that ACK was not received.
 */
static struct k_sem ifl_ack_sem;

/** Indicate whether an ACK is being waited for. */
static bool ifl_ack_waiting;


static uint8_t ifl_ack_cmd;
static uint8_t ifl_ack_seq;
static ifl_status_t ifl_ack_status;

/** Calculate the CRC-16/CCITT-FALSE used for IFL frames. */
static uint16_t ifl__crc16(uint8_t const *data, size_t const len)
{
	uint16_t crc = INTERFW_LINK_UART_CRC_INIT;

	/* The protocol uses polynomial 0x1021, no reflection, and no XOR-out. */
	for (size_t i = 0u; i < len; ++i) {
		crc ^= (uint16_t)data[i] << 8;
		for (uint8_t bit = 0u; bit < 8u; ++bit) {
			crc = (crc & 0x8000u) != 0u
				? (uint16_t)((crc << 1) ^ 0x1021u)
				: (uint16_t)(crc << 1);
		}
	}

	return crc;
}

/** Map a peer ACK status into the local application status namespace. */
static int ifl__ack_status_to_easn_status(ifl_status_t const status)
{
	switch (status) {
	case IFL_STAT__OK:
		return 0;
	case IFL_STAT__UNKNOWN_CMD:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_UNKNOWN_CMD;
	case IFL_STAT__BAD_LEN:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_BAD_LEN;
	case IFL_STAT__BAD_PARAM:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_BAD_PARAM;
	case IFL_STAT__BUSY:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_BUSY;
	case IFL_STAT__NOT_SUPPORTED:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_NOT_SUPPORTED;
	case IFL_STAT__BAD_OFFSET:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_BAD_OFFSET;
	case IFL_STAT__NO_MEMORY:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_NO_MEMORY;
	case IFL_STAT__TRANSFER_ERR:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_TRANSFER_ERR;
	case IFL_STAT__INTERNAL_ERR:
	default:
		return -EASNFW_STATUS__INTERFW_LINK_ACK_INTERNAL_ERR;
	}
}

static void ifl__rx_frame_reset(void)
{
	ifl_rx_frame_len = 0u;
	ifl_rx_frame_expected_len = 0u;
}

/** Validate a complete frame and route ACKs or commands to their consumers. */
static void ifl__rx_frame_valid(void)
{
	easn_frame_hdr_t header;
	uint16_t received_crc;
	uint16_t calculated_crc;

	memcpy(&header, ifl_rx_frame, sizeof(header));
	received_crc = (uint16_t)ifl_rx_frame[ifl_rx_frame_expected_len - 2u] |
		       ((uint16_t)ifl_rx_frame[ifl_rx_frame_expected_len - 1u] << 8);
	calculated_crc = ifl__crc16(&ifl_rx_frame[1],
				    ifl_rx_frame_expected_len -
				    INTERFW_LINK_UART_CRC_SIZE - 1u);
	if (received_crc != calculated_crc) {
		LOG_WRN("Discarding IFL frame with invalid CRC");
		return;
	}

	if ((header.type == EASN_FRAME_ACK) &&
	    (header.cmd_id == ifl_ack_cmd) &&
	    (header.seq == ifl_ack_seq) &&
	    (header.len == sizeof(easn_ack_payload_t)) &&
	    ifl_ack_waiting) {
		ifl_ack_status = (ifl_status_t)ifl_rx_frame[INTERFW_LINK_UART_HDR_SIZE];
		LOG_DBG("Received ACK cmd=0x%02x seq=%u status=0x%02x",
			header.cmd_id, header.seq, ifl_ack_status);
		k_sem_give(&ifl_ack_sem);
		return;
	}

	if (header.type == EASN_FRAME_CMD) {
		struct ifl_received_frame received = {
			.len = ifl_rx_frame_expected_len,
		};

		memcpy(received.data, ifl_rx_frame, received.len);
		if (k_msgq_put(&ifl_rx_msgq, &received, K_NO_WAIT) != 0) {
			LOG_WRN("Dropping queued IFL command cmd=0x%02x: queue full",
				header.cmd_id);
		} else {
			LOG_DBG("Queued inbound IFL command cmd=0x%02x seq=%u",
				header.cmd_id, header.seq);
		}
	} else {
		LOG_DBG("Ignoring IFL frame type=0x%02x cmd=0x%02x seq=%u",
			header.type, header.cmd_id, header.seq);
	}
}

/** Incrementally assemble frames from arbitrary asynchronous RX chunks. */
static void ifl__rx_data_handler(uint8_t const *data, size_t const len)
{
	for (size_t i = 0u; i < len; ++i) {
		uint8_t const byte = data[i];

		if ((ifl_rx_frame_len == 0u) && (byte != INTERFW_LINK_UART_SOF)) {
			continue;
		}

		if (ifl_rx_frame_len >= sizeof(ifl_rx_frame)) {
			ifl__rx_frame_reset();
			continue;
		}

		ifl_rx_frame[ifl_rx_frame_len++] = byte;

		if (ifl_rx_frame_len == INTERFW_LINK_UART_HDR_SIZE) {
			uint16_t const payload_len =
				(uint16_t)ifl_rx_frame[5] |
				((uint16_t)ifl_rx_frame[6] << 8);

			if (payload_len > INTERFW_LINK_UART_MAX_PAYLOAD) {
				LOG_WRN("Discarding oversized IFL payload (%u bytes)",
					payload_len);
				ifl__rx_frame_reset();
				continue;
			}

			ifl_rx_frame_expected_len = INTERFW_LINK_UART_HDR_SIZE +
						    payload_len +
						    INTERFW_LINK_UART_CRC_SIZE;
		}

		if ((ifl_rx_frame_expected_len != 0u) &&
		    (ifl_rx_frame_len == ifl_rx_frame_expected_len)) {
			ifl__rx_frame_valid();
			ifl__rx_frame_reset();
		}
	}
}

static int ifl__protocol_state_init(void)
{
	/* SENSOR receives COMMON and SENSOR commands and sends CLOUD commands. */
	ifl_state.role = EASN_ROLE_SENSOR;
	ifl_state.next_tx_seq = 0u;
	ifl_state.last_rx_seq = 0u;
	ifl_state.last_rx_seq_valid = false;

	if (!ifl__is_cmd_valid(ifl_state.role, IFL_CMD_COMMON__PING)) {
		return -EASNFW_STATUS__INTERFW_LINK_PROTOCOL_INIT_FAILED;
	}

	if (!ifl__is_cmd_valid(ifl_state.role, IFL_CMD_SENSOR__CLOUD_READY)) {
		return -EASNFW_STATUS__INTERFW_LINK_PROTOCOL_INIT_FAILED;
	}

	if (ifl__is_cmd_valid(ifl_state.role, IFL_CMD_CLOUD__RESET)) {
		return -EASNFW_STATUS__INTERFW_LINK_PROTOCOL_INIT_FAILED;
	}

	return 0;
}

bool ifl__is_init(void)
{
	return ifl_init;
}

int ifl__init(void)
{
	/* Initialization is safe to repeat and does not reset active state. */
	if (ifl_init) {
		LOG_DBG("IFL already initialized");
		return 0;
	}

	/* UART must be ready before the protocol can transmit any frame. */
	int err = uart__ifl_init();

	if (err != 0) {
		LOG_ERR("Failed to initialize IFL UART (%d)", err);
		return err;
	}

	err = ifl__protocol_state_init();
	if (err != 0) {
		LOG_ERR("Failed to initialize IFL protocol state (%d)", err);
		return err;
	}

	/* Initialize binary semaphore with count 0 and limit 1 */
	k_sem_init(&ifl_ack_sem, 0, 1);
	err = uart__ifl_rx_handler_set(ifl__rx_data_handler);
	if (err != 0) {
		LOG_ERR("Failed to initialize IFL RX (%d)", err);
		return err;
	}

	ifl_init = true;
	LOG_INF("IFL initialized as SENSOR");
	return 0;
}

int ifl__cmd_tx(uint8_t const cmd, uint8_t const *payload,
		uint16_t const payload_len)
{
	/*
	 * Keep the complete frame local while validating and constructing it.
	 * uart__ifl_tx() copies it into the asynchronous transport buffer.
	 */
	uint8_t frame[INTERFW_LINK_UART_MAX_FRAME_SIZE];
	easn_frame_hdr_t header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_CMD,
		.cmd_id = cmd,
		.seq = ifl_state.next_tx_seq,
		.flags = 0u,
		.len = payload_len,
	};
	size_t const body_len = (size_t)INTERFW_LINK_UART_HDR_SIZE + payload_len;
	uint16_t crc;
	int err;
	k_timeout_t const ack_timeout =
		K_MSEC(CONFIG_EASNFW_IFL_UART_ACK_TIMEOUT_MS);

	/* A sequence number is meaningful only after protocol initialization. */
	if (!ifl_init) {
		LOG_ERR("Cannot send IFL command 0x%02x: IFL is not initialized",
			cmd);
		return -EASNFW_STATUS__INTERFW_LINK_PROTOCOL_INIT_FAILED;
	}

	/* SENSOR must not transmit commands from its own receive range. */
	if (!ifl__is_cmd_valid(EASN_ROLE_CLOUD, cmd)) {
		LOG_ERR("Rejecting invalid outbound IFL command 0x%02x", cmd);
		return -EASNFW_STATUS__INTERFW_LINK_INVALID_COMMAND;
	}

	if ((payload_len > INTERFW_LINK_UART_MAX_PAYLOAD) ||
	    ((payload_len > 0u) && (payload == NULL))) {
		LOG_ERR("Invalid payload for IFL command 0x%02x (len=%u)",
			cmd, payload_len);
		return -EASNFW_STATUS__INTERFW_LINK_INVALID_PAYLOAD;
	}

	/* PING is defined as a command without a payload. */
	if (((cmd == IFL_CMD_COMMON__PING) ||
	     (cmd == IFL_CMD_CLOUD__RESET)) && (payload_len != 0u)) {
		LOG_ERR("IFL command 0x%02x does not accept a payload", cmd);
		return -EASNFW_STATUS__INTERFW_LINK_INVALID_PAYLOAD;
	}

	for (uint32_t attempt = 0u;
	     attempt <= CONFIG_EASNFW_IFL_UART_MAX_RETRIES;
	     ++attempt) {
		header.flags = (attempt == 0u) ? 0u : EASN_FLAG_RETRANSMIT;
		LOG_DBG("Sending IFL command 0x%02x seq=%u attempt=%u",
			cmd, header.seq, attempt + 1u);
		ifl_ack_cmd = cmd;
		ifl_ack_seq = header.seq;
		ifl_ack_waiting = true;
		ifl_ack_status = IFL_STAT__INTERNAL_ERR;

		/* Copy the packed header and optional payload into the wire buffer. */
		memcpy(frame, &header, sizeof(header));
		if (payload_len > 0u) {
			memcpy(&frame[INTERFW_LINK_UART_HDR_SIZE], payload, payload_len);
		}

		/* CRC covers TYPE through PAYLOAD, excluding SOF and the CRC itself. */
		crc = ifl__crc16(&frame[1], body_len - 1u);
		frame[body_len] = (uint8_t)(crc & 0xFFu);
		frame[body_len + 1u] = (uint8_t)(crc >> 8);

		k_sem_reset(&ifl_ack_sem);
		err = uart__ifl_tx(frame, body_len + INTERFW_LINK_UART_CRC_SIZE);
		if (err != 0) {
			ifl_ack_waiting = false;
			LOG_ERR("Failed to send IFL command 0x%02x (%d)", cmd, err);
			return err;
		}

		/** 
		 * Waits for ACK, ifl_ack_sem is given when ACK is received. If
		 * ACK is not received after ack_timeout miliseconds,
		 * k_sem_take() times out and returns non-zero, and
		 * command transmission is retried up to
		 * CONFIG_EASNFW_IFL_UART_MAX_RETRIES times.
		 */
		if (k_sem_take(&ifl_ack_sem, ack_timeout) == 0) {
			if (ifl_ack_status == IFL_STAT__BUSY ||
			    ifl_ack_status == IFL_STAT__TRANSFER_ERR) {
				LOG_WRN("Retryable ACK for IFL command 0x%02x: status=0x%02x",
					cmd, ifl_ack_status);
				if (attempt ==
				    CONFIG_EASNFW_IFL_UART_MAX_RETRIES) {
					ifl_ack_waiting = false;
					++ifl_state.next_tx_seq;
					return ifl__ack_status_to_easn_status(
						ifl_ack_status);
				}
				continue;
			}

			ifl_ack_waiting = false;
			++ifl_state.next_tx_seq;
			if (ifl_ack_status != IFL_STAT__OK) {
				LOG_ERR("IFL command 0x%02x completed with status=0x%02x",
				        cmd, ifl_ack_status);
			} else {
				LOG_INF("IFL command 0x%02x completed with status=0x%02x",
				        cmd, ifl_ack_status);
			}

			/* Let upstream logic handle error if any, may depend
			   on the specific command or other context */
			return ifl__ack_status_to_easn_status(ifl_ack_status);
		} else {
			LOG_ERR("ACK for IFL command 0x%02x seq=%u attempt=%u timed out",
			        cmd, ifl_ack_seq, attempt);
		}
	}

	ifl_ack_waiting = false;
	ifl_state.next_tx_seq++;
	LOG_WRN("IFL command 0x%02x timed out after %u attempts", cmd,
		CONFIG_EASNFW_IFL_UART_MAX_RETRIES + 1u);
	return -EASNFW_STATUS__INTERFW_LINK_ACK_TIMEOUT;
}

int ifl__reset_easnfwcloud_tx(void)
{
	/**
	 * Clear the previous ready indication before requesting another CLOUD
	 * reset. A new CLOUD_READY command must be received after this request.
	 */
	reset__easnfwcloud_ready_clear();

	/**
	 * Let upstream logic to decide how to handle error if any.
	 * ifl__cmd_tx() already handles retriable ACK status and ACK timeout.
	 */
	return ifl__cmd_tx(IFL_CMD_CLOUD__RESET, NULL, 0u);
}

int ifl__easnfwcloud_ready_rx(void)
{
	/**
	 * EASNFW_TODO: implement according to function header in
	 * interfw_link.h. Clear the easnfwcloud_ready flag with
	 * reset__easnfwcloud_ready_clear().
	 *
	 * See also:
	 * - interfw_link_protocol.h
	 */
	return -EASNFW_STATUS__INTERFW_LINK_NOT_IMPLEMENTED;
}
