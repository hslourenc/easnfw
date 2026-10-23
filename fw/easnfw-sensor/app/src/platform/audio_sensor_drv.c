#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/audio_sensor_drv.h>

LOG_MODULE_REGISTER(audio_sensor_drv, LOG_LEVEL_INF);

#define I2S_NODE DT_NODELABEL(i2s0)
#define I2S_RX_BLOCK_COUNT 8U
#define I2S_READ_TIMEOUT_MS 1000U
#define RAW_BLOCK_BYTES \
	(CONFIG_EASNFW_AUDIO_SAMPLES_PER_BLOCK * sizeof(int32_t))

K_MEM_SLAB_DEFINE(i2s_rx_slab, RAW_BLOCK_BYTES, I2S_RX_BLOCK_COUNT,
		  sizeof(uint32_t));

static const struct device *const i2s_device = DEVICE_DT_GET(I2S_NODE);
static bool i2s_configured;
static bool i2s_started;

/** Configure the Audio DK I2S peripheral for mono 32-bit microphone words. */
int app_audio_sensor__init(void)
{
	struct i2s_config config = {
		.word_size = 32U,
		.channels = 1U,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
		.frame_clk_freq = CONFIG_EASNFW_AUDIO_SAMPLE_RATE_HZ,
		.mem_slab = &i2s_rx_slab,
		.block_size = RAW_BLOCK_BYTES,
		.timeout = I2S_READ_TIMEOUT_MS,
	};
	int err;

	if (!device_is_ready(i2s_device)) {
		LOG_ERR("I2S microphone device is not ready");
		return -ENODEV;
	}

	err = i2s_configure(i2s_device, I2S_DIR_RX, &config);
	if (err != 0) {
		LOG_ERR("I2S microphone configuration failed: %d", err);
		return err;
	}

	i2s_configured = true;
	return 0;
}

/** Start receiving microphone blocks. */
int app_audio_sensor__start(void)
{
	int err;

	if (!i2s_configured) {
		err = app_audio_sensor__init();
		if (err != 0) {
			return err;
		}
	}
	if (i2s_started) {
		return -EALREADY;
	}

	err = i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_START);
	if (err != 0) {
		LOG_ERR("I2S microphone start failed: %d", err);
		return err;
	}
	i2s_started = true;
	return 0;
}

/** Read a single full buffer from I2S. */
int app_audio_sensor__read(struct app_audio_raw_block *block)
{
	if (!i2s_started || block == NULL) {
		return -EINVAL;
	}
	block->data = NULL;
	block->size = 0U;
	return i2s_read(i2s_device, &block->data, &block->size);
}

/** Return a received buffer to Zephyr's I2S slab. */
void app_audio_sensor__release(struct app_audio_raw_block *block)
{
	if (block != NULL && block->data != NULL) {
		k_mem_slab_free(&i2s_rx_slab, block->data);
		block->data = NULL;
		block->size = 0U;
	}
}

/** Stop receiving and return the I2S peripheral to its idle state. */
int app_audio_sensor__stop(void)
{
	int err;

	if (!i2s_started) {
		return 0;
	}
	err = i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_DROP);
	if (err != 0) {
		LOG_ERR("I2S microphone stop failed: %d", err);
		return err;
	}
	i2s_started = false;
	return 0;
}
