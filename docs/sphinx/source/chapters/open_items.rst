***********************
Open Items and Closure
***********************

This chapter is the single authoritative register of unresolved project
decisions and validation work that affect the firmware. Other chapters describe
current behavior and link here when a caveat affects that behavior. A
corresponding ``EASNFW_REVISIT`` source comment points to this register.

Hardware, procurement, and environmental qualification work that does not
change the firmware (microphone and SD-card qualification, board revisions,
cellular drift, energy source and limits) is intentionally not tracked here.

Firmware Implementation
=======================

These items block or will change firmware code and the test cases that verify
it.

Cloud ingestion and delivery
----------------------------

EASNFW-CLOUD can attach to LTE and obtain modem network time, but the HTTP
client, payload encoder, and cloud self-test deliberately return unavailable
until the backend contract is agreed. Define and implement:

* endpoint and record schema/encoding, including maximum payload size;
* device authentication, TLS trust-anchor provisioning, and certificate
  lifecycle;
* idempotency using a stable record identifier and whole-record integrity;
* retryable versus permanent response classification, and from it the SENSOR
  retry policy for failed record transfers;
* the durable-commit boundary that permits SENSOR to mark or remove a record;
* the laboratory ingress approach, including public-IP/NAT/firewall and
  institutional security constraints;
* retention periods for raw audio, derived artifacts, and delivered on-node
  records; and
* how a full-length record reaches the backend. A 60 s, 48 kHz mono PCM16 track
  is about 5.76 MB, but EASNFW-CLOUD stages a whole record in RAM, limited by
  ``CONFIG_EASNFW_CLOUD_MAX_RECORD_SIZE`` (at most 64 KiB), so it rejects such a
  record. The IFL transfer itself already streams in bounded fragments; the
  cloud side needs chunked or streamed upload once the contract is known.

Until then, neither IFL frame receipt nor local HTTP acceptance is proof of
durable cloud delivery. The CLOUD code must not issue a successful durable
acknowledgement, and SENSOR keeps every record on the SD card after a
successful IFL transfer. The backend chapter (:doc:`cloud_backend`) records the
deployment context and candidate approaches.

Durable acknowledgement of record transfer
------------------------------------------

SENSOR streams committed records over the IFL (see :doc:`interfw_link`), and
CLOUD verifies and de-duplicates them. What remains open is the end-to-end
durable-cloud acknowledgement that lets SENSOR mark a record delivered
(REQ-034), which depends on the cloud contract above. TC-044, TC-045 and
TC-067 stay not fully implemented for that reason.

Audio processing
----------------

The current algorithm is a bit-exact PCM pass-through. The scientific
processing algorithm, algorithm version policy, and any changes to sample or
block requirements remain open.

Firmware update
---------------

Firmware download, update authorization, version discovery, update transport,
and coordinated SENSOR/CLOUD rollout are unimplemented and depend on the cloud
platform. Confirmation and rollback criteria are defined (see
:doc:`design`), but the signature-rejection test cases TC-053 and TC-054 need
an update transport and remain not fully implemented.

Production key custody and trust anchors
----------------------------------------

Production signing-key custody and the matching bootloader trust anchors must
be established. The SDK development key must never be used for deployment.

External environmental sensor
-----------------------------

The BME688-compatible sensor address ``0x76`` was confirmed on the bench
module (SDO tied to GND, CS tied high for I2C, wired to the Audio DK Arduino
I2C pins P1.02/P1.03): TC-007 acquired a full sample. Confirm the wiring and
the address again on the production hardware. TC-008 verifies detection only
through fault injection; physical disconnection has not been tested. Gas
resistance in ohms, as
reported by the sensor driver, is the VOC quantity the firmware records; no
index derivation is required.

Firmware Validation
===================

These items do not change the firmware design but must be measured on target
hardware before the corresponding claims are made.

Real-time and memory behavior
-----------------------------

The SENSOR pipeline uses a bounded eight-message slab and aborts incomplete
tracks when allocation or downstream processing fails. TC-046 verifies slab
exhaustion and reuse deterministically. Measure I2S timing, queue wait,
processing/storage latency, peak memory, and track-abort behavior under
representative SD-card load before claiming the real-time budget is met.

Open Issues
===========

TC-043 can time out after preceding HIL cases
---------------------------------------------

**Reproduction.** Connect the SENSOR and CLOUD boards with the IFL wiring,
flash the SENSOR HIL image and a compatible CLOUD image, then run:

.. code-block:: console

   easncli tcfull --implemented-only --no-human --no-env-sensor --no-sim --port COM3

The runner resets SENSOR before each test. In the observed full run, TC-042
passed, then TC-043 produced no terminal test status before the CLI's
120-second timeout. The aggregate result was 12 passed and TC-043 failed; TC-046
ran and passed afterward. Running TC-043 alone, or issuing TC-042, reset, then
TC-043 manually, has also passed, so those checks do not reliably reproduce the
failure.

**Findings so far.** The 8 MHz line is only the last line captured by CLI_C,
not evidence that SENSOR remains blocked in SD initialization. In a reproduced
clean-HIL sequence, a debugger attached after CLI_C timed out found the shell
and logging threads idle, mass storage initialized, the record file closed, and
the cloud-transfer counter advanced. A later serial capture contained the
missing ``SD card mounted``, record-commit, IFL acknowledgement, and
``TCSTATUS_SUCCESS`` messages, timestamped within the first second after boot.
That run completed on SENSOR; CLI_C did not receive its terminal result.

The SENSOR UART0 overlay enables hardware flow control, while ``easncli``
currently opens COM3 with PySerial's default ``rtscts=False``. A manual
TC-042/TC-043 sequence with RTS/CTS enabled also timed out, so the mismatch is
a candidate cause, not a verified fix. Enabling verbose SD/SDHC logging changed
the timing, dropped thousands of log messages, and allowed TC-043 to pass; it
is not a reliable negative result. No SD initialization failure or firmware
deadlock has been established.

Raising ``CONFIG_LOG_BUFFER_SIZE`` from the deferred-mode default of 1024 to
8192 in ``hil.conf`` removed the timeout in about ten TC-042/TC-043 sequences,
which points to dropped log lines. In those runs TC-043 instead failed about
half of the time with ``TCSTATUS_FAIL - could not commit the test record: -17``
(``-EEXIST`` from ``app_mass_storage__record_begin``). The cause was the test
record ID: TC-042/TC-043 derived it from uptime, cycle counter and a counter
that resets on boot, so after the per-case reset it was nearly identical every
run and collided with the record left on the SD card by the previous run. The
tests now use ``sys_rand32_get()`` like production; 10 consecutive
TC-042/TC-043 sequences passed, and a full ``tcfull --implemented-only`` run
passed 13 of 13 executed cases.

Previously planned: capture the COM3/VCOM0 receive and flow-control behavior during the
sequential run and verify a host configuration that reliably receives the
terminal result. Do not change SD initialization or the default test order
until the serial result path is understood.

Closed items
============

For reference, the following decisions are made and implemented:

* **Recovery resets.** Reset causes, a five-entry reset history, and recovery
  bookkeeping are kept, but no fault class is recoverable by reset. The
  firmware never starts an automatic recovery reset, and TC-055 and TC-056
  stay non-applicable until the requirements define such a fault.
* **Time outage handling.** UTC is requested repeatedly at startup until
  received, without entering diagnostic state (TC-048). A reboot requires
  synchronization again; there is no independent RTC.
* **Image confirmation.** See :doc:`design`.
* **Record streaming.** See :doc:`interfw_link`; verified by TC-043.

The verification chapter contains the current procedures; CLI_C's
``easncli cases`` output in :doc:`verification` is the maintained list of test
cases that are not completely implemented.
