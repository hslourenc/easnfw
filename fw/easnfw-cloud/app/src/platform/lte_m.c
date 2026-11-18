#include <errno.h>

#include <date_time.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/lte_m.h>

LOG_MODULE_REGISTER(easnfw_lte_m, LOG_LEVEL_INF);

K_MUTEX_DEFINE(modem_lock);
static bool modem_initialized;
static bool registration_started;
static int64_t next_time_update_ms;

#define DIAG_PERIOD_S 20
#define TIME_UPDATE_RETRY_MS 30000

static void diag_at(const char *cmd)
{
	static char resp[400];
	int err = nrf_modem_at_cmd(resp, sizeof(resp), "%s", cmd);

	if (err == 0) {
		LOG_INF("DIAG %s -> %s", cmd, resp);
	} else {
		LOG_WRN("DIAG %s failed: %d", cmd, err);
	}
}

static void diag_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(diag_work, diag_work_fn);

static void diag_work_fn(struct k_work *work)
{
	diag_at("AT+CEREG?");
	diag_at("AT%CESQ=1");
	diag_at("AT%XMONITOR");
	k_work_reschedule(&diag_work, K_SECONDS(DIAG_PERIOD_S));
}

static void lte_event_handler(const struct lte_lc_evt *evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		LOG_INF("LTE reg status: %d", (int)evt->nw_reg_status);
		break;
	case LTE_LC_EVT_LTE_MODE_UPDATE:
		LOG_INF("LTE mode: %d", (int)evt->lte_mode);
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		LOG_INF("Cell: id=%u tac=%u", evt->cell.id, evt->cell.tac);
		break;
	case LTE_LC_EVT_MODEM_EVENT:
		LOG_INF("Modem event: %d", (int)evt->modem_evt.type);
		break;
	default:
		break;
	}
}

static void diag_start(void)
{
	lte_lc_register_handler(lte_event_handler);
	diag_at("AT+CGMR");
	diag_at("AT%CSUS?");
	diag_at("AT+CIMI");
	diag_at("AT%XICCID");
	diag_at("AT%XSYSTEMMODE?");
	diag_at("AT%XBANDLOCK?");
	diag_at("AT+CPSMS?");
	k_work_reschedule(&diag_work, K_SECONDS(DIAG_PERIOD_S));
}

/**
 * @brief Initialize the modem and start LTE registration without waiting.
 *
 * @return 0 when registered, -EAGAIN while registering, or an error.
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
		diag_start();
	}

	if (!registration_started) {
		LOG_INF("Starting LTE network registration");
		err = lte_lc_connect_async(NULL);
		if (err != 0) {
			LOG_ERR("Could not start LTE registration: %d", err);
			k_mutex_unlock(&modem_lock);
			return err;
		}
		registration_started = true;
	}

	enum lte_lc_nw_reg_status status;

	err = lte_lc_nw_reg_status_get(&status);
	k_mutex_unlock(&modem_lock);
	if (err != 0) {
		LOG_ERR("Could not query LTE registration: %d", err);
		return err;
	}
	return status == LTE_LC_NW_REG_REGISTERED_HOME ||
	       status == LTE_LC_NW_REG_REGISTERED_ROAMING ? 0 : -EAGAIN;
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
	if (date_time_is_valid()) {
		return date_time_now(unix_time_ms);
	}

	k_mutex_lock(&modem_lock, K_FOREVER);
	if (k_uptime_get() >= next_time_update_ms) {
		err = date_time_update_async(NULL);
		if (err != 0) {
			LOG_ERR("Could not request cellular time update: %d", err);
			k_mutex_unlock(&modem_lock);
			return err;
		}
		next_time_update_ms = k_uptime_get() + TIME_UPDATE_RETRY_MS;
	}
	k_mutex_unlock(&modem_lock);
	return -EAGAIN;
}
