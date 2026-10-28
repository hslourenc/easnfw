#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include <easnfw/cloud_link.h>
#include <easnfw/interfw_link.h>
#include <easnfw/mass_storage.h>
#include <easnfw/pipeline.h>
#include <easnfw/status.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(easnfw_cloud_link, LOG_LEVEL_INF);

#define CLOUD_LINK_STACK_SIZE 6144U
/** Maximum number of times a transfer is resumed from a BAD_OFFSET ACK. */
#define CLOUD_LINK_MAX_RESUMES 3U
#define CLOUD_LINK_IO_SIZE 512U

K_THREAD_STACK_DEFINE(cloud_link_stack, CLOUD_LINK_STACK_SIZE);
static struct k_thread cloud_link_thread_data;

/* Static: too large for the worker stack. */
static uint8_t tx_payload[INTERFW_LINK_UART_MAX_PAYLOAD];
static uint8_t crc_buffer[CLOUD_LINK_IO_SIZE];
static uint32_t transfer_counter;

/** Compute the CRC-32 (IEEE) of the record file for the blob trailer. */
static int record_file_crc(uint32_t file_size, uint32_t *crc)
{
	uint32_t offset = 0U;

	*crc = 0U;
	while (offset < file_size) {
		uint32_t const chunk = MIN(file_size - offset, sizeof(crc_buffer));
		int err = app_mass_storage__record_read(offset, crc_buffer, chunk);

		if (err != 0) {
			return err;
		}
		*crc = crc32_ieee_update(*crc, crc_buffer, chunk);
		offset += chunk;
	}
	return 0;
}

/** Read blob bytes: the record file followed by its little-endian CRC-32. */
static int blob_read(uint32_t file_size, uint32_t crc, uint32_t offset,
		     uint8_t *data, uint32_t len)
{
	uint32_t from_file = 0U;
	int err;

	if (offset < file_size) {
		from_file = MIN(len, file_size - offset);
		err = app_mass_storage__record_read(offset, data, from_file);
		if (err != 0) {
			return err;
		}
	}
	for (uint32_t i = from_file; i < len; ++i) {
		uint32_t const trailer_index = offset + i - file_size;

		data[i] = (uint8_t)(crc >> (8U * trailer_index));
	}
	return 0;
}

/** Ask EASNFW-CLOUD to discard the active transfer; failure is not fatal. */
static void transfer_abort(uint32_t transfer_id, uint32_t total_len)
{
	easn_frag_hdr_t const header = {
		.transfer_id = transfer_id,
		.total_len = total_len,
		.offset = 0U,
	};

	memcpy(tx_payload, &header, sizeof(header));
	(void)ifl__cmd_tx_flags(IFL_CMD_CLOUD__RECORD_DATA,
				EASN_FLAG_FRAGMENT | EASN_FLAG_ABORT, tx_payload,
				sizeof(header), NULL);
}

/** Stream one committed record file plus CRC trailer over the IFL. */
int cloud_link__record_send(const uint8_t record_id[16])
{
	uint32_t file_size;
	uint32_t crc;
	uint32_t total;
	uint32_t offset = 0U;
	uint32_t resumes = 0U;
	uint32_t const transfer_id = ++transfer_counter;
	int err;

	err = app_mass_storage__record_read_open(record_id, &file_size);
	if (err != 0) {
		return err;
	}
	total = file_size + EASN_RECORD_TRAILER_SIZE;
	err = record_file_crc(file_size, &crc);

	while (err == 0 && offset < total) {
		uint32_t const chunk =
			MIN(total - offset, INTERFW_LINK_UART_MAX_FRAG_DATA);
		easn_frag_hdr_t const header = {
			.transfer_id = transfer_id,
			.total_len = total,
			.offset = offset,
		};
		uint8_t flags = EASN_FLAG_FRAGMENT;
		uint32_t next_offset = 0U;

		if (offset + chunk == total) {
			flags |= EASN_FLAG_LAST;
		}
		memcpy(tx_payload, &header, sizeof(header));
		err = blob_read(file_size, crc, offset,
				&tx_payload[sizeof(header)], chunk);
		if (err != 0) {
			break;
		}
		err = ifl__cmd_tx_flags(IFL_CMD_CLOUD__RECORD_DATA, flags,
					tx_payload, sizeof(header) + chunk,
					&next_offset);
		if (err == 0) {
			offset += chunk;
		} else if (err == -EASNFW_STATUS__INTERFW_LINK_ACK_BAD_OFFSET &&
			   next_offset < total && resumes < CLOUD_LINK_MAX_RESUMES) {
			LOG_WRN("Resuming record transfer at offset %u", next_offset);
			++resumes;
			offset = next_offset;
			err = 0;
		}
	}
	if (err != 0) {
		transfer_abort(transfer_id, total);
	}
	app_mass_storage__record_read_close();
	return err;
}

/** Stream each committed record; the file stays on the SD card regardless. */
static void cloud_link_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (true) {
		struct easnfw_pipeline_message *message;
		int err = k_msgq_get(&storage_tx_q, &message, K_FOREVER);

		if (err != 0) {
			LOG_ERR("Could not receive committed-record notification: %d",
				err);
			continue;
		}
		err = cloud_link__record_send(message->metadata.record_id);
		if (err == 0) {
			/*
			 * EASNFW_REVISIT: the final ACK only means EASNFW-CLOUD
			 * received and verified the record. Wait for a distinct
			 * durable-cloud acknowledgement before marking it sent,
			 * and define the retry policy for failed transfers.
			 */
			LOG_INF("Record %02x... transferred to EASNFW-CLOUD",
				message->metadata.record_id[0]);
		} else {
			LOG_WRN("Record %02x... transfer failed (%d); record retained",
				message->metadata.record_id[0], err);
		}
		easnfw_pipeline_message_free(message);
	}
}

void cloud_link_start(void)
{
	k_thread_create(&cloud_link_thread_data, cloud_link_stack,
		K_THREAD_STACK_SIZEOF(cloud_link_stack), cloud_link_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}
