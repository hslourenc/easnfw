#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <mic_testbed/audio_config.h>
#include <mic_testbed/i2s_mic.h>

LOG_MODULE_REGISTER(mic_testbed_i2s, LOG_LEVEL_INF);

#define I2S_NODE DT_NODELABEL(i2s0)
#define I2S_RX_BLOCK_COUNT 8U
#define I2S_READ_TIMEOUT_MS 1000U

K_MEM_SLAB_DEFINE(i2s_rx_slab, MIC_TESTBED_RAW_BLOCK_BYTES,
	I2S_RX_BLOCK_COUNT, sizeof(uint32_t));

static const struct device *const i2s_device = DEVICE_DT_GET(I2S_NODE);

int i2s_mic_init(void)
{
	struct i2s_config config = {
		.word_size = 32U,
		.channels = 1U,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
		.frame_clk_freq = MIC_TESTBED_SAMPLE_RATE_HZ,
		.mem_slab = &i2s_rx_slab,
		.block_size = MIC_TESTBED_RAW_BLOCK_BYTES,
		.timeout = I2S_READ_TIMEOUT_MS,
	};

	if (!device_is_ready(i2s_device)) {
		LOG_ERR("I2S device is not ready");
		return -ENODEV;
	}

	/*
	 * The DMM-4026 uses 32 BCLK periods per channel, hence 64 BCLK
	 * periods per 48-kHz stereo frame even when only the left slot is
	 * captured. This produces a 3.072-MHz microphone input clock.
	 */
	return i2s_configure(i2s_device, I2S_DIR_RX, &config);
}

int i2s_mic_start(void)
{
	return i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_START);
}

int i2s_mic_read(struct mic_testbed_raw_block *block)
{
	return i2s_read(i2s_device, &block->data, &block->size);
}

void i2s_mic_release(struct mic_testbed_raw_block *block)
{
	if ((block != NULL) && (block->data != NULL)) {
		k_mem_slab_free(&i2s_rx_slab, block->data);
		block->data = NULL;
		block->size = 0U;
	}
}

int i2s_mic_stop(void)
{
	return i2s_trigger(i2s_device, I2S_DIR_RX, I2S_TRIGGER_DROP);
}
