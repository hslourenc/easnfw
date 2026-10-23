/**
 * @file selftest.h
 * @brief CLOUD startup self-test interface.
 */
#ifndef EASNFW_SELFTEST_H
#define EASNFW_SELFTEST_H

/** Run the CLOUD-local self-test checks and return the aggregate result. */
int easnfw_selftest_run(void);

#endif