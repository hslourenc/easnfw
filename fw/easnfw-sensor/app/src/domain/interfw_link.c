#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <easnfw/interfw_link.h>
#include <easnfw/reset.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>
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
static bool ifl_verification_ping_override;

static uint8_t ifl_ack_cmd;
static uint8_t ifl_ack_seq;
static ifl_status_t ifl_ack_status;
static uint32_t ifl_ack_next_offset;
static uint8_t ifl_last_rx_cmd;
static uint8_t ifl_last_rx_seq;
static ifl_status_t ifl_last_rx_status;

#define IFL_RX_THREAD_STACK_SIZE 1024
#define IFL_RX_THREAD_PRIORITY   5

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
		ifl_ack_next_offset = sys_get_le32(
			&ifl_rx_frame[INTERFW_LINK_UART_HDR_SIZE + 2u]);
		LOG_DBG("Received ACK cmd=0x%02x seq=%u status=0x%02x",
			header.cmd_id, header.seq, ifl_ack_status);
		k_sem_give(&ifl_ack_sem);
		return;
	}

	if (header.type == EASN_FRAME_CMD) {
		/* Static: too large for the callback or RX worker stacks. */
		static struct ifl_received_frame received;

		received.len = ifl_rx_frame_expected_len;
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

/** Transmit an ACK for a completely received command frame. */
static void ifl__ack_tx(easn_frame_hdr_t const *const command_header,
			ifl_status_t const status)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE +
		      sizeof(easn_ack_payload_t) +
		      INTERFW_LINK_UART_CRC_SIZE];
	easn_frame_hdr_t const header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_ACK,
		.cmd_id = command_header->cmd_id,
		.seq = command_header->seq,
		.flags = 0u,
		.len = sizeof(easn_ack_payload_t),
	};
	easn_ack_payload_t const payload = {
		.status = status,
		.reserved = 0u,
		.next_offset = 0u,
	};
	uint16_t crc;
	int err;

	memcpy(frame, &header, sizeof(header));
	memcpy(&frame[INTERFW_LINK_UART_HDR_SIZE], &payload, sizeof(payload));
	crc = ifl__crc16(&frame[1],
			 INTERFW_LINK_UART_HDR_SIZE + sizeof(payload) - 1u);
	frame[sizeof(frame) - 2u] = (uint8_t)(crc & 0xFFu);
	frame[sizeof(frame) - 1u] = (uint8_t)(crc >> 8);

	err = uart__ifl_tx(frame, sizeof(frame));
	if (err != 0) {
		LOG_ERR("Failed to send ACK cmd=0x%02x seq=%u status=0x%02x (%d)",
			command_header->cmd_id, command_header->seq, status, err);
		return;
	}

	LOG_DBG("Sent ACK cmd=0x%02x seq=%u status=0x%02x",
		command_header->cmd_id, command_header->seq, status);
}

/** Find a command in the SENSOR receive table and validate its payload. */
static ifl_status_t ifl__received_command_status(
	easn_frame_hdr_t const *const header, uint8_t const *const payload)
{
	if (!ifl__is_cmd_valid(ifl_state.role, header->cmd_id)) {
		return IFL_STAT__UNKNOWN_CMD;
	}

	for (size_t i = 0u; i < EASNFW_SENSOR_IFL_COMMAND_COUNT; ++i) {
		easn_ifl_command_entry_t const *const entry =
			&easnfw_sensor_ifl_command_table[i];

		if (entry->cmd_id != header->cmd_id) {
			continue;
		}

		if (((entry->flags & EASN_CMDF_NO_PAYLOAD) != 0u &&
		     header->len != 0u) ||
		    ((entry->flags & EASN_CMDF_NO_PAYLOAD) == 0u &&
		     header->len != entry->payload_len)) {
			return IFL_STAT__BAD_LEN;
		}

		if (header->cmd_id == IFL_CMD_SENSOR__CLOUD_READY) {
			if (payload == NULL) {
				return IFL_STAT__BAD_PARAM;
			}
			/* The ready flag is recorded after the ACK is sent. */
			return IFL_STAT__OK;
		}
		if (header->cmd_id == IFL_CMD_SENSOR__UTC_TIME) {
			uint64_t utc_ms;
			uint32_t cloud_reset_cause;

			if (payload == NULL) {
				return IFL_STAT__BAD_PARAM;
			}
			utc_ms = sys_get_le64(payload);
			cloud_reset_cause = sys_get_le32(
				payload + sizeof(utc_ms));
			if (app_utc_time__set(utc_ms) != 0) {
				LOG_ERR("Rejected invalid UTC time from CLOUD");
				return IFL_STAT__BAD_PARAM;
			}
			if (reset__cloud_reason_set(cloud_reset_cause) != 0) {
				LOG_WRN("Could not persist CLOUD reset cause");
			}
			LOG_INF("UTC time synchronized from cellular network");
			return IFL_STAT__OK;
		}

		return IFL_STAT__OK;
	}

	return IFL_STAT__UNKNOWN_CMD;
}

/** Consume complete commands, execute them, and return their ACK status. */
static void ifl__rx_worker(void *arg1, void *arg2, void *arg3)
{
	/* Static: a full frame does not fit on the worker stack. */
	static struct ifl_received_frame received;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	for (;;) {
		easn_frame_hdr_t header;
		ifl_status_t status;
		uint8_t const *payload;

		k_msgq_get(&ifl_rx_msgq, &received, K_FOREVER);
		memcpy(&header, received.data, sizeof(header));
		payload = &received.data[INTERFW_LINK_UART_HDR_SIZE];

		/*
		 * Only retransmissions are duplicates: a rebooted peer restarts
		 * its sequence numbers and must not be mistaken for a retry.
		 */
		if (ifl_state.last_rx_seq_valid &&
		    ((header.flags & EASN_FLAG_RETRANSMIT) != 0u) &&
		    (header.cmd_id == ifl_last_rx_cmd) &&
		    (header.seq == ifl_last_rx_seq)) {
			/* The original ACK may have been lost; do not execute twice. */
			ifl__ack_tx(&header, ifl_last_rx_status);
			continue;
		}

		status = ifl__received_command_status(&header, payload);
		ifl_last_rx_cmd = header.cmd_id;
		ifl_last_rx_seq = header.seq;
		ifl_last_rx_status = status;
		ifl_state.last_rx_seq = header.seq;
		ifl_state.last_rx_seq_valid = true;
		ifl__ack_tx(&header, status);

		/*
		 * Signal readiness only after the ACK is on the wire: the reset
		 * sequence reboots SENSOR as soon as it is signalled, and an
		 * unsent ACK would make CLOUD's handshake fail.
		 */
		if ((header.cmd_id == IFL_CMD_SENSOR__CLOUD_READY) &&
		    (status == IFL_STAT__OK)) {
			(void)ifl__easnfwcloud_ready_rx();
		}
	}
}

K_THREAD_DEFINE(ifl_rx_thread_id, IFL_RX_THREAD_STACK_SIZE, ifl__rx_worker,
		NULL, NULL, NULL, IFL_RX_THREAD_PRIORITY, 0, 0);

/** Incrementally assemble frames from arbitrary asynchronous RX chunks. */
static void ifl__rx_data_handler(uint8_t const *data, size_t const len)
{
	static int64_t last_byte_ms;
	int64_t const now_ms = k_uptime_get();

	/* Drop a stale partial frame so a glitch cannot swallow later frames. */
	if ((ifl_rx_frame_len != 0u) &&
	    ((now_ms - last_byte_ms) > INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS)) {
		ifl__rx_frame_reset();
	}
	last_byte_ms = now_ms;

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
	return ifl__cmd_tx_flags(cmd, 0u, payload, payload_len, NULL);
}

int ifl__cmd_tx_flags(uint8_t const cmd, uint8_t const flags,
		      uint8_t const *payload, uint16_t const payload_len,
		      uint32_t *const next_offset)
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
		.flags = flags,
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
		header.flags = (attempt == 0u) ? flags : (flags | EASN_FLAG_RETRANSMIT);
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
			if (next_offset != NULL) {
				*next_offset = ifl_ack_next_offset;
			}
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

/** Set the HIL-only response override used to test the IFL success path. */
void ifl__verification_ping_override_set(bool enabled)
{
	ifl_verification_ping_override = enabled;
}

/** Verify the peer is responsive with a stop-and-wait PING. */
int ifl__ping_tx(void)
{
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (ifl_verification_ping_override) {
		return 0;
	}
#endif
	return ifl__cmd_tx(IFL_CMD_COMMON__PING, NULL, 0u);
}

/** Request a time synchronization response from EASNFW-CLOUD. */
int ifl__utc_time_request_tx(void)
{
	return ifl__cmd_tx(IFL_CMD_CLOUD__TIME_SYNC_REQUEST, NULL, 0u);
}

int ifl__easnfwcloud_ready_rx(void)
{
	/* The ACK confirms receipt; the ready flag records the peer's state. */
	return reset__easnfwcloud_ready_set();
}
