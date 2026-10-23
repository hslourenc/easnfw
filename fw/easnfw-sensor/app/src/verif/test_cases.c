#include <errno.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <easnfw/config.h>
#include <easnfw/interfw_link.h>
#include <easnfw/mass_storage.h>
#include <easnfw/nvs.h>
#include <easnfw/pipeline.h>
#include <easnfw/selftest.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>

#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(test_cases, LOG_LEVEL_INF);
static uint32_t tc042_id_counter;

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
		LOG_ERR("TCSTATUS_FAIL - self-test NVS check failed, error %d", err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - self-test NVS check passed");
	}
}

void app_test_cases__tc002(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_nvs_fault();
	int err = selftest__nvs();
	if (err != -EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH) {
		LOG_ERR("TCSTATUS_FAIL - self-test NVS check did not fail with error %d and returned %d instead",
			-EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH, err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - self-test NVS failed with error %d as expected",
			-EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH);
	}
}

void app_test_cases__tc003(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	int err = selftest__mass_storage();
	if (err < 0) {
		LOG_ERR("TCSTATUS_FAIL - mass-storage check failed, error %d", err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - mass-storage check passed");
	}
}

void app_test_cases__tc004(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_mass_storage_fault();
	int err = selftest__mass_storage();
	if (err != -EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED) {
		LOG_ERR("TCSTATUS_FAIL - mass-storage check did not fail with error %d and returned %d instead",
			-EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED, err);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - mass-storage check failed with error %d as expected",
			-EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED);
	}
}

/** Verify successful capture from the microphone in the configured format. */
void app_test_cases__tc005(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	err = selftest__audio();
	if (err == 0) {
		LOG_INF("TCSTATUS_SUCCESS - audio self-test acquired a PCM block");
	} else {
		LOG_ERR("TCSTATUS_FAIL - audio self-test returned %d", err);
	}
}

/** Verify the injected microphone failure is detected. */
void app_test_cases__tc006(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.selftest_audio_fault = 1;
	err = selftest__audio();
	if (err == -EASNFW_STATUS__SELFTEST_AUDIO_ACQUISITION_FAILED) {
		LOG_INF("TCSTATUS_SUCCESS - injected audio failure was detected");
	} else {
		LOG_ERR("TCSTATUS_FAIL - audio failure returned %d", err);
	}
}

/** Verify the BME688-compatible sensor returns all tracked channels. */
void app_test_cases__tc007(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	err = selftest__environment();
	if (err == 0) {
		LOG_INF("TCSTATUS_SUCCESS - environmental sensor sample acquired");
	} else {
		LOG_ERR("TCSTATUS_FAIL - environmental self-test returned %d", err);
	}
}

/** Verify the injected environmental-sensor failure is detected. */
void app_test_cases__tc008(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.selftest_environment_fault = 1;
	err = selftest__environment();
	if (err == -EASNFW_STATUS__SELFTEST_ENVIRONMENT_ACQUISITION_FAILED) {
		LOG_INF("TCSTATUS_SUCCESS - injected environmental failure was detected");
	} else {
		LOG_ERR("TCSTATUS_FAIL - environmental failure returned %d", err);
	}
}

/** Verify an acknowledged CLOUD PING over the inter-firmware UART link. */
void app_test_cases__tc009(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	err = selftest__interfw_link();
	if (err == 0) {
		LOG_INF("TCSTATUS_SUCCESS - CLOUD acknowledged IFL PING");
	} else {
		LOG_ERR("TCSTATUS_FAIL - IFL PING returned %d", err);
	}
}

/** Verify an unavailable inter-firmware link is classified as a failure. */
void app_test_cases__tc010(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.selftest_ifl_fault = 1;
	err = selftest__interfw_link();
	if (err == -EASNFW_STATUS__SELFTEST_IFL_FAILED) {
		LOG_INF("TCSTATUS_SUCCESS - injected IFL failure was detected");
	} else {
		LOG_ERR("TCSTATUS_FAIL - IFL failure returned %d", err);
	}
}

/** Report the cloud check as inconclusive until a backend contract exists. */
void app_test_cases__tc011(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	if (selftest__cloud() == -EASNFW_STATUS__SELFTEST_CLOUD_UNAVAILABLE) {
		LOG_WRN("TCSTATUS_INCONCLUSIVE - backend endpoint/authentication are TBD");
	} else {
		LOG_ERR("TCSTATUS_FAIL - cloud self-test returned an unexpected result");
	}
}

/** Verify that the cloud check does not report unavailable service as success. */
void app_test_cases__tc012(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.selftest_cloud_fault = 1;
	if (selftest__cloud() == -EASNFW_STATUS__SELFTEST_CLOUD_UNAVAILABLE) {
		LOG_INF("TCSTATUS_SUCCESS - unavailable cloud service was reported");
	} else {
		LOG_ERR("TCSTATUS_FAIL - unavailable cloud service was not reported");
	}
}

/** Execute all self-tests and expose backend-dependent results as inconclusive. */
void app_test_cases__tc013(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	err = selftest__run();
	if (err == 0) {
		LOG_INF("TCSTATUS_SUCCESS - all self-tests passed");
	} else if (err == -EASNFW_STATUS__SELFTEST_CLOUD_UNAVAILABLE) {
		LOG_WRN("TCSTATUS_INCONCLUSIVE - cloud backend self-test is not specified");
	} else {
		LOG_ERR("TCSTATUS_FAIL - one or more self-tests failed, first error %d",
			err);
	}
}

/** Verify that a non-NVS self-test failure is persisted to NVS. */
void app_test_cases__tc020(unsigned int test_case_step)
{
	uint8_t failure_mask = 0U;
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.selftest_audio_fault = 1;
	err = selftest__run();
	if (err != -EASNFW_STATUS__SELFTEST_AUDIO_ACQUISITION_FAILED) {
		LOG_ERR("TCSTATUS_FAIL - expected audio self-test failure, got %d",
			err);
		app_test_cases__clear_all_faults();
		return;
	}

	err = app_nvs__read(NVS_ENTRY_ID__SELFTEST_FAILURES,
			    &failure_mask, sizeof(failure_mask));
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - failed-check mask read from NVS: %d", err);
	} else if ((failure_mask & EASNFW_SELFTEST_FAILURE_AUDIO) == 0U) {
		LOG_ERR("TCSTATUS_FAIL - persisted mask 0x%02x omits audio failure",
			failure_mask);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - audio failure persisted in NVS mask 0x%02x",
			failure_mask);
	}
	app_test_cases__clear_all_faults();
}

/** Verify recovery quarantines a truncated temporary record. */
void app_test_cases__tc042(unsigned int test_case_step)
{
	static const char incomplete_record[] = "incomplete-record";
	char temporary_path[80];
	char committed_path[80];
	char quarantined_path[80];
	char record_id[33];
	struct fs_file_t file;
	struct fs_dirent entry;
	int err;

	(void)test_case_step;
	err = app_mass_storage__init();
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - mass storage initialization failed: %d", err);
		return;
	}

	for (unsigned int attempt = 0U; attempt < 4U; ++attempt) {
		uint8_t id_bytes[16];
		uint32_t id_seed[4] = {
			(uint32_t)k_uptime_get(),
			k_cycle_get_32(),
			++tc042_id_counter,
			0x54433042U,
		};

		memcpy(id_bytes, id_seed, sizeof(id_bytes));
		for (size_t i = 0U; i < 16U; ++i) {
			snprintk(&record_id[i * 2U], sizeof(record_id) - i * 2U,
				 "%02x", id_bytes[i]);
		}
		record_id[sizeof(record_id) - 1U] = '\0';
		snprintk(temporary_path, sizeof(temporary_path),
			 "/SD:/records/%s.tmp", record_id);
		snprintk(committed_path, sizeof(committed_path),
			 "/SD:/records/%s.rec", record_id);
		snprintk(quarantined_path, sizeof(quarantined_path),
			 "/SD:/records/%s.bad", record_id);
		if (fs_stat(temporary_path, &entry) == -ENOENT &&
		    fs_stat(committed_path, &entry) == -ENOENT &&
		    fs_stat(quarantined_path, &entry) == -ENOENT) {
			break;
		}
		if (attempt == 3U) {
			LOG_ERR("TCSTATUS_FAIL - could not allocate unique test record ID");
			return;
		}
	}

	fs_file_t_init(&file);
	err = fs_open(&file, temporary_path, FS_O_CREATE | FS_O_WRITE);
	if (err == 0) {
		ssize_t written = fs_write(&file, incomplete_record,
					   sizeof(incomplete_record) - 1U);

		err = written == (ssize_t)(sizeof(incomplete_record) - 1U)
			? fs_sync(&file) : (written < 0 ? (int)written : -EIO);
		if (fs_close(&file) != 0 && err == 0) {
			err = -EIO;
		}
	}
	if (err == 0) {
		err = app_mass_storage__recover_records();
	}
	if (err == 0) {
		err = fs_stat(quarantined_path, &entry);
	}
	if (err == 0 && entry.type == FS_DIR_ENTRY_FILE &&
	    fs_stat(temporary_path, &entry) == -ENOENT &&
	    fs_stat(committed_path, &entry) == -ENOENT) {
		LOG_INF("TCSTATUS_SUCCESS - truncated temporary record quarantined");
	} else {
		if (err == 0) {
			err = -EIO;
		}
		LOG_ERR("TCSTATUS_FAIL - recovery did not quarantine incomplete record: %d",
			err);
	}

	char const *const test_paths[] = {
		temporary_path,
		committed_path,
		quarantined_path,
	};

	for (size_t i = 0U; i < ARRAY_SIZE(test_paths); ++i) {
		int cleanup_err = fs_unlink(test_paths[i]);

		if (cleanup_err != 0 && cleanup_err != -ENOENT) {
			LOG_WRN("Could not remove TC-042 test file %s: %d",
				test_paths[i], cleanup_err);
		}
	}
}

/** Verify bounded pipeline allocation fails cleanly at pool exhaustion. */
void app_test_cases__tc046(unsigned int test_case_step)
{
	struct easnfw_pipeline_message *messages[EASNFW_PIPELINE_POOL_BLOCKS];
	struct easnfw_pipeline_message *extra = NULL;
	size_t allocated = 0U;
	int err = 0;

	(void)test_case_step;
	for (; allocated < ARRAY_SIZE(messages); ++allocated) {
		err = easnfw_pipeline_message_alloc(&messages[allocated]);
		if (err != 0) {
			break;
		}
	}
	if (err == 0) {
		err = easnfw_pipeline_message_alloc(&extra);
		if (err != -ENOMEM) {
			LOG_ERR("TCSTATUS_FAIL - pool exhaustion returned %d, expected %d",
				err, -ENOMEM);
		} else {
			err = 0;
		}
	} else {
		LOG_ERR("TCSTATUS_FAIL - could allocate only %u of %u pool blocks: %d",
			(unsigned int)allocated,
			(unsigned int)ARRAY_SIZE(messages), err);
	}
	while (allocated > 0U) {
		easnfw_pipeline_message_free(messages[--allocated]);
	}
	if (err == 0) {
		err = easnfw_pipeline_message_alloc(&extra);
		if (err == 0) {
			easnfw_pipeline_message_free(extra);
			LOG_INF("TCSTATUS_SUCCESS - pool exhaustion was bounded and released blocks reusable");
		} else {
			LOG_ERR("TCSTATUS_FAIL - released pool block could not be reused: %d",
				err);
		}
	}
}

/** Verify TC-048's UTC prerequisite and synchronization path over IFL. */
void app_test_cases__tc048(unsigned int test_case_step)
{
	char timestamp[EASNFW_UTC_TIMESTAMP_SIZE];
	int64_t before_ms;
	int64_t after_ms;
	int err;

	(void)test_case_step;
	err = app_utc_time__now_ms(&before_ms);
	if (err != -ENODATA) {
		LOG_ERR("TCSTATUS_FAIL - unsynchronized UTC was accessible: %d", err);
		return;
	}
	err = ifl__utc_time_request_tx();
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - UTC request was not acknowledged: %d", err);
		return;
	}
	err = app_utc_time__wait(K_SECONDS(70));
	if (err != 0) {
		LOG_WRN("TCSTATUS_INCONCLUSIVE - cellular UTC was unavailable: %d",
			err);
		return;
	}
	err = app_utc_time__now_ms(&before_ms);
	if (err == 0) {
		k_sleep(K_MSEC(100));
		err = app_utc_time__now_ms(&after_ms);
	}
	if (err == 0) {
		err = app_utc_time__format(after_ms, timestamp, sizeof(timestamp));
	}
	if (err != 0 || after_ms < before_ms ||
	    timestamp[sizeof(timestamp) - 2U] != 'Z') {
		LOG_ERR("TCSTATUS_FAIL - synchronized UTC was invalid: %d", err);
		return;
	}
	LOG_INF("TCSTATUS_SUCCESS - UTC synchronized and formatted as %s",
		timestamp);
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
	ifl__verification_ping_override_set(false);
}

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */