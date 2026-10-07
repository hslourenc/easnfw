#ifndef EASNFW_STATUS_H
#define EASNFW_STATUS_H

/**
 * EASNFW_TODO: add comments using doxygen style and add more status codes as
 * needed. The status codes should be unique and not overlap with errno.h
 * macros.
 */
enum easnfw_status {
	EASNFW_STATUS__SELFTEST_NVS_INIT_FAILED               = 1001,
	EASNFW_STATUS__SELFTEST_NVS_WR_FAILED                 = 1002,
	EASNFW_STATUS__SELFTEST_NVS_RD_FAILED                 = 1003,
	EASNFW_STATUS__SELFTEST_NVS_RD_WR_NO_MATCH            = 1004,
	EASNFW_STATUS__SELFTEST_MASS_STORAGE_INIT_FAILED      = 1005,
	EASNFW_STATUS__SELFTEST_MASS_STORAGE_WR_FAILED        = 1006,
	EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_FAILED        = 1007,
	EASNFW_STATUS__SELFTEST_MASS_STORAGE_RD_WR_NO_MATCH   = 1008,

	EASNFW_STATUS__INTERFW_LINK_TX_FAILED                 = 2001,
};

#endif /* EASNFW_STATUS_H */
