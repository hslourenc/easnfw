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

The BME688-compatible sensor address ``0x76`` is provisional. Confirm the
wiring and the address on the production hardware. Gas resistance in ohms, as
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
