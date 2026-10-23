#include <errno.h>
#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <easnfw/time_sync.h>

#define UTC_MIN_VALID_MS INT64_C(946684800000)
#define UTC_MAX_VALID_MS INT64_C(253402300799999)

static struct k_spinlock utc_lock;
static int64_t utc_anchor_ms;
static int64_t uptime_anchor_ms;
static bool utc_valid;
K_SEM_DEFINE(utc_sync_sem, 0, 1);

/** Return whether an initial valid UTC synchronization has been received. */
bool app_utc_time__is_valid(void)
{
	k_spinlock_key_t key = k_spin_lock(&utc_lock);
	bool valid = utc_valid;

	k_spin_unlock(&utc_lock, key);
	return valid;
}

/** Set the UTC epoch anchor and monotonic uptime at the same instant. */
int app_utc_time__set(uint64_t unix_time_ms)
{
	if (unix_time_ms < UTC_MIN_VALID_MS ||
	    unix_time_ms > UTC_MAX_VALID_MS) {
		return -ERANGE;
	}

	k_spinlock_key_t key = k_spin_lock(&utc_lock);
	utc_anchor_ms = (int64_t)unix_time_ms;
	uptime_anchor_ms = k_uptime_get();
	utc_valid = true;
	k_spin_unlock(&utc_lock, key);
	k_sem_give(&utc_sync_sem);
	return 0;
}

/** Convert the current monotonic uptime to UTC using the latest anchor. */
int app_utc_time__now_ms(int64_t *unix_time_ms)
{
	k_spinlock_key_t key;
	int64_t now_uptime_ms;

	if (unix_time_ms == NULL) {
		return -EINVAL;
	}

	key = k_spin_lock(&utc_lock);
	if (!utc_valid) {
		k_spin_unlock(&utc_lock, key);
		return -ENODATA;
	}
	now_uptime_ms = k_uptime_get();
	*unix_time_ms = utc_anchor_ms + (now_uptime_ms - uptime_anchor_ms);
	k_spin_unlock(&utc_lock, key);
	return 0;
}

/** Format UTC milliseconds in the fixed YYYY-MM-DDTHH:MM:SS.mmmZ form. */
int app_utc_time__format(int64_t unix_time_ms, char *buffer,
			 size_t buffer_size)
{
	time_t seconds;
	struct tm broken_down;
	int64_t milliseconds;
	int written;

	if (buffer == NULL || buffer_size < EASNFW_UTC_TIMESTAMP_SIZE ||
	    unix_time_ms < UTC_MIN_VALID_MS ||
	    unix_time_ms > UTC_MAX_VALID_MS) {
		return -EINVAL;
	}
	seconds = (time_t)(unix_time_ms / 1000);
	milliseconds = unix_time_ms % 1000;
	if (gmtime_r(&seconds, &broken_down) == NULL) {
		return -ERANGE;
	}
	written = snprintk(buffer, buffer_size,
			   "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
			   broken_down.tm_year + 1900, broken_down.tm_mon + 1,
			   broken_down.tm_mday, broken_down.tm_hour,
			   broken_down.tm_min, broken_down.tm_sec,
			   (long long)milliseconds);
	if (written != (int)(EASNFW_UTC_TIMESTAMP_SIZE - 1U)) {
		return -ERANGE;
	}
	return 0;
}

/** Wait until the initial UTC anchor is set or the supplied timeout expires. */
int app_utc_time__wait(k_timeout_t timeout)
{
	if (app_utc_time__is_valid()) {
		return 0;
	}
	if (k_sem_take(&utc_sync_sem, timeout) != 0) {
		return -ETIMEDOUT;
	}
	return 0;
}
