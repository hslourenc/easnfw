#ifndef EASNFW_HTTP_CLIENT_H
#define EASNFW_HTTP_CLIENT_H

/** Post a payload using the configured cloud ingestion contract. */
int http_client_post(const void *payload, unsigned int payload_size);

#endif
