#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/byteorder.h>

#include "../../../../shared/interfw_link_protocol.h"
#include <easnfw/pipeline.h>
#include <easnfw/lte_m.h>
#include <easnfw/sensor_link.h>
#include <easnfw/uart.h>

LOG_MODULE_REGISTER(easnfw_sensor_link, LOG_LEVEL_INF);

/**
 * EASNFW_REVISIT: define record_id and end-to-end integrity metadata with
 * SENSOR before this transfer is used for production records.
 */
#define IFL_RX_QUEUE_DEPTH 2u

struct ifl_frame {
	uint8_t bytes[INTERFW_LINK_UART_MAX_FRAME_SIZE];
	size_t length;
};

struct ifl_ack_result {
	uint8_t command;
	uint8_t sequence;
	uint8_t status;
};

static uint8_t rx_frame[INTERFW_LINK_UART_MAX_FRAME_SIZE];
static size_t rx_len;
static size_t rx_expected_len;
static int64_t rx_last_byte_ms;
static struct ifl_frame rx_queue_frame;
static struct ifl_frame worker_frame;
static bool last_rx_valid;
static uint8_t last_rx_command;
static uint8_t last_rx_sequence;
static uint8_t last_rx_status;
static uint32_t last_rx_offset;
static bool transfer_active;
/** Identity of the most recently accepted record, for idempotent retries. */
static bool last_record_valid;
static uint8_t last_record_id[EASN_RECORD_ID_SIZE];
static uint32_t last_transfer_id;
static uint32_t last_transfer_total;
static uint32_t transfer_id;
static uint32_t transfer_total;
static uint32_t transfer_offset;
static uint8_t *transfer_buffer;
static uint32_t cloud_reset_cause;
static uint8_t cloud_tx_sequence = 1u;

K_MSGQ_DEFINE(ifl_rx_frame_q, sizeof(struct ifl_frame), IFL_RX_QUEUE_DEPTH, 4);
K_MSGQ_DEFINE(ifl_ack_q, sizeof(struct ifl_ack_result), 4, 4);
K_MSGQ_DEFINE(time_request_q, sizeof(uint8_t), 1, 1);
K_THREAD_STACK_DEFINE(ifl_worker_stack, 2048);
static struct k_thread ifl_worker_thread_data;
K_THREAD_STACK_DEFINE(result_stack, 1024);
static struct k_thread result_thread_data;
K_THREAD_STACK_DEFINE(time_worker_stack, 1536);
static struct k_thread time_worker_thread_data;

/** @brief Calculate CRC-16/CCITT-FALSE over TYPE through PAYLOAD. */
static uint16_t crc16(uint8_t const *data, size_t len)
{
	uint16_t crc = INTERFW_LINK_UART_CRC_INIT;

	for (size_t i = 0u; i < len; ++i) {
		crc ^= (uint16_t)data[i] << 8;
		for (uint8_t bit = 0u; bit < 8u; ++bit) {
			crc = (crc & 0x8000u) != 0u ?
				(uint16_t)((crc << 1) ^ 0x1021u) :
				(uint16_t)(crc << 1);
		}
	}
	return crc;
}

/** @brief Calculate a diagnostic CRC-32 for a complete record transfer. */
static uint32_t record_crc32(uint8_t const *data, size_t len)
{
	uint32_t crc = 0xffffffffu;

	for (size_t i = 0u; i < len; ++i) {
		crc ^= data[i];
		for (uint8_t bit = 0u; bit < 8u; ++bit) {
			crc = (crc & 1u) != 0u ?
				(crc >> 1) ^ 0xedb88320u : crc >> 1;
		}
	}
	return ~crc;
}

/** @brief Encode and send a command ACK. */
static int send_ack(easn_frame_hdr_t const *command, ifl_status_t status,
		    uint32_t next_offset)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE + sizeof(easn_ack_payload_t) +
		      INTERFW_LINK_UART_CRC_SIZE];
	easn_frame_hdr_t const header = {
		.sof = INTERFW_LINK_UART_SOF, .type = EASN_FRAME_ACK,
		.cmd_id = command->cmd_id, .seq = command->seq,
		.flags = 0u, .len = sizeof(easn_ack_payload_t),
	};
	easn_ack_payload_t const payload = {
		.status = status, .reserved = 0u, .next_offset = next_offset,
	};
	uint16_t crc;

	memcpy(frame, &header, sizeof(header));
	memcpy(frame + INTERFW_LINK_UART_HDR_SIZE, &payload, sizeof(payload));
	crc = crc16(frame + 1, INTERFW_LINK_UART_HDR_SIZE +
		    sizeof(payload) - 1u);
	frame[sizeof(frame) - 2u] = (uint8_t)crc;
	frame[sizeof(frame) - 1u] = (uint8_t)(crc >> 8);
	return easnfw_cloud_uart_tx(frame, sizeof(frame));
}

/** @brief Send a UTC timestamp to SENSOR and wait for its link ACK. */
static int send_utc_time(uint64_t unix_time_ms)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE +
		      sizeof(ifl_cmd_payload_utc_time_t) +
		      INTERFW_LINK_UART_CRC_SIZE];
	uint8_t payload[sizeof(ifl_cmd_payload_utc_time_t)];
	uint8_t const sequence = cloud_tx_sequence++;
	easn_frame_hdr_t header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_CMD,
		.cmd_id = IFL_CMD_SENSOR__UTC_TIME,
		.seq = sequence,
		.flags = 0u,
		.len = sizeof(payload),
	};

	sys_put_le64(unix_time_ms, payload);
	sys_put_le32(cloud_reset_cause, payload + sizeof(uint64_t));
	for (uint32_t attempt = 0U; attempt <= INTERFW_LINK_UART_MAX_RETRIES;
	     ++attempt) {
		uint16_t crc;
		int64_t deadline;

		header.flags = attempt == 0U ? 0U : EASN_FLAG_RETRANSMIT;
		memcpy(frame, &header, sizeof(header));
		memcpy(frame + INTERFW_LINK_UART_HDR_SIZE, payload,
		       sizeof(payload));
		crc = crc16(frame + 1, INTERFW_LINK_UART_HDR_SIZE +
			    sizeof(payload) - 1U);
		frame[sizeof(frame) - 2U] = (uint8_t)crc;
		frame[sizeof(frame) - 1U] = (uint8_t)(crc >> 8);

		k_msgq_purge(&ifl_ack_q);
		if (easnfw_cloud_uart_tx(frame, sizeof(frame)) != 0) {
			continue;
		}
		deadline = k_uptime_get() +
			INTERFW_LINK_UART_ACK_TIMEOUT_MS;
		while (k_uptime_get() < deadline) {
			struct ifl_ack_result ack;
			int64_t remaining = deadline - k_uptime_get();

			if (k_msgq_get(&ifl_ack_q, &ack,
				       K_MSEC(remaining)) != 0) {
				break;
			}
			if (ack.command == header.cmd_id &&
			    ack.sequence == header.seq) {
				return ack.status == IFL_STAT__OK ? 0 : -EPROTO;
			}
		}
	}
	return -ETIMEDOUT;
}

/** @brief Queue a complete frame only when its type and CRC are valid. */
static void queue_valid_frame(void)
{
	easn_frame_hdr_t header;
	uint16_t received_crc;

	memcpy(&header, rx_frame, sizeof(header));
	received_crc = (uint16_t)rx_frame[rx_expected_len - 2u] |
		((uint16_t)rx_frame[rx_expected_len - 1u] << 8);
	if (header.sof != INTERFW_LINK_UART_SOF ||
	    (header.type != EASN_FRAME_CMD && header.type != EASN_FRAME_ACK) ||
	    received_crc != crc16(rx_frame + 1, rx_expected_len - 3u)) {
		return;
	}
	rx_queue_frame.length = rx_expected_len;
	memcpy(rx_queue_frame.bytes, rx_frame, rx_queue_frame.length);
	/* Queue-full drops are recovered by the sender's ACK timeout/retry. */
	(void)k_msgq_put(&ifl_rx_frame_q, &rx_queue_frame, K_NO_WAIT);
}

/** @brief Incrementally assemble UART bytes into bounded protocol frames. */
static void rx_handler(uint8_t const *data, size_t len)
{
	if (rx_len != 0u &&
	    k_uptime_get() - rx_last_byte_ms >
		    INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS) {
		rx_len = 0u;
		rx_expected_len = 0u;
	}
	for (size_t i = 0u; i < len; ++i) {
		uint8_t const byte = data[i];

		rx_last_byte_ms = k_uptime_get();
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
			uint16_t const payload_len =
				(uint16_t)rx_frame[5] |
				((uint16_t)rx_frame[6] << 8);
			if (payload_len > INTERFW_LINK_UART_MAX_PAYLOAD) {
				rx_len = 0u;
				rx_expected_len = 0u;
				continue;
			}
			rx_expected_len = INTERFW_LINK_UART_HDR_SIZE + payload_len +
					  INTERFW_LINK_UART_CRC_SIZE;
		}
		if (rx_expected_len != 0u && rx_len == rx_expected_len) {
			queue_valid_frame();
			rx_len = 0u;
			rx_expected_len = 0u;
		}
	}
}

/** @brief Discard the staged transfer and free its buffer. */
static void transfer_discard(void)
{
	if (transfer_buffer != NULL) {
		easnfw_record_buffer_free(transfer_buffer);
	}
	transfer_buffer = NULL;
	transfer_active = false;
	transfer_offset = 0u;
}

/**
 * @brief Verify and enqueue a complete staged record blob.
 *
 * The blob is the SENSOR record file followed by a little-endian CRC-32 of
 * that file. A repeat of the last accepted record_id is acknowledged without
 * being delivered again.
 */
static ifl_status_t deliver_record(uint32_t id, uint8_t sequence, uint32_t len)
{
	uint32_t file_len;
	uint32_t trailer_crc;

	if (len < EASN_RECORD_ID_OFFSET + EASN_RECORD_ID_SIZE +
			  EASN_RECORD_TRAILER_SIZE) {
		transfer_discard();
		return IFL_STAT__TRANSFER_ERR;
	}
	file_len = len - EASN_RECORD_TRAILER_SIZE;
	trailer_crc = sys_get_le32(transfer_buffer + file_len);
	if (record_crc32(transfer_buffer, file_len) != trailer_crc) {
		LOG_ERR("Rejected record transfer id=%u: CRC-32 mismatch", id);
		transfer_discard();
		return IFL_STAT__TRANSFER_ERR;
	}
	if (last_record_valid &&
	    memcmp(last_record_id, transfer_buffer + EASN_RECORD_ID_OFFSET,
		   EASN_RECORD_ID_SIZE) == 0) {
		LOG_INF("Duplicate record transfer id=%u acknowledged", id);
		last_transfer_id = id;
		last_transfer_total = len;
		transfer_discard();
		return IFL_STAT__OK;
	}

	struct easnfw_pipeline_message message = {
		.sequence = sequence,
		.length = file_len,
		.checksum = trailer_crc,
		.transfer_id = (uint8_t)id,
		.result = EASNFW_PIPELINE_PENDING,
		.data = transfer_buffer,
	};

	if (k_msgq_put(&rx_payload_q, &message, K_NO_WAIT) != 0) {
		return IFL_STAT__NO_MEMORY;
	}
	memcpy(last_record_id, transfer_buffer + EASN_RECORD_ID_OFFSET,
	       EASN_RECORD_ID_SIZE);
	last_record_valid = true;
	last_transfer_id = id;
	last_transfer_total = len;
	transfer_buffer = NULL;
	transfer_active = false;
	LOG_INF("Accepted complete record transfer id=%u bytes=%u crc32=%08x",
		id, file_len, trailer_crc);
	return IFL_STAT__OK;
}

/** @brief Validate fragment sequence and reassemble one bounded transfer. */
static ifl_status_t receive_record(easn_frame_hdr_t const *header,
				   uint8_t const *payload, uint32_t *next_offset)
{
	if ((header->flags & EASN_FLAG_ABORT) != 0u) {
		if ((header->flags & EASN_FLAG_FRAGMENT) == 0u ||
		    header->len != sizeof(easn_frag_hdr_t)) {
			return IFL_STAT__BAD_LEN;
		}
		easn_frag_hdr_t abort_header;
		memcpy(&abort_header, payload, sizeof(abort_header));
		if (transfer_active &&
		    abort_header.transfer_id != transfer_id) {
			return IFL_STAT__BAD_PARAM;
		}
		transfer_discard();
		*next_offset = 0u;
		return IFL_STAT__OK;
	}

	if ((header->flags & EASN_FLAG_FRAGMENT) == 0u) {
		if (header->len == 0u ||
		    header->len > CONFIG_EASNFW_CLOUD_MAX_RECORD_SIZE) {
			return IFL_STAT__BAD_LEN;
		}
		if (transfer_active ||
		    easnfw_record_buffer_alloc(&transfer_buffer) != 0) {
			return IFL_STAT__NO_MEMORY;
		}
		memcpy(transfer_buffer, payload, header->len);
		transfer_active = true;
		transfer_total = header->len;
		transfer_offset = header->len;
		*next_offset = transfer_offset;
		return deliver_record(header->seq, header->seq, transfer_total);
	}

	if (header->len < sizeof(easn_frag_hdr_t)) {
		return IFL_STAT__BAD_LEN;
	}
	easn_frag_hdr_t fragment;
	memcpy(&fragment, payload, sizeof(fragment));
	uint32_t const chunk_len = header->len - sizeof(fragment);
	uint32_t const end_offset = fragment.offset + chunk_len;

	if (chunk_len == 0u || fragment.total_len == 0u ||
	    fragment.total_len > CONFIG_EASNFW_CLOUD_MAX_RECORD_SIZE ||
	    end_offset < fragment.offset || end_offset > fragment.total_len) {
		return IFL_STAT__TRANSFER_ERR;
	}
	if (((header->flags & EASN_FLAG_LAST) != 0u &&
	     end_offset != fragment.total_len) ||
	    ((header->flags & EASN_FLAG_LAST) == 0u &&
	     end_offset == fragment.total_len)) {
		return IFL_STAT__TRANSFER_ERR;
	}
	if (!transfer_active && last_record_valid &&
	    fragment.transfer_id == last_transfer_id &&
	    fragment.total_len == last_transfer_total &&
	    (header->flags & EASN_FLAG_LAST) != 0u) {
		/* The ACK of the final fragment was lost; it was already applied. */
		*next_offset = end_offset;
		return IFL_STAT__OK;
	}
	if (transfer_active && fragment.offset == 0u &&
	    fragment.transfer_id != transfer_id) {
		/* SENSOR started a new transfer, e.g. after a reboot. */
		transfer_discard();
	}
	if (!transfer_active) {
		if (fragment.offset != 0u) {
			*next_offset = 0u;
			return IFL_STAT__BAD_OFFSET;
		}
		if (easnfw_record_buffer_alloc(&transfer_buffer) != 0) {
			return IFL_STAT__NO_MEMORY;
		}
		transfer_active = true;
		transfer_id = fragment.transfer_id;
		transfer_total = fragment.total_len;
		transfer_offset = 0u;
	} else if (fragment.transfer_id != transfer_id ||
		   fragment.total_len != transfer_total) {
		*next_offset = transfer_offset;
		return IFL_STAT__TRANSFER_ERR;
	}
	if (fragment.offset != transfer_offset) {
		*next_offset = transfer_offset;
		return IFL_STAT__BAD_OFFSET;
	}

	memcpy(transfer_buffer + transfer_offset,
	       payload + sizeof(fragment), chunk_len);
	transfer_offset = end_offset;
	*next_offset = transfer_offset;
	if ((header->flags & EASN_FLAG_LAST) != 0u) {
		return deliver_record(transfer_id, header->seq, transfer_total);
	}
	return IFL_STAT__OK;
}

/** @brief Process one received frame in thread context. */
static void handle_frame(struct ifl_frame const *frame)
{
	easn_frame_hdr_t header;
	uint8_t const *payload;

	memcpy(&header, frame->bytes, sizeof(header));
	payload = frame->bytes + INTERFW_LINK_UART_HDR_SIZE;
	if (header.type == EASN_FRAME_ACK) {
		if (header.len == sizeof(easn_ack_payload_t)) {
			easn_ack_payload_t payload_ack;
			struct ifl_ack_result ack;
			memcpy(&payload_ack, payload, sizeof(payload_ack));
			if (payload_ack.reserved != 0u) {
				return;
			}
			ack.command = header.cmd_id;
			ack.sequence = header.seq;
			ack.status = payload_ack.status;
			(void)k_msgq_put(&ifl_ack_q, &ack, K_NO_WAIT);
		}
		return;
	}

	/* Link-level duplicate suppression: resend the cached ACK, do not re-run. */
	if (last_rx_valid &&
	    (header.flags & EASN_FLAG_RETRANSMIT) != 0u && header.cmd_id == last_rx_command &&
	    header.seq == last_rx_sequence) {
		(void)send_ack(&header, last_rx_status, last_rx_offset);
		return;
	}

	last_rx_offset = 0u;
	if (header.cmd_id == IFL_CMD_COMMON__PING) {
		last_rx_status = header.len == 0u && header.flags == 0u ?
			IFL_STAT__OK : IFL_STAT__BAD_LEN;
	} else if (header.cmd_id == IFL_CMD_CLOUD__RESET) {
		last_rx_status = header.len == 0u && header.flags == 0u ?
			IFL_STAT__OK : IFL_STAT__BAD_LEN;
	} else if (header.cmd_id == IFL_CMD_CLOUD__RECORD_DATA) {
		last_rx_status = receive_record(&header, payload, &last_rx_offset);
	} else if (header.cmd_id == IFL_CMD_CLOUD__TIME_SYNC_REQUEST) {
		if (header.len != 0u || header.flags != 0u) {
			last_rx_status = IFL_STAT__BAD_LEN;
		} else {
			last_rx_status = k_msgq_put(&time_request_q,
						    &header.seq,
						    K_NO_WAIT) == 0 ?
				IFL_STAT__OK : IFL_STAT__BUSY;
		}
	} else {
		last_rx_status = IFL_STAT__UNKNOWN_CMD;
	}

	last_rx_command = header.cmd_id;
	last_rx_sequence = header.seq;
	last_rx_valid = true;
	if (send_ack(&header, last_rx_status, last_rx_offset) != 0) {
		LOG_WRN("Failed to send IFL ACK for command 0x%02x", header.cmd_id);
	}
	if (header.cmd_id == IFL_CMD_CLOUD__RESET &&
	    last_rx_status == IFL_STAT__OK) {
		/* SENSOR requires a positive ACK before CLOUD cold-resets. */
		k_msleep(20);
		sys_reboot(SYS_REBOOT_COLD);
	}
}

/** @brief Fetch cellular UTC and deliver it to SENSOR when requested. */
static void time_worker(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	for (;;) {
		uint8_t request_sequence;
		int64_t unix_time_ms;
		int err;

		if (k_msgq_get(&time_request_q, &request_sequence,
			       K_FOREVER) != 0) {
			continue;
		}
		ARG_UNUSED(request_sequence);
		err = lte_m_get_utc_time_ms(&unix_time_ms);
		if (err != 0) {
			LOG_WRN("Cannot provide UTC to SENSOR yet (%d)", err);
			continue;
		}
		err = send_utc_time((uint64_t)unix_time_ms);
		if (err != 0) {
			LOG_ERR("UTC response was not acknowledged by SENSOR: %d",
				err);
		} else {
			LOG_INF("Cellular UTC delivered to SENSOR");
		}
	}
}

/** @brief Process queued frames and deliver cloud-transmission outcomes. */
static void ifl_worker(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	for (;;) {
		if (k_msgq_get(&ifl_rx_frame_q, &worker_frame, K_FOREVER) == 0) {
			handle_frame(&worker_frame);
		}
	}
}

/** @brief Consume pipeline results without inventing a durable cloud ACK. */
static void result_worker(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	for (;;) {
		struct easnfw_pipeline_message message;

		if (k_msgq_get(&tx_result_q, &message, K_FOREVER) != 0) {
			continue;
		}
		LOG_WRN("Transfer id=%u result=%u; SENSOR retains record because "
			"no durable cloud-commit acknowledgement exists",
			message.transfer_id, message.result);
		easnfw_record_buffer_free(message.data);
	}
}

/** @brief Send the versioned CLOUD_READY command and wait for SENSOR ACK. */
static int send_cloud_ready(void)
{
	uint8_t frame[INTERFW_LINK_UART_HDR_SIZE + 3u +
		      INTERFW_LINK_UART_CRC_SIZE];
	uint8_t const payload[3] = {1u, 0u, 0u};
	easn_frame_hdr_t header = {
		.sof = INTERFW_LINK_UART_SOF,
		.type = EASN_FRAME_CMD,
		.cmd_id = IFL_CMD_SENSOR__CLOUD_READY,
		.seq = 0u,
		.flags = 0u,
		.len = sizeof(payload),
	};
	uint16_t crc;

	memcpy(frame, &header, sizeof(header));
	memcpy(frame + INTERFW_LINK_UART_HDR_SIZE, payload, sizeof(payload));
	for (uint32_t attempt = 0u; attempt <= INTERFW_LINK_UART_MAX_RETRIES;
	     ++attempt) {
		struct ifl_ack_result ack;

		header.flags = attempt == 0u ? 0u : EASN_FLAG_RETRANSMIT;
		memcpy(frame, &header, sizeof(header));
		crc = crc16(frame + 1, INTERFW_LINK_UART_HDR_SIZE +
			    sizeof(payload) - 1u);
		frame[sizeof(frame) - 2u] = (uint8_t)crc;
		frame[sizeof(frame) - 1u] = (uint8_t)(crc >> 8);
		if (easnfw_cloud_uart_tx(frame, sizeof(frame)) != 0) {
			continue;
		}
		if (k_msgq_get(&ifl_ack_q, &ack,
			       K_MSEC(INTERFW_LINK_UART_ACK_TIMEOUT_MS)) == 0 &&
		    ack.command == header.cmd_id && ack.sequence == header.seq) {
			return ack.status == IFL_STAT__OK ? 0 : -EPROTO;
		}
	}
	return -ETIMEDOUT;
}

/**
 * @brief Initialize UART receive handling and finish the SENSOR handshake.
 *
 * @return 0 after CLOUD_READY is acknowledged, or a negative errno.
 */
int easnfw_cloud_ifl_init(void)
{
	int err = easnfw_cloud_uart_init();

	if (err != 0) {
		return err;
	}
	err = hwinfo_get_reset_cause(&cloud_reset_cause);
	if (err != 0) {
		LOG_WRN("Could not read CLOUD reset cause: %d", err);
		cloud_reset_cause = 0U;
	} else if (hwinfo_clear_reset_cause() != 0) {
		LOG_WRN("Could not clear CLOUD reset cause register");
	}
	err = easnfw_cloud_uart_rx_set(rx_handler);
	if (err != 0) {
		return err;
	}
	k_thread_create(&ifl_worker_thread_data, ifl_worker_stack,
			K_THREAD_STACK_SIZEOF(ifl_worker_stack), ifl_worker,
			NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	err = send_cloud_ready();
	if (err != 0) {
		LOG_ERR("CLOUD_READY handshake failed (%d)", err);
		return err;
	}
	LOG_INF("CLOUD_READY acknowledged by EASNFW-SENSOR");
	return 0;
}

/** @brief Log why a completed transfer is not a durable cloud delivery. */
void sensor_link_start(void)
{
	k_thread_create(&time_worker_thread_data, time_worker_stack,
			K_THREAD_STACK_SIZEOF(time_worker_stack), time_worker,
			NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	k_thread_create(&result_thread_data, result_stack,
			K_THREAD_STACK_SIZEOF(result_stack), result_worker,
			NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	LOG_INF("Sensor-link result relay enabled; durable ACK awaits backend contract");
}
