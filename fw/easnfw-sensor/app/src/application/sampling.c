#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/domain.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_sampling, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(sampling_stack, 1024);
static struct k_thread sampling_thread_data;

static void sampling_thread(void *a, void *b, void *c)
{
	uint32_t sequence = 0;
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_audio_block block = { .sequence = sequence++ };
		struct easnfw_environment_sample sample;
		/* EASNFW_TODO: Enforce track timing and capture real sensor data. */
		audio_sampling_acquire(&block);
		env_sampling_acquire(&sample);
		struct easnfw_pipeline_message message = { block.sequence, 0 };
		k_msgq_put(&audio_block_q, &message, K_NO_WAIT);
		k_msgq_put(&track_meta_q, &message, K_NO_WAIT);
		k_sleep(K_SECONDS(1));
	}
}

void sampling_start(void)
{
	k_thread_create(&sampling_thread_data, sampling_stack,
		K_THREAD_STACK_SIZEOF(sampling_stack), sampling_thread, NULL, NULL,
		NULL, 5, 0, K_NO_WAIT);
}