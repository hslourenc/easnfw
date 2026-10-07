#ifndef EASNFW_SELFTEST_H
#define EASNFW_SELFTEST_H

/**
 * @brief Run the complete self-test sequence.
 *
 * EASNFW_TODO: update as selftest__run() implementation is developed.
 */
int selftest__run(void);

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