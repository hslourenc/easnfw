#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#ifdef CONFIG_BOOTLOADER_MCUBOOT
#include <zephyr/dfu/mcuboot.h>
#endif
#include <easnfw/pipeline.h>
#include <easnfw/cloud_tx.h>
#include <easnfw/payload_assembler.h>
#include <easnfw/sensor_link.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(easnfw_cloud, LOG_LEVEL_INF);

K_MSGQ_DEFINE(rx_payload_q, sizeof(struct easnfw_pipeline_message), 1, 4);
K_MSGQ_DEFINE(assembled_payload_q, sizeof(struct easnfw_pipeline_message), 1, 4);
K_MSGQ_DEFINE(tx_result_q, sizeof(struct easnfw_pipeline_message), 1, 4);
K_MEM_SLAB_DEFINE(easnfw_record_pool, CONFIG_EASNFW_CLOUD_MAX_RECORD_SIZE, 1, 4);

/**
 * @brief Allocate bounded volatile staging memory for one incoming record.
 *
 * @param data Receives the allocated buffer address.
 * @return 0 on success or a negative errno value.
 */
int easnfw_record_buffer_alloc(uint8_t **data)
{
	if (data == NULL) {
		return -EINVAL;
	}
	return k_mem_slab_alloc(&easnfw_record_pool, (void **)data, K_NO_WAIT);
}

/**
 * @brief Release the cloud's single record staging buffer.
 *
 * @param data Buffer previously returned by easnfw_record_buffer_alloc().
 */
void easnfw_record_buffer_free(uint8_t *data)
{
	if (data != NULL) {
		k_mem_slab_free(&easnfw_record_pool, data);
	}
}

int main(void)
{
	int err;

	LOG_INF("EASNFW-CLOUD starting");
	err = easnfw_cloud_ifl_init();
	if (err != 0) {
		LOG_ERR("Cloud IFL initialization failed");
		return -1;
	}

	if (easnfw_selftest_run() != 0) {
		LOG_ERR("Cloud self-test reports degraded mode; backend transport unavailable");
	}

#ifdef CONFIG_BOOTLOADER_MCUBOOT
	/* EASNFW_REVISIT: refine confirmation criteria with the cloud contract. */
	err = boot_write_img_confirmed();
	if (err != 0) {
		LOG_ERR("Could not confirm MCUboot image: %d", err);
	}
#endif
	sensor_link_start();
	payload_assembler_start();
	cloud_tx_start();
	LOG_INF("EASNFW-CLOUD ready (receive-only until backend contract is implemented)");
	return 0;
}
