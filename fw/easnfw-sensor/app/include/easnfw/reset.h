#ifndef EASNFW_RESET_H
#define EASNFW_RESET_H

/**
 * @brief Set easnfwcloud_ready flag.
 */
void reset__easnfwcloud_ready_set(void);

/**
 * @brief Clear easnfwcloud_ready flag.
 */
void reset__easnfwcloud_ready_clear(void);

/**
 * @brief Reset EASNFW-SENSOR and EASNFW-CLOUD.
 *
 * @details Reset EASNFW-CLOUD, wait for its reset to complete, and then
 * self-reset.
 *
 * @param force if true, skip bounded reset policy and any other existing
 * checks before resetting.
 *
 * @return EASNFW_TODO: ideally the function should never return because the
 * firmware should reset within it, but the function should account for
 * possible errors during the reset sequence and return an appropriate error
 * code, which will depend on the function implementation. If applicable,
 * create an enum with custom error codes for this, but they should not overlap
 * with errno.h macros (see selftest.c/h).
 */
int reset__easnfw_reset(bool const force);

#endif /* EASNFW_RESET_H */
