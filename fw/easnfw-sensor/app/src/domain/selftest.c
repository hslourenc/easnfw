#include <zephyr/logging/log.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(easnfw_selftest, LOG_LEVEL_INF);

int easnfw_selftest_run(void)
{
	/* EASNFW_TODO: Aggregate NVS, storage, sensors, SPI, and cloud checks. */
	LOG_INF("Sensor self-test placeholder passed");
	return 0;
}