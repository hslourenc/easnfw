#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>

#include <easnfw/interfw_link.h>
#include <easnfw/nvs.h>
#include <easnfw/reset.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>

LOG_MODULE_REGISTER(easnfw_reset, LOG_LEVEL_INF);

#define RESET_HISTORY_MAGIC 0x45525354U
#define DIAGNOSTIC_MAGIC 0x45444941U
#define DIAGNOSTIC_LED_HALF_PERIOD_SECONDS 15U

struct reset_causes {
	uint32_t sensor;
	uint32_t cloud;
};

struct reset_history {
	uint32_t magic;
	uint16_t version;
	uint16_t count;
	uint16_t next_index;
	uint16_t reserved;
	int64_t timestamps_ms[CONFIG_EASNFW_MAX_RECOVERY_RESETS];
	uint32_t crc;
};

struct diagnostic_reason {
	uint32_t magic;
	int32_t error;
	uint8_t has_record_id;
	uint8_t reserved[3];
	uint8_t record_id[16];
};

static struct reset_causes startup_reset_causes;
static bool sensor_reset_cause_valid;
static bool recovery_timer_started;
static struct k_thread recovery_timer_thread_data;
K_THREAD_STACK_DEFINE(recovery_timer_stack, 1024U);

static const struct gpio_dt_spec diagnostic_led =
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/** Time allowing the last IFL ACK to leave the UART before rebooting. */
#define RESET_ACK_FLUSH_MS 20

/* Set when EASNFW-CLOUD has completed the requested reset. */
static bool easnfwcloud_ready;
static struct k_sem easnfwcloud_ready_sem;
static bool reset_sync_initialized;

/** Calculate the reset-history CRC, excluding its stored CRC field. */
static uint32_t reset_history_crc(const struct reset_history *history)
{
	return crc32_ieee((const uint8_t *)history,
			  offsetof(struct reset_history, crc));
}

/** Store both processor reset causes under one versioned NVS entry. */
static int reset_causes_persist(void)
{
	return app_nvs__write(NVS_ENTRY_ID__RESET_REASON, &startup_reset_causes,
			      sizeof(startup_reset_causes));
}

/** Read the SENSOR reset cause, persist it, then clear the hardware latch. */
int reset__capture_startup_reason(void)
{
	int err = hwinfo_get_reset_cause(&startup_reset_causes.sensor);

	if (err != 0) {
		LOG_ERR("Could not read SENSOR reset cause: %d", err);
		return err;
	}
	sensor_reset_cause_valid = true;
	startup_reset_causes.cloud = 0U;
	err = reset_causes_persist();
	if (err != 0) {
		LOG_ERR("Could not persist SENSOR reset cause: %d", err);
		return err;
	}
	err = hwinfo_clear_reset_cause();
	if (err != 0) {
		LOG_ERR("Could not clear SENSOR reset-cause register: %d", err);
	}
	LOG_INF("Captured SENSOR reset cause 0x%08x",
		startup_reset_causes.sensor);
	return err;
}

/** Persist the CLOUD reset cause when received with the UTC response. */
int reset__cloud_reason_set(uint32_t cause)
{
	if (!sensor_reset_cause_valid) {
		return -EAGAIN;
	}
	startup_reset_causes.cloud = cause;
	return reset_causes_persist();
}

/** Append one synchronized UTC timestamp to the fixed-size NVS ring. */
int reset__history_update(void)
{
	struct reset_history history = {0};
	int64_t timestamp_ms;
	int err;

	err = app_utc_time__now_ms(&timestamp_ms);
	if (err != 0) {
		return err;
	}
	err = app_nvs__read(NVS_ENTRY_ID__RESET_HISTORY, &history,
			    sizeof(history));
	if (err == -ENOENT) {
		history.magic = RESET_HISTORY_MAGIC;
		history.version = 1U;
	} else if (err != 0) {
		LOG_ERR("Could not read reset history from NVS: %d", err);
		return err;
	} else if (history.magic != RESET_HISTORY_MAGIC ||
		   history.version != 1U ||
		   history.count > CONFIG_EASNFW_MAX_RECOVERY_RESETS ||
		   history.next_index >= CONFIG_EASNFW_MAX_RECOVERY_RESETS ||
		   history.crc != reset_history_crc(&history)) {
		LOG_ERR("Reset history in NVS is corrupt or incompatible");
		return -EBADMSG;
	}

	history.timestamps_ms[history.next_index] = timestamp_ms;
	history.next_index = (history.next_index + 1U) %
			     CONFIG_EASNFW_MAX_RECOVERY_RESETS;
	if (history.count < CONFIG_EASNFW_MAX_RECOVERY_RESETS) {
		++history.count;
	}
	history.crc = reset_history_crc(&history);
	err = app_nvs__write(NVS_ENTRY_ID__RESET_HISTORY, &history,
			     sizeof(history));
	if (err != 0) {
		LOG_ERR("Could not update reset history in NVS: %d", err);
		return err;
	}
	LOG_INF("Added UTC boot timestamp to reset history (%u/%u)",
		history.count, CONFIG_EASNFW_MAX_RECOVERY_RESETS);
	return 0;
}

/** Clear the recovery counter once this boot has remained stable long enough. */
static void recovery_timer_thread(void *arg1, void *arg2, void *arg3)
{
	uint32_t zero = 0U;
	int err;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	k_sleep(K_SECONDS(CONFIG_EASNFW_RECOVERY_RESET_WINDOW_SECONDS));
	err = app_nvs__write(NVS_ENTRY_ID__RECOVERY_RESET_COUNT, &zero,
			     sizeof(zero));
	if (err != 0) {
		LOG_ERR("Could not clear recovery-reset counter: %d", err);
	}
}

/** Start a one-shot uptime timer for clearing the recovery-reset counter. */
void reset__start_recovery_counter_timer(void)
{
	if (recovery_timer_started) {
		return;
	}
	recovery_timer_started = true;
	k_thread_create(&recovery_timer_thread_data, recovery_timer_stack,
			K_THREAD_STACK_SIZEOF(recovery_timer_stack),
			recovery_timer_thread, NULL, NULL, NULL, 7, 0,
			K_NO_WAIT);
}

/** Persist diagnostic context and blink LED0 with a 30-second period. */
void reset__diagnostic_state(int reason, const uint8_t *record_id)
{
	struct diagnostic_reason diagnostic = {
		.magic = DIAGNOSTIC_MAGIC,
		.error = reason,
		.has_record_id = record_id != NULL,
	};
	int err;

	if (record_id != NULL) {
		memcpy(diagnostic.record_id, record_id,
		       sizeof(diagnostic.record_id));
	}
	err = app_nvs__write(NVS_ENTRY_ID__DIAGNOSTIC_REASON, &diagnostic,
			    sizeof(diagnostic));
	if (err != 0) {
		LOG_ERR("Could not persist diagnostic reason %d: %d",
			reason, err);
	}
	LOG_ERR("Diagnostic state entered (reason=%d); automatic reset is "
		"disabled until a recoverable fault is specified", reason);

	if (!gpio_is_ready_dt(&diagnostic_led)) {
		LOG_ERR("Diagnostic LED is not ready");
	} else {
		err = gpio_pin_configure_dt(&diagnostic_led, GPIO_OUTPUT_INACTIVE);
		if (err != 0) {
			LOG_ERR("Could not configure diagnostic LED: %d", err);
		}
	}
	for (;;) {
		k_sleep(K_SECONDS(DIAGNOSTIC_LED_HALF_PERIOD_SECONDS));
		if (gpio_is_ready_dt(&diagnostic_led)) {
			err = gpio_pin_toggle_dt(&diagnostic_led);
			if (err != 0) {
				LOG_ERR("Could not toggle diagnostic LED: %d", err);
			}
		}
	}
}

int reset__easnfwcloud_ready_set(void)
{
	easnfwcloud_ready = true;
	if (reset_sync_initialized) {
		k_sem_give(&easnfwcloud_ready_sem);
	}
	return 0;
}

int reset__easnfwcloud_ready_clear(void)
{
	easnfwcloud_ready = false;
	return 0;
}

int reset__easnfw_reset(bool const force)
{
	int err;

	ARG_UNUSED(force);

	if (!ifl__is_init()) {
		err = ifl__init();
		if (err != 0) {
			return err;
		}
	}

	if (!reset_sync_initialized) {
		k_sem_init(&easnfwcloud_ready_sem, 0, 1);
		reset_sync_initialized = true;
	}

	reset__easnfwcloud_ready_clear();
	k_sem_reset(&easnfwcloud_ready_sem);

	err = ifl__reset_easnfwcloud_tx();
	if (err != 0) {
		LOG_ERR("Failed to request EASNFW-CLOUD reset (%d)", err);
		return err;
	}

	if (k_sem_take(&easnfwcloud_ready_sem, K_SECONDS(5)) != 0 ||
	    !easnfwcloud_ready) {
		LOG_ERR("Timed out waiting for EASNFW-CLOUD ready indication");
		return -EASNFW_STATUS__INTERFW_LINK_ACK_TIMEOUT;
	}

	LOG_INF("EASNFW-CLOUD reset complete; resetting EASNFW-SENSOR");
	/* Let the asynchronous CLOUD_READY ACK finish transmitting. */
	k_sleep(K_MSEC(RESET_ACK_FLUSH_MS));
	sys_reboot(SYS_REBOOT_COLD);
	return -EASNFW_STATUS__INTERFW_LINK_NOT_IMPLEMENTED;
}
