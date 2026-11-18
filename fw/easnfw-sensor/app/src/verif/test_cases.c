#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/printk.h>
#include <easnfw/cloud_link.h>
#include <easnfw/config.h>
#include <easnfw/interfw_link.h>
#include <easnfw/mass_storage.h>
#include <easnfw/nvs.h>
#include <easnfw/pipeline.h>
#include <easnfw/reset.h>
#include <easnfw/selftest.h>
#include <easnfw/status.h>
#include <easnfw/time_sync.h>

#include "test_cases.h"
#include "cli.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(test_cases, LOG_LEVEL_INF);
static uint32_t tc042_id_counter;
static uint32_t tc041_id_counter;

struct tc062_record_counts {
	unsigned int committed;
	unsigned int temporary;
	unsigned int quarantined;
};

static struct tc062_record_counts tc062_initial_counts;
static bool tc062_initial_counts_valid;

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
	} else if (faults.failure_mask_write_attempts != 1U) {
		LOG_ERR("TCSTATUS_FAIL - expected one failure-mask write, observed %u",
			faults.failure_mask_write_attempts);
	} else if ((failure_mask & EASNFW_SELFTEST_FAILURE_AUDIO) == 0U) {
		LOG_ERR("TCSTATUS_FAIL - persisted mask 0x%02x omits audio failure",
			failure_mask);
	} else {
		LOG_INF("TCSTATUS_SUCCESS - audio failure persisted in NVS mask 0x%02x",
			failure_mask);
	}
	app_test_cases__clear_all_faults();
}

/** Verify no failure-mask write is attempted when the NVS self-test fails. */
void app_test_cases__tc022(unsigned int test_case_step)
{
	(void)test_case_step;
	app_test_cases__clear_all_faults();
	app_test_cases__set_selftest_nvs_fault();
	cli__run_production_startup(22U, K_SECONDS(60));
}

/** Observe two production sampling cycles with a short HIL interval. */
static void run_sampling_observation(unsigned int test_case)
{
	int err;

	app_test_cases__clear_all_faults();
	faults.ignore_cloud_unavailable = 1;
	faults.short_sampling_cycle = 1;
	faults.verify_record_metadata = test_case == 32U;
	err = app_utc_time__set(946684800000ULL);
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not set synthetic test UTC: %d", err);
		return;
	}
	cli__run_production_startup(test_case, K_SECONDS(60));
}

void app_test_cases__tc025(unsigned int test_case_step)
{
	(void)test_case_step;
	run_sampling_observation(25U);
}

void app_test_cases__tc026(unsigned int test_case_step)
{
	(void)test_case_step;
	run_sampling_observation(26U);
}

void app_test_cases__tc032(unsigned int test_case_step)
{
	(void)test_case_step;
	run_sampling_observation(32U);
}

static int tc062_record_counts_get(struct tc062_record_counts *counts)
{
	struct fs_dir_t directory;
	struct fs_dirent entry;
	int err;

	if (counts == NULL) {
		return -EINVAL;
	}
	*counts = (struct tc062_record_counts){0};
	fs_dir_t_init(&directory);
	err = fs_opendir(&directory, "/SD:/records");
	if (err != 0) {
		return err;
	}
	for (;;) {
		err = fs_readdir(&directory, &entry);
		if (err != 0 || entry.name[0] == '\0') {
			break;
		}
		if (entry.type != FS_DIR_ENTRY_FILE) {
			continue;
		}
		size_t const name_len = strlen(entry.name);

		if (name_len >= 4U &&
		    strcmp(&entry.name[name_len - 4U], ".rec") == 0) {
			++counts->committed;
		} else if (name_len >= 4U &&
			   strcmp(&entry.name[name_len - 4U], ".tmp") == 0) {
			++counts->temporary;
		} else if (name_len >= 4U &&
			   strcmp(&entry.name[name_len - 4U], ".bad") == 0) {
			++counts->quarantined;
		}
	}
	int const close_err = fs_closedir(&directory);

	if (err != 0) {
		return err;
	}
	return close_err;
}

/** Verify an in-progress record is unavailable until metadata is committed. */
void app_test_cases__tc041(unsigned int test_case_step)
{
	static const int64_t start_utc_ms = 946684800000LL;
	struct app_mass_storage_record_metadata metadata = {
		.sample_rate_hz = CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ,
		.track_duration_seconds = 1U,
		.total_sample_count = 4U,
		.processing_algorithm_version = 1U,
		.channels = 1U,
		.bits_per_sample = 16U,
		.schema_version = 1U,
		.timestamp_source = APP_TIMESTAMP_SOURCE_CELLULAR_NETWORK,
	};
	struct app_mass_storage_record_metadata persisted;
	struct fs_dirent entry;
	int16_t samples[4] = {101, -202, 303, -404};
	uint32_t id_seed[4] = {
		sys_rand32_get(), sys_rand32_get(), ++tc041_id_counter,
		0x54433041U,
	};
	char record_id_hex[33] = {0};
	char temporary_path[80] = {0};
	char committed_path[80] = {0};
	uint32_t file_size;
	bool record_open = false;
	bool owns_paths = false;
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	memcpy(metadata.record_id, id_seed, sizeof(metadata.record_id));
	err = app_utc_time__format(start_utc_ms, metadata.environment_sample_utc,
				   sizeof(metadata.environment_sample_utc));
	if (err == 0) {
		err = app_utc_time__format(start_utc_ms,
					   metadata.audio_start_utc,
					   sizeof(metadata.audio_start_utc));
	}
	if (err == 0) {
		err = app_utc_time__format(start_utc_ms + 1000,
					   metadata.audio_end_utc,
					   sizeof(metadata.audio_end_utc));
	}
	if (err == 0) {
		err = app_mass_storage__init();
	}
	if (err != 0) {
		goto fail;
	}

	for (size_t i = 0U; i < sizeof(metadata.record_id); ++i) {
		snprintk(&record_id_hex[i * 2U],
			 sizeof(record_id_hex) - i * 2U,
			 "%02x", metadata.record_id[i]);
	}
	snprintk(committed_path, sizeof(committed_path),
		 "/SD:/records/%s.rec", record_id_hex);
	snprintk(temporary_path, sizeof(temporary_path),
		 "/SD:/records/%s.tmp", record_id_hex);
	err = fs_stat(temporary_path, &entry);
	if (err != -ENOENT) {
		err = err == 0 ? -EEXIST : err;
		goto fail;
	}
	err = fs_stat(committed_path, &entry);
	if (err != -ENOENT) {
		err = err == 0 ? -EEXIST : err;
		goto fail;
	}
	owns_paths = true;

	err = app_mass_storage__record_begin(&metadata);
	if (err != 0) {
		goto fail;
	}
	record_open = true;
	err = app_mass_storage__record_append(samples, sizeof(samples) / 2U);
	if (err != 0) {
		goto fail;
	}
	err = fs_stat(temporary_path, &entry);
	if (err != 0 || entry.type != FS_DIR_ENTRY_FILE) {
		err = err != 0 ? err : -EBADMSG;
		goto fail;
	}
	err = fs_stat(committed_path, &entry);
	if (err != -ENOENT) {
		err = err == 0 ? -EBADMSG : err;
		goto fail;
	}
	err = app_mass_storage__record_read_open(metadata.record_id, &file_size);
	if (err != -ENOENT) {
		if (err == 0) {
			app_mass_storage__record_read_close();
			err = -EBADMSG;
		}
		goto fail;
	}

	err = app_mass_storage__record_append(
		(uint8_t *)samples + sizeof(samples) / 2U,
		sizeof(samples) / 2U);
	if (err != 0) {
		goto fail;
	}
	err = app_mass_storage__record_commit(&metadata);
	record_open = false;
	if (err != 0) {
		goto fail;
	}
	err = fs_stat(committed_path, &entry);
	if (err != 0 || entry.type != FS_DIR_ENTRY_FILE) {
		err = err != 0 ? err : -EBADMSG;
		goto fail;
	}
	err = fs_stat(temporary_path, &entry);
	if (err != -ENOENT) {
		err = err == 0 ? -EBADMSG : err;
		goto fail;
	}
	err = app_mass_storage__record_read_open(metadata.record_id, &file_size);
	if (err != 0) {
		goto fail;
	}
	app_mass_storage__record_read_close();
	err = app_mass_storage__verification_read_last_metadata(
		metadata.record_id, &persisted);
	if (err == 0 &&
	    (memcmp(metadata.record_id, persisted.record_id,
		    sizeof(metadata.record_id)) != 0 ||
	     metadata.sample_rate_hz != persisted.sample_rate_hz ||
	     metadata.track_duration_seconds != persisted.track_duration_seconds ||
	     metadata.temperature_milli_c != persisted.temperature_milli_c ||
	     metadata.pressure_pa != persisted.pressure_pa ||
	     metadata.humidity_milli_pct != persisted.humidity_milli_pct ||
	     metadata.gas_resistance_ohms != persisted.gas_resistance_ohms ||
	     metadata.total_sample_count != persisted.total_sample_count ||
	     metadata.processing_algorithm_version !=
		persisted.processing_algorithm_version ||
	     metadata.channels != persisted.channels ||
	     metadata.bits_per_sample != persisted.bits_per_sample ||
	     metadata.schema_version != persisted.schema_version ||
	     metadata.timestamp_source != persisted.timestamp_source ||
	     strcmp(metadata.audio_start_utc, persisted.audio_start_utc) != 0 ||
	     strcmp(metadata.audio_end_utc, persisted.audio_end_utc) != 0 ||
	     strcmp(metadata.environment_sample_utc,
		    persisted.environment_sample_utc) != 0 ||
	     file_size <= sizeof(metadata))) {
		err = -EBADMSG;
	}

fail:
	if (record_open) {
		int const abort_err = app_mass_storage__record_abort();

		if (err == 0) {
			err = abort_err;
		}
	}
	if (owns_paths && temporary_path[0] != '\0') {
		int const cleanup_err = fs_unlink(temporary_path);

		if (cleanup_err != 0 && cleanup_err != -ENOENT && err == 0) {
			err = cleanup_err;
		}
	}
	if (owns_paths && committed_path[0] != '\0') {
		int const cleanup_err = fs_unlink(committed_path);

		if (cleanup_err != 0 && cleanup_err != -ENOENT && err == 0) {
			err = cleanup_err;
		}
	}
	if (err == 0) {
		LOG_INF("TCSTATUS_SUCCESS - temporary record was ineligible until complete metadata was committed");
	} else {
		LOG_ERR("TCSTATUS_FAIL - record commit eligibility or metadata verification failed: %d",
			err);
	}
}

/** Verify a simulated storage shortage delays acquisition without deleting records. */
void app_test_cases__tc062(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	err = app_mass_storage__init();
	if (err == 0) {
		err = tc062_record_counts_get(&tc062_initial_counts);
	}
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not snapshot mass-storage records: %d",
			err);
		return;
	}
	tc062_initial_counts_valid = true;
	faults.ignore_cloud_unavailable = 1;
	faults.short_sampling_cycle = 1;
	faults.storage_space_shortage = 1;
	faults.storage_space_shortage_checks = 2U;
	err = app_utc_time__set(946684800000ULL);
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not set synthetic test UTC: %d", err);
		return;
	}
	cli__run_production_startup(62U, K_SECONDS(60));
}

void app_test_cases__tc062_observation_complete(void)
{
	struct tc062_record_counts counts;
	int err = tc062_initial_counts_valid
		? tc062_record_counts_get(&counts) : -EINVAL;

	if (err == 0 &&
	    (counts.committed != tc062_initial_counts.committed + 2U ||
	     counts.temporary != tc062_initial_counts.temporary ||
	     counts.quarantined != tc062_initial_counts.quarantined)) {
		err = -EIO;
	}
	if (err == 0) {
		LOG_INF("HIL_RECORD_PRESERVATION_PASSED committed=%u temporary=%u quarantined=%u",
			counts.committed, counts.temporary, counts.quarantined);
	} else {
		LOG_ERR("HIL_RECORD_PRESERVATION_FAILED error=%d", err);
	}
	tc062_initial_counts_valid = false;
}

/** Observe permanent runtime storage failure handling through diagnostic entry. */
void app_test_cases__tc063(unsigned int test_case_step)
{
	int err;

	(void)test_case_step;
	app_test_cases__clear_all_faults();
	faults.ignore_cloud_unavailable = 1;
	faults.short_sampling_cycle = 1;
	faults.storage_append_permanent_failures = 1U;
	err = app_utc_time__set(946684800000ULL);
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not set synthetic test UTC: %d", err);
		return;
	}
	cli__run_production_startup(63U, K_SECONDS(60));
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
			sys_rand32_get(),
			sys_rand32_get(),
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
			if (err == 0) {
				easnfw_pipeline_message_free(extra);
				err = -EIO;
			}
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

/** Duration of the simulated network outage in TC-048. */
#define TC048_OUTAGE_MS 5000
#define TC048_ACQUISITION_TIMEOUT_SECONDS 900

/**
 * Verify TC-048: UTC is inaccessible before synchronization, and acquisition
 * keeps retrying through a simulated network outage until UTC is restored.
 */
void app_test_cases__tc048(unsigned int test_case_step)
{
	char timestamp[EASNFW_UTC_TIMESTAMP_SIZE];
	int64_t before_ms;
	int64_t after_ms;
	int64_t start_ms;
	uint32_t attempts;
	int err;

	(void)test_case_step;
	err = reset__capture_startup_reason();
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not capture SENSOR reset cause: %d",
			err);
		return;
	}
	app_utc_time__verification_outage_start(TC048_OUTAGE_MS);
	err = app_utc_time__now_ms(&before_ms);
	if (err != -ENODATA) {
		LOG_ERR("TCSTATUS_FAIL - unsynchronized UTC was accessible: %d", err);
		return;
	}
	start_ms = k_uptime_get();
	err = app_utc_time__acquire(K_SECONDS(2), 1U,
				    K_SECONDS(TC048_ACQUISITION_TIMEOUT_SECONDS),
				    &attempts);
	if (err != 0) {
		LOG_WRN("TCSTATUS_INCONCLUSIVE - cellular UTC was unavailable: %d",
			err);
		return;
	}
	if (k_uptime_get() - start_ms < TC048_OUTAGE_MS || attempts < 2U) {
		LOG_ERR("TCSTATUS_FAIL - acquisition ended during the outage (%u requests)",
			attempts);
		return;
	}
	err = reset__verify_cloud_reason();
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - CLOUD reset cause was not persisted: %d", err);
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
	LOG_INF("TCSTATUS_SUCCESS - UTC restored after %u requests, formatted as %s",
		attempts, timestamp);
}

/** Number of 16-bit samples in the synthetic TC-043 record (spans 4 fragments). */
#define TC043_SAMPLE_COUNT 4000U

/**
 * Verify TC-043: commit a synthetic record spanning several IFL fragments and
 * stream it to EASNFW-CLOUD. The final ACK confirms that CLOUD reassembled the
 * blob and that its CRC-32 and record_id checks passed.
 */
void app_test_cases__tc043(unsigned int test_case_step)
{
	static const int64_t start_utc_ms = 946684800000LL;
	struct app_mass_storage_record_metadata metadata = {
		.sample_rate_hz = CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ,
		.track_duration_seconds = 1U,
		.total_sample_count = TC043_SAMPLE_COUNT,
		.channels = 1U,
		.bits_per_sample = 16U,
		.schema_version = 1U,
		.timestamp_source = APP_TIMESTAMP_SOURCE_CELLULAR_NETWORK,
	};
	static int16_t samples[TC043_SAMPLE_COUNT];
	uint32_t id_seed[4] = {
		sys_rand32_get(), sys_rand32_get(), ++tc042_id_counter,
		0x54433433U,
	};
	int err;

	(void)test_case_step;
	err = app_utc_time__format(start_utc_ms, metadata.audio_start_utc,
				   sizeof(metadata.audio_start_utc));
	if (err == 0) {
		err = app_utc_time__format(start_utc_ms + 1000,
					   metadata.audio_end_utc,
					   sizeof(metadata.audio_end_utc));
	}
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not prepare synthetic UTC metadata: %d",
			err);
		return;
	}
	memcpy(metadata.record_id, id_seed, sizeof(metadata.record_id));
	for (size_t i = 0U; i < ARRAY_SIZE(samples); ++i) {
		samples[i] = (int16_t)(i * 7U);
	}
	err = app_mass_storage__init();
	if (err == 0) {
		err = app_mass_storage__record_begin(&metadata);
	}
	if (err == 0) {
		err = app_mass_storage__record_append(samples, sizeof(samples));
	}
	if (err == 0) {
		err = app_mass_storage__record_commit(&metadata);
	}
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - could not commit the test record: %d", err);
		return;
	}
	err = cloud_link__record_send(metadata.record_id);
	if (err != 0) {
		LOG_ERR("TCSTATUS_FAIL - multi-frame record transfer failed: %d", err);
		return;
	}
	LOG_INF("TCSTATUS_SUCCESS - multi-frame record reassembled and verified by CLOUD");
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