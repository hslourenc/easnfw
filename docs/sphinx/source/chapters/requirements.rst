************
Requirements
************

.. _section_req_ota_rollback:

REQ-022: FUOTA Rollback
=======================

Out of reset, EASNFW shall validate the loaded firmware image by checking
whether it is signed with the correct key and, if not, EASNFW shall roll back to the
previously known-good firmware version.

REQ-023: Self-test Sequence
===========================

After validating the loaded firmware image (:ref:`section_req_ota_rollback`), if
the firmware image is found to be acceptable, EASNFW shall execute the self-test
sequence to check whether the following functions are operational:

* NVS access (data can be written to and read from NVS);
* mass storage access (data can be written to and read from mass storage);
* audio data acquisition (audio data can be sampled from the audio sensor);
* environmental data acquisition (environmental data can be sampled from the
  environmental sensor);
* inter-firmware link (EASNFW-SENSOR and EASNFW-CLOUD can communicate); and
* cloud platform communication (data can be transmitted to and fetched from the
  cloud platform).

.. _section_req_power_on_log_tx:

REQ-024: Power-On Log Payload Transmission
==========================================

When the self-test sequence finishes, if both the cloud platform communication
and the inter-firmware link are classified as operational by the self-test
sequence, EASNFW shall schedule the transmission of the power-on log payload to
the cloud platform. Otherwise, EASNFW shall enter diagnostic state.

The power-on log payload includes:

* the revision of EASNFW-SENSOR and EASNFW-CLOUD;
* the reset reason from NVS, if NVS access is classified as operational by the
  self-test sequence;
* the reset reason from nRF5340's RESETREAS register;
* the reset reason from nRF9151's RESETREAS register, if the inter-firmware link
  is classified as operational by the self-test sequence; and
* the results of the self-test sequence.

REQ-025: Reset Reason and History Update
========================================

When the power-on log payload transmission is scheduled, EASNFW shall attempt to
clear the reset reason from NVS and update the reset history on NVS.

The reset history on NVS is a ring buffer that contains a rolling history of the
date and time of the last ``PARAM_MAX_RECOVERY_RESETS`` resets.

.. note::

   "attempt" is used here because at this point, the self-test sequence may have
   found NVS access as faulty.

.. note::

   The reset history is needed for the recovery reset mechanism described in
   :ref:`section_req_diag_state`.

REQ-026: Pending Ecoacoustic Data Transmission
==============================================

When the attempt to clear the reset reason from NVS is finished, if all the
following are classified as operational by the self-test sequence:

* mass storage access;
* inter-firmware link; and
* cloud platform communication;

EASNFW shall schedule any pending ecoacoustic records from previous reset cycles
for transmission to the cloud platform.

Otherwise, EASNFW shall enter diagnostic state.

.. _section_req_diag_state:

REQ-027: Diagnostic State
=========================

When EASNFW enters the diagnostic state, EASNFW shall:

#. store the reason why it entered diagnostic state to NVS, identifying the
   affected ecoacoustic record (``record_id``) when applicable;
#. transmit a diagnostic log payload to the cloud;

#. if the reason why it entered diagnostic state is recoverable by reset, and
   the recovery reset counter is less than ``PARAM_MAX_RECOVERY_RESETS``:

   #. increment the recovery reset counter;
   #. reset;

#. otherwise:

  #. stop the data acquisition cycle, if it is running;
  #. start the FUOTA cycle (described in :ref:`section_req_ota_update`), if it has
     not already been started;
  #. blink an LED with a period of 30 seconds to provide a visual indication that
     the device is in diagnostic state.

The diagnostic log payload contains all the information of the power-on log
payload (see :ref:`section_req_power_on_log_tx`) plus the reason why it entered
diagnostic state.

.. note::

   All actions in this requirement should be considered attempts, as their
   success depends on the specific failure that led EASNFW to enter diagnostic
   state (e.g. if it was a failure related to NVS, EASNFW may not be able to
   store the reason why it entered diagnostic state to NVS).

.. note::

   As of the current specification, no recoverable-by-reset failures have
   been identified. Such failures will be properly defined if and when a
   plausible case is identified.

.. note::

   An implementation consequence of the described recovery reset mechanism is
   that the recovery reset counter needs to be stored in NVS to persist across
   resets.

REQ-028: Recovery Reset Counter Clearing
========================================

After ``PARAM_RECOVERY_RESET_WINDOW`` seconds (initial baseline: 120 seconds) of
uptime, EASNFW shall clear the recovery reset counter.

.. _section_req_data_acq_cycle:

REQ-029: Data Acquisition Cycle
===============================

After the transmission of the power-on log payload and any pending ecoacoustic
record from a previous reset cycle is scheduled, if the self-test sequence
identifies all the checked capabilities as operational, EASNFW shall start
the data acquisition cycle.

For each data acquisition cycle, EASNFW shall wait until there is sufficient
mass storage free-space for a new ecoacoustic record, acquire audio and
environmental data according to :ref:`section_req_audio_acquisition` and
:ref:`section_req_environ_acquisition` and wait ``PARAM_INTER_TRACK_INTERVAL``
seconds (initial baseline: 840 seconds) before starting the next data
acquisition cycle. 

.. note::

   Ideally, EASNFW should not need to wait for mass storage free-space, as
   :ref:`section_req_ecoacoustic_data_rm` takes care of immediately removing
   delivered ecoacoustic records. EASNFW should only ever need to wait in case
   there are issues transmitting the ecoacoustic records to the cloud.

REQ-030: Data Acquisition Cycle Priority
========================================

When an action from a requirement different from
:ref:`section_req_data_acq_cycle` conflicts with any of its actions, EASNFW
shall prioritize the actions from :ref:`section_req_data_acq_cycle`.

.. _section_req_audio_acquisition:

REQ-031: Audio Data Acquisition
===============================

For each data acquisition cycle, EASNFW shall capture a
``PARAM_TRACK_LEN``-second audio track using the parameters from
:ref:`table_audio_sample_param` with ISO-8601 timestamps with UTC offset
identifying the beginning and the end of the capture.

.. _table_audio_sample_param:

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
     - ``PARAM_TRACK_LEN`` seconds (initial baseline: 60 seconds)
   * - Bandwidth
     - ``PARAM_AUDIO_SAMPLE_BW_LO`` Hz to ``PARAM_AUDIO_SAMPLE_BW_HI`` Hz

.. _section_req_environ_acquisition:

REQ-032: Environmental Data Acquisition
=======================================

For each data acquisition cycle, EASNFW shall capture one sample of each of the
tracked environmental variables with an ISO-8601 timestamp with UTC offset
identifying the moment of the capture.

Tracked environmental variables:

* temperature;
* pressure;
* humidity; and
* volatile organic compounds (VOCs).

REQ-033: Audio Data Processing
==============================

After a block of ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` audio samples
have been captured, EASNFW shall process this block according to the audio
processing algorithm.

.. note::

   Details on the audio processing algorithm are yet to be defined. This
   requirement is subject to significant change and expansion.
   ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` will be determined once more
   details on the audio processing algorithm are defined.

REQ-034: Ecoacoustic Data Persistence
=====================================

After a block of ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` audio samples has
been processed, EASNFW shall append the result to a temporary record in mass
storage. Once the results of every expected block (i.e. all blocks relative to a
data acquisition cycle), have been stored in mass storage and validated
according to the applicable validation process, EASNFW shall assemble the
canonical ecoacoustic record, store it in mass storage, atomically mark it as
complete and eligible for transmission, and schedule its transmission to the
cloud platform.

The canonical ecoacoustic record includes:

* the processed audio track;
* the environmental data;
* a globally unique ``record_id``;
* the sampling parameters used to capture the audio track;
* the timestamps associated to the audio and environmental data acquisition (as
  per :ref:`section_req_audio_acquisition` and
  :ref:`section_req_environ_acquisition`);
* the source used for timestamp synchronization; and
* the processing algorithm version.

.. note::

   The "applicable validation process" is to be identified during the
   implementation. It should be treated as a placeholder and not be considered
   for verification purposes for now. It is possible that no applicable
   validation process is identified.

.. note::

   The canonical ecoacoustic record may include additional metadata that the
   developers find relevant during the implementation, but for verification
   purposes, only the items listed above should be considered. This requirement
   and the relevant associated test cases will be updated when and if additional
   metadata is found to be needed.

.. _section_req_ecoacoustic_data_rm:

REQ-035: Ecoacoustic Data Removal
=================================

When a durable-storage acknowledgement for a given ecoacoustic record is
received from the cloud platform, EASNFW shall remove that ecoacoustic record
from mass storage.

REQ-036: Run-time Mass Storage Access Failure
=============================================

When mass storage access fails, EASNFW shall retry the operation if the failure
is transient, or enter diagnostic state otherwise.

.. note::

   No transient failures on mass storage access have been identified so far.
   This case is included as a placeholder and should not be considered for
   verification purposes right now. This requirement and the relevant associated
   test cases will be updated with a detailed retry policy if any transient
   failures on mass storage access are identified.

.. _section_req_tx_error:

REQ-037: Payload Transmission Error Handling
============================================

When transmission of any payload to the cloud platform fails, EASNFW shall:

#. if the failure is transient: retry after a uniformly random period in
   [(3/4)*min(``PARAM_MAX_RETRY_DELAY``, :math:`2^c`),
   min(``PARAM_MAX_RETRY_DELAY``, :math:`2^c`)], where `c` is the global
   (payload- and record-independent) retry count, starting at zero and
   stopping at ``PARAM_MAX_RETRY_COUNT``;
#. if the failure is permanent: enter diagnostic state.

The initial baseline for ``PARAM_MAX_RETRY_DELAY`` and ``PARAM_MAX_RETRY_COUNT``
is 30 seconds and 10, respectively.

.. note::

   Transient errors may include connection loss, registration timeout, DNS or
   TLS timeout, server unavailability, and retryable HTTP responses such as
   408, 429, and 5xx.

   Permanent errors may include: record-specific errors, such as malformed,
   corrupted, unsupported, or oversized payloads rejected by the cloud platform;
   or permanent transmission errors, such as modem hardware failure, inactive or
   rejected SIM service, invalid provisioning or credentials, or an unavailable
   data subscription or account balance.

.. note::

   Nothing special happens when the retry count reaches
   ``PARAM_MAX_RETRY_COUNT``; it simply stops incrementing.

REQ-038: Retry Count Reset
==========================

When a payload is successfully transmitted to the cloud platform, the retry
count (see :ref:`section_req_tx_error`) is reset to zero.

.. _section_req_save_energy:

REQ-039: Save Energy While Idle
===============================

While EASNFW is idle, EASNFW shall enter an energy-saving state.

.. note::

   From a firmware perspective, what this means is having efficient thread
   design, such as putting threads to sleep when there is no work to be done by
   them and preferring design patterns such as interrupts and events over
   polling.

.. note::

   For verification purposes, EASNFW's HIL verification image needs to expose
   test cases or commands to execute specific tasks or code paths isolated from
   normal application logic, to allow for the measurement of the energy consumed
   by each of them separately. The target tasks and code paths are: data
   acquisition (acquisition of a full audio track and one sample of each
   monitored environmental variable), audio processing, data storage to mass
   storage (storage of a processed audio track and one sample of each monitored
   environmental variable), and transmission cycle (sampling, processing,
   storing and transmitting one ecoacoustic record to the cloud platform).
   Acceptance limits for each operating state will be established after the
   first hardware characterization campaign.

.. _section_logging:

REQ-040: Logging
================

The firmware shall log information relevant to verifying correct system
operation and identifying and diagnosing failures, in a way that logs can be
monitored in real time from a host PC through USB.

.. _section_req_ota_update:

REQ-041: Firmware Update Over-The-Air (FUOTA) Cycle
====================================================

After the transmission of the power-on log payload and any pending ecoacoustic
record from a previous reset cycle is scheduled, if the self-test sequence
identifies all the checked capabilities as operational, EASNFW shall start
the FUOTA cycle.

For each FUOTA cycle, EASNFW shall:

#. check whether a newer firmware version is available on the cloud platform;
#. if so:

   #. if not in diagnostic state, then:

      #. pause the data acquisition cycle after the current cycle ends;
      #. wait until all the pending ecoacoustic records are transmitted to the
         cloud platform;

   #. perform the firmware update;

#. otherwise: wait ``PARAM_FUOTA_CYCLE_PERIOD`` seconds.

.. note::

   The FUOTA cycle may also be started when in diagnostic state, see
   :ref:`section_req_diag_state`.
