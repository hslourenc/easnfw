#ifndef MIC_TESTBED_I2S_MIC_H_
#define MIC_TESTBED_I2S_MIC_H_

#include <stddef.h>

struct mic_testbed_raw_block {
	void *data;
	size_t size;
};

int i2s_mic_init(void);
int i2s_mic_start(void);
int i2s_mic_read(struct mic_testbed_raw_block *block);
void i2s_mic_release(struct mic_testbed_raw_block *block);
int i2s_mic_stop(void);

#endif /* MIC_TESTBED_I2S_MIC_H_ */
