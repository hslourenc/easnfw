#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <easnfw/interfw_link.h>
#include <easnfw/reset.h>
#include <easnfw/status.h>

LOG_MODULE_REGISTER(easnfw_reset, LOG_LEVEL_INF);

/* Set when EASNFW-CLOUD has completed the requested reset. */
static bool easnfwcloud_ready;
static struct k_sem easnfwcloud_ready_sem;
static bool reset_sync_initialized;

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
	sys_reboot(SYS_REBOOT_COLD);
	return -EASNFW_STATUS__INTERFW_LINK_NOT_IMPLEMENTED;
}
