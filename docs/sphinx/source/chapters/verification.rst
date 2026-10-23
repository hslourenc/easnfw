************
Verification
************

The verification infrastrcture consists in a client CLI denominated CLI_C, that
runs on a PC, interfacing through USB with a host CLI denominated CLI_H, running
on EASN, specifically on nRF5340 Audio DK. CLI_C translates each user command
into one or more commands to CLI_H, parses CLI_H responses, performs additional
checks if needed, and prints the result to the user. For convenience, the
commands issued by the user to CLI_C are denominates primary commands, and the
commands issued by CLI_C to CLI_H are denominated secondary commands.

For test cases, specifically, this is the flow:

#. CLI_C breaks the test case down into multiple steps in case a reset of
   EASNFW is needed in the middle of the test case.
#. For each test case step:

   #. CLI_C issues the `easnfwcli2_reset` secondary command to CLI_H (needed
      even if for one-step test cases, to restore EASNFW to a clean state).
   #. Upon receiving the `easnfwcli2_reset` secondary command from CLI_C, CLI_H
      requests EASNFW-SENSOR to reset itself and EASNFW-CLOUD. EASNFW-SENSOR
      requests EASNFW-CLOUD to reset through the `reset-fw-cloud` handshake
      (see :ref:`section_handshake_reset_fw_cloud`).
   #. CLI_H logs `TCSTATUS_RESETDONE` to CLI_C after the sensor reset
      sequence is complete. CLI_C waits for this status before continuing.
   #. CLI_C issues the `easnfwcli2_tc <test-case-number> [<test-case-step>]`
      secondary command to CLI_H.
   #. CLI_H executes the test case step by exercising the applicable pieces of
      code from EASNFW and verifying the produced output.
   #. CLI_H logs the test case status to CLI_C indicating the result of the test
      case, if there is one, or 


   #. If the test case already failed:
      
      #. CLI_H logs a "test case failed" status to CLI_C.
   
   #. Else:
   
      #. If it is the last step in the test case:
     
         #. CLI_H decides if the test case failed, passed or its results are
            inconclusive (needs further verification from either CLI_C or the
            user) and logs the corresponding test case status to CLI_H.
      
      #. Else:

         #. CLI_H logs a "step done" status to CLI_C.

#. If the results of the test case were inconclusive, and CLI_C needs to
   perform additional verification, CLI_C does so and decides on the result
   itself.
#. CLI_C assembles the output as per `section_verif_easncli_tc_output`.

Test case status log format: `TCSTATUS_<STATUS> - <INFO>`.

* `<STATUS>`: 

   * `FAIL`: test case failed.
   * `SUCCESS`: test case passed.
   * `INCONCLUSIVE`: test case result is inconclusive, either CLI_H or the
      user needs to perform additional verification.
   * `STEPDONE`: test case is not finished yet, CLI_H logs this status once
      it is done with the current step.
   * `RESETDONE`: the reset sequence, including the coordinated reset of
     EASNFW-CLOUD and EASNFW-SENSOR (when applicable), is complete.


* `<INFO>`: provides a summary of the returned status with any additional data
  that may be useful to analyze the results.


.. note::

   All logs from EASNFW, including from CLI_H, are redirected to the USB
   interface through which CLI_C and CLI_H communicate.

Primary Commands
================

**Primary commands format**: ``easncli <command> [options] [arguments]``

The supported primary commands are detailed below.

easncli tc
----------

Format
^^^^^^

``easncli tc <test-case-number> [--port PORT]``

Description
^^^^^^^^^^^

Executes the test case specified by `test-case-number`.

Arguments
^^^^^^^^^

* ``test-case-number``: number of the test case to be executed (e.g.
  ``easncli tc 1`` executes TC-001).

.. _section_verif_easncli_tc_output:

Output
^^^^^^

The main results are returned in the JSON file ``result.json`` with the following
content:

* `passed`: `true` if the test case passes, `false` otherwise.
* `need_further_verif`: `true` if the the test case results needs further
  verification (e.g. if the audio data needs to be analyzed by an external tool
  to check if it was sampled with the expected parameters, or if there is
  anything that needs to be checked on the cloud platform), `false` otherwise.
  The need for further verification does not make test cases fail, meaning that
  a test case that need further verification is treated as passed if all its
  other checks succeed.
* `further_verif_type`: string that indicates how the test case results are
  supposed to be further verified if `need_further_verif` is `true`, `null`
  otherwise.
* `message`: status message from CLI_H, starting at the `TCSTATUS_` marker with
  shell prompt and terminal color escape sequences removed.
* `data`: list of strings indicating the path to the any additional files
  created (e.g. path to the file of the raw audio track).

Test case numbers are always integers in the JSON output.

The terminal output has three sections separated by banners: the logs of each
test case, the contents of ``result.json``, and a ``RESULTS SUMMARY`` with the
number and list of executed, skipped, passed, and failed test cases.

The results are organized within a new directory relative to where the command
was run. This new directory is structured as follows:

.. code-block::

   tc-[test-case-number]-[timestamp]/
   |
   |__result.json
   |
   |__tc-[test-case-number]_cli_c.log (CLI_C logs)
   |
   |__tc-[test-case-number].log (CLI_H / EASNFW logs)
   |
   |__data/ (additional data, optional)
      |
      |__audio/
         |
         |__audio_raw.wav (raw audio track)
         |
         |__audio_processed.wav (processed audio track)
      |
      ...

easncli tcfull
--------------

Format
^^^^^^

``easncli tcfull [--port PORT] [--implemented-only] [--no-human]
[--no-env-sensor] [--no-sim]``

Description
^^^^^^^^^^^

Executes all test cases currently exposed by CLI_H: TC-001 through TC-013,
TC-020, TC-042, TC-046, and TC-048.

Options
^^^^^^^

* ``--implemented-only`` excludes test cases whose firmware or test is only
  partially implemented; these count as not fully implemented. This currently
  excludes TC-005, TC-011, and TC-013.
* ``--no-human`` excludes test cases that require a person to verify sensor
  behavior or physical connections. This currently excludes TC-005 and
  TC-007.
* ``--no-env-sensor`` excludes test cases that require the external
  environmental sensor. This currently excludes TC-007.
* ``--no-sim`` excludes test cases that require a SIM and cellular service.
  This currently excludes TC-048.

The options may be combined. The aggregate ``success`` is true only when at
least one test case executed and every executed test case passed. Skipped cases
are listed separately and do not affect the aggregate. Documented cases that
have no CLI_H automation at all are always listed as skipped, with the same
reason ``not fully implemented`` used for ``--implemented-only``. An
inconclusive test whose automated checks pass follows the single-test result policy: it is
reported as passed with ``need_further_verif`` set.

Output
^^^^^^

The main results are returned in the JSON file ``result.json`` with the following
content:

* `success`: `true` if all test cases pass, `false` otherwise.
* `executed_count`, `executed`: number and list of executed test cases.
* `skipped_count`, `skipped_cases`: number and list of skipped test cases.
* `passed_count`, `passed`: number and list of executed test cases that passed.
* `failed_count`, `failed`: number and list of executed test cases that failed.
* `individual_results`: array containing one object per executed test case
  with its number, result, and message.
* `skipped`: array of test cases excluded by a selection option and the reason
  they were skipped, including every documented case without CLI_H
  automation.

Test case numbers are always integers in the JSON output.

The terminal output has three sections separated by banners: the logs of each
test case, the contents of ``result.json``, and a ``RESULTS SUMMARY`` with the
number and list of executed, skipped, passed, and failed test cases.

The results are organized within a new directory relative to where the command
was run. This new directory is structured as follows:

.. code-block::

   tc-full-[timestamp]/
   |
   |__result.json
   |
   |__tc-000/ (results for TC-000, as it would be presented by easncli tc)
   |
   |__tc-001/
   |
   ...

easncli cases
-------------

Format
^^^^^^

``easncli cases``

Description
^^^^^^^^^^^

Prints the CLI_C test-case catalog as JSON, including the HIL-available cases,
whether their automation is complete, which require human intervention, and
the identifiers of documented cases that are not completely implemented.
The catalog is maintained in ``tools/easncli/easncli.py``.

Secondary Commands
==================

easnfwcli2_tc
-------------

Format
^^^^^^

`easnfwcli2_tc <test-case-number> [test-case-step]`

Description
^^^^^^^^^^^

Executes the test case specified by `<test-case-number>`.

The optional `<test-case-step>` selects a specific step when the test case is
split into multiple steps. If it is omitted, CLI_H executes the first step.

easnfwcli2_reset
----------------

Format
^^^^^^

`easnfwcli2_reset`

Description
^^^^^^^^^^^

Upon receiving this command, CLI_H requests a complete EASNFW reset to
EASNFW-SENSOR. EASNFW-SENSOR requests EASNFW-CLOUD to reset through the
`reset-fw-cloud` handshake (see :ref:`section_handshake_reset_fw_cloud`), waits
for it to reset, and then EASNFW-SENSOR resets itself.


HIL Verification Image Test Cases
=================================

Since the HIL verification image does not execute business logic automatically
out of reset, but instead exposes a command line interface (CLI) over USB that
triggers specific code paths on demand, each test case below assumes it is
driven by a host PC issuing CLI commands to EASNFW-SENSOR.

The current HIL CLI exposes TC-001 through TC-013, TC-020, TC-042, TC-046, and
TC-048. TC-005 and TC-007 only verify that sensor data can be read; physical
signal accuracy and configured sensor behavior still require human
verification. TC-011 is deliberately inconclusive until a cloud backend
contract exists, and TC-013 therefore remains partial; partial cases count as
not implemented. TC-007 is completely implemented but requires the external
environmental sensor to be connected, and TC-048 is completely implemented but
requires cellular service and a SIM to complete (without them it reports an
inconclusive result). Use ``--no-env-sensor`` and ``--no-sim`` on benches
lacking this hardware. The remaining
documented cases are manual procedures or await implementation of their
required interfaces. Run ``easncli cases`` for the current maintained coverage
list.
The remaining test cases are verification procedures, but are not yet
automated by the firmware CLI; do not interpret their presence here as evidence
that the corresponding production behavior has been implemented.
Coverage for remaining storage, cloud-delivery, scheduling, diagnostics, and
FUOTA procedures is listed in :doc:`open_items` and by ``easncli cases``.

HIL hardware bring-up
---------------------

Make all inter-board signal and ground connections while both boards are
unpowered. Connect SENSOR and CLOUD grounds together, cross TX/RX and RTS/CTS
as shown in :doc:`interfw_link`, and do not connect either board's power rail
to the other. Verify that the UART I/O voltage levels match before connecting
signal wires.

Flash the HIL image to the nRF5340 Audio DK and the normal CLOUD image to the
nRF9151 SMA DK. Connect both boards to USB; start SENSOR first and then CLOUD
so SENSOR's IFL receiver is available for CLOUD's startup handshake. Once both
have booted, run CLI_C against SENSOR's VCOM console. CLI_C's reset command
coordinates a CLOUD reset and then resets SENSOR before each test case, so
leave both boards connected and powered throughout the suite.

The current bench does not have the environmental sensor or a SIM. Therefore,
do not treat TC-007 environmental sampling or TC-048 cellular UTC synchronization
as completed verification. Use
``easncli tcfull --implemented-only --no-human --no-env-sensor --no-sim`` to run
only implemented cases that need none of these. TC-048 may still be run
separately to verify its pre-sync checks, but with no SIM or cellular service
its final result is inconclusive.

REQ-023: Self-test Sequence
---------------------------

TC-001: Self-test NVS check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the NVS check of the self-test sequence reports success when NVS is
functional.

Preconditions
"""""""""""""

NVS is in a known-good state.

Procedure
"""""""""

#. Run the NVS check of the self-test sequence.

Expected results
""""""""""""""""

* NVS check of the self-test sequence succeeds.

TC-002: Self-test NVS check reports failure when NVS is faulty
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the NVS check of the self-test sequence reports failure when read
data does not match written data.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Inject a fault in the self-test NVS check so that the data read from a given
   entry does not match the data that was just written.
#. Run the NVS check of the self-test sequence.

Expected results
""""""""""""""""

* NVS check of the self-test sequence fails with an error code that indicates
  that read data does not match written data.

TC-003: Mass storage check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the mass storage check of the self-test sequence reports success
when the SD card is present and functional.

Preconditions
"""""""""""""

Mass storage is in a known-good state: the SD card is inserted and functional.

Procedure
"""""""""

#. Run the mass storage check of the self-test sequence.

Expected results
""""""""""""""""

* Mass storage check of the self-test sequence succeeds.

TC-004: Mass storage check reports an injected write failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the mass storage check of the self-test sequence reports a write
failure when an injected fault prevents the second test write.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Inject a fault in the self-test mass-storage check.
#. Run the mass storage check of the self-test sequence.

Expected results
""""""""""""""""

* The mass-storage check returns an error code indicating a write failure.

TC-005: Audio sensor sampling check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the audio self-test acquires one complete mono PCM16 block at the
configured sample rate.

Preconditions
"""""""""""""

The external I2S microphone is connected and operational.

Procedure
"""""""""

#. Run ``easncli tc 5``.

Expected results
""""""""""""""""

* The self-test starts I2S, discards startup blocks, acquires a complete PCM
  block, and reports ``TCSTATUS_SUCCESS``.

TC-006: Audio sensor sampling check reports failure when disconnected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that an injected audio acquisition fault is classified as a failed
self-test without requiring physical disconnection.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Run ``easncli tc 6``; the HIL test injects a failure in the audio
   self-test path.

Expected results
""""""""""""""""

* The audio check returns its acquisition-failed status and the test reports
  ``TCSTATUS_SUCCESS``.

TC-007: Environmental sensor sampling check succeeds under normal conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the environmental self-test reads temperature, pressure, relative
humidity, and gas resistance from the configured BME688-compatible sensor.

Preconditions
"""""""""""""

The external sensor is connected to I2C1 at the configured address.

Procedure
"""""""""

#. Run ``easncli tc 7``.

Expected results
""""""""""""""""

* A fresh sample of every required channel is obtained and the test reports
  ``TCSTATUS_SUCCESS``.

.. note::

   The BME680-compatible driver reports gas resistance, which is not a
   calibrated VOC concentration. See the implementation note in the design
   chapter.

TC-008: Environmental sensor sampling check reports failure when disconnected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that an injected environmental acquisition fault is detected.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Run ``easncli tc 8``; the HIL test injects a failure in the environmental
   self-test path.

Expected results
""""""""""""""""

* The environmental check returns its acquisition-failed status and the test
  reports ``TCSTATUS_SUCCESS``.

TC-009: EASNFW-SENSOR/EASNFW-CLOUD communication check succeeds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a PING command is acknowledged by EASNFW-CLOUD over the
inter-firmware UART link.

Preconditions
"""""""""""""

Both firmware images are running and the boards are connected using the UART2
wiring in :doc:`interfw_link`.

Procedure
"""""""""

#. Run ``easncli tc 9``.

Expected results
""""""""""""""""

* EASNFW-CLOUD acknowledges the PING and the test reports
  ``TCSTATUS_SUCCESS``.

TC-010: EASNFW-SENSOR/EASNFW-CLOUD communication check reports failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that an injected IFL failure is classified as a failed self-test.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Run ``easncli tc 10``; the HIL test injects a PING failure without changing
   the physical UART wiring.

Expected results
""""""""""""""""

* The IFL check returns a failure and the test reports ``TCSTATUS_SUCCESS``.

TC-011: Cloud platform transmission check succeeds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify cloud platform communication and durable receipt.

Preconditions
"""""""""""""

An authenticated, reachable backend implementing the agreed ingestion API and
durable-commit acknowledgement is required.

Procedure
"""""""""

#. Run ``easncli tc 11``.

Expected results
""""""""""""""""

* **Blocked / inconclusive:** the cloud endpoint, authentication, payload
  schema, and commit-acknowledgement contract are not yet defined. The firmware
  must not report a successful transmission without a durable acknowledgement.

.. note::

   The cloud-backend contract that blocks this test is listed in
   :doc:`open_items`.

TC-012: Cloud platform transmission check reports failure without connectivity
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify the cloud check reports unavailable service rather than claiming a
successful transmission.

Preconditions
"""""""""""""

None.

Procedure
"""""""""

#. Run ``easncli tc 12``; HIL injects an unavailable-cloud result.

Expected results
""""""""""""""""

* The cloud self-test returns the unavailable status, and the test reports
  ``TCSTATUS_SUCCESS``.

TC-013: Full self-test sequence runs all checks and aggregates results
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify one self-test invocation runs all six checks and records each result
without stopping after the first failure.

Preconditions
"""""""""""""

NVS, SD card, audio microphone, environmental sensor, and inter-FW link are
functional. The cloud check remains unavailable until the backend is defined.

Procedure
"""""""""

#. Run ``easncli tc 13``.
#. Inspect the USB output for a result line for each check.

Expected results
""""""""""""""""

* All six checks are attempted and their individual outcomes are logged.
* The result is inconclusive only because the cloud backend check cannot yet
  be completed; a failure in any other check reports ``TCSTATUS_FAIL``.
* ``easncli tcfull`` runs all cases exposed by CLI_H. Use
  ``--implemented-only``, ``--no-human``, ``--no-env-sensor``, and
  ``--no-sim`` to exclude cases by implementation status or bench
  requirements.

REQ-024: Power-On Log Payload Transmission
------------------------------------------

TC-014: Power-on log payload transmitted when all checks succeed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the power-on log payload is transmitted to the
cloud platform when the self-test sequence completes with no failures.

Preconditions
"""""""""""""

All modules in a known-good state.

Procedure
"""""""""


1. Trigger the full self-test sequence.
2. Monitor the cloud-facing interface (or a mock/stub cloud endpoint) and
   observe the returned result and USB log output.

Expected results
""""""""""""""""

The power-on log payload is transmitted and its content
matches the current firmware revisions, the last reset reason, and the
self-test results (all passing); this is reflected in the returned CLI result /
USB log.

TC-015: Power-on log payload transmitted despite a non-blocking failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the power-on log payload is still transmitted
when a failed check does not impact the ability to transmit data to the
cloud platform.

Procedure
"""""""""


1. Fault a subsystem that is unrelated to the transmit path (e.g. the
   audio sensor).
2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface and observe the returned result and USB
   log output.

Expected results
""""""""""""""""

The power-on log payload is transmitted, and its content
reflects the audio sensor check as failed; this is reflected in the returned
CLI result / USB log.

TC-016: Power-on log payload skipped when transmission is impacted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the power-on log payload transmission is skipped
when a failed check does impact EASNFW's ability to transmit data to the cloud
platform (e.g. the cloud transmission check itself, or the inter-component
communication check).

Procedure
"""""""""


1. Fault the UART link between EASNFW-SENSOR and EASNFW-CLOUD (or disable
   LTE-M connectivity).
2. Trigger the full self-test sequence.
3. Observe the returned result and USB log output.

Expected results
""""""""""""""""

No power-on log payload transmission is attempted, and
this is reflected in the returned CLI result / USB log.

REQ-025: Reset Reason and History Update
----------------------------------------

TC-057: Reset reason is cleared and reset history is updated
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that scheduling the power-on log transmission attempts to
clear the reset reason and update the reset-history ring buffer in NVS.

Procedure
"""""""""


1. Configure a known reset reason and a reset-history ring buffer in NVS.
2. Trigger and complete more than ``PARAM_MAX_RECOVERY_RESETS`` reset
   sequences.
3. Inspect the reset reason and reset-history entries in NVS.

Expected results
""""""""""""""""

The reset reason is cleared, and the current reset
timestamp is added to the reset-history ring buffer. The buffer contains only
the most recent ``PARAM_MAX_RECOVERY_RESETS`` reset timestamps. If NVS is
faulty, the actions are attempted and the failure is reported without invalid
NVS data.

REQ-026: Pending Ecoacoustic Data Transmission
----------------------------------------------

TC-017: Pending records are scheduled without delaying acquisition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that ecoacoustic records pending from a previous reset
cycle are scheduled after the power-on log, while the next acquisition starts
according to schedule.

Preconditions
"""""""""""""

One or more pending ecoacoustic records are present in mass
storage (simulating records left over from a prior cycle); all acquisition
modules are in a known-good state.

Procedure
"""""""""


1. Trigger a reset, then the full self-test sequence.
2. Populate mass storage with more pending records than can be transmitted
   before the next scheduled acquisition.
3. Monitor the cloud-facing interface after the power-on log payload has
   been sent, and observe the acquisition timestamps and USB log output.
4. Inspect mass storage contents.

Expected results
""""""""""""""""

Pending records enter the transmission queue in age order.
The next acquisition starts according to schedule without waiting for the
backlog to drain. Transmission pauses or is deferred as necessary. Records
remain in mass storage until durable cloud delivery is confirmed.

TC-018: Acquisition-capability failure prevents normal acquisition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a self-test failure affecting acquisition prevents
new acquisition while already committed records remain recoverable.

Preconditions
"""""""""""""

One or more complete ecoacoustic records are present in mass
storage.

Procedure
"""""""""


1. Fault any module checked by the self-test sequence.
2. Trigger the full self-test sequence.
3. Monitor the cloud-facing interface after the power-on log payload
   step and observe the returned result and USB log output.
4. Inspect mass storage contents.

Expected results
""""""""""""""""

No new track is acquired. Existing complete records remain
in mass storage and are neither corrupted nor deleted.

TC-019: No pending data results in no transmission attempt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify graceful behavior when there is no pending ecoacoustic
data to transmit.

Preconditions
"""""""""""""

Mass storage contains no complete ecoacoustic records, all
modules are in a known-good state.

Procedure
"""""""""


1. Trigger the full self-test sequence.
2. Monitor the cloud-facing interface after the power-on log payload
   step and observe the returned result and USB log output.

Expected results
""""""""""""""""

The power-on log payload is transmitted; no ecoacoustic
record transmission is attempted and no error is reported.

REQ-027: Diagnostic State
-----------------------------------

TC-020: Failed checks are stored to NVS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that, after the power-on log payload step, the set of
failed self-test checks is written to NVS.

Procedure
"""""""""


1. Run ``easncli tc 20``. The HIL procedure injects an audio self-test failure
   and runs the self-test sequence.
2. The firmware reads the resulting failed-check mask back from NVS.

Expected results
""""""""""""""""

The failed-check mask can be read from NVS and includes the audio sensor
failure bit. The cloud-dependent power-on log step is not claimed by this HIL
test; it remains pending a backend contract.

TC-021: Self-test failures are classified
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that self-test failures are classified as acquisition-
blocking, transient transmission-blocking, permanent transmission-blocking, or
non-blocking and produce the corresponding system behavior.

Procedure
"""""""""


1. In separate runs, fault the audio sensor, a temporarily unavailable cloud
   connection, a permanently unavailable transmission path, and a non-essential
   diagnostic function.
2. Trigger the full self-test sequence and observe the reported classification
   and subsequent acquisition/transmission behavior.

Expected results
""""""""""""""""

The audio-sensor fault is acquisition-blocking; the
temporary connectivity fault preserves local acquisition and storage while
deferring transmission; the permanent transmission fault enters a degraded
state; and the non-blocking fault is reported without preventing either
available function. None produces an unbounded reset loop.

TC-022: Failed-checks storage is skipped when NVS itself is faulty
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that EASNFW does not attempt to store the failed
checks to NVS when the NVS check itself is among the failures.

Procedure
"""""""""


1. Fault NVS.
2. Trigger the full self-test sequence and observe the returned result and USB
   logs.

Expected results
""""""""""""""""

No attempt to write the failed-checks record to NVS
is logged; the component handles the NVS failure according to its reset or
degraded-state policy.

TC-023: Connectivity failure enters diagnostic state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a failure causing the power-on log transmission to
be skipped enters diagnostic state.

Procedure
"""""""""


1. Fault the UART link between EASNFW-SENSOR and EASNFW-CLOUD.
2. Trigger the self-test sequence and observe the USB log.

Expected results
""""""""""""""""

The power-on log payload transmission is skipped, the
failure is recorded as an attempted diagnostic-state action, and the device
enters diagnostic state.

TC-058: Diagnostic state stops data acquisition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that diagnostic state stops data acquisition and starts
the FUOTA cycle when cloud communication remains available.

Procedure
"""""""""


1. Inject a permanent transmission-blocking fault.
2. Trigger the self-test sequence.
3. Reset the node without clearing the fault.
4. Observe the acquisition, storage, transmission, firmware-update, and
   degraded-state logs.

Expected results
""""""""""""""""

Data acquisition stops, durable and undelivered records
are preserved, the diagnostic state survives the reset, and the FUOTA cycle
starts if it was not already running.

TC-059: Diagnostic log payload contains required information
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the diagnostic log payload contains the power-on
log information and the diagnostic-state reason.

Procedure
"""""""""


1. Trigger a diagnostic-state entry with a known failure and affected
   ``record_id``.
2. Inspect the diagnostic log payload.

Expected results
""""""""""""""""

The payload contains the power-on log information and
identifies the diagnostic-state reason and affected ``record_id`` when
applicable.

TC-060: Diagnostic-state indication is provided
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the diagnostic-state visual indication is provided.

Procedure
"""""""""


1. Enter diagnostic state with a persistent failure.
2. Monitor the diagnostic-state LED for at least 60 seconds.

Expected results
""""""""""""""""

The LED blinks with a 30-second period.

REQ-029: Data Acquisition Cycle
--------------------------------

TC-025: Inter-track interval is honored between consecutive tracks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that, after the first audio track, sampling of the
next track does not begin until ``PARAM_INTER_TRACK_INTERVAL`` seconds
have elapsed since the end of the previous track's sampling.

Procedure
"""""""""


1. Configure a short test value for ``PARAM_INTER_TRACK_INTERVAL`` and
   ``PARAM_TRACK_LEN`` via the CLI.
2. Let two consecutive tracks be sampled.
3. Measure the elapsed time between the end-of-sampling timestamp of
   track 1 and the start-of-sampling timestamp of track 2.

Expected results
""""""""""""""""

The measured interval is ``PARAM_INTER_TRACK_INTERVAL``
seconds.

REQ-030: Data Acquisition Cycle Priority
----------------------------------------

TC-061: Data acquisition takes priority over conflicting actions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that data-acquisition actions take priority when another
requirement schedules a conflicting action.

Procedure
"""""""""


1. Schedule a data-acquisition cycle and a conflicting transmission or
   firmware-update action at the same time.
2. Observe the acquisition and competing action timestamps.

Expected results
""""""""""""""""

The data-acquisition actions are performed first, and the
conflicting action is deferred until the acquisition actions are complete.

REQ-032: Environmental Data Acquisition
---------------------------------------

TC-026: One environmental sample and two timestamps are captured per track
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that exactly one sample of each environmental
variable is captured at the start of a track, along with a start
timestamp and an end timestamp.

Procedure
"""""""""


1. Trigger a single track's sampling.
2. Inspect the CLI/log output for the captured environmental sample and
   the two timestamps.

Expected results
""""""""""""""""

Exactly one temperature, humidity, pressure, and VOCs
sample is captured; a start timestamp is captured before audio sampling begins
and an end timestamp after it ends, with end greater than start.

REQ-031: Audio Data Acquisition
-------------------------------

TC-027: Audio sampling parameters match configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that acquired audio matches the configured parameters: 1
channel (mono), ``PARAM_AUDIO_SAMPLE_RATE``-kHz sample rate,
``PARAM_AUDIO_SAMPLE_WIDTH``-bit sample width, and ``PARAM_TRACK_LEN``-second
duration.

Procedure
"""""""""


1. Configure known test values for ``PARAM_AUDIO_SAMPLE_RATE``,
   ``PARAM_AUDIO_SAMPLE_WIDTH``, and ``PARAM_TRACK_LEN``.
2. Feed a known test signal to the audio sensor input via the test bench.
3. Retrieve the raw acquired samples and verify channel count, sample rate,
   sample width, and total duration.

.. note::

   This test case implies the need for a CLI command to dump the raw audio
   samples to the host PC for analysis, and the ability to skip the audio
   processing step so that the raw samples can be retrieved.

Expected results
""""""""""""""""

All parameters match the configured parameter values
within acceptable tolerance.

TC-028: Acquired audio is band-limited as configured
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the acquired audio signal's energy outside the
``[PARAM_AUDIO_SAMPLE_BW_LO, PARAM_AUDIO_SAMPLE_BW_HI]`` (Hz) band is
attenuated as expected.

Procedure
"""""""""


1. Configure test values for the bandwidth parameters.
2. Feed a known test signal to the audio sensor input via the test bench.
3. Retrieve the raw acquired track and perform a frequency-domain analysis on
   the host PC.

Expected results
""""""""""""""""

Signal content within the configured band is
preserved; content outside the band is attenuated or not present.

REQ-033: Audio Data Processing
------------------------------

TC-029: Processing is triggered per block of required samples
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that EASNFW-SENSOR processes each acquired block of
``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` samples according to the
audio processing algorithm.

.. warning::

   Blocked / pending. The audio processing algorithm and the value of
   ``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` is TBD. This test case should
   be detailed once the algorithm is specified; see :doc:`open_items`.

REQ-034: Ecoacoustic Data Persistence
-------------------------------------

TC-030: Processed block results are persisted to mass storage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that, after each block of
``REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING`` samples is processed, the
processing result is written to mass storage.

Procedure
"""""""""


1. Trigger sampling and processing of a full track.
2. After each processed block, inspect mass storage contents.

Expected results
""""""""""""""""

A new persisted entry appears in mass storage after
each processed block, matching the processing output for that block.

TC-031: All blocks of a track are persisted by the time the track completes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that, once an entire track has finished processing,
mass storage holds the processed results for every block of that track.

Procedure
"""""""""


1. Trigger sampling and processing of a full track.
2. After processing completes, list all mass storage-persisted blocks
   associated with the track.

Expected results
""""""""""""""""

The number of persisted blocks equals
``ceil(total_track_samples / REQUIRED_NUM_AUDIO_SAMPLES_FOR_PROCESSING)``, with
no missing or duplicate sample ranges. The final block may be shorter than a
full processing block.

Environmental Data and Timestamps Persistence
----------------------------------------------

TC-032: Environmental data and timestamps persisted after track completion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the environmental sample and the two timestamps
captured for a track (REQ-029) are written to mass storage once all of the
track's audio samples have been processed.

Procedure
"""""""""


1. Trigger sampling and processing of a full track.
2. After the last block is processed, inspect mass storage for the
   environmental data and timestamp entries associated with the track.

Expected results
""""""""""""""""

The persisted entry's environmental values and
timestamps match those captured during REQ-029's sampling step for the
same track.

Ecoacoustic Record Transmission Scheduling
------------------------------------------

TC-033: Ecoacoustic record transmitted once fully stored
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a complete ecoacoustic record (processed audio
track, environmental samples, and timestamps) is transmitted to the cloud
platform once all of its data is stored in mass storage.

Preconditions
"""""""""""""

Cloud connectivity available.

Procedure
"""""""""


1. Trigger sampling and processing of a full track through to persistence
   (REQ-034).
2. Monitor the cloud-facing interface for the resulting ecoacoustic
   record payload(s).

Expected results
""""""""""""""""

The record is transmitted, split across one or more
payloads as needed. The server validates and durably commits the complete
``record_id`` before returning a cloud commit acknowledgement.

REQ-035: Ecoacoustic Data Removal
---------------------------------------------------

TC-034: Record marked delivered only after durable cloud acknowledgement
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that an ecoacoustic record is removed only after the
cloud platform confirms durable storage.

Procedure
"""""""""


1. Trigger sampling, processing, persistence and transmission of a full
   track (REQ-029 through REQ-026).
2. Confirm link-level receipt without returning a cloud commit acknowledgement
   and inspect the record state.
3. Return the cloud commit acknowledgement and inspect the record state.

Expected results
""""""""""""""""

Link-level receipt does not mark the record delivered.
The durable cloud acknowledgement marks it delivered, after which the record
is removed from mass storage. Undelivered records remain untouched.

TC-035: Record retained in mass storage when transmission fails
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a record is not removed from mass storage
while its transmission has not yet succeeded.

Procedure
"""""""""


1. Disable cloud connectivity.
2. Trigger sampling, processing, and persistence of a full track.
3. Allow the transmission attempt(s) to occur and fail.
4. Inspect mass storage for the record.

Expected results
""""""""""""""""

The record remains present in mass storage.

TC-062: Acquisition waits when storage cannot provide durable space
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that storage exhaustion does not cause silent data loss.

Procedure
"""""""""


1. Fill mass storage with undelivered records and incomplete records until
   permitted delivered-record removal cannot provide sufficient space.
2. Start the next scheduled acquisition.
3. Inspect the record states and the system logs.

Expected results
""""""""""""""""

The data-acquisition cycle waits until sufficient space
is available. Incomplete and undelivered records remain preserved.

REQ-036: Run-time Mass Storage Access Failure
---------------------------------------------

TC-063: Runtime mass-storage access failure enters diagnostic state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a permanent runtime mass-storage access failure
enters diagnostic state.

Procedure
"""""""""


1. Start a data-acquisition cycle with mass storage operating normally.
2. Inject a mass-storage access failure during the cycle.
3. Observe the storage result and system state.

Expected results
""""""""""""""""

The failed operation is retried when classified as
transient. For a permanent failure, the device enters diagnostic state and
does not silently discard the affected data.

REQ-037: Payload Transmission Error Handling
--------------------------------------------

TC-036: Transient transmission failure is retried
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a transient payload transmission failure is retried.

Procedure
"""""""""


1. Introduce a transient cloud connectivity failure.
2. Trigger transmission of any payload (e.g. a pending ecoacoustic
   record).
3. Monitor the USB log for retry attempts.

Expected results
""""""""""""""""

At least one retry attempt is observed following the
initial failure.

TC-037: Retries follow bounded exponential backoff with jitter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the time interval between successive retries falls
within the configured jitter range, where ``c`` is the retry count, per the
bounded exponential backoff description.

Procedure
"""""""""


1. Introduce a transient cloud connectivity failure.
2. Trigger transmission of a payload.
3. Record the timestamps of the initial attempt and each subsequent retry
   from the USB log.

Expected results
""""""""""""""""

For retry count ``c`` starting at zero, the interval before
each retry is uniformly selected from
:math:`[(3/4) \mathop{\mathrm{min}}(PARAM\_MAX\_RETRY\_DELAY, 2^c),
\mathop{\mathrm{min}}(PARAM\_MAX\_RETRY\_DELAY, 2^c)]` seconds, within timing
tolerance.

TC-038: Retry count does not exceed the configured maximum
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the retry count does not exceed
``PARAM_MAX_RETRY_COUNT``.

Procedure
"""""""""


1. Configure a small test value for ``PARAM_MAX_RETRY_COUNT``.
2. Disable cloud connectivity.
3. Trigger transmission of a payload and count retry attempts from the
   USB log.

Expected results
""""""""""""""""

The retry count starts at zero and does not exceed
``PARAM_MAX_RETRY_COUNT``. Once the limit is reached, the retry count stops
incrementing.

TC-039: Failure is recorded and delivery deferred after maximum retries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that transmission failure details are recorded and the
affected payload remains available for a subsequent attempt.

Procedure
"""""""""


1. Configure a small test value for ``PARAM_MAX_RETRY_COUNT``.
2. Disable cloud connectivity.
3. Trigger transmission of a payload and let all retries fail.
4. Read back the failure record and inspect the pending queue.

Expected results
""""""""""""""""

The affected payload remains available for a subsequent
attempt, and the retry count stops incrementing at
``PARAM_MAX_RETRY_COUNT``.

TC-064: Permanent transmission failure enters diagnostic state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a permanent transmission failure enters diagnostic
state.

Procedure
"""""""""


1. Place a malformed record in the pending queue.
2. Trigger transmission with cloud connectivity available.
3. Inspect both record states and the transmission log.

Expected results
""""""""""""""""

The malformed record is rejected, the failure is recorded
as a diagnostic-state action, and the node enters diagnostic state.

REQ-038: Retry Count Reset
--------------------------

TC-065: Successful payload transmission resets the retry count
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a successful payload transmission resets the global
retry count to zero.

Procedure
"""""""""


1. Cause one or more transient transmission failures.
2. Successfully transmit a payload.
3. Cause another transient transmission failure and inspect the retry count.

Expected results
""""""""""""""""

The retry count is zero immediately after the successful
transmission, and the next retry sequence starts at zero.

REQ-039: Save Energy While Idle
-------------------------------

TC-040: Operating-state energy is measurable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that acquisition, processing, storage, transmission, and
idle intervals can be identified in a current trace and assigned an energy
cost.

Procedure
"""""""""


1. Power the complete node through a calibrated power profiler.
2. Run one shortened acquisition/transmission cycle with GPIO trace markers or
   timestamped logs identifying each operating state.
3. Integrate current over each marked interval.

Expected results
""""""""""""""""

Every operating state has an identifiable interval and a
reported energy value. No thread prevents entry into the configured idle state
between scheduled activities.

REQ-040: Logging
----------------

TC-066: Operational and failure events are logged
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that logs contain information useful for verifying
operation and diagnosing failures and can be monitored through USB.

Procedure
"""""""""


1. Run a normal acquisition and transmission cycle.
2. Inject a self-test or transmission failure.
3. Monitor the USB log during both runs.

Expected results
""""""""""""""""

The USB log reports relevant operational events in the
normal run and identifies the failure and its context in the faulted run.

Canonical Ecoacoustic Record Assembly
--------------------------------------

TC-041: Complete record is committed atomically
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that a record becomes eligible for transmission only
after all expected blocks and metadata are persisted and validated.

Procedure
"""""""""


1. Trigger acquisition and processing of one track.
2. Inspect mass storage while blocks are being appended.
3. Inspect the record again after the final block and metadata are written.

Expected results
""""""""""""""""

The in-progress record is marked temporary and is not
queued for transmission. After completion it is atomically committed, has a
unique ``record_id``, and contains the configured schema, acquisition, and
processing versions.

TC-042: Interrupted record remains recoverable but is not transmitted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CLI_H coverage
""""""""""""""

``easncli tc 42`` creates a uniquely named truncated temporary record on the
mounted SD card, invokes the same recovery scan used at startup, and verifies
that the file was quarantined as ``.bad`` rather than promoted to ``.rec``.
This deterministic check exercises recovery policy without cutting power.

Procedure
"""""""""


For the physical power-interruption procedure, remove power while a real record
is being appended, restore it, then inspect the recovered files and transmission
queue.

Expected results
""""""""""""""""

The partial record is detected and retained or quarantined
according to policy; it is never represented as a complete record and is not
transmitted.

Inter-component Transfer Integrity
-------------------------------------------

TC-043: Multi-frame record is reassembled correctly
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""

Transfer a record larger than one UART frame and compare the
reassembled bytes and metadata on EASNFW-CLOUD with the committed source
record.

Expected results
""""""""""""""""

All fragments are transferred in a bounded-memory manner,
the reassembled record is byte-for-byte equivalent to the source, and every
fragment receives a link acknowledgement.

TC-044: Corrupted or missing UART fragment is rejected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""

Inject, in separate runs, a corrupted CRC, a missing fragment,
an out-of-order fragment, and an unsupported protocol version.

Expected results
""""""""""""""""

EASNFW-CLOUD does not produce a cloud payload from an
invalid or incomplete transfer, reports the precise error, and does not issue a
successful link acknowledgement for the affected fragment or record.

TC-067: Lost link acknowledgement permits retransmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Drop a link acknowledgement after EASNFW-CLOUD has validated a complete
   frame.
2. Allow EASNFW-SENSOR to retry the affected transfer.
3. Observe the link and cloud commit acknowledgements.

Expected results
""""""""""""""""

The transfer is accepted again without duplicating the
   cloud record, and only a cloud commit acknowledgement allows the local
   record to be marked delivered.

Idempotent Cloud Delivery
----------------------------------

TC-045: Retransmission does not duplicate a cloud record
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Deliver a complete record to the cloud platform.
2. Drop the cloud commit acknowledgement before it reaches EASNFW-SENSOR.
3. Allow EASNFW-SENSOR to retransmit the same ``record_id``.

Expected results
""""""""""""""""

The cloud contains one logical record, returns a durable-
storage acknowledgement for the repeated ``record_id``, and the local record
is subsequently marked delivered.

Buffer Ownership and Backpressure
---------------------------------

TC-046: Slow processing applies the defined backpressure policy
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CLI_H coverage
""""""""""""""

``easncli tc 46`` acquires every entry in the fixed eight-message pipeline
pool, verifies that the next allocation fails with ``-ENOMEM``, releases all
entries, and verifies that a released entry can be allocated again. This
deterministically validates the bounded resource limit; it does not replace
measurement of real processing/storage latency or observe an audio track
abort under sustained load.

Procedure
"""""""""

Artificially delay the Processing and Storage threads until all eight
pipeline-pool blocks are occupied while a track is being acquired. The
deterministic HIL pool exhaustion check may be run first.

Expected results
""""""""""""""""

No buffer is reused while owned by another stage. Acquisition stops when the
pool is exhausted, the incomplete track is aborted, and no record for that
track is committed. Logs identify the resource exhaustion; no partial audio
record is presented as complete.

TC-047: SD writes do not introduce silent audio gaps
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""

Capture a deterministic continuous test signal while writing a
complete baseline track to the SD card and exercising worst-case expected SD
latency.

Expected results
""""""""""""""""

Sample sequence and timestamps contain no unexplained
gaps, duplicates, or reordering; any detected overrun causes the track to be
flagged rather than silently accepted.

TC-048: Acquisition waits until UTC synchronization succeeds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CLI_H coverage
""""""""""""""

``easncli tc 48`` verifies that UTC access returns ``-ENODATA`` before initial
synchronization, requests time from EASNFW-CLOUD, and checks that the returned
time advances monotonically and formats as ISO-8601 UTC. If cellular time
cannot be obtained, the case is inconclusive rather than a failure.

Procedure
"""""""""

For the full startup-gating check, boot the production image without cellular
time, verify that no record is acquired or committed, restore the network, and
confirm acquisition starts only after synchronization.

Expected results
""""""""""""""""

No record is acquired or committed before time synchronization. The firmware
continues retrying without entering diagnostic state solely for time
unavailability, then starts acquisition after valid UTC is received. New
records contain ISO-8601 UTC timestamps with the ``Z`` designator and identify
cellular network time as their source.

REQ-041: Firmware Update Over-The-Air (FUOTA) Cycle
---------------------------------------------------

TC-049: Firmware availability is checked after complete record transmission
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that EASNFW checks the cloud platform for firmware
versions only after an ecoacoustic record has been completely transmitted and
then defers the firmware update itself until the current audio recording cycle
has ended.

Procedure
"""""""""


1. Configure the cloud platform to report a newer firmware version.
2. Acquire and completely transmit one ecoacoustic record.
3. Monitor the cloud requests, audio-cycle state, and firmware version.

Expected results
""""""""""""""""

The firmware-availability check occurs after complete
record transmission, without waiting for the current recording cycle to end.
The latest available firmware versions are scheduled for update, and the
update is performed only after the current recording cycle ends; the active
recording is not interrupted.

TC-068: FUOTA can start from diagnostic state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Objective
"""""""""

Verify that the FUOTA cycle can be started from diagnostic
state when cloud communication is available.

Procedure
"""""""""


1. Enter diagnostic state with cloud communication available.
2. Ensure that no FUOTA cycle is already running.
3. Monitor firmware-availability checks and the diagnostic-state log.

Expected results
""""""""""""""""

The FUOTA cycle starts, and firmware availability is
checked without resuming normal data acquisition.

TC-050: No firmware update is attempted when no newer version is available
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Configure the cloud platform to report no firmware version newer than the
   versions currently installed.
2. Completely transmit an ecoacoustic record.
3. Inspect cloud requests, the firmware version, and the next recording cycle.

Expected results
""""""""""""""""

EASNFW performs the availability check, does not download
or install firmware, and starts the next recording cycle normally.

TC-051: Latest available firmware version is installed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Configure the cloud platform with multiple valid, newer firmware versions.
2. Completely transmit an ecoacoustic record and allow the current recording
   cycle to end.
3. Inspect the installed EASNFW-SENSOR and EASNFW-CLOUD versions after reboot.

Expected results
""""""""""""""""

EASNFW installs the latest available compatible versions
of both components and reports those versions in the next power-on log.

REQ-022: FUOTA Rollback
------------------------------

TC-053: Unsigned firmware image triggers rollback
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Configure the cloud platform to provide an otherwise valid but unsigned
   firmware image.
2. Complete an ecoacoustic record transmission and allow the OTA update to
   proceed.
3. Reboot the node and inspect the boot result and firmware versions.

Expected results
""""""""""""""""

The unsigned image is rejected; EASNFW rolls back to the
previously known good versions and records the signature-validation failure.

TC-054: Firmware image signed with the wrong key triggers rollback
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Procedure
"""""""""


1. Configure the cloud platform to provide an image signed with a key not
   trusted by EASNFW.
2. Complete an ecoacoustic record transmission and allow the OTA update to
   proceed.
3. Reboot the node and inspect the boot result and firmware versions.

Expected results
""""""""""""""""

The image signed with the wrong key is rejected; EASNFW
rolls back to the previously known good versions and records the key-validation
failure.

Recovery Reset Behavior
-----------------------

TC-055: Recovery resets are bounded and persisted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. warning::

   Non-applicable for now. No recoverable-by-reset failures have been
   identified in the current specification. This test case will be revisited
   if and when such a failure is defined.

Procedure
"""""""""


1. Configure small test values for ``PARAM_MAX_RECOVERY_RESETS`` and
   ``PARAM_RECOVERY_RESET_WINDOW``.
2. Inject a fault classified as recoverable by reset and reboot the node
   repeatedly within the configured window.
3. Inspect the recovery-reset counter and system state after each reboot.

Expected results
""""""""""""""""

The fault is recorded before each reset, the counter
survives reboot, and the node enters a degraded state instead of requesting
another automatic reset after the configured limit is reached.

REQ-028: Recovery Reset Counter Clearing
----------------------------------------

TC-056: Recovery-reset counter clears after stable operation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. warning::

   Non-applicable for now. No recoverable-by-reset failures have been
   identified in the current specification. This test case will be revisited
   if and when such a failure is defined.

Procedure
"""""""""


1. Recover from a reset-classified fault and complete the self-test sequence.
2. Run for ``PARAM_RECOVERY_RESET_WINDOW`` seconds without recurrence.
3. Inject the same fault after the recovery window and inspect the counter.

Expected results
""""""""""""""""

The recovery-reset counter is cleared after
``PARAM_RECOVERY_RESET_WINDOW`` seconds of uptime, and a subsequent
recoverable fault starts a new recovery sequence.

Known Gaps
==========

TC-040 defines how the baseline energy measurements shall be collected.
Acceptance limits and the remaining power decisions are tracked in
:doc:`open_items`.

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
