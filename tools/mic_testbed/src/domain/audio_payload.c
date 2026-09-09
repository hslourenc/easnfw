#include <limits.h>

#include <mic_testbed/audio_payload.h>

void audio_payload_stats_init(struct mic_testbed_audio_stats *stats)
{
	stats->minimum = INT16_MAX;
	stats->maximum = INT16_MIN;
	stats->zero_samples = 0U;
	stats->clipped_samples = 0U;
	stats->sample_count = 0U;
}

size_t audio_payload_pcm32_to_pcm16(const int32_t *input, size_t input_bytes,
	int16_t *output, struct mic_testbed_audio_stats *stats)
{
	const size_t sample_count = input_bytes / sizeof(*input);

	for (size_t i = 0U; i < sample_count; ++i) {
		/*
		 * The microphone emits a 24-bit I2S sample in a 32-bit word. The
		 * nRF I2S peripheral stores the left-aligned signed word in RAM.
		 * Keeping the upper 16 bits provides deterministic PCM16 without
		 * adding gain or changing the scientific content.
		 */
		const int16_t sample = (int16_t)(input[i] >> 16);

		output[i] = sample;
		if (sample < stats->minimum) {
			stats->minimum = sample;
		}
		if (sample > stats->maximum) {
			stats->maximum = sample;
		}
		if (sample == 0) {
			stats->zero_samples++;
		}
		if ((sample == INT16_MIN) || (sample == INT16_MAX)) {
			stats->clipped_samples++;
		}
	}

	stats->sample_count += sample_count;
	return sample_count * sizeof(*output);
}

uint16_t audio_payload_absolute_peak(const struct mic_testbed_audio_stats *stats)
{
	const uint16_t negative_peak = (stats->minimum == INT16_MIN) ? 32768U :
		(stats->minimum < 0 ? (uint16_t)-stats->minimum : 0U);
	const uint16_t positive_peak = (stats->maximum > 0) ?
		(uint16_t)stats->maximum : 0U;

	return negative_peak > positive_peak ? negative_peak : positive_peak;
}
