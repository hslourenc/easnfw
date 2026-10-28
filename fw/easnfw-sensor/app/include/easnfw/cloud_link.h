/**
 * @file cloud_link.h
 * @brief Starts the SENSOR-to-CLOUD transmission worker.
 *
 * The UART protocol is defined in the shared Inter-FW Link header. Link-level
 * acknowledgement is distinct from durable backend delivery.
 */
#ifndef EASNFW_CLOUD_LINK_H
#define EASNFW_CLOUD_LINK_H

#include <stdint.h>

/** Start the inter-component transmission worker. */
void cloud_link_start(void);

/**
 * @brief Stream one committed record to EASNFW-CLOUD over the IFL.
 *
 * The blob is the record file followed by a little-endian CRC-32 of the
 * file, sent as acknowledged fragments. A BAD_OFFSET acknowledgement resumes
 * the transfer from the offset EASNFW-CLOUD reports. Success means the
 * record was received and verified, not durably stored by the cloud platform.
 *
 * @param record_id 16-byte identifier of a committed record.
 * @return 0 on success, otherwise a negative errno or EASNFW status code.
 */
int cloud_link__record_send(const uint8_t record_id[16]);

#endif
