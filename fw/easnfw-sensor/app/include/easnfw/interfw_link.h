#ifndef EASNFW_INTERFW_LINK_H
#define EASNFW_INTERFW_LINK_H

#include <interfw_link_protocol.h>
#include <stdbool.h>

/**
 * Commands accepted by EASNFW-SENSOR's IFL endpoint.
 *
 * The table includes common commands and SENSOR-range commands sent by
 * EASNFW-CLOUD. CLOUD-range commands are sent by SENSOR and therefore are not
 * entries in this receive table.
 */
static const easn_ifl_command_entry_t easnfw_sensor_ifl_command_table[] = {
	{
		.cmd_id = IFL_CMD_COMMON__PING,
		.flags = EASN_CMDF_NO_PAYLOAD,
		.payload_len = 0u,
	},
	{
		.cmd_id = IFL_CMD_SENSOR__CLOUD_READY,
		.flags = 0u,
		.payload_len = sizeof(ifl_cmd_payload_cloud_ready_t),
	},
	{
		.cmd_id = IFL_CMD_SENSOR__UTC_TIME,
		.flags = 0u,
		.payload_len = sizeof(ifl_cmd_payload_utc_time_t),
	},
};

#define EASNFW_SENSOR_IFL_COMMAND_COUNT \
	(sizeof(easnfw_sensor_ifl_command_table) / \
	 sizeof(easnfw_sensor_ifl_command_table[0]))

/**
 * Size of the message queue to receive commands through IFL.
 */
#define EASNFW_SENSOR_IFL_RX_MSGQ_SIZE 8u

/**
 * @brief Request that EASNFW-CLOUD reset.
 *
 * @return 0 when the reset request is acknowledged, otherwise a negative
 * EASNFW status code.
 */
int ifl__reset_easnfwcloud_tx(void);

/**
 * @brief Send a stop-and-wait PING command to EASNFW-CLOUD.
 *
 * @return 0 on successful acknowledgement, otherwise a negative EASNFW
 * status code.
 */
int ifl__ping_tx(void);

/** @brief Request UTC time from EASNFW-CLOUD and wait for its link ACK. */
int ifl__utc_time_request_tx(void);

/** @brief Inject a positive PING acknowledgement for isolated HIL runs. */
void ifl__verification_ping_override_set(bool enabled);

/**
 * @brief Handler for the EASNFWCLOUD_READY command from EASNFW-CLOUD.
 *
 * The handler acknowledges the command, records the CLOUD-ready state, and
 * wakes any waiting reset coordination.
 *
 * @return 0 on success, otherwise a negative EASNFW status code.
 */
int ifl__easnfwcloud_ready_rx(void);

/**
 * @brief Send a command with frame flags and report the ACK's next offset.
 *
 * Same stop-and-wait behavior as ifl__cmd_tx(). Retransmissions add
 * EASN_FLAG_RETRANSMIT to @p flags. For fragment commands, @p next_offset
 * receives the offset the receiver expects next, which is the resume point
 * when the result is -EASNFW_STATUS__INTERFW_LINK_ACK_BAD_OFFSET.
 *
 * @param cmd Command to send.
 * @param flags Frame flags, see EASN_FLAG_*.
 * @param payload Command payload, or NULL when payload_len is zero.
 * @param payload_len Payload length in bytes.
 * @param next_offset Optional; receives the ACK's next_offset.
 * @return 0 on success, otherwise a negative EASNFW status code.
 */
int ifl__cmd_tx_flags(uint8_t const cmd, uint8_t const flags,
		      uint8_t const *payload, uint16_t const payload_len,
		      uint32_t *const next_offset);

#endif /* EASNFW_INTERFW_LINK_H */