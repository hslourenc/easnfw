#ifndef MIC_TESTBED_AUDIO_CONFIG_H_
#define MIC_TESTBED_AUDIO_CONFIG_H_

#include <stdint.h>

/* REQ-005 initial baseline. */
#define MIC_TESTBED_SAMPLE_RATE_HZ 48000U
#define MIC_TESTBED_PCM_BITS       16U
#define MIC_TESTBED_CHANNELS       1U

/*
 * 1536 samples correspond to 32 ms at 48 kHz. The resulting 3072-byte
 * PCM16 block is an integer multiple of a 512-byte SD sector.
 */
#define MIC_TESTBED_SAMPLES_PER_BLOCK 1536U
#define MIC_TESTBED_PCM_BLOCK_BYTES \
	(MIC_TESTBED_SAMPLES_PER_BLOCK * sizeof(int16_t))
#define MIC_TESTBED_RAW_BLOCK_BYTES \
	(MIC_TESTBED_SAMPLES_PER_BLOCK * sizeof(int32_t))

#define MIC_TESTBED_TOTAL_SAMPLES \
	(MIC_TESTBED_SAMPLE_RATE_HZ * CONFIG_MIC_TESTBED_TRACK_SECONDS)
#define MIC_TESTBED_TOTAL_BLOCKS \
	(MIC_TESTBED_TOTAL_SAMPLES / MIC_TESTBED_SAMPLES_PER_BLOCK)

#if (MIC_TESTBED_TOTAL_SAMPLES % MIC_TESTBED_SAMPLES_PER_BLOCK) != 0
#error "Capture length must contain an integer number of 32-ms blocks"
#endif

#endif /* MIC_TESTBED_AUDIO_CONFIG_H_ */
