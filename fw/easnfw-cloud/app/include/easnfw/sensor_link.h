#ifndef EASNFW_SENSOR_LINK_H
#define EASNFW_SENSOR_LINK_H

/** Initialize IFL UART and complete CLOUD_READY handshake. */
int easnfw_cloud_ifl_init(void);

/** Start the IFL receiver, UTC worker, and result relay. */
void sensor_link_start(void);

#endif
