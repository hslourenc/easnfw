void app_test_cases__set_selftest_nvs_fault(void);
void app_test_cases__clear_all_faults(void);

// maybe it is better if easnfw resets before every test case

void app_test_cases__tc001(void)
{
	app_test_cases__clear_all_faults();
	int err = app_selftest__nvs();
	if (err < 0) {
		LOG_ERR("TCSTATUS_FAIL: self-test NVS check failed, error %d", err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: self-test NVS check passed");
	}
}

void app_test_cases__tc002(void)
{
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_nvs_fault();
	int err = app_selftest__nvs();
	if (err != APP_STATUS__ST_NVS_RD_WR_NO_MATCH) {
		LOG_ERR("TCSTATUS_FAIL: self-test NVS check did not fail with error %d and returned %d instead",
			APP_STATUS__ST_NVS_RD_WR_NO_MATCH, err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS: self-test NVS failed with error %d as expected",
			APP_STATUS__ST_NVS_RD_WR_NO_MATCH);
	}
}

void app_test_cases__set_selftest_nvs_fault(void)
{
	injected_faults.selftest_nvs_fault = 1;
}

void app_test_cases__clear_all_faults(void)
{
	injected_faults = (struct faults){0};
}