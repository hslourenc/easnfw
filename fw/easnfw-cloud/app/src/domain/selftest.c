#include <errno.h>

#include <zephyr/logging/log.h>
#include <easnfw/retry.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(easnfw_selftest, LOG_LEVEL_INF);

/**
 * @brief Exercise cloud-owned startup checks without faking backend access.
 *
 * The inter-firmware link handshake is completed before this function is
 * called. This check validates retry ceilings. LTE-M attachment belongs to the
 * connectivity cycle, not startup self-test, so it cannot block service startup.
 *
 * @return 0 when checks are operational, or a negative errno in degraded mode.
 */
int easnfw_selftest_run(void)
{
	if (easnfw_retry_delay_seconds(0u) != 1u ||
	    easnfw_retry_delay_seconds(4u) != 16u ||
	    easnfw_retry_delay_seconds(5u) != 30u ||
	    easnfw_retry_delay_seconds(10u) != 30u) {
		LOG_ERR("Retry backoff self-test failed");
		return -EIO;
	}

	LOG_INF("Inter-firmware UART handshake self-test passed");
	/* EASNFW_REVISIT: verify authenticated HTTPS and durable read-back. */
	LOG_WRN("Cloud communication self-test unavailable; backend "
		"transport is not implemented");
	return -ENOTSUP;
}
