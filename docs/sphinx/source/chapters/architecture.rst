**************************
Firmware Architecture
**************************

Overview
=========

EASNFW is built on `Zephyr RTOS <https://zephyrproject.org/>`_ and nRF Connect
SDK, and is split across its two components, EASNFW-SENSOR and EASNFW-CLOUD,
each running as its own Zephyr application image on its respective board and
communicating with one another over SPI.

Within each component, the firmware is organized as a small pipeline of
Zephyr threads, one per functional stage, connected by Zephyr message
queues (``k_msgq``). This section describes that thread layout at a high
level; it intentionally omits details (algorithms, payload formats,
priorities, stack sizes, buffer/memory management) that are still TBD
elsewhere in the specification.

Design Principles
===================

* Each functional stage of the data pipeline (sampling, processing,
  storage, transmission, etc.) runs as its own Zephyr thread.
* Threads communicate through message queues rather than shared global
  state. As a consequence, each shared resource (mass storage, NVS, the
  SPI link, the LTE-M modem) is only ever accessed by a single, dedicated
  thread, which avoids the need for additional locking around it.
* Messages are expected to be small (e.g. buffer handles/pointers and
  metadata) rather than large payloads passed by value, to keep queue
  memory usage bounded. Sample buffers use a fixed-size pool and explicit
  ownership transfer. A producer must not reuse a buffer until the consumer
  releases it, and queue exhaustion must result in a reported backpressure or
  loss condition rather than silent data loss (REQ-017).
* Self-test (REQ-001 to REQ-004), power management (REQ-012), and logging
  (REQ-013) are cross-cutting concerns and are not modeled as dedicated
  application threads at this stage:

  * The self-test sequence runs during system initialization, in each
    component's main thread, before the application threads described
    below start their steady-state loops.
  * Logging uses Zephyr's built-in logging subsystem.
  * Power management is expected to mostly fall out of threads blocking on
    message queues when idle, complemented by Zephyr's power management
    subsystem; no specific energy-saving strategy is defined.

EASNFW-SENSOR
===============

Threads
--------

.. list-table:: EASNFW-SENSOR threads
   :header-rows: 1
   :widths: 20 50 30

   * - Thread
     - Responsibility
     - Related requirements
   * - Sampling
     - Owns the audio and environmental sensor drivers. After
       initialization, runs the self-test sequence, then loops
       acquiring audio and environmental data for each track and
       forwarding it downstream.
     - REQ-001 to REQ-004, REQ-005
   * - Processing
     - Consumes blocks of audio samples and runs the (TBD) audio
       processing algorithm on each block.
     - REQ-006
   * - Storage
     - Sole owner of mass storage and NVS. Persists processed audio
       blocks and environmental data/timestamps, assembles and atomically
       commits canonical ecoacoustic records, and applies the retention policy
       once durable cloud delivery has been confirmed. Also stores self-test
       and transmission failure details to NVS.
     - REQ-004, REQ-007, REQ-008, REQ-010, REQ-011
   * - Transmission
     - Sole owner of the SPI link to EASNFW-CLOUD. Sends the power-on
       log, pending ecoacoustic records, and newly committed records to
       EASNFW-CLOUD using versioned and checksummed fragments. Reports both
       inter-component receipt and durable cloud-delivery outcomes back to the
       Storage thread.
     - REQ-002, REQ-003, REQ-009, REQ-011

Message queues
----------------

.. list-table:: EASNFW-SENSOR message queues
   :header-rows: 1
   :widths: 22 18 18 42

   * - Queue
     - Producer
     - Consumer
     - Carries
   * - ``audio_block_q``
     - Sampling
     - Processing
     - Blocks of raw audio samples, as they are acquired.
   * - ``track_meta_q``
     - Sampling
     - Storage
     - Environmental sample and start/end timestamps captured for a
       track.
   * - ``processed_block_q``
     - Processing
     - Storage
     - Processed audio block results.
   * - ``storage_tx_q``
     - Storage
     - Transmission
     - Notifications that a payload (power-on log, pending record, or
       newly persisted record) is ready to be sent to EASNFW-CLOUD.
   * - ``tx_ack_q``
     - Transmission
     - Storage
     - Delivery outcome (success/failure) for a previously queued
       payload, so Storage can remove it (REQ-010) or handle the
       failure (REQ-011).

EASNFW-CLOUD
==============

Threads
--------

.. list-table:: EASNFW-CLOUD threads
   :header-rows: 1
   :widths: 20 50 30

   * - Thread
     - Responsibility
     - Related requirements
   * - Receiving
     - Sole owner of the CLOUD-side SPI link. Receives payloads sent by
       EASNFW-SENSOR and forwards them for assembly. Relays delivery
       acknowledgements back to EASNFW-SENSOR once available.
     - REQ-002, REQ-003, REQ-009
   * - Assembling
     - Reassembles and validates SPI fragments, then wraps the canonical
       record in the transport envelope expected by the cloud platform. It
       does not redefine or reconstruct the scientific record.
     - REQ-002, REQ-003, REQ-009
   * - Transmitting
     - Sole owner of the LTE-M link to the cloud platform. Transmits
       assembled payloads, implements the retry-with-backoff behavior
       on failure, and reports the outcome back to the Receiving
       thread.
     - REQ-009, REQ-011

Message queues
----------------

.. list-table:: EASNFW-CLOUD message queues
   :header-rows: 1
   :widths: 22 18 18 42

   * - Queue
     - Producer
     - Consumer
     - Carries
   * - ``rx_payload_q``
     - Receiving
     - Assembling
     - Payload data as received from EASNFW-SENSOR over SPI.
   * - ``assembled_payload_q``
     - Assembling
     - Transmitting
     - Payloads ready to be sent to the cloud platform.
   * - ``tx_result_q``
     - Transmitting
     - Receiving
     - Delivery outcome (success/failure) for a payload, to be relayed
       back to EASNFW-SENSOR as an acknowledgement.

Inter-component Communication
================================

EASNFW-SENSOR and EASNFW-CLOUD communicate over the SPI link described in
the system architecture. Records may be divided into multiple SPI transfer
frames so no stage needs to hold a complete track in RAM. Each frame includes
protocol and schema versions, message type, ``record_id``, fragment index and
count, payload length, and an integrity check.

The link is used in both directions and has two distinct acknowledgement
levels:

* a **link acknowledgement** confirms that EASNFW-CLOUD received and validated
  a frame; and
* a **cloud commit acknowledgement** confirms that the cloud platform durably
  stored the complete record.

Only the cloud commit acknowledgement permits Storage to mark a record as
delivered. If either component resets or an acknowledgement is lost, the same
``record_id`` may be retransmitted safely because cloud delivery is idempotent
(REQ-016).

Data Representations
====================

The pipeline deliberately uses three separate representations:

* the **canonical ecoacoustic record**, assembled and persisted by
  EASNFW-SENSOR;
* the **SPI transfer frame**, used only for reliable inter-component
  fragmentation and transfer; and
* the **cloud payload**, assembled by EASNFW-CLOUD by adding transport and
  network metadata to canonical record data.

This separation prevents cloud schema changes from altering the on-device
scientific data model and keeps HTTP-specific concerns out of EASNFW-SENSOR.

Diagram
=========

.. uml::

   @startuml
   skinparam componentStyle rectangle
   skinparam backgroundColor transparent

   package "EASNFW-SENSOR" {
     [Sampling] as Sampling
     [Processing] as Processing
     [Storage] as Storage
     [Transmission] as TxSensor
   }

   package "EASNFW-CLOUD" {
     [Receiving] as Receiving
     [Assembling] as Assembling
     [Transmitting] as TxCloud
   }

   component "Audio sensor" as AudioSensor
   component "Environmental sensor" as EnvSensor
   database "Mass storage" as MassStorage
   database "NVS" as NVS
   cloud "Cloud platform" as CloudPlatform

   AudioSensor --> Sampling
   EnvSensor --> Sampling

   Sampling --> Processing : audio_block_q
   Sampling --> Storage : track_meta_q
   Processing --> Storage : processed_block_q

   Storage <--> MassStorage
   Storage <--> NVS

   Storage --> TxSensor : storage_tx_q\nrecord handle
   TxSensor --> Storage : tx_ack_q\nlink/cloud acknowledgement

   TxSensor <..> Receiving : versioned SPI frames

   Receiving --> Assembling : rx_payload_q\nvalidated fragments
   Assembling --> TxCloud : assembled_payload_q\ncloud envelope
   TxCloud --> Receiving : tx_result_q

   TxCloud --> CloudPlatform : LTE-M
   @enduml

Open Items
============

* Audio processing algorithm and its threading/timing implications on the
  Sampling/Processing/Storage threads (REQ-006).
* Binary encoding of the canonical record and SPI frames (CBOR is the initial
  candidate).
* Cloud platform payload envelope and endpoint contract, owned by the
  Assembling and Transmitting threads.
* Power management strategy (REQ-012) and its interaction with thread
  scheduling.
