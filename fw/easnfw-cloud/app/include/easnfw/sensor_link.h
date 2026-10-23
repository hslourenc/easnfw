/**
 * @file sensor_link.h
 * @brief CLOUD-side Inter-FW Link initialization and worker interface.
 *
 * Initialization completes the startup handshake with SENSOR before the
 * receive, UTC synchronization, and result-relay workers are started.
 */
#ifndef EASNFW_SENSOR_LINK_H
#define EASNFW_SENSOR_LINK_H

/** Initialize IFL UART and complete CLOUD_READY handshake. */
int easnfw_cloud_ifl_init(void);

/** Start the IFL receiver, UTC worker, and result relay. */
void sensor_link_start(void);

#endif
