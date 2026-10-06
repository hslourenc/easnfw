#include <string.h>

#include <zephyr/logging/log.h>
#include <easnfw/config.h>
#include <easnfw/platform.h>
#include <easnfw/selftest.h>

#if CONFIG_EASNFW_VERIFICATION_CODE
#include "../verif/test_cases.h"
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

LOG_MODULE_REGISTER(selftest, LOG_LEVEL_INF);

int app_selftest__run(void)
{
	/* EASNFW_TODO: Aggregate sensors, SPI, and cloud checks.
	   Need a struct to aggregate results later. */
	app_selftest__nvs();
	app_selftest__mass_storage();
	LOG_INF("All tests passed");
	return 0;
}

int app_selftest__nvs(void)
{
	int err = app_nvs__init();
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS init failed, error %d", err);
		return -SELFTEST_STATUS__NVS_INIT_FAILED;
	}

	int write[] = {0, 1};
	err = app_nvs__write(NVS_ENTRY_ID__DUMMY, write, sizeof(write));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -SELFTEST_STATUS__NVS_WR_FAILED;
	}

	int read[] = {-1,-1};
	err = app_nvs__read(NVS_ENTRY_ID__DUMMY, read, sizeof(read));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -SELFTEST_STATUS__NVS_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -SELFTEST_STATUS__NVS_RD_WR_NO_MATCH;
	}

	++write[0];
	++write[1];
	err = app_nvs__write(NVS_ENTRY_ID__DUMMY, write, sizeof(write));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -SELFTEST_STATUS__NVS_WR_FAILED;
	}

	err = app_nvs__read(NVS_ENTRY_ID__DUMMY, read, sizeof(read));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -SELFTEST_STATUS__NVS_RD_FAILED;
	}
#if CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_nvs_fault != 0) {
		read[0] = ++write[0];
		read[1] = ++write[1];
	}
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -SELFTEST_STATUS__NVS_RD_WR_NO_MATCH;
	}

	return 0;
}

int app_selftest__mass_storage(void)
{
	int err = app_mass_storage__init();
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: init failed, error %d", err);
		return -SELFTEST_STATUS__MASS_STORAGE_INIT_FAILED;
	}

	int write[] = {0, 1};
	int read[] = {-1, -1};
	err = app_mass_storage__write(write, sizeof(write));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: write failed, error %d", err);
		return -SELFTEST_STATUS__MASS_STORAGE_WR_FAILED;
	}

	err = app_mass_storage__read(read, sizeof(read));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: read failed, error %d", err);
		return -SELFTEST_STATUS__MASS_STORAGE_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("Mass-storage check failed: read data does not match written data");
		return -SELFTEST_STATUS__MASS_STORAGE_RD_WR_NO_MATCH;
	}

	++write[0];
	++write[1];
#if CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_mass_storage_fault != 0) {
		LOG_ERR("Mass-storage check failed: injected write failure");
		return -SELFTEST_STATUS__MASS_STORAGE_WR_FAILED;
	}
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
	err = app_mass_storage__write(write, sizeof(write));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: write failed, error %d", err);
		return -SELFTEST_STATUS__MASS_STORAGE_WR_FAILED;
	}

	err = app_mass_storage__read(read, sizeof(read));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: read failed, error %d", err);
		return -SELFTEST_STATUS__MASS_STORAGE_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("Mass-storage check failed: read data does not match written data");
		return -SELFTEST_STATUS__MASS_STORAGE_RD_WR_NO_MATCH;
	}

	return 0;
}