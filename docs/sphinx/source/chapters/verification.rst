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

**Expected result:** The check reports failure because no samples could be
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

1. Trigger the full self-test sequence.
2. Monitor the cloud-facing interface (or a mock/stub cloud endpoint) and
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

TC-017: Pending ecoacoustic records are scheduled after self-test success
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that ecoacoustic records pending from a previous reset
cycle are scheduled after the power-on log without blocking scheduled
acquisition.

**Preconditions:** One or more pending ecoacoustic records are present in mass
storage (simulating records left over from a prior cycle); all modules are in a
known-good state.

**Procedure:**

1. Trigger a reset, then the full self-test sequence.
2. Monitor the cloud-facing interface after the power-on log payload has
   been sent and observe the returned result and USB log output.
3. Inspect mass storage contents.

**Expected result:** Pending records enter the transmission queue in age order.
Transmission pauses or is deferred when necessary to start the next acquisition
on schedule. Records remain in mass storage until durable cloud delivery is
confirmed.

TC-018: Acquisition-capability failure prevents normal acquisition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a self-test failure affecting acquisition prevents
new acquisition while already committed records remain recoverable.

**Preconditions:** One or more complete ecoacoustic records are present in mass
storage.

**Procedure:**

1. Fault any module checked by the self-test sequence.
2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface after the power-on log payload
   step and observe the returned result and USB log output.
4. Inspect mass storage contents.

**Expected result:** No new track is acquired. Existing complete records remain
in mass storage and are neither corrupted nor deleted.

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

TC-021: Self-test failures are classified
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that self-test failures are classified as acquisition-
blocking, transmission-blocking, or non-blocking and produce the corresponding
system behavior.

**Procedure:**

1. In separate runs, fault the audio sensor, cloud connectivity, and a
   non-essential diagnostic function.
2. Trigger the full self-test sequence and observe the reported classification
   and subsequent acquisition/transmission behavior.

**Expected result:** The audio-sensor fault prevents normal acquisition; the
connectivity fault preserves local acquisition and storage while deferring
transmission; and the non-blocking fault is reported without preventing either
available function. None produces an unbounded reset loop.

TC-022: Failed-checks storage is skipped when NVS itself is faulty
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that EASNFW does not attempt to store the failed
checks to NVS when the NVS check itself is among the failures.

**Procedure:**

1. Fault NVS.
2. Trigger the full self-test sequence and observe the returned result and USB
   logs.

**Expected result:** No attempt to write the failed-checks record to NVS
is logged; the storage failure is reported through the remaining available
diagnostic channel and the component enters its defined degraded state.

TC-023: Connectivity failure does not reset or stop acquisition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a failure causing the power-on log transmission to
be skipped does not reset the device or prevent locally supported acquisition.

**Procedure:**

1. Fault the SPI link between EASNFW-SENSOR and EASNFW-CLOUD.
2. Trigger the self-test sequence and observe the USB log.

**Expected result:** The power-on log payload transmission is skipped, the
failure is stored, the modem enters the deferred-retry policy, and scheduled
acquisition continues without a connectivity-induced reset.

REQ-005: Audio and Environmental Data Sampling
-------------------------------------------------

TC-024: Sampling starts after initialization without waiting for backlog
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that audio and environmental sampling begins after
initialization and is not blocked by a pending-transmission backlog.

**Procedure:**

1. Ensure all acquisition subsystems are in a known-good state and populate
   mass storage with more pending records than can be sent before the next
   scheduled acquisition.
2. Trigger the self-test sequence and monitor the USB log for the start
   of sampling.

**Expected result:** Initial sampling begins after initialization without
waiting for the backlog to drain. Subsequent sampling honors
``PARAM_INTER_TRACK_INTERVAL`` while transmission is paused or deferred as
needed.

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

**Expected result:** The number of persisted blocks equals
``ceil(total_track_samples / REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING)``, with
no missing or duplicate sample ranges. The final block may be shorter than a
full processing block.

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
payloads as needed. The server validates and durably commits the complete
``record_id`` before returning a cloud commit acknowledgement.

REQ-010: Ecoacoustic Data Removal from Mass Storage
---------------------------------------------------

TC-034: Record marked delivered only after durable cloud acknowledgement
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that an ecoacoustic record is marked delivered only after
the cloud platform confirms durable storage, and is removed only when permitted
by the configured retention policy.

**Procedure:**

1. Trigger sampling, processing, persistence and transmission of a full
   track (REQ-005 through REQ-009).
2. Confirm link-level receipt without returning a cloud commit acknowledgement
   and inspect the record state.
3. Return the cloud commit acknowledgement and inspect the record state.
4. Apply the configured retention policy.

**Expected result:** Link-level receipt does not mark the record delivered.
Cloud commit marks it delivered. Removal occurs only when the retention policy
permits it.

TC-035: Record retained in mass storage when transmission fails
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

TC-036: Failed transmission is retried
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a failed payload transmission is retried.

**Procedure:**

1. Disable cloud connectivity.
2. Trigger transmission of any payload (e.g. a pending ecoacoustic
   record).
3. Monitor the USB log for retry attempts.

**Expected result:** At least one retry attempt is observed following the
initial failure.

TC-037: Retries follow exponential backoff with base 2
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

TC-038: Retry count does not exceed the configured maximum
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

TC-039: Failure is recorded and delivery deferred after maximum retries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that, once ``PARAM_NUM_RETRY_TX_CLOUD`` immediate retries
have failed, EASNFW records the failure, preserves the record, defers delivery,
and continues scheduled acquisition without resetting solely because the
network is unavailable.

**Procedure:**

1. Configure a small test value for ``PARAM_NUM_RETRY_TX_CLOUD``.
2. Disable cloud connectivity.
3. Trigger transmission of a payload and let all retries fail.
4. Read back the failure record and inspect the pending queue.
5. Keep connectivity unavailable through the next scheduled acquisition.

**Expected result:** The failure record identifies the affected ``record_id``;
the corresponding record remains pending in mass storage; no connectivity-
induced reset occurs; and the next acquisition starts according to schedule.

REQ-012: Save Energy While Idle
-------------------------------

TC-040: Operating-state energy is measurable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that acquisition, processing, storage, transmission, and
idle intervals can be identified in a current trace and assigned an energy
cost.

**Procedure:**

1. Power the complete node through a calibrated power profiler.
2. Run one shortened acquisition/transmission cycle with GPIO trace markers or
   timestamped logs identifying each operating state.
3. Integrate current over each marked interval.

**Expected result:** Every operating state has an identifiable interval and a
reported energy value. No thread prevents entry into the configured idle state
between scheduled activities.

REQ-014: Canonical Ecoacoustic Record Assembly
----------------------------------------------

TC-041: Complete record is committed atomically
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Objective:** Verify that a record becomes eligible for transmission only
after all expected blocks and metadata are persisted and validated.

**Procedure:**

1. Trigger acquisition and processing of one track.
2. Inspect mass storage while blocks are being appended.
3. Inspect the record again after the final block and metadata are written.

**Expected result:** The in-progress record is marked temporary and is not
queued for transmission. After completion it is atomically committed, has a
unique ``record_id``, and contains the configured schema, acquisition, and
processing versions.

TC-042: Interrupted record remains recoverable but is not transmitted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:**

1. Remove power while a record is being appended.
2. Restore power and allow storage recovery to run.
3. Inspect the transmission queue and mass storage.

**Expected result:** The partial record is detected and retained or quarantined
according to policy; it is never represented as a complete record and is not
transmitted.

REQ-015: Inter-component Transfer Integrity
-------------------------------------------

TC-043: Multi-frame record is reassembled correctly
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:** Transfer a record larger than one SPI frame and compare the
reassembled bytes and metadata on EASNFW-CLOUD with the committed source
record.

**Expected result:** All fragments are transferred in a bounded-memory manner,
the reassembled record is byte-for-byte equivalent to the source, and every
fragment receives a link acknowledgement.

TC-044: Corrupted or missing SPI fragment is rejected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:** Inject, in separate runs, a corrupted CRC, a missing fragment,
an out-of-order fragment, and an unsupported protocol version.

**Expected result:** EASNFW-CLOUD does not produce a cloud payload from an
invalid or incomplete transfer, reports the precise error, and does not issue a
successful link acknowledgement for the affected fragment or record.

REQ-016: Idempotent Cloud Delivery
----------------------------------

TC-045: Retransmission does not duplicate a cloud record
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:**

1. Deliver a complete record to the cloud platform.
2. Drop the cloud commit acknowledgement before it reaches EASNFW-SENSOR.
3. Allow EASNFW-SENSOR to retransmit the same ``record_id``.

**Expected result:** The cloud contains one logical record, returns a durable-
storage acknowledgement for the repeated ``record_id``, and the local record
is subsequently marked delivered.

REQ-017: Intermediate Buffer Ownership
--------------------------------------

TC-046: Slow processing applies the defined backpressure policy
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:** Artificially delay the Processing and Storage threads while
audio acquisition continues until the configured buffer pool is exhausted.

**Expected result:** No buffer is reused while owned by another stage. The
configured backpressure or loss policy is applied, and any lost interval is
explicitly represented in record quality flags and logs.

TC-047: SD writes do not introduce silent audio gaps
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:** Capture a deterministic continuous test signal while writing a
complete baseline track to the SD card and exercising worst-case expected SD
latency.

**Expected result:** Sample sequence and timestamps contain no unexplained
gaps, duplicates, or reordering; any detected overrun causes the track to be
flagged rather than silently accepted.

REQ-018: Time Validity
----------------------

TC-048: Unsynchronized acquisition is marked explicitly
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Procedure:** Boot the node without an available absolute-time source and
acquire one record, then restore time synchronization and acquire another.

**Expected result:** The first record contains monotonic timing and an invalid
or unsynchronized wall-clock flag. The second contains a synchronized timestamp
and identifies its synchronization source.

Known Gaps
==========

Requirement :ref:`section_logging` does not have associated test cases because
it does not explicitly define the activities that EASNFW should log, and it is
rather left to the developer's discretion.

Numerical energy acceptance limits remain open pending the first complete-node
characterization campaign. TC-040 defines how the baseline measurements shall
be collected.

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
