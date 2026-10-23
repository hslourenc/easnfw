#include <errno.h>

#include <date_time.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/lte_m.h>

LOG_MODULE_REGISTER(easnfw_lte_m, LOG_LEVEL_INF);

K_MUTEX_DEFINE(modem_lock);
static bool modem_initialized;

/**
 * @brief Initialize the modem and synchronously attach to an LTE network.
 *
 * @return 0 when registered, otherwise the modem or LTE link-control error.
 */
int lte_m_connect(void)
{
	int err;

	k_mutex_lock(&modem_lock, K_FOREVER);
	if (!modem_initialized) {
		err = nrf_modem_lib_init();
		if (err != 0) {
			LOG_ERR("nRF modem library initialization failed: %d", err);
			k_mutex_unlock(&modem_lock);
			return err;
		}
		modem_initialized = true;
	}

	err = lte_lc_connect();
	if (err != 0 && err != -EINPROGRESS) {
		LOG_ERR("LTE network registration failed: %d", err);
	}
	k_mutex_unlock(&modem_lock);
	return err == -EINPROGRESS ? 0 : err;
}

/**
 * @brief Obtain UTC from the cellular network through the modem date-time API.
 *
 * @param unix_time_ms Receives UTC milliseconds since 1970-01-01.
 * @return 0 on success, otherwise a negative errno/modem error.
 */
int lte_m_get_utc_time_ms(int64_t *unix_time_ms)
{
	int err;

	if (unix_time_ms == NULL) {
		return -EINVAL;
	}
	err = lte_m_connect();
	if (err != 0) {
		return err;
	}
	err = date_time_now(unix_time_ms);
	if (err == 0) {
		return 0;
	}
	err = date_time_update_async(NULL);
	if (err != 0) {
		LOG_ERR("Could not request cellular time update: %d", err);
		return err;
	}

	for (uint32_t elapsed = 0U; elapsed < 60U; ++elapsed) {
		k_sleep(K_SECONDS(1));
		err = date_time_now(unix_time_ms);
		if (err == 0) {
			return 0;
		}
		if (err != -ENODATA) {
			return err;
		}
	}
	LOG_WRN("Cellular network has not provided UTC time");
	return -ETIMEDOUT;
}
