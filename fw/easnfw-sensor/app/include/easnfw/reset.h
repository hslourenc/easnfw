#ifndef EASNFW_RESET_H
#define EASNFW_RESET_H

#include <stdint.h>

/** @brief Set the CLOUD-ready indication and wake any waiting reset flow. */
int reset__easnfwcloud_ready_set(void);

/** @brief Clear the CLOUD-ready indication before a coordinated reset. */
int reset__easnfwcloud_ready_clear(void);

/**
 * @brief Reset EASNFW-SENSOR and EASNFW-CLOUD.
 *
 * @details Reset EASNFW-CLOUD, wait for its reset to complete, and then
 * self-reset.
 *
 * @param force if true, skip bounded reset policy and any other existing
 * checks before resetting.
 *
 * @return 0 only if the system reset call unexpectedly returns; otherwise a
 * negative EASNFW status code describing the failed coordination step.
 */
int reset__easnfw_reset(bool const force);

/** @brief Capture and persist the SENSOR processor's hardware reset cause. */
int reset__capture_startup_reason(void);

/** @brief Persist the CLOUD processor's reset cause received over IFL. */
int reset__cloud_reason_set(uint32_t cause);

/** @brief Append the synchronized boot time to the NVS reset-history ring. */
int reset__history_update(void);

/** @brief Persist a diagnostic reason and remain in the indicated state. */
void reset__diagnostic_state(int reason, const uint8_t *record_id);

/** @brief Clear the recovery-reset counter after a stable uptime window. */
void reset__start_recovery_counter_timer(void);

#endif /* EASNFW_RESET_H */
