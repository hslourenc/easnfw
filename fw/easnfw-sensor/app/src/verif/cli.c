#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

#include <easnfw/config.h>
#include "test_cases.h"

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

LOG_MODULE_REGISTER(verif_cli, LOG_LEVEL_INF);

static uint32_t reset_pending = 0;

static int cmd_tc(const struct shell *shell, size_t argc, char **argv)
{
	char *end;
	unsigned long test_case;
	unsigned long test_case_step = 1U;

	test_case = strtoul(argv[1], &end, 10);
	if (*argv[1] == '\0' || *end != '\0' || test_case < 1U || test_case > 4U) {
		shell_error(shell, "test case must be an integer from 1 to 4");
		return -EINVAL;
	}

	if (argc == 3) {
		test_case_step = strtoul(argv[2], &end, 10);
		if (*argv[2] == '\0' || *end != '\0' || test_case_step < 1U ||
		    test_case_step > UINT_MAX) {
			shell_error(shell, "test case step must be a positive integer");
			return -EINVAL;
		}
	}

	switch (test_case) {
	case 1U:
		app_test_cases__tc001((unsigned int)test_case_step);
		break;
	case 2U:
		app_test_cases__tc002((unsigned int)test_case_step);
		break;
	case 3U:
		app_test_cases__tc003((unsigned int)test_case_step);
		break;
	case 4U:
		app_test_cases__tc004((unsigned int)test_case_step);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	reset_pending = RESET_PENDING_MAGIC;
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

void app_verif__notify_reset_done(void)
{
	if (reset_pending == RESET_PENDING_MAGIC) {
		reset_pending = 0U;
		LOG_INF("TCSTATUS_RESETDONE - reset sequence complete");
	}
}

SHELL_CMD_ARG_REGISTER(easnfwcli2_tc, NULL, "Run a verification test case", cmd_tc, 2, 1);
SHELL_CMD_REGISTER(easnfwcli2_reset, NULL, "Reset the application", cmd_reset);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */