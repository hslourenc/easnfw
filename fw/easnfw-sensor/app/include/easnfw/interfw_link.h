#ifndef EASNFW_INTERFW_LINK_H
#define EASNFW_INTERFW_LINK_H

/**
 * @brief Send the RESET_EASNFWCLOUD command to EASNFW-CLOUD.
 *
 * @return 0 on success, error code from enum easnfw_status on failure.
 */
int interfw_link__reset_easnfwcloud_tx(void);

/**
 * @brief Handler for the EASNFWCLOUD_READY command from EASNFW-CLOUD.
 *
 * Acknowledge command and set a global flag indicating 
 *
 * @return 0 on success, error code from enum easnfw_status on failure.
 */
int interfw_link__easnfwcloud_ready_rx(void);

#endif /* EASNFW_INTERFW_LINK_H */