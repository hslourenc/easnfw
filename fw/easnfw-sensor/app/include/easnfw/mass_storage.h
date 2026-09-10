#ifndef EASNFW_MASS_STORAGE_H_
#define EASNFW_MASS_STORAGE_H_

#include <stddef.h>
#include <stdint.h>

int mass_storage_init(void);
int mass_storage_begin_capture(uint32_t sample_rate, uint16_t channels,
	uint16_t bits_per_sample);
int mass_storage_append_audio(const void *data, size_t size);
int mass_storage_commit_capture(void);
int mass_storage_abort_capture(void);
const char *mass_storage_capture_path(void);

#endif
