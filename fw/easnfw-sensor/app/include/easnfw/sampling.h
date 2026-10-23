/**
 * @file sampling.h
 * @brief Starts periodic track acquisition after application startup checks.
 *
 * The sampling worker coordinates audio and environmental acquisition and
 * passes completed blocks and track metadata to downstream pipeline stages.
 */
#ifndef EASNFW_SAMPLING_H
#define EASNFW_SAMPLING_H

/** Start the periodic track-acquisition worker. */
void sampling_start(void);

#endif
