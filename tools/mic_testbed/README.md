# EASN DMM-4026 I2S microphone testbed

This Zephyr image validates a PUI Audio DMM-4026-B-I2S-R connected to the
external-codec header of an nRF5340 Audio DK. It deliberately follows the
EASNFW-SENSOR data-path decisions instead of being a disposable microphone
demo:

- mono acquisition at 48 kHz;
- 16-bit PCM output;
- 60-second track by default (REQ-005);
- block-based acquisition with bounded RAM usage;
- progressive SD-card persistence;
- an incomplete `.tmp` file on failure; and
- an atomically renamed, directly playable WAV payload on success.

The CS47L63 is not initialized or used. The microphone is a digital source and
the Audio DK's microSD interface is independent from the audio codec.

## Electrical connection

The application drives the nRF5340 I2S interface as clock master and routes it
to the Audio DK external-codec header P10 by driving `HW_CODEC_SELECT` high.

| DMM-4026 pin | Connect to Audio DK | Notes |
| --- | --- | --- |
| 3, VDD | 1.8 V or 3.3 V | Add 100 nF close to the microphone. |
| 4, GND | GND | Common ground is mandatory. |
| 6, SCK/BCLK | P10 BCLK (`P0.14`) | 3.072 MHz at 48 kHz. |
| 5, WS/LRCLK | P10 FSYNC (`P0.16`) | 48 kHz. |
| 7, SD | P10 DOUT (`P0.15`) | Microphone data into nRF SDIN. |
| 1, L/R | GND | Selects the left slot used by this image. |
| 2, CONFIG | GND | Normal single-microphone operation. |

The microphone supports 1.5-3.6 V operation, but its signal voltage must not
exceed the DK I/O supply. Power and logic must use the same compatible domain.
Do not hot-plug the microphone while the DK is powered.

Insert a FAT32-formatted microSD card before boot. The image stores captures in
`/easn/capture_NNNN.wav` and never overwrites an existing capture.

## Why the I2S stream is configured as 32 bit

The DMM-4026 emits one 24-bit I2S sample with 18-bit precision in each 32-bit
channel slot. The nRF5340 therefore receives a 32-bit left-channel word and the
domain layer retains its upper 16 bits as PCM16. Configuring the wire interface
as 16 bit would generate the wrong BCLK/frame relationship for this microphone.

## Build

The image is validated with nRF Connect SDK v3.0.1:

```sh
west build --no-sysbuild -p always \
  -b nrf5340_audio_dk/nrf5340/cpuapp \
  tools/mic_testbed -d build/mic_testbed
```

Flash with:

```sh
west flash -d build/mic_testbed
```

The flashable files are generated under `build/mic_testbed/zephyr/`, including
`zephyr.hex` and `zephyr.bin`.

## Validation procedure

1. Check all wiring with the DK powered off, insert the SD card, and power on.
2. Monitor the DK USB serial log at 115200 baud.
3. Speak, clap, and generate a steady tone during the 60-second capture.
4. Wait for `Capture completed successfully`, then power the DK off.
5. Copy the WAV file from the SD card and inspect it in an audio editor.
6. Confirm that the file is mono PCM16/48 kHz, lasts 60 seconds, is not silent,
   is not clipped, and contains the expected speech/tone events.

The first two 32-ms blocks are discarded because the microphone datasheet
specifies up to 20 ms to reach normal sensitivity after its clock starts.

Before the temporary record is committed, the image automatically verifies
that the exact expected number of samples was captured, that the signal is not
all zero, and that its absolute peak exceeds a conservative wiring-diagnostic
threshold. After the rename, it verifies the committed WAV file size. These
checks exercise the raw-audio path needed by TC-027 and the progressive,
complete-record persistence behavior needed by TC-030 and TC-031.

If the application leaves a `.tmp` file, the capture was interrupted or an
I2S/SD error occurred. The file is intentionally retained for diagnosis and is
not treated as a complete payload.

## Relationship to the production firmware

The WAV file is an audio-only validation payload, not yet the complete
canonical ecoacoustic record. Its sample blocks, conversion rule, capture
length, failure semantics, and progressive persistence can be moved into the
corresponding EASNFW-SENSOR sampling, processing, and storage modules. Future
record metadata and environmental samples can then wrap the validated audio
body without replacing the acquisition path.
