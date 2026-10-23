#ifndef EASNFW_NVS_H
#define EASNFW_NVS_H

#include <stddef.h>
#include <stdint.h>

enum {
	NVS_ENTRY_ID__DUMMY = 0,
	NVS_ENTRY_ID__RESET_REASON = 1,
	NVS_ENTRY_ID__RESET_HISTORY = 2,
	NVS_ENTRY_ID__SELFTEST_FAILURES = 3,
	NVS_ENTRY_ID__RECOVERY_RESET_COUNT = 4,
	NVS_ENTRY_ID__DIAGNOSTIC_REASON = 5,
};

/** Initialize the NVS partition used by EASNFW-SENSOR. */
int app_nvs__init(void);

/** Write one NVS entry, replacing any existing value for the ID. */
int app_nvs__write(uint16_t id, const void *data, size_t len);

/** Read one NVS entry into a caller-provided buffer. */
int app_nvs__read(uint16_t id, void *data, size_t len);

#endif
