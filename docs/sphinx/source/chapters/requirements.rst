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
checks required for acquisition succeed, EASNFW shall schedule any pending
ecoacoustic records from previous reset cycles for transmission to the cloud
platform. Pending transmissions shall not postpone a scheduled acquisition;
transmission shall be paused or deferred when necessary to honor the sampling
schedule.

REQ-004: Self-test Sequence Failure
===================================

After the power-on log payload transmission step, whether the payload
transmission has happened or been skipped, if any of the self-test sequence
checks fails, EASNFW shall store the failed checks and classify the failure as
acquisition-blocking, transmission-blocking, or non-blocking.

Failed checks storage to NVS is conditioned to the self-test sequence failed
checks not impacting EASNFW's ability to do it. A transmission-blocking failure
shall not cause a reset and shall not prevent acquisition when local sensing and
storage remain operational. An acquisition-blocking failure shall prevent
normal acquisition and place the affected component in a diagnosable safe or
degraded state. Reset may be used only for faults explicitly classified as
recoverable by reset and shall be subject to a bounded reset policy.

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
     - ``PARAM_AUDIO_SAMPLE_RATE`` kHz (initial baseline: 48 kHz)
   * - Sample width
     - ``PARAM_AUDIO_SAMPLE_WIDTH`` bits (initial baseline: 16 bits)
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
has been processed, EASNFW shall append the result to a temporary record in
mass storage. Once every expected block and the associated metadata have been
written and validated, EASNFW shall atomically mark the record as complete and
eligible for transmission. Incomplete records shall not be transmitted.

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

After the cloud platform confirms that the complete ecoacoustic record has
been durably stored, EASNFW shall mark the local record as delivered. The
record may then be removed according to a configurable retention policy.

Receipt of a record or fragment by EASNFW-CLOUD alone shall not be considered
confirmation of cloud delivery.

REQ-011: Payload Transmission Error Handling
============================================

When transmission of any payload to the cloud platform fails with a retryable
error, EASNFW shall retry up to ``PARAM_NUM_RETRY_TX_CLOUD`` times. If all
immediate retries fail, EASNFW shall preserve the associated record, store
failure details, place the record back in the pending-transmission queue, and
defer further attempts until a subsequent transmission window. Loss of
connectivity shall not, by itself, cause a system reset or stop scheduled data
acquisition.

Retry algorithm: exponential backoff with base 2 (:math:`t[s]=2^c`, where
:math:`t[s]` is the time interval between retries in seconds, and :math:`c` is
the retry count), subject to a configurable maximum delay and optional jitter.

.. _section_req_save_energy:

REQ-012: Save Energy While Idle
===============================

While EASNFW is idle, EASNFW shall enter an energy-saving state.

The production image shall expose sufficient instrumentation to measure, at a
minimum, energy consumed per acquisition, processing, storage, and transmission
cycle. Acceptance limits for each operating state shall be established after
the first hardware characterization campaign.

.. _section_logging:

REQ-013: Logging
================

The firmware shall log information relevant to verifying correct system
operation and identifying and diagnosing failures, in a way that logs can be
monitored in real time from a host PC through USB.

REQ-014: Canonical Ecoacoustic Record Assembly
==============================================

EASNFW-SENSOR shall assign a globally unique ``record_id`` to each acquisition
event and assemble the canonical ecoacoustic record before transmission. The
record shall include explicit format, acquisition-configuration, and
processing-algorithm versions so that stored data remains interpretable after
firmware updates.

REQ-015: Inter-component Transfer Integrity
===========================================

EASNFW shall transfer records between EASNFW-SENSOR and EASNFW-CLOUD using a
versioned, fragmented protocol with integrity checking. EASNFW-CLOUD shall
reject corrupted, incomplete, or unsupported frames without acknowledging them
as successfully received.

REQ-016: Idempotent Cloud Delivery
==================================

Each cloud transmission shall identify the corresponding ``record_id``. The
cloud platform shall treat repeated delivery of the same ``record_id`` as an
idempotent operation and shall return a durable-storage acknowledgement for a
record that has already been committed.

REQ-017: Intermediate Buffer Ownership
======================================

Audio and payload data shall be transferred between threads using bounded
buffers and explicit ownership. When a downstream stage cannot accept data,
EASNFW shall apply a defined backpressure or loss policy and shall not silently
discard audio blocks, record fragments, or acknowledgements.

REQ-018: Time Validity
======================

Every ecoacoustic record shall indicate whether its timestamp is synchronized
and the source used for synchronization. A record acquired without valid
absolute time shall retain monotonic timing information and shall not be
silently assigned an unverified wall-clock timestamp.
