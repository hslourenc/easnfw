/**
 * @file storage.h
 * @brief Starts the worker that persists and assembles SENSOR records.
 *
 * Record lifecycle and on-disk format operations are exposed by
 * ``mass_storage.h``; this application interface starts their pipeline
 * consumer.
 */
#ifndef EASNFW_STORAGE_H
#define EASNFW_STORAGE_H

/** Start the record-storage worker. */
void storage_start(void);

#endif
