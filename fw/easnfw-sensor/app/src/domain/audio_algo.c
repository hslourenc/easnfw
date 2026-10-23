#include <errno.h>
#include <string.h>

#include <easnfw/audio_algo.h>

/** The baseline processor is intentionally bit-exact pending algorithm design. */
int audio_algo_process(const struct easnfw_audio_block *input,
	struct easnfw_audio_block *output)
{
	if (input == NULL || output == NULL ||
	    input->sample_count > EASNFW_AUDIO_BLOCK_SAMPLES) {
		return -EINVAL;
	}
	if (input != output) {
		memcpy(output, input, sizeof(*output));
	}
	return 0;
}