#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#ifdef CONFIG_BOOTLOADER_MCUBOOT
#include <zephyr/dfu/mcuboot.h>
#endif

#include <easnfw/config.h>
#include <easnfw/audio_processing.h>
#include <easnfw/cloud_link.h>
#include <easnfw/interfw_link.h>
#include <easnfw/pipeline.h>
#include <easnfw/reset.h>
#include <easnfw/sampling.h>
#include <easnfw/selftest.h>
#include <easnfw/storage.h>
#include <easnfw/time_sync.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include "verif/cli.h"
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

LOG_MODULE_REGISTER(easnfw_sensor, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("EASNFW-SENSOR application code under execution");

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	LOG_INF("CLI_H starting");
	cli__wait_for_shell_ready();
#else
	int err;
	bool selftest_ok = true;

	LOG_INF("EASNFW-SENSOR starting");
	err = reset__capture_startup_reason();
	if (err != 0) {
		LOG_ERR("Could not capture SENSOR reset cause: %d", err);
	}
	err = selftest__run();
	if (err != 0) {
		selftest_ok = false;
		reset__diagnostic_state(err, NULL);
	}
	reset__start_recovery_counter_timer();

	while (!app_utc_time__is_valid()) {
		err = ifl__utc_time_request_tx();
		if (err != 0) {
			LOG_WRN("UTC request failed (%d); acquisition remains blocked",
				err);
		}
		err = app_utc_time__wait(K_SECONDS(30));
		if (err != 0) {
			LOG_WRN("UTC is still unavailable; retrying in %u seconds",
				CONFIG_EASNFW_UTC_SYNC_RETRY_SECONDS);
			k_sleep(K_SECONDS(CONFIG_EASNFW_UTC_SYNC_RETRY_SECONDS));
		}
	}
	err = reset__history_update();
	if (err != 0) {
		LOG_ERR("Reset history update failed: %d", err);
	}
#ifdef CONFIG_BOOTLOADER_MCUBOOT
	if (selftest_ok && err == 0) {
		err = boot_write_img_confirmed();
		if (err != 0) {
			LOG_ERR("Could not confirm MCUboot image: %d", err);
		}
	}
#endif
	sampling_start();
	audio_processing_start();
	storage_start();
	cloud_link_start();
	LOG_INF("EASNFW-SENSOR ready");
	return 0;
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
}
