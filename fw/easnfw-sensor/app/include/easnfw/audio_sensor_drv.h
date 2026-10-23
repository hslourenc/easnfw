#ifndef EASNFW_AUDIO_SENSOR_DRV_H
#define EASNFW_AUDIO_SENSOR_DRV_H

#include <stddef.h>

/** One raw, left-aligned 32-bit I2S receive buffer. */
struct app_audio_raw_block {
	void *data;
	size_t size;
};

/** Configure the I2S microphone peripheral. */
int app_audio_sensor__init(void);

/** Start I2S receive. */
int app_audio_sensor__start(void);

/** Read one I2S buffer. */
int app_audio_sensor__read(struct app_audio_raw_block *block);

/** Return an I2S buffer to the receive slab. */
void app_audio_sensor__release(struct app_audio_raw_block *block);

/** Stop I2S receive. */
int app_audio_sensor__stop(void);

#endif
