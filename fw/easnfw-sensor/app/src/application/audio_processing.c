#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <easnfw/pipeline.h>

LOG_MODULE_REGISTER(easnfw_audio_processing, LOG_LEVEL_INF);
K_THREAD_STACK_DEFINE(audio_processing_stack, 1024);
static struct k_thread audio_processing_thread_data;

static void audio_processing_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	while (true) {
		struct easnfw_pipeline_message message;
		k_msgq_get(&audio_block_q, &message, K_FOREVER);
		/* EASNFW_TODO: Replace metadata pass-through with DSP processing. */
		k_msgq_put(&processed_block_q, &message, K_NO_WAIT);
	}
}

void audio_processing_start(void)
{
	k_thread_create(&audio_processing_thread_data, audio_processing_stack,
		K_THREAD_STACK_SIZEOF(audio_processing_stack), audio_processing_thread,
		NULL, NULL, NULL, 5, 0, K_NO_WAIT);
}