************
Verification
************

HIL Verification Image Test Cases
=================================

Since the HIL verification image does not execute business logic automatically
out of reset, but instead exposes a command line interface (CLI) over USB that
triggers specific code paths on demand, each test case below assumes it is
driven by a host PC issuing CLI commands to EASNFW-SENSOR.

REQ-001: Self-test Sequence
---------------------------

TC-001: NVS check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the NVS read/write self-test check reports
success when NVS is functional.

**Preconditions:** NVS is in a known-good state.

**Procedure:**

1. Trigger the NVS check of the self-test sequence.
2. Observe the returned result and the USB log output.

**Expected result:** The check reports success after data was written to and
read back from NVS correctly.

TC-002: NVS check reports failure when NVS is faulty
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the NVS check correctly reports failure when
NVS cannot be written to or read from.

**Procedure:**

1. Inject an NVS fault via the test bench by simulating a write failure.
2. Trigger the NVS check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure after data could not be written
to NVS.

TC-003: Mass storage check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the mass storage read/write self-test check
reports success when the SD card is present and functional.

**Preconditions:** Mass storage is in a known-good state: SD card is inserted
and functional.

**Procedure:**

1. Trigger the mass storage check of the self-test sequence.
2. Observe the returned result and the USB log output.
3. Read the mass storage contents.

.. note::

   This test case implies the need for a CLI command to dump mass storage
   contents.

**Expected result:** The check reports success after data was written to and
read back from mass storage correctly.

TC-004: Mass storage check reports failure when the SD card is absent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the mass storage check reports failure when
the SD card is missing or unresponsive.

**Procedure:**

1. Remove the SD card or otherwise inject a mass storage fault via the test
   bench.
2. Trigger the mass storage check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure after data could not be written
to mass storage.

TC-005: Audio sensor sampling check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the audio sensor check reports success when
audio samples can be acquired.

**Preconditions:** The audio sensor is connected and operational.

**Procedure:**

1. Trigger the audio sensor check of the self-test sequence.
2. Observe the returned result and the USB log output.

**Expected result:** The check reports success after a block of audio sample
was acquired successfully.

TC-006: Audio sensor sampling check reports failure when disconnected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the audio sensor check reports failure when
the sensor cannot be sampled.

**Procedure:**

1. Disconnect the audio sensor or otherwise inject an audio sensor fault.
2. Trigger the audio sensor check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure because an audio sample could
not be acquired.

TC-007: Environmental sensor sampling check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the environmental sensor check reports success
when a sample can be acquired from the BME688.

**Preconditions:** The environmental sensor is connected over I2C and
operational.

**Procedure:**

1. Trigger the environmental sensor check of the self-test sequence.
2. Observe the returned result and the USB log output.

**Expected result:** The check reports success after a sample of each
environmental variable sensed by BME688 was acquired successfully.

TC-008: Environmental sensor sampling check reports failure when disconnected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the environmental sensor check reports failure
when the sensor cannot be sampled.

**Procedure:**

1. Disconnect the environmental sensor from the I2C bus or otherwise inject
   an environmental sensor fault.
2. Trigger the environmental sensor check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure because no samples could not be
acquired from the environmental sensor.

TC-009: EASNFW-SENSOR/EASNFW-CLOUD communication check succeeds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the inter-component communication check reports
success when the SPI link between EASNFW-SENSOR and EASNFW-CLOUD is functional.

**Preconditions:** Both boards are connected via SPI and are in a known-good
state.

**Procedure:**

1. Trigger the inter-component communication check of the self-test sequence.
2. Observe the returned result and the USB log output.

**Expected result:** The check reports success after communication over the SPI
link completed successfully.

TC-010: EASNFW-SENSOR/EASNFW-CLOUD communication check reports failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the communication check reports failure when
the SPI link is unavailable.

**Procedure:**

1. Disconnect the SPI link between the two boards or power off EASNFW-CLOUD.
2. Trigger the inter-component communication check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure because communication over the
SPI link could not be completed.

TC-011: Cloud platform transmission check succeeds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the check confirming data can be transmitted
to the cloud platform reports success under normal network conditions.

**Preconditions:** LTE-M connectivity available and cloud platform reachable.

**Procedure:**

1. Trigger the cloud transmission check of the self-test sequence.
2. Observe the returned result and the USB log output.

**Expected result:** The check reports success because data was transmitted
to the cloud platform successfully over the available LTE-M connection.

TC-012: Cloud platform transmission check reports failure without connectivity
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the check reports failure when the LTE-M link
or the cloud platform is unreachable.

**Procedure:**

1. Disable LTE-M connectivity (e.g. by removing the SIM card).
2. Trigger the cloud transmission check of the self-test sequence.
3. Observe the returned result and the USB log output.

**Expected result:** The check reports failure after data could not be
transmitted over LTE-M.

TC-013: Full self-test sequence runs all checks and aggregates results
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a single invocation of the full self-test sequence
executes all six checks (NVS, mass storage, audio sensor, environmental sensor,
inter-component communication, cloud transmission) and reports an aggregated
pass/fail result per check.

**Preconditions:** All modules checked by the self-test sequence are in a
known-good state.

**Procedure:**

1. Trigger the full self-test sequence.
2. Observe the returned result and the USB log output.
3. Repeat with exactly one subsystem faulted at a time (six repetitions,
   one per check).

**Expected result:** In step 1, all six checks report success. In each
repetition of step 2, exactly the faulted check reports failure and the
remaining five report success; this is all reflected in the returned CLI result
/ USB log.

REQ-002: Power-On Log Payload Transmission
------------------------------------------

TC-014: Power-on log payload transmitted when all checks succeed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the power-on log payload is transmitted to the
cloud platform when the self-test sequence completes with no failures.

**Preconditions:** All modules in a known-good state.

**Procedure:**

2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface (or a mock/stub cloud endpoint) and
   observe the returned result and USB log output.

**Expected result:** The power-on log payload is transmitted and its content
matches the current firmware revisions, the last reset reason, and the
self-test results (all passing); this is reflected in the returned CLI result /
USB log.

TC-015: Power-on log payload transmitted despite a non-blocking failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the power-on log payload is still transmitted
when a failed check does not impact the ability to transmit data to the
cloud platform.

**Procedure:**

1. Fault a subsystem that is unrelated to the transmit path (e.g. the
   audio sensor).
2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface and observe the returned result and USB
   log output.

**Expected result:** The power-on log payload is transmitted, and its content
reflects the audio sensor check as failed; this is reflected in the returned
CLI result / USB log.

TC-016: Power-on log payload skipped when transmission is impacted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the power-on log payload transmission is skipped
when a failed check does impact EASNFW's ability to transmit data to the cloud
platform (e.g. the cloud transmission check itself, or the inter-component
communication check).

**Procedure:**

1. Fault the SPI link between EASNFW-SENSOR and EASNFW-CLOUD (or disable
   LTE-M connectivity).
2. Trigger the full self-test sequence.
3. Observe the returned result and USB log output.

**Expected result:** No power-on log payload transmission is attempted, and
this is reflected in the returned CLI result / USB log.

REQ-003: Pending Ecoacoustic Data Transmission
----------------------------------------------

TC-017: Pending ecoacoustic records transmitted after full self-test success
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that ecoacoustic record data pending from a previous
reset cycle is transmitted to the cloud platform after the power-on log payload
step, when all self-test checks succeed.

**Preconditions:** One or more pending ecoacoustic records are present in mass
storage (simulating records left over from a prior cycle); all modules are in a
known-good state.

**Procedure:**

1. Trigger a reset, then the full self-test sequence.
2. Monitor the cloud-facing interface after the power-on log payload has
   been sent and observe the returned result and USB log output.
3. Inspect mass storage contents.

**Expected result:** All pending ecoacoustic record payloads are transmitted to
the cloud platform; the records are removed from mass storage after successful
transmission; all this is reflected in the returned CLI result / USB log.

TC-018: Pending ecoacoustic records not transmitted after a self-test failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that pending ecoacoustic record data is not transmitted
when at least one self-test check fails.

**Preconditions:** One or more complete ecoacoustic records are present in mass
storage.

**Procedure:**

1. Fault any module checked by the self-test sequence.
2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface after the power-on log payload
   step and observe the returned result and USB log output.
4. Inspect mass storage contents.

**Expected result:** No pending ecoacoustic record payloads are transmitted and
the records remain in mass storage.

TC-019: No pending data results in no transmission attempt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify graceful behavior when there is no pending ecoacoustic
data to transmit.

**Preconditions:** Mass storage contains no complete ecoacoustic records, all
modules are in a known-good state.

**Procedure:**

1. Trigger the full self-test sequence.
2. Monitor the cloud-facing interface after the power-on log payload
   step and observe the returned result and USB log output.

**Expected result:** The power-on log payload is transmitted; no ecoacoustic
record transmission is attempted and no error is reported.

REQ-004: Self-test Sequence Failure
-----------------------------------

TC-020: Failed checks are stored to NVS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, after the power-on log payload step, the set of
failed self-test checks is written to NVS.

**Procedure:**

1. Fault a subsystem other than NVS (e.g. the audio sensor).
2. Trigger the self-test sequence.
3. After the power-on log payload step completes (or is skipped), read
   back the failed-checks record from NVS.

.. note::

   This test case implies the need for a CLI command to dump the NVS content.

**Expected result:** The stored record identifies the audio sensor check
as failed.

TC-021: Device resets after storing failed checks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that EASNFW resets after the failed-checks storage
step, following the power-on log payload step.

**Procedure:**

1. Fault a subsystem other than NVS.
2. Trigger the full self-test sequence and observe device behavior via the USB
   logging interface.

**Expected result:** The sequence of events observed is: self-test sequence
runs to completion, power-on log payload step occurs (sent or skipped per
REQ-002), failed checks are stored to NVS, device resets.

TC-022: Failed-checks storage is skipped when NVS itself is faulty
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that EASNFW does not attempt to store the failed
checks to NVS when the NVS check itself is among the failures.

**Procedure:**

1. Fault NVS.
2. Trigger the full self-test sequence and observe the returned result and USB
   logs.

**Expected result:** No attempt to write the failed-checks record to NVS
is logged; the device still resets after the power-on log payload step.

TC-023: Device resets even when the power-on log payload step was skipped
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify the reset still occurs when a failure caused the
power-on log payload transmission to be skipped (per REQ-002/TC-016).

**Procedure:**

1. Fault the SPI link between EASNFW-SENSOR and EASNFW-CLOUD.
2. Trigger the self-test sequence and observe the USB log.

**Expected result:** The power-on log payload transmission is skipped,
the failed checks are stored to NVS, and the device resets.

REQ-005: Audio and Environmental Data Sampling
-------------------------------------------------

TC-024: Sampling starts after the initialization transmissions complete
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that audio and environmental sampling begins
immediately after the power-on log payload and any pending ecoacoustic
data have been transmitted.

**Procedure:**

1. Ensure all subsystems are in a known-good state and no ecoacoustic data
   pending transmission exists.
2. Trigger the self-test sequence and monitor the USB log for the start
   of sampling.

**Expected result:** Sampling begins as soon as the power-on log payload is
transmitted, without waiting for ``PARAM_INTER_TRACK_INTERVAL``.

TC-025: Inter-track interval is honored between consecutive tracks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, after the first audio track, sampling of the
next track does not begin until ``PARAM_INTER_TRACK_INTERVAL`` seconds
have elapsed since the end of the previous track's sampling.

**Procedure:**

1. Configure a short test value for ``PARAM_INTER_TRACK_INTERVAL`` and
   ``PARAM_TRACK_LEN`` via the CLI.
2. Let two consecutive tracks be sampled.
3. Measure the elapsed time between the end-of-sampling timestamp of
   track 1 and the start-of-sampling timestamp of track 2.

**Expected result:** The measured interval is ``PARAM_INTER_TRACK_INTERVAL``
seconds.

TC-026: One environmental sample and two timestamps are captured per track
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that exactly one sample of each environmental
variable is captured at the start of a track, along with a start
timestamp and an end timestamp.

**Procedure:**

1. Trigger a single track's sampling.
2. Inspect the CLI/log output for the captured environmental sample and
   the two timestamps.

**Expected result:** Exactly one temperature, humidity, pressure, and VOCs
sample is captured; a start timestamp is captured before audio sampling begins
and an end timestamp after it ends, with end greater than start.

TC-027: Audio sampling parameters match configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that acquired audio matches the configured parameters: 1
channel (mono), ``PARAM_AUDIO_SAMPLE_RATE``-kHz sample rate,
``PARAM_AUDIO_SAMPLE_WIDTH``-bit sample width, and ``PARAM_TRACK_LEN``-second
duration.

**Procedure:**

1. Configure known test values for ``PARAM_AUDIO_SAMPLE_RATE``,
   ``PARAM_AUDIO_SAMPLE_WIDTH``, and ``PARAM_TRACK_LEN``.
2. Feed a known test signal to the audio sensor input via the test bench.
3. Retrieve the raw acquired samples and verify channel count, sample rate,
   sample width, and total duration.

.. note::

   This test case implies the need for a CLI command to dump the raw audio
   samples to the host PC for analysis, and the ability to skip the audio
   processing step so that the raw samples can be retrieved.

**Expected result:** All parameters match the configured parameter values
within acceptable tolerance.

TC-028: Acquired audio is band-limited as configured
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the acquired audio signal's energy outside the
``[PARAM_AUDIO_SAMPLE_BW_LO, PARAM_AUDIO_SAMPLE_BW_HI]`` (Hz) band is
attenuated as expected.

**Procedure:**

1. Configure test values for the bandwidth parameters.
2. Feed a known test signal to the audio sensor input via the test bench.
3. Retrieve the raw acquired track and perform a frequency-domain analysis on
   the host PC.

**Expected result:** Signal content within the configured band is
preserved; content outside the band is attenuated or not present.

REQ-006: Audio Data Processing
------------------------------

TC-029: Processing is triggered per block of required samples
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that EASNFW-SENSOR processes each acquired block of
``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` samples according to the
audio processing algorithm.

.. warning::

   Blocked / pending. The audio processing algorithm and the value of
   ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` is TBD. This test case should
   be detailed once the algorithm is specified.

REQ-007: Audio Data Persistence
------------------------------------

TC-030: Processed block results are persisted to mass storage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, after each block of
``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` samples is processed, the
processing result is written to mass storage.

**Procedure:**

1. Trigger sampling and processing of a full track.
2. After each processed block, inspect mass storage contents.

**Expected result:** A new persisted entry appears in mass storage after
each processed block, matching the processing output for that block.

TC-031: All blocks of a track are persisted by the time the track completes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, once an entire track has finished processing,
mass storage holds the processed results for every block of that track.

**Procedure:**

1. Trigger sampling and processing of a full track.
2. After processing completes, list all mass storage-persisted blocks
   associated with the track.

**Expected result:** The number of persisted blocks equals ``PARAM_TRACK_LEN``
worth of samples divided into blocks of
``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING``, with no gaps.

REQ-008: Environmental Data and Timestamps Persistence
------------------------------------------------------

TC-032: Environmental data and timestamps persisted after track completion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the environmental sample and the two timestamps
captured for a track (REQ-005) are written to mass storage once all of the
track's audio samples have been processed.

**Procedure:**

1. Trigger sampling and processing of a full track.
2. After the last block is processed, inspect mass storage for the
   environmental data and timestamp entries associated with the track.

**Expected result:** The persisted entry's environmental values and
timestamps match those captured during REQ-005's sampling step for the
same track.

REQ-009: Ecoacoustic Data Transmission to the Cloud Platform
------------------------------------------------------------

TC-033: Ecoacoustic record transmitted once fully stored
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a complete ecoacoustic record (processed audio
track, environmental samples, and timestamps) is transmitted to the cloud
platform once all of its data is stored in mass storage.

**Preconditions:** Cloud connectivity available.

**Procedure:**

1. Trigger sampling and processing of a full track through to persistence
   (REQ-007/REQ-008).
2. Monitor the cloud-facing interface for the resulting ecoacoustic
   record payload(s).

**Expected result:** The record is transmitted, split across one or more
payloads as needed.

REQ-010: Ecoacoustic Data Removal from Mass Storage
---------------------------------------------------

TC-034: Record removed from mass storage after successful transmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that an ecoacoustic record is deleted from mass
storage once it has been fully transmitted to the cloud platform.

**Procedure:**

1. Trigger sampling, processing, persistence and transmission of a full
   track (REQ-005 through REQ-009).
2. After transmission completes, inspect mass storage for the record.

**Expected result:** The record is no longer present in mass storage.

TC-037: Record retained in mass storage when transmission fails
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a record is not removed from mass storage
while its transmission has not yet succeeded.

**Procedure:**

1. Disable cloud connectivity.
2. Trigger sampling, processing, and persistence of a full track.
3. Allow the transmission attempt(s) to occur and fail.
4. Inspect mass storage for the record.

**Expected result:** The record remains present in mass storage.

REQ-011: Payload Transmission Error Handling
--------------------------------------------

TC-038: Failed transmission is retried
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a failed payload transmission is retried.

**Procedure:**

1. Disable cloud connectivity.
2. Trigger transmission of any payload (e.g. a pending ecoacoustic
   record).
3. Monitor the USB log for retry attempts.

**Expected result:** At least one retry attempt is observed following the
initial failure.

TC-039: Retries follow exponential backoff with base 2
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that the time interval between successive retries
follows :math:`t[s]=2^c`, where ``c`` is the retry count, per the
"exponential backoff with base 2" description.

**Procedure:**

1. Disable cloud connectivity.
2. Trigger transmission of a payload.
3. Record the timestamps of the initial attempt and each subsequent retry
   from the USB log.

**Expected result:** The interval before retry ``c`` is
:math:`2^c` seconds (within timing tolerance), for c = 1, 2, 3, ...

TC-040: Retry count does not exceed the configured maximum
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that EASNFW stops retrying after
``PARAM_NUM_RETRY_TX_CLOUD`` attempts.

**Procedure:**

1. Configure a small test value for ``PARAM_NUM_RETRY_TX_CLOUD``.
2. Disable cloud connectivity.
3. Trigger transmission of a payload and count retry attempts from the
   USB log.

**Expected result:** Exactly ``PARAM_NUM_RETRY_TX_CLOUD`` retries occur
after the initial attempt, then no further retries are made.

TC-041: Failure details stored to NVS and device resets after max retries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, once ``PARAM_NUM_RETRY_TX_CLOUD`` retries have
failed, EASNFW stores failure details (including information identifying
the failed payload) to NVS and resets.

**Procedure:**

1. Configure a small test value for ``PARAM_NUM_RETRY_TX_CLOUD``.
2. Disable cloud connectivity.
3. Trigger transmission of a payload and let all retries fail.
4. After the reset, read back the failure record from NVS.

**Expected result:** The device resets after the final failed retry; the
NVS record identifies the payload whose transmission failed.

Known Gaps
==========

Requirement :ref:`section_req_save_energy` does not have associated test cases
because it does not explicitly define when EASNFW is considered idle nor
measurable criterion for the energy saving.

Requirement :ref:`section_logging` does not have associated test cases because
it does not explicitly define the activities that EASNFW should log, and it is
rather left to the developer's discretion.

..
   HIL Verification Image Special Commands
   -----------------------------------------
   
   The HIL verification image supports special commands to return data to be verified
   
   SC-001: Sample and Process Audio Track
   """"""""""""""""""""""""""""""""""""""
   
   Sample an audio track, ideally with a known input, according to parameters
   specified either at compile-time or via command line (test case arguments are
   used to specify the source of the parameters, as well as the parameters
   themselves in case the compile-time parameters are not used), and return the
   captured samples to the connected host PC.
   
   Verify the sample count, sample rate, sample width, bandwidth, track duration
   and sample ordering.

