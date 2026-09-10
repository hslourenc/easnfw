#ifndef EASNFW_AUDIO_SENSOR_H_
#define EASNFW_AUDIO_SENSOR_H_

#include <stddef.h>

struct easnfw_raw_audio_block {
	void *data;
	size_t size;
};

int audio_sensor_drv_init(void);
int audio_sensor_drv_start(void);
int audio_sensor_drv_read(struct easnfw_raw_audio_block *block);
void audio_sensor_drv_release(struct easnfw_raw_audio_block *block);
int audio_sensor_drv_stop(void);

#endif
