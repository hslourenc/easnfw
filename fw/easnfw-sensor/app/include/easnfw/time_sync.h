/**
 * @file time_sync.h
 * @brief Runtime UTC synchronization and timestamp formatting interface.
 *
 * The current UTC anchor is valid only for the current boot and advances from
 * Zephyr's monotonic uptime. Callers must treat UTC as unavailable until a
 * time anchor has been received from CLOUD.
 */
#ifndef EASNFW_TIME_SYNC_H
#define EASNFW_TIME_SYNC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#define EASNFW_UTC_TIMESTAMP_SIZE 25U

/** @brief Return whether UTC has been synchronized during this boot. */
bool app_utc_time__is_valid(void);

/**
 * @brief Set the UTC epoch anchor received from EASNFW-CLOUD.
 *
 * @param unix_time_ms UTC milliseconds since 1970-01-01.
 * @return 0 on success, -ERANGE when the time is outside years 2000-9999.
 */
int app_utc_time__set(uint64_t unix_time_ms);

/**
 * @brief Convert current monotonic uptime to UTC milliseconds.
 *
 * @param unix_time_ms Receives the UTC epoch time.
 * @return 0 when synchronized, otherwise -ENODATA.
 */
int app_utc_time__now_ms(int64_t *unix_time_ms);

/**
 * @brief Format UTC milliseconds as an ISO-8601 UTC timestamp.
 *
 * @param unix_time_ms UTC milliseconds since 1970-01-01.
 * @param buffer Output buffer.
 * @param buffer_size Output capacity; must be at least
 *        EASNFW_UTC_TIMESTAMP_SIZE bytes.
 * @return 0 on success, otherwise a negative errno value.
 */
int app_utc_time__format(int64_t unix_time_ms, char *buffer,
			 size_t buffer_size);

/** @brief Wait for the initial UTC synchronization event. */
int app_utc_time__wait(k_timeout_t timeout);

/**
 * @brief Request UTC from EASNFW-CLOUD until it has been synchronized.
 *
 * Acquisition is blocked while this runs; failure to obtain time never
 * enters diagnostic state.
 *
 * @param request_timeout Time to wait for a response after each request.
 * @param retry_seconds Delay before the next request after a timeout.
 * @param deadline Overall limit; K_FOREVER for the production startup.
 * @param attempts Optional; receives the number of requests issued.
 * @return 0 once synchronized, -ETIMEDOUT when the deadline expires.
 */
int app_utc_time__acquire(k_timeout_t request_timeout, uint32_t retry_seconds,
			  k_timeout_t deadline, uint32_t *attempts);

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
/**
 * @brief Simulate a network outage (TC-048).
 *
 * Invalidates the UTC anchor and ignores UTC updates from EASNFW-CLOUD for
 * the given duration.
 *
 * @param duration_ms Outage duration in milliseconds.
 */
void app_utc_time__verification_outage_start(uint32_t duration_ms);
#endif

#endif
