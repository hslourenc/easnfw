#ifndef MIC_TESTBED_AUDIO_PAYLOAD_H_
#define MIC_TESTBED_AUDIO_PAYLOAD_H_

#include <stddef.h>
#include <stdint.h>

struct mic_testbed_audio_stats {
	int16_t minimum;
	int16_t maximum;
	uint32_t zero_samples;
	uint32_t clipped_samples;
	uint32_t sample_count;
};

void audio_payload_stats_init(struct mic_testbed_audio_stats *stats);
size_t audio_payload_pcm32_to_pcm16(const int32_t *input, size_t input_bytes,
	int16_t *output, struct mic_testbed_audio_stats *stats);
uint16_t audio_payload_absolute_peak(const struct mic_testbed_audio_stats *stats);

#endif /* MIC_TESTBED_AUDIO_PAYLOAD_H_ */
