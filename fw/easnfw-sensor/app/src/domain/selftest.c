#include <zephyr/logging/log.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(selftest, LOG_LEVEL_INF);

int app_selftest__run(void)
{
	/* EASNFW_TODO: Aggregate NVS, storage, sensors, SPI, and cloud checks. */
	LOG_INF("Sensor self-test placeholder passed");
	return 0;
}

int app_selftest__nvs(void)
{
	int err = 0;
	if (!app_nvs_initialized) {
		err = app_nvs__init();
	}
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS init failed, error %d", err);
		return -APP_STATUS__ST_NVS_INIT_FAILED;
	}

	int nvs_wr_data = 0;
	err = app_nvs__write_dummy(nvs_wr_data);
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -APP_STATUS__ST_NVS_WR_FAILED;
	}

	int nvs_rd_data = -1;
	err = app_nvs__read_dummy(&nvs_rd_data);
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -APP_STATUS__ST_NVS_RD_FAILED;
	}
	if (nvs_rd_data != nvs_wr_data) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -APP_STATUS__ST_NVS_RD_WR_NO_MATCH;
	}

	err = app_nvs__write_dummy(++nvs_wr_data);
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -APP_STATUS__ST_NVS_WR_FAILED;
	}

	err = app_nvs__read_dummy(&nvs_rd_data);
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -APP_STATUS__ST_NVS_RD_FAILED;
	}
#if FAULT_INJECTTION
	if (injected_faults.selftest_nvs_fault != 0) {
		nvd_rd_data = ++nvs_wr_data;
	}
#endif /* FAULT_INJECTTION */
	if (nvs_rd_data != nvs_wr_data) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -APP_STATUS__ST_NVS_RD_WR_NO_MATCH;
	}

	return 0;
}