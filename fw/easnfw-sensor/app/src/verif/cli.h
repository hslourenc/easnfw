#ifndef EASNFW_CLI_H
#define EASNFW_CLI_H

#include <zephyr/kernel.h>

/**
 * @brief Wait until shell is ready to excute CLI commands and log the
 * appropriate test case status message once ready. 
 */
void cli__wait_for_shell_ready(void);

/** Run production startup on a separate thread with a bounded reset watchdog. */
void cli__run_production_startup(unsigned int test_case, k_timeout_t timeout);

/** Signal when a sampling-observation record has been committed and queued. */
void cli__sampling_track_committed(void);

/** Signal diagnostic-state entry for the startup observation test. */
void cli__diagnostic_entered(void);

#endif /* EASNFW_CLI_H */