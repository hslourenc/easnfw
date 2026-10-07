**************************
Firmware Architecture
**************************

Overview
=========

EASNFW is built on `Zephyr RTOS <https://zephyrproject.org/>`_ and nRF Connect
SDK, and is split across its two components, EASNFW-SENSOR and EASNFW-CLOUD,
each running as its own Zephyr application image on its respective board and
communicating with one another over UART.

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
  UART link, the LTE-M modem) is only ever accessed by a single, dedicated
  thread, which avoids the need for additional locking around it.
* Messages are expected to be small (e.g. buffer handles/pointers and
  metadata) rather than large payloads passed by value, to keep queue
  memory usage bounded. Sample buffers use a fixed-size pool and explicit
  ownership transfer. A producer must not reuse a buffer until the consumer
  releases it, and queue exhaustion must result in a reported backpressure or
  loss condition rather than silent data loss.
* Self-test (REQ-023), power management (REQ-039), and logging
  (REQ-040) are cross-cutting concerns and are not modeled as dedicated
  application threads at this stage:

  * The self-test sequence runs during system initialization, in each
    component's main thread, before the application threads described
    below start their steady-state loops.
  * Logging uses Zephyr's built-in logging subsystem.
  * Power management is expected to mostly fall out of threads blocking on
    message queues when idle, complemented by Zephyr's power management
    subsystem; no specific energy-saving strategy is defined.

Fault handling and degraded operation
=====================================

EASNFW-SENSOR and EASNFW-CLOUD shall coordinate entry into diagnostic state.

The Storage thread is responsible for preserving incomplete and undelivered
records and for preventing records from becoming eligible for transmission until
they are complete. The Transmission and Receiving threads are responsible for
distinguishing link acknowledgement from cloud commit acknowledgement and for
making an unacknowledged payload available for safe retransmission. No thread
shall make a record appear delivered solely because the other component received
a link frame.

When diagnostic state requires acquisition to stop, the Sampling thread shall
stop starting new data-acquisition cycles. The cloud-side transmission path may
remain active to check for and fetch a firmware update. Fault state and recovery
notifications shall be exchanged through the existing message queues or an
equivalent application-level path; the specific scheduling and synchronization
mechanism remains an implementation detail.

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
     - REQ-023, REQ-029, REQ-031, REQ-032
   * - Processing
     - Consumes blocks of audio samples and runs the (TBD) audio
       processing algorithm on each block.
     - REQ-033
   * - Storage
     - Sole owner of mass storage and NVS. Persists processed audio
       blocks and environmental data/timestamps, assembles and atomically
       commits canonical ecoacoustic records, and removes them once durable
       cloud delivery has been confirmed. Also stores self-test
       and transmission failure details to NVS.
     - REQ-025, REQ-027, REQ-034, REQ-035, REQ-036, REQ-037
   * - Transmission
     - Sole owner of the UART link to EASNFW-CLOUD. Sends the power-on
       log, pending ecoacoustic records, and newly committed records to
       EASNFW-CLOUD using versioned and checksummed fragments. Reports both
       inter-component receipt and durable cloud-delivery outcomes back to the
       Storage thread.
     - REQ-024, REQ-026, REQ-037

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
       payload, so Storage can remove it (REQ-035) or handle the
       failure (REQ-037).

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
     - Sole owner of the CLOUD-side UART link. Receives payloads sent by
       EASNFW-SENSOR and forwards them for assembly. Relays delivery
       acknowledgements back to EASNFW-SENSOR once available.
     - REQ-024, REQ-026
   * - Assembling
     - Reassembles and validates UART fragments, then wraps the canonical
       record in the transport envelope expected by the cloud platform. It
       does not redefine or reconstruct the scientific record.
     - REQ-024, REQ-026
   * - Transmitting
     - Sole owner of the LTE-M link to the cloud platform. Transmits
       assembled payloads, implements the retry-with-backoff behavior
       on failure, and reports the outcome back to the Receiving
       thread.
     - REQ-026, REQ-037

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
     - Payload data as received from EASNFW-SENSOR over UART.
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

EASNFW-SENSOR and EASNFW-CLOUD communicate over the UART link described in
the system architecture. Records may be divided into multiple UART transfer
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
(REQ-035).

Data Representations
====================

The pipeline deliberately uses three separate representations:

* the **canonical ecoacoustic record**, assembled and persisted by
  EASNFW-SENSOR;
* the **UART transfer frame**, used only for reliable inter-component
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

   TxSensor <..> Receiving : versioned UART frames

   Receiving --> Assembling : rx_payload_q\nvalidated fragments
   Assembling --> TxCloud : assembled_payload_q\ncloud envelope
   TxCloud --> Receiving : tx_result_q

   TxCloud --> CloudPlatform : LTE-M
   @enduml

Open Items
============

* Audio processing algorithm and its threading/timing implications on the
  Sampling/Processing/Storage threads (REQ-033).
* Binary encoding of the canonical record and UART frames (CBOR is the initial
  candidate).
* Cloud platform payload envelope and endpoint contract, owned by the
  Assembling and Transmitting threads.
* Power management strategy (REQ-039) and its interaction with thread
  scheduling.
