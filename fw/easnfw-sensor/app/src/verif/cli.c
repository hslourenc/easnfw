#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/reboot.h>

#include <easnfw/config.h>
#include <easnfw/interfw_link.h>
#include <easnfw/reset.h>
#include <easnfw/startup.h>

#include "cli.h"
#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(verif_cli, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(startup_test_stack, 6144);
K_THREAD_STACK_DEFINE(startup_watchdog_stack, 8192);
static struct k_thread startup_test_thread;
static struct k_thread startup_watchdog_thread;
static k_timeout_t startup_test_timeout;
static unsigned int startup_test_case;
static bool startup_test_active;
static unsigned int sampling_commits;
K_SEM_DEFINE(sampling_complete, 0, 1);

static void startup_test_run(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	(void)main_continue();
}

static void startup_test_watchdog(void *a, void *b, void *c)
{
	int err;

	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	if (startup_test_case == 22U || startup_test_case == 25U ||
	    startup_test_case == 26U || startup_test_case == 32U ||
	    startup_test_case == 62U || startup_test_case == 63U) {
		err = k_sem_take(&sampling_complete, startup_test_timeout);
		if (err != 0) {
			shell_error(shell_backend_uart_get_ptr(), "HIL_OBSERVATION_TIMEOUT");
		} else {
			k_sleep(K_SECONDS(1));
		}
	} else {
		k_sleep(startup_test_timeout);
	}
	shell_print(shell_backend_uart_get_ptr(), "HIL_WATCHDOG case=%u",
		    startup_test_case);
	err = reset__easnfw_reset(true);
	shell_error(shell_backend_uart_get_ptr(), "HIL_RESET_FAILED error=%d", err);
	k_sleep(K_MSEC(200));
	sys_reboot(SYS_REBOOT_COLD);
}

void cli__sampling_track_committed(void)
{
	unsigned int const committed = ++sampling_commits;

	if (startup_test_case == 62U) {
		shell_print(shell_backend_uart_get_ptr(),
			    "HIL_TRACK_COMMITTED case=62 count=%u", committed);
		if (committed == 2U) {
			app_test_cases__tc062_observation_complete();
			k_sem_give(&sampling_complete);
		}
	} else if (committed == 2U) {
		k_sem_give(&sampling_complete);
	}
}

void cli__diagnostic_entered(void)
{
	if (startup_test_active) {
		shell_print(shell_backend_uart_get_ptr(),
			    "HIL_DIAGNOSTIC_ENTERED case=%u", startup_test_case);
	}
	if (startup_test_active &&
	    (startup_test_case == 22U || startup_test_case == 63U)) {
		k_sem_give(&sampling_complete);
	}
}

void cli__run_production_startup(unsigned int test_case, k_timeout_t timeout)
{
	startup_test_timeout = timeout;
	startup_test_case = test_case;
	startup_test_active = true;
	sampling_commits = 0U;
	k_sem_reset(&sampling_complete);
	shell_print(shell_backend_uart_get_ptr(), "HIL_START case=%u", test_case);
	if (test_case == 62U) {
		shell_print(shell_backend_uart_get_ptr(),
			    "HIL_STORAGE_SPACE_SHORTAGE injected=2");
	}
	k_thread_create(&startup_watchdog_thread, startup_watchdog_stack,
			K_THREAD_STACK_SIZEOF(startup_watchdog_stack),
			startup_test_watchdog, NULL, NULL, NULL, 4, 0, K_NO_WAIT);
	k_thread_create(&startup_test_thread, startup_test_stack,
			K_THREAD_STACK_SIZEOF(startup_test_stack),
			startup_test_run, NULL, NULL, NULL, 4, 0, K_NO_WAIT);
}

static int cli__cmd_tc(struct shell* const shell, size_t argc, char** argv)
{
	char *end;
	unsigned long test_case;
	unsigned long step = 1;

	if (startup_test_active) {
		shell_error(shell, "production startup test is already running");
		return -EBUSY;
	}
	errno = 0;
	test_case = strtoul(argv[1], &end, 10);
	if (errno == ERANGE || end == argv[1] || *end != '\0' ||
	    test_case == 0UL) {
		shell_error(shell, "could not convert argument test-case-number to integer");
		return -EINVAL;
	}
	if (test_case > 13UL && test_case != 20UL && test_case != 22UL &&
	    test_case != 25UL && test_case != 26UL && test_case != 32UL &&
	    test_case != 41UL && test_case != 42UL && test_case != 43UL &&
	    test_case != 46UL && test_case != 48UL && test_case != 62UL &&
	    test_case != 63UL) {
		shell_error(shell,
			    "supported test cases are 1-13, 20, 22, 25, 26, 32, 41, 42, 43, 46, 48, 62, and 63");
		return -EINVAL;
	}

	if (argc == 3) {
		errno = 0;
		step = strtoul(argv[2], &end, 10);
		if (errno == ERANGE || end == argv[2] || *end != '\0' ||
		    step == 0UL || step > UINT_MAX) {
			shell_error(shell, "could not convert argument test-case-step to integer");
			return -EINVAL;
		}
	}

	switch (test_case) {
	case 1U:
		app_test_cases__tc001((unsigned int)step);
		break;
	case 2U:
		app_test_cases__tc002((unsigned int)step);
		break;
	case 3U:
		app_test_cases__tc003((unsigned int)step);
		break;
	case 4U:
		app_test_cases__tc004((unsigned int)step);
		break;
	case 5U:
		app_test_cases__tc005((unsigned int)step);
		break;
	case 6U:
		app_test_cases__tc006((unsigned int)step);
		break;
	case 7U:
		app_test_cases__tc007((unsigned int)step);
		break;
	case 8U:
		app_test_cases__tc008((unsigned int)step);
		break;
	case 9U:
		app_test_cases__tc009((unsigned int)step);
		break;
	case 10U:
		app_test_cases__tc010((unsigned int)step);
		break;
	case 11U:
		app_test_cases__tc011((unsigned int)step);
		break;
	case 12U:
		app_test_cases__tc012((unsigned int)step);
		break;
	case 13U:
		app_test_cases__tc013((unsigned int)step);
		break;
	case 20U:
		app_test_cases__tc020((unsigned int)step);
		break;
	case 22U:
		app_test_cases__tc022((unsigned int)step);
		break;
	case 25U:
		app_test_cases__tc025((unsigned int)step);
		break;
	case 26U:
		app_test_cases__tc026((unsigned int)step);
		break;
	case 32U:
		app_test_cases__tc032((unsigned int)step);
		break;
	case 41U:
		app_test_cases__tc041((unsigned int)step);
		break;
	case 42U:
		app_test_cases__tc042((unsigned int)step);
		break;
	case 43U:
		app_test_cases__tc043((unsigned int)step);
		break;
	case 46U:
		app_test_cases__tc046((unsigned int)step);
		break;
	case 48U:
		app_test_cases__tc048((unsigned int)step);
		break;
	case 62U:
		app_test_cases__tc062((unsigned int)step);
		break;
	case 63U:
		app_test_cases__tc063((unsigned int)step);
		break;
	default:
		shell_error(shell, "test case not yet implemented");
		return -EINVAL;
	}

	return 0;
}

static void cli__cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (reset__easnfw_reset(true) != 0) {
		shell_error(shell, "reset sequence failed");
	}
}

void cli__wait_for_shell_ready(void)
{
	if (ifl__init() != 0) {
		LOG_ERR("IFL initialization failed");
	}
	while (!shell_ready(shell_backend_uart_get_ptr())) {
		k_msleep(10);
	}
	shell_print(shell_backend_uart_get_ptr(),
		    "TCSTATUS_RESETDONE - reset complete");
}

SHELL_CMD_ARG_REGISTER(easnfwcli2_tc, NULL, "Run a test case", cli__cmd_tc, 2, 1);
SHELL_CMD_REGISTER(easnfwcli2_reset, NULL, "Reset the application", cli__cmd_reset);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */