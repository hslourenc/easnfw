#include <stddef.h>
#include <easnfw/domain.h>

int audio_algo_process(const struct easnfw_audio_block *input,
	struct easnfw_audio_block *output)
{
	/* EASNFW_TODO: Implement the audio processing algorithm. */
	if (input != NULL && output != NULL) {
		*output = *input;
	}
	return 0;
}