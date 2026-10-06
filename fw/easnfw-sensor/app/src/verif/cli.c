#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

#include <easnfw/config.h>

#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(verif_cli, LOG_LEVEL_INF);

static int cli__cmd_tc(struct shell* const shell, size_t argc, char** argv)
{
	char* end;
	unsigned long test_case;
	unsigned long step = 1;

	/* convert test-case-number argument to integer */
	test_case = strtoul(argv[1], &end, 10);
	if (test_case == 0) {
		shell_error(shell, "could not convert argument test-case-number to integer");
		return -EINVAL;
	}
	if (test_case == ULONG_MAX) {
		shell_error(shell, "converted value for test-case-number argument fell out of range of ulong");
		return -EINVAL;
	}
	if (test_case > 4) {
		shell_error(shell, "test case must be an integer from 1 to 4");
		return -EINVAL;
	}

	/* check if test-case-step argument is present and convert to integer if yes */
	if (argc == 3) {
		step = strtoul(argv[2], &end, 10);
		if (step == 0) {
			shell_error(shell, "could not convert argument test-case-step to integer");
			return -EINVAL;
		}
		if (step == ULONG_MAX) {
			shell_error(shell, "converted value for test-case-step argument fell out of range of ulong");
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

	reset__easnfw_reset();
}

void cli__wait_for_shell_ready(void)
{
	while (!shell_ready()) {
		k_msleep(10);
	}
	LOG_INF("TCSTATUS_RESETDONE - reset complete");
}

SHELL_CMD_ARG_REGISTER(easnfwcli2_tc, NULL, "Run a verification test case", cli__cmd_tc, 2, 1);
SHELL_CMD_REGISTER(easnfwcli2_reset, NULL, "Reset the application", cli__cmd_reset);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */