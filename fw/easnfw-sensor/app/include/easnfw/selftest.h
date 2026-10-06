#ifndef EASNFW_SELFTEST_H
#define EASNFW_SELFTEST_H

enum selftest_status {
	SELFTEST_STATUS__NVS_INIT_FAILED     = 1001,
	SELFTEST_STATUS__NVS_WR_FAILED       = 1002,
	SELFTEST_STATUS__NVS_RD_FAILED       = 1003,
	SELFTEST_STATUS__NVS_RD_WR_NO_MATCH  = 1004,
	SELFTEST_STATUS__MASS_STORAGE_INIT_FAILED = 1101,
	SELFTEST_STATUS__MASS_STORAGE_WR_FAILED   = 1102,
	SELFTEST_STATUS__MASS_STORAGE_RD_FAILED   = 1103,
	SELFTEST_STATUS__MASS_STORAGE_RD_WR_NO_MATCH = 1104,
};

/**
 * @brief Run the NVS check of the self-test sequence.
 *
 * @return 0 if NVS is functional, a negative error code from enum
 * selftest_status otherwise.
 */
int app_selftest__nvs(void);

/**
 * @brief Run the complete self-test sequence.
 *
 * EASNFW_TODO: update as app_selftest__run() implementation is developed.
 */
int app_selftest__run(void);

/**
 * @brief Run the mass-storage check of the self-test sequence.
 *
 * @return 0 if mass storage is functional, a negative error code from enum
 * selftest_status otherwise.
 */
int app_selftest__mass_storage(void);

#endif