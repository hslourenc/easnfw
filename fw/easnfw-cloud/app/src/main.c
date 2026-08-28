#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>
#include <easnfw/selftest.h>

LOG_MODULE_REGISTER(easnfw_cloud, LOG_LEVEL_INF);


K_MSGQ_DEFINE(rx_payload_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(assembled_payload_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(tx_result_q, sizeof(struct easnfw_pipeline_message), 4, 4);

int main(void)
{
	LOG_INF("EASNFW-CLOUD starting");

	if (easnfw_selftest_run() != 0) {
		LOG_ERR("Cloud self-test failed");
		/* EASNFW_TODO: Store failures and follow the reset policy. */
		return -1;
	}

	sensor_link_start();
	payload_assembler_start();
	cloud_tx_start();
	LOG_INF("EASNFW-CLOUD ready");
	return 0;
}
