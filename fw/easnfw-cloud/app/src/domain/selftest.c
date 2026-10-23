#include <errno.h>

#include <zephyr/logging/log.h>
#include <easnfw/lte_m.h>
#include <easnfw/retry.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(easnfw_selftest, LOG_LEVEL_INF);

/**
 * @brief Exercise cloud-owned startup checks without faking backend access.
 *
 * The inter-firmware link handshake is completed before this function is
 * called. This check validates retry ceilings and reports LTE-M/backend access
 * as unavailable until those contracts and drivers are implemented.
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
	if (lte_m_connect() != 0) {
		LOG_ERR("Cloud communication self-test unavailable; backend "
			"transport is not implemented");
		return -ENOTSUP;
	}

	/* EASNFW_REVISIT: verify authenticated HTTPS and durable read-back. */
	return -ENOTSUP;
}
