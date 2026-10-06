#ifndef TEST_CASES_H
#define TEST_CASES_H

#include <easnfw/config.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

struct injected_faults {
	int selftest_nvs_fault;
	int selftest_mass_storage_fault;
};

extern struct injected_faults faults;

void app_test_cases__tc001(unsigned int test_case_step);
void app_test_cases__tc002(unsigned int test_case_step);
void app_test_cases__tc003(unsigned int test_case_step);
void app_test_cases__tc004(unsigned int test_case_step);
void app_test_cases__clear_all_faults(void);

void app_verif__notify_reset_done(void);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

#endif
