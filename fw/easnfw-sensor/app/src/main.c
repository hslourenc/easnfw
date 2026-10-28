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
#ifdef CONFIG_BOOTLOADER_MCUBOOT
	/*
	 * The self-test sequence includes the IFL PING handshake with
	 * EASNFW-CLOUD. A new image is confirmed only when it passes, so a
	 * reboot reverts an image that cannot reach its peer. Cellular UTC is
	 * deliberately not part of the criterion.
	 */
	if (selftest_ok) {
		err = boot_write_img_confirmed();
		if (err != 0) {
			LOG_ERR("Could not confirm MCUboot image: %d", err);
		}
	} else {
		LOG_ERR("Self-test failed; MCUboot image left unconfirmed");
	}
#endif

	(void)app_utc_time__acquire(K_SECONDS(30),
				    CONFIG_EASNFW_UTC_SYNC_RETRY_SECONDS,
				    K_FOREVER, NULL);
	err = reset__history_update();
	if (err != 0) {
		LOG_ERR("Reset history update failed: %d", err);
	}
	sampling_start();
	audio_processing_start();
	storage_start();
	cloud_link_start();
	LOG_INF("EASNFW-SENSOR ready");
	return 0;
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
}
