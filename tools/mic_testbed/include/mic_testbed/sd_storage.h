#ifndef MIC_TESTBED_SD_STORAGE_H_
#define MIC_TESTBED_SD_STORAGE_H_

#include <stddef.h>
#include <stdint.h>

int sd_storage_init(void);
int sd_storage_begin_capture(uint32_t sample_rate, uint16_t channels,
	uint16_t bits_per_sample);
int sd_storage_append_audio(const void *data, size_t size);
int sd_storage_commit_capture(void);
int sd_storage_abort_capture(void);
const char *sd_storage_capture_path(void);

#endif /* MIC_TESTBED_SD_STORAGE_H_ */
