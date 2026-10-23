#ifndef EASNFW_SELFTEST_H
#define EASNFW_SELFTEST_H

/** Bit assignments stored in the NVS self-test failure mask. */
enum easnfw_selftest_failure {
	EASNFW_SELFTEST_FAILURE_NVS = 1U << 0,
	EASNFW_SELFTEST_FAILURE_MASS_STORAGE = 1U << 1,
	EASNFW_SELFTEST_FAILURE_AUDIO = 1U << 2,
	EASNFW_SELFTEST_FAILURE_ENVIRONMENT = 1U << 3,
	EASNFW_SELFTEST_FAILURE_IFL = 1U << 4,
	EASNFW_SELFTEST_FAILURE_CLOUD = 1U << 5,
};

/**
 * @brief Run every self-test and persist the failure mask when NVS is usable.
 *
 * @return 0 if all checks pass, otherwise the first negative EASNFW status.
 */
int selftest__run(void);

/** @brief Verify I2S microphone capture by acquiring and releasing one block. */
int selftest__audio(void);

/** @brief Verify the environmental sensor can return all required channels. */
int selftest__environment(void);

/** @brief Verify that EASNFW-SENSOR can exchange an acknowledged IFL command. */
int selftest__interfw_link(void);

/** @brief Verify cloud connectivity when the backend protocol is available. */
int selftest__cloud(void);

/**
 * @brief Run the NVS check of the self-test sequence.
 *
 * @return 0 if NVS is functional, a negative error code from enum
 * selftest_status otherwise.
 */
int selftest__nvs(void);

/**
 * @brief Run the mass-storage check of the self-test sequence.
 *
 * @return 0 if mass storage is functional, a negative error code from enum
 * selftest_status otherwise.
 */
int selftest__mass_storage(void);

#endif