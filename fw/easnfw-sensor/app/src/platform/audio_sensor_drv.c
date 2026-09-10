#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>

#include <easnfw/audio_sensor.h>

#define I2S_NODE DT_NODELABEL(i2s0)
#define I2S_BLOCK_SAMPLES 1536U
#define I2S_BLOCK_BYTES (I2S_BLOCK_SAMPLES * sizeof(int32_t))
#define I2S_BLOCK_COUNT 8U

K_MEM_SLAB_DEFINE(easnfw_i2s_slab, I2S_BLOCK_BYTES, I2S_BLOCK_COUNT,
	sizeof(uint32_t));

static const struct device *const i2s_device = DEVICE_DT_GET(I2S_NODE);

int audio_sensor_drv_init(void)
{
	struct i2s_config config = {
		.word_size = 32U,
		.channels = 1U,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
		.frame_clk_freq = 48000U,
		.mem_slab = &easnfw_i2s_slab,
		.block_size = I2S_BLOCK_BYTES,
		.timeout = 1000U,
	};

	if (!device_is_ready(i2s_device)) {
		return -ENODEV;
	}
	return i2s_configure(i2s_device, I2S_DIR_RX, &config);
}

int audio_sensor_drv_start(void)
{
	return i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_START);
}

int audio_sensor_drv_read(struct easnfw_raw_audio_block *block)
{
	if (block == NULL) {
		return -EINVAL;
	}
	return i2s_read(i2s_device, &block->data, &block->size);
}

void audio_sensor_drv_release(struct easnfw_raw_audio_block *block)
{
	if (block != NULL && block->data != NULL) {
		k_mem_slab_free(&easnfw_i2s_slab, block->data);
		block->data = NULL;
		block->size = 0U;
	}
}

int audio_sensor_drv_stop(void)
{
	return i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_DROP);
}
