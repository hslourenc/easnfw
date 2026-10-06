#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <easnfw/config.h>
#include <easnfw/pipeline.h>
#include <easnfw/selftest.h>

#ifdef CONFIG_EASNFW_VERIFICATION_CODE
#include "verif/cli.h"
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */

LOG_MODULE_REGISTER(easnfw_sensor, LOG_LEVEL_INF);

K_MSGQ_DEFINE(audio_block_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(track_meta_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(processed_block_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(storage_tx_q, sizeof(struct easnfw_pipeline_message), 4, 4);
K_MSGQ_DEFINE(tx_ack_q, sizeof(struct easnfw_pipeline_message), 4, 4);

int main(void)
{
#ifdef CONFIG_EASNFW_VERIFICATION_CODE
	cli__wait_for_shell_ready();
#else
	LOG_INF("EASNFW-SENSOR starting");
	if (app_selftest__run() != 0) {
		LOG_ERR("Sensor self-test failed");
		/* EASNFW_TODO: Store failures and follow the reset policy. */
		return -1;
	}
	sampling_start();
	audio_processing_start();
	storage_start();
	cloud_link_start();
	LOG_INF("EASNFW-SENSOR ready");
	return 0;
#endif /* CONFIG_EASNFW_VERIFICATION_CODE */
}
