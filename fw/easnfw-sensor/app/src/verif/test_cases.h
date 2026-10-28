#ifndef TEST_CASES_H
#define TEST_CASES_H

#include <easnfw/config.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE

struct injected_faults {
	int selftest_nvs_fault;
	int selftest_mass_storage_fault;
	int selftest_audio_fault;
	int selftest_environment_fault;
	int selftest_ifl_fault;
	int selftest_cloud_fault;
};

extern struct injected_faults faults;

void app_test_cases__tc001(unsigned int test_case_step);
void app_test_cases__tc002(unsigned int test_case_step);
void app_test_cases__tc003(unsigned int test_case_step);
void app_test_cases__tc004(unsigned int test_case_step);
void app_test_cases__tc005(unsigned int test_case_step);
void app_test_cases__tc006(unsigned int test_case_step);
void app_test_cases__tc007(unsigned int test_case_step);
void app_test_cases__tc008(unsigned int test_case_step);
void app_test_cases__tc009(unsigned int test_case_step);
void app_test_cases__tc010(unsigned int test_case_step);
void app_test_cases__tc011(unsigned int test_case_step);
void app_test_cases__tc012(unsigned int test_case_step);
void app_test_cases__tc013(unsigned int test_case_step);
void app_test_cases__tc042(unsigned int test_case_step);
void app_test_cases__tc043(unsigned int test_case_step);
void app_test_cases__tc046(unsigned int test_case_step);
void app_test_cases__tc048(unsigned int test_case_step);
void app_test_cases__tc020(unsigned int test_case_step);
void app_test_cases__clear_all_faults(void);

#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

#endif
