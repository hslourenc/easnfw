#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <easnfw/config.h>
#include <easnfw/audio_sampling.h>
#include <easnfw/env_sampling.h>
#include <easnfw/interfw_link.h>
#include <easnfw/mass_storage.h>
#include <easnfw/nvs.h>
#include <easnfw/selftest.h>
#include <easnfw/status.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include "../verif/test_cases.h"
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

LOG_MODULE_REGISTER(selftest, LOG_LEVEL_INF);

int selftest__run(void)
{
	static const struct {
		const char *name;
		uint8_t failure_bit;
		int (*run)(void);
	} checks[] = {
		{"NVS", EASNFW_SELFTEST_FAILURE_NVS, selftest__nvs},
		{"mass storage", EASNFW_SELFTEST_FAILURE_MASS_STORAGE,
		 selftest__mass_storage},
		{"audio", EASNFW_SELFTEST_FAILURE_AUDIO, selftest__audio},
		{"environment", EASNFW_SELFTEST_FAILURE_ENVIRONMENT,
		 selftest__environment},
		{"inter-FW link", EASNFW_SELFTEST_FAILURE_IFL,
		 selftest__interfw_link},
		{"cloud platform", EASNFW_SELFTEST_FAILURE_CLOUD, selftest__cloud},
	};
	int first_error = 0;
	uint8_t failure_mask = 0U;
	bool nvs_operational = false;

	for (size_t i = 0U; i < ARRAY_SIZE(checks); ++i) {
		const int err = checks[i].run();

		if (err == 0) {
			LOG_INF("Self-test %s: PASS", checks[i].name);
			if (i == 0U) {
				nvs_operational = true;
			}
		} else {
			LOG_ERR("Self-test %s: FAIL (%d)", checks[i].name, err);
			failure_mask |= checks[i].failure_bit;
			if (first_error == 0) {
				first_error = err;
			}
		}
	}

	if (nvs_operational && failure_mask != 0U) {
		const int err = app_nvs__write(NVS_ENTRY_ID__SELFTEST_FAILURES,
					       &failure_mask,
					       sizeof(failure_mask));

		if (err != 0) {
			LOG_ERR("Could not persist self-test failure mask: %d", err);
			if (first_error == 0) {
				first_error =
					-EASNFW_STATUS__SELFTEST_NVS_WR_FAILED;
			}
		}
	}
	return first_error;
}

int selftest__nvs(void)
{
	int err = app_nvs__init();
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS init failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_NVS_INIT_FAILED;
	}

	int write[] = {0, 1};
	err = app_nvs__write(NVS_ENTRY_ID__DUMMY, write, sizeof(write));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_NVS_WR_FAILED;
	}

	int read[] = {-1,-1};
	err = app_nvs__read(NVS_ENTRY_ID__DUMMY, read, sizeof(read));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_NVS_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH;
	}

	++write[0];
	++write[1];
	err = app_nvs__write(NVS_ENTRY_ID__DUMMY, write, sizeof(write));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS write failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_NVS_WR_FAILED;
	}

	err = app_nvs__read(NVS_ENTRY_ID__DUMMY, read, sizeof(read));
	if (err < 0) {
		LOG_ERR("NVS check failed: NVS read failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_NVS_RD_FAILED;
	}
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_nvs_fault != 0) {
		++read[0];
	}
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("NVS check failed: read data does not match written data.");
		return -EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH;
	}

	return 0;
}

int selftest__mass_storage(void)
{
	int err = app_mass_storage__init();
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: init failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_INIT_FAILED;
	}

	int write[] = {0, 1};
	int read[] = {-1, -1};
	err = app_mass_storage__write(write, sizeof(write));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: write failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED;
	}

	err = app_mass_storage__read(read, sizeof(read));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: read failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("Mass-storage check failed: read data does not match written data");
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_WR_NO_MATCH;
	}

	++write[0];
	++write[1];
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_mass_storage_fault != 0) {
		LOG_ERR("Mass-storage check failed: injected write failure");
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED;
	}
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
	err = app_mass_storage__write(write, sizeof(write));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: write failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED;
	}

	err = app_mass_storage__read(read, sizeof(read));
	if (err < 0) {
		LOG_ERR("Mass-storage check failed: read failed, error %d", err);
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_FAILED;
	}
	if (memcmp(read, write, sizeof(write)) != 0) {
		LOG_ERR("Mass-storage check failed: read data does not match written data");
		return -EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_WR_NO_MATCH;
	}

	return 0;
}

/** Check that a full I2S block can be acquired and the peripheral stopped. */
int selftest__audio(void)
{
	struct easnfw_audio_block block;

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_audio_fault != 0) {
		return -EASNFW_STATUS__SELFTEST_AUDIO_ACQUISITION_FAILED;
	}
#endif
	int err = audio_sampling_start();

	if (err != 0) {
		LOG_ERR("Audio check failed: I2S start failed (%d)", err);
		return -EASNFW_STATUS__SELFTEST_AUDIO_INIT_FAILED;
	}
	err = audio_sampling_acquire(&block);
	{
		const int stop_err = audio_sampling_stop();

		if (err == 0 && stop_err != 0) {
			err = stop_err;
		}
	}
	if (err != 0 || block.sample_count != EASNFW_AUDIO_BLOCK_SAMPLES) {
		LOG_ERR("Audio check failed: block acquisition returned %d", err);
		return -EASNFW_STATUS__SELFTEST_AUDIO_ACQUISITION_FAILED;
	}
	return 0;
}

/** Check that temperature, pressure, humidity, and gas resistance are read. */
int selftest__environment(void)
{
	struct easnfw_environment_sample sample;

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_environment_fault != 0) {
		return -EASNFW_STATUS__SELFTEST_ENVIRONMENT_ACQUISITION_FAILED;
	}
#endif
	const int err = env_sampling_acquire(&sample);

	if (err != 0) {
		LOG_ERR("Environmental check failed: sample returned %d", err);
		return -EASNFW_STATUS__SELFTEST_ENVIRONMENT_ACQUISITION_FAILED;
	}
	return 0;
}

/** Check that the CLOUD processor acknowledges a PING command. */
int selftest__interfw_link(void)
{
	int err = ifl__init();

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_ifl_fault != 0) {
		return -EASNFW_STATUS__SELFTEST_IFL_FAILED;
	}
#endif
	if (err == 0) {
		err = ifl__ping_tx();
	}
	if (err != 0) {
		LOG_ERR("Inter-FW link check failed: PING returned %d", err);
		return -EASNFW_STATUS__SELFTEST_IFL_FAILED;
	}
	return 0;
}

/**
 * The cloud ingestion endpoint and authentication contract are not specified.
 * Do not report success until a durable backend acknowledgement is defined.
 */
int selftest__cloud(void)
{
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	if (faults.selftest_cloud_fault != 0) {
		return -EASNFW_STATUS__SELFTEST_CLOUD_UNAVAILABLE;
	}
#endif
	LOG_WRN("EASNFW_REVISIT: cloud-platform self-test awaits backend contract");
	return -EASNFW_STATUS__SELFTEST_CLOUD_UNAVAILABLE;
}