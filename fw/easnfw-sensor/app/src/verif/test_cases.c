#include <zephyr/logging/log.h>
#include <easnfw/config.h>
#include <easnfw/selftest.h>
#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(test_cases, LOG_LEVEL_INF);

void app_test_cases__set_selftest_nvs_fault(void);
void app_test_cases__set_selftest_mass_storage_fault(void);
void app_test_cases__clear_all_faults(void);

struct injected_faults faults;

void app_test_cases__tc001(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	int err = selftest__nvs();
	if (err < 0) {
		LOG_ERR("TCSTATUS_FAIL: self-test NVS check failed, error %d", err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: self-test NVS check passed");
	}
}

void app_test_cases__tc002(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_nvs_fault();
	int err = selftest__nvs();
	if (err != -EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH) {
		LOG_ERR("TCSTATUS_FAIL: self-test NVS check did not fail with error %d and returned %d instead",
			-EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH, err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: self-test NVS failed with error %d as expected",
			-EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH);
	}
}

void app_test_cases__tc003(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	int err = selftest__mass_storage();
	if (err < 0) {
		LOG_ERR("TCSTATUS_FAIL: mass-storage check failed, error %d", err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: mass-storage check passed");
	}
}

void app_test_cases__tc004(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_mass_storage_fault();
	int err = selftest__mass_storage();
	if (err != -EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED) {
		LOG_ERR("TCSTATUS_FAIL: mass-storage check did not fail with error %d and returned %d instead",
			-EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED, err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: mass-storage check failed with error %d as expected",
			-EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED);
	}
}

void app_test_cases__set_selftest_nvs_fault(void)
{
	faults.selftest_nvs_fault = 1;
}

void app_test_cases__set_selftest_mass_storage_fault(void)
{
	faults.selftest_mass_storage_fault = 1;
}

void app_test_cases__clear_all_faults(void)
{
	faults = (struct injected_faults){0};
}

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */