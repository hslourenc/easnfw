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

#endif
