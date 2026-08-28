************
Requirements
************

REQ-001: Self-test Sequence
===========================

Out of reset, EASNFW shall execute the self-test sequence, where it shall
verify that:

* data can be written to and read from NVS;
* data can be written to and read from mass storage;
* audio data can be sampled from the audio sensor;
* environmental data can be sampled from the environmental sensor;
* EASNFW-SENSOR and EASNFW-CLOUD can communicate; and
* data can be transmitted to the cloud platform.

REQ-002: Power-On Log Payload Transmission
==========================================

If all the self-test sequence checks succeed, or if the failed checks do not
impact EASNFW's ability to transmit data to the cloud platform, EASNFW shall
transmit the power-on log payload to the cloud platform. 

REQ-003: Pending Ecoacoustic Data Transmission
==============================================

After the power-on log payload transmission, if all the self-test sequence
checks succeed, EASNFW shall transmit any pending ecoacoustic record data from
previous reset cycles to the cloud platform.

REQ-004: Self-test Sequence Failure
===================================

After the power-on log payload transmission step, whether the payload
transmission has happened or been skipped, if any of the self-test sequence
checks fails, EASNFW shall store the failed checks to NVS and reset after the
power-on log payload transmission step.

Failed checks storage to NVS is conditioned to the self-test sequence failed
checks not impacting EASNFW's ability to do it.

REQ-005: Audio and Environmental Data Sampling
==============================================

After transmitting the power-on log payload and any pending ecoacoustic record
data to the cloud platform, or after ``PARAM_INTER_TRACK_INTERVAL`` seconds
(expected to be around 840 seconds, or 14 minutes) have passed since the end of the sampling of the last audio track sampling,
EASNFW shall acquire one sample of each environmental data variable, capture a
timestamp to mark the beginning of the audio sampling, acquire a number of
audio samples according to the parameters defined below, and capture another
timestamp to mark the end of the audio sampling.

Parameters for audio sampling:

.. list-table:: Audio sampling parameters
   :header-rows: 1
   :widths: 30 70

   * - Parameter
     - Value
   * - Number of channels
     - 1 (mono audio)
   * - Sample rate
     - ``PARAM_AUDIO_SAMPLE_RATE`` kHz (expected to be around 44.1 kHz)
   * - Sample width
     - ``PARAM_AUDIO_SAMPLE_WIDTH`` bits (expected to be around 16 bits)
   * - Audio track duration
     - ``PARAM_TRACK_LEN`` seconds (expected to be around 60 seconds)
   * - Bandwidth
     - ``PARAM_AUDIO_SAMPLE_BW_LO`` Hz to ``PARAM_AUDIO_SAMPLE_BW_HI`` Hz

REQ-006: Audio Data Processing
==============================

After a block of ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` audio samples
have been acquired, EASNFW shall process this block according to the audio
processing algorithm.

.. note::

   Details on the audio processing algorithm are still TBD, this requirement is
   subject to significant change and expansion.
   ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` will be determined once more
   details on the audio processing algorithm are defined.

REQ-007: Audio Data Persistence
===============================

After a block of ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` audio samples
have been processed, EASNFW shall store the results to mass storage.

REQ-008: Environmental Data and Timestamps Persistence
======================================================

After all the audio samples relative to a full audio track are processed,
EASNFW shall store the associated environmental data and timestamps to mass
storage.

REQ-009: Ecoacoustic Data Transmission to the Cloud Platform
============================================================

After all the data relative to an ecoacoustic record is stored to mass storage,
EASNFW shall transmit it to the cloud platform.

REQ-010: Ecoacoustic Data Removal from Mass Storage
===================================================

After all the data relative to an ecoacoustic record is transmitted to the
cloud platform, EASNFW shall remove it from mass storage.

REQ-011: Payload Transmission Error Handling
============================================

When transmission of any payload to the cloud platform fails, EASNFW shall
retry up to ``PARAM_NUM_RETRY_TX_CLOUD`` times. If ``PARAM_NUM_RETRY_TX_CLOUD``
retries fail, EASNFW shall store failure details including information about
the payload whose transmission failed to NVS and reset.

Retry algorithm: exponential backoff with base 2 (:math:`t[s]=2^c`, where
:math:`t[s]` is the time interval between retries in seconds, and :math:`c` is
the retry count).

.. _section_req_save_energy:

REQ-012: Save Energy While Idle
===============================

While EASNFW is idle, EASNFW shall enter an energy-saving state.

.. note::

   No measurable criterion is defined because applicable energy-saving
   strategies will be investigated during development.

.. _section_logging:

REQ-013: Logging
================

The firmware shall log information relevant to verifying correct system
operation and identifying and diagnosing failures, in a way that logs can be
monitored in real time from a host PC through USB.

