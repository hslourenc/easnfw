#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/domain.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_sampling, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(sampling_stack, 1024);
static struct k_thread sampling_thread_data;

static void sampling_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		static uint32_t sequence;
		int ret = audio_capture_run();

		if (ret == 0) {
			struct easnfw_pipeline_message message = {
				.sequence = sequence++,
				.kind = EASNFW_MESSAGE_RECORD,
			};
			(void)k_msgq_put(&audio_block_q, &message, K_NO_WAIT);
			(void)k_msgq_put(&track_meta_q, &message, K_NO_WAIT);
		} else {
			LOG_ERR("Audio capture failed: %d", ret);
		}
		k_sleep(K_SECONDS(840));
	}
}

void sampling_start(void)
{
	k_thread_create(&sampling_thread_data, sampling_stack,
		K_THREAD_STACK_SIZEOF(sampling_stack), sampling_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}