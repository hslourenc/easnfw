#ifndef EASNFW_INTERFW_LINK_H
#define EASNFW_INTERFW_LINK_H

#include <interfw_link_protocol.h>

/**
 * Commands accepted by EASNFW-SENSOR.
 *
 * The table includes common commands and commands exposed by SENSOR to
 * EASNFW-CLOUD. Commands in the CLOUD range are transmitted by SENSOR and
 * therefore are not entries in this receive table.
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
};

#define EASNFW_SENSOR_IFL_COMMAND_COUNT \
	(sizeof(easnfw_sensor_ifl_command_table) / \
	 sizeof(easnfw_sensor_ifl_command_table[0]))

/**
 * Size of the message queue to receive commands through IFL.
 */
#define EASNFW_SENSOR_IFL_RX_MSGQ_SIZE 8u

/**
 * @brief Send the RESET_EASNFWCLOUD command to EASNFW-CLOUD.
 *
 * @return 0 on success, error code from enum easnfw_status on failure.
 */
int ifl__reset_easnfwcloud_tx(void);

/**
 * @brief Handler for the EASNFWCLOUD_READY command from EASNFW-CLOUD.
 *
 * Acknowledge command and set a global flag indicating 
 *
 * @return 0 on success, error code from enum easnfw_status on failure.
 */
int ifl__easnfwcloud_ready_rx(void);

#endif /* EASNFW_INTERFW_LINK_H */