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

#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(verif_cli, LOG_LEVEL_INF);

static int cli__cmd_tc(struct shell* const shell, size_t argc, char** argv)
{
	char *end;
	unsigned long test_case;
	unsigned long step = 1;

	errno = 0;
	test_case = strtoul(argv[1], &end, 10);
	if (errno == ERANGE || end == argv[1] || *end != '\0' ||
	    test_case == 0UL) {
		shell_error(shell, "could not convert argument test-case-number to integer");
		return -EINVAL;
	}
	if (test_case > 13UL && test_case != 20UL &&
	    test_case != 42UL && test_case != 46UL && test_case != 48UL) {
		shell_error(shell,
			    "supported test cases are 1-13, 20, 42, 46, and 48");
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
	case 42U:
		app_test_cases__tc042((unsigned int)step);
		break;
	case 46U:
		app_test_cases__tc046((unsigned int)step);
		break;
	case 48U:
		app_test_cases__tc048((unsigned int)step);
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
	LOG_INF("TCSTATUS_RESETDONE - reset complete");
}

SHELL_CMD_ARG_REGISTER(easnfwcli2_tc, NULL, "Run a test case", cli__cmd_tc, 2, 1);
SHELL_CMD_REGISTER(easnfwcli2_reset, NULL, "Reset the application", cli__cmd_reset);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */