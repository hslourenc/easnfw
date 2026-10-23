***********************
Open Items and Closure
***********************

This chapter is the single authoritative register of unresolved project
decisions, implementation gaps, and validation work. Other chapters describe
current behavior and link here when a caveat affects that behavior. A
corresponding ``EASNFW_REVISIT`` source comment points to this register.

Firmware and Backend Contracts
==============================

Cloud ingestion and delivery
----------------------------

EASNFW-CLOUD can attach to LTE and obtain modem network time, but the HTTP
client, payload encoder, and cloud self-test deliberately return unavailable
until the backend contract is agreed. Define and implement:

* endpoint and record schema/encoding, including maximum payload size;
* device authentication, TLS trust-anchor provisioning, and certificate
  lifecycle;
* idempotency using a stable record identifier and whole-record integrity;
* retryable versus permanent response classification;
* the durable-commit boundary that permits SENSOR to mark or remove a record;
* the laboratory ingress approach, including public-IP/NAT/firewall and
  institutional security constraints; and
* retention periods for raw audio, derived artifacts, and delivered on-node
  records.

Until then, neither IFL frame receipt nor local HTTP acceptance is proof of
durable cloud delivery. The CLOUD code must not issue a successful durable
acknowledgement. The backend chapter (:doc:`cloud_backend`) records the
deployment context and candidate approaches.

Inter-firmware record transfer
------------------------------

The IFL framing and CLOUD-side fragment receiver exist, but SENSOR does not yet
stream committed record files through the link and there is no end-to-end
durable-cloud acknowledgement that SENSOR can use. Agree record identity,
versioning, checksums, resume behavior, duplicate handling, and acknowledgement
semantics before completing the transfer path.

Audio processing
----------------

The current algorithm is a bit-exact PCM pass-through. The scientific
processing algorithm, algorithm version policy, and any changes to sample or
block requirements remain open.

Hardware and Time Validation
============================

Sensors and mass storage
------------------------

The DMM-4026-B-I2S-R and its P10 signal routing are the current microphone
baseline, but production-level signal, clock, environmental, and temperature
validation remains outstanding. The external BME688-compatible sensor address
``0x76`` is provisional; confirm its wiring, address, and whether raw gas
resistance is sufficient for the VOC requirement. Select and qualify the SD
card and production board revisions.

Storage durability
------------------

Records remain PC-readable FAT files. Header/PCM/footer checksums, synchronization,
and boot-time validation detect many incomplete or corrupt records, but neither
FAT metadata updates nor SD-card flash translation are guaranteed atomic on
power loss. TC-042 validates quarantine of a truncated temporary file; it does
not prove power-cut durability. Exercise abrupt power removal, card-specific
failure behavior, filesystem repair, and recovery on the target hardware.

Timekeeping
-----------

UTC is provided by the nRF9151 modem's cellular network-time service, passed to
SENSOR over IFL, and advanced locally using monotonic uptime. Validate cellular
availability, provisioning, accuracy, and drift over the intended operating
environment. A reboot requires synchronization again; no independent
persistent RTC source is implemented. TC-048 checks the request, initial
unsynchronized state, and formatting, but it does not simulate an actual
network outage followed by restoration through the production startup loop.

Runtime and Fault Handling
==========================

Real-time and memory behavior
-----------------------------

The SENSOR pipeline uses a bounded eight-message slab and aborts incomplete
tracks when allocation or downstream processing fails. TC-046 verifies slab
exhaustion and reuse deterministically. Measure I2S timing, queue wait,
processing/storage latency, peak memory, and track-abort behavior under
representative SD-card load before claiming the real-time budget is met.

Failure classification and resets
----------------------------------

Reset causes, a five-entry reset history, recovery bookkeeping, diagnostic
reason persistence, and indication are implemented. No automatic recovery
reset is currently initiated because no reset-recoverable fault class has been
approved. Define which faults permit a retry/reset, diagnostic-state behavior,
and recovery criteria before implementing an automatic reset policy. Existing
manual test procedures for recovery bookkeeping still require target validation.

Firmware Update
===============

MCUboot and signed-image rollback provide only a boot foundation. Production
key custody and matching bootloader trust anchors must be established; never
use the SDK development key for deployment. Firmware download, update
authorization, version discovery, update transport, coordinated SENSOR/CLOUD
rollout, confirmation criteria, and rollback after a real update failure remain
unimplemented or unvalidated on hardware.

Power and Verification
======================

The final energy source, charging/power-path design, and acceptable
energy-per-track/transmission limits require hardware measurements and a
production decision. The verification chapter contains the current procedures;
CLI_C's ``easncli cases`` output in :doc:`verification` is the maintained list
of test cases that are not completely implemented. Automated HIL coverage does
not replace the hardware, power-cut, field-sensor, radio, or backend validation
listed above.
