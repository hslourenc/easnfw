/**
 * @file cloud_link.h
 * @brief Starts the SENSOR-to-CLOUD transmission worker.
 *
 * The UART protocol is defined in the shared Inter-FW Link header. Link-level
 * acknowledgement is distinct from durable backend delivery.
 */
#ifndef EASNFW_CLOUD_LINK_H
#define EASNFW_CLOUD_LINK_H

/** Start the inter-component transmission worker. */
void cloud_link_start(void);

#endif
