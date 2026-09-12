**********************
Firmware Design
**********************

Overview
=========

This section breaks the firmware architecture described previously down
into modules. Each thread from the architecture document is implemented
as a thin orchestration module, which in turn relies on lower-level
modules that encapsulate specific concerns (sensor sampling, on-the-wire
protocols, storage, connectivity, and so on). This separation keeps
thread modules focused on sequencing and data pipeline handling, while
the underlying logic and hardware access stay independently reusable and
testable.

As with the architecture document, this design intentionally stays at a high
level and does not go into implementation detail in areas that are still TBD
elsewhere in the specification (e.g. the audio processing algorithm).

Module Layering
=================

Modules are organized in three layers, plus a shared layer for code used
by both components:

* **Application layer** — one module per thread (as defined in the
  architecture document). These modules own a thread's main loop, read
  from and write to its message queues, and call into the domain and
  driver layers to do the actual work.
* **Domain layer** — modules that implement a specific piece of
  functional logic (e.g. assembling an ecoacoustic record, running the
  self-test checks, formatting a cloud payload), independent of which
  thread calls them.
* **Driver / platform layer** — thin wrappers around Zephyr drivers and
  subsystems (sensors, SPI, file system, NVS, modem), isolating the rest
  of the firmware from hardware and Zephyr API specifics.
* **Shared layer** — modules used by both EASNFW-SENSOR and
  EASNFW-CLOUD, mainly data structure and protocol definitions that need
  to stay consistent on both ends of the SPI link.

Logging (REQ-013) is not modeled as a separate module: each module
registers its own Zephyr log module (``LOG_MODULE_REGISTER``) and uses
the logging subsystem directly. Power management (REQ-012) is likewise
not a standalone module at this stage; it is expected to rely on Zephyr's
power management subsystem together with threads naturally idling while
blocked on message queues.

Shared Modules
================

.. list-table:: Modules shared between EASNFW-SENSOR and EASNFW-CLOUD
   :header-rows: 1
   :widths: 22 15 63

   * - Module
     - Layer
     - Responsibility
   * - ``spi_protocol``
     - Shared
     - Defines the message/frame types exchanged over the SPI link
       (payload transfer, delivery acknowledgements) and their
       (de)serialization. Used by ``cloud_link`` and ``sensor_link``.
   * - ``ecoacoustic_record``
     - Shared
     - Defines the in-memory representation of an ecoacoustic record
       (processed audio blocks, environmental sample, timestamps) so
       that both components interpret the same data consistently.
   * - ``retry``
     - Shared
     - Generic exponential backoff/retry utility. Used by ``cloud_tx``
       (REQ-011); kept generic in case other retry needs arise later.

EASNFW-SENSOR Modules
========================

Application layer
--------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Module
     - Owns thread
     - Responsibility
   * - ``sampling``
     - Sampling
     - Runs the self-test sequence at boot, then loops acquiring audio
       and environmental data per track (REQ-001 to REQ-005).
   * - ``audio_processing``
     - Processing
     - Consumes audio blocks and runs the audio processing algorithm on
       each (REQ-006).
   * - ``storage``
     - Storage
     - Persists processed blocks and environmental data/timestamps,
       assembles ecoacoustic records, removes them once delivery is
       confirmed, and stores failure details to NVS (REQ-004, REQ-007,
       REQ-008, REQ-010, REQ-011).
   * - ``cloud_link``
     - Transmission
     - Sends payloads to EASNFW-CLOUD and reports delivery outcomes back
       to ``storage`` (REQ-002, REQ-003, REQ-009).

Domain layer
--------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Module
     - Responsibility
   * - ``audio_sampling``
     - Encapsulates audio acquisition: configuring and triggering the
       audio sensor, buffering samples. Used by ``sampling``.
   * - ``env_sampling``
     - Encapsulates environmental data acquisition. Used by
       ``sampling``.
   * - ``audio_algo``
     - Implementation of the (TBD) audio processing algorithm. Used by
       ``audio_processing``.
   * - ``selftest``
     - Implements the individual self-test checks and their aggregation
       (REQ-001). Used by ``sampling`` during initialization.

Driver / platform layer
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Module
     - Responsibility
   * - ``audio_sensor_drv``
     - Thin wrapper around the Zephyr driver for the audio sensor. The initial
       baseline is a 16-bit/48-kHz mono I2S digital microphone. Used by
       ``audio_sampling``.
   * - ``env_sensor_drv``
     - Thin wrapper around the Zephyr sensor driver for the
       environmental sensor (I2C). Used by ``env_sampling``.
   * - ``mass_storage``
     - Wraps the Zephyr file system used on the SD card. Used by
       ``storage`` and ``selftest``.
   * - ``nvs``
     - Wraps Zephyr's NVS subsystem. Used by ``storage`` and
       ``selftest``.
   * - ``spi_transport``
     - Low-level SPI send/receive and framing, used together with
       ``spi_protocol``. Used by ``cloud_link``.

EASNFW-CLOUD Modules
=======================

Application layer
--------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Module
     - Owns thread
     - Responsibility
   * - ``sensor_link``
     - Receiving
     - Receives payloads from EASNFW-SENSOR over SPI and relays delivery
       acknowledgements back to it (REQ-002, REQ-003, REQ-009).
   * - ``payload_assembler``
     - Assembling
     - Reassembles and validates SPI fragments and adds the transport envelope
       needed by the cloud platform. The canonical record itself is assembled
       by EASNFW-SENSOR (REQ-002, REQ-003, REQ-009, REQ-014, REQ-015).
   * - ``cloud_tx``
     - Transmitting
     - Transmits payloads to the cloud platform, with retry/backoff on
       failure (REQ-009, REQ-011).

Domain layer
--------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Module
     - Responsibility
   * - ``payload_format``
     - Formats/serializes data into the representation expected by the
       cloud platform (e.g. JSON). Used by ``payload_assembler``.

Driver / platform layer
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Module
     - Responsibility
   * - ``spi_transport``
     - Low-level SPI send/receive and framing, used together with
       ``spi_protocol``. Used by ``sensor_link``.
   * - ``http_client``
     - Handles request/response exchanges with the cloud platform. Used
       by ``cloud_tx``.
   * - ``lte_m``
     - Wraps modem/connectivity management for the LTE-M link. Used by
       ``http_client``.

Diagram
=========

.. uml::

   @startuml
   skinparam componentStyle rectangle
   skinparam backgroundColor transparent

   package "Shared" {
     [spi_protocol] as SpiProto
     [ecoacoustic_record] as Record
     [retry] as Retry
   }

   package "EASNFW-SENSOR" {
     package "Application" as SensorApp {
       [sampling] as Sampling
       [audio_processing] as AudioProc
       [storage] as Storage
       [cloud_link] as CloudLink
     }
     package "Domain" as SensorDomain {
       [audio_sampling] as AudioSampling
       [env_sampling] as EnvSampling
       [audio_algo] as AudioAlgo
       [selftest] as Selftest
     }
     package "Driver / Platform" as SensorDriver {
       [audio_sensor_drv] as AudioDrv
       [env_sensor_drv] as EnvDrv
       [mass_storage] as MassStorage
       [nvs] as Nvs
       [spi_transport] as SpiTransportS
     }
   }

   package "EASNFW-CLOUD" {
     package "Application" as CloudApp {
       [sensor_link] as SensorLink
       [payload_assembler] as Assembler
       [cloud_tx] as CloudTx
     }
     package "Domain" as CloudDomain {
       [payload_format] as PayloadFormat
     }
     package "Driver / Platform" as CloudDriver {
       [spi_transport] as SpiTransportC
       [http_client] as HttpClient
       [lte_m] as LteM
     }
   }

   Sampling --> AudioSampling
   Sampling --> EnvSampling
   Sampling --> Selftest
   AudioSampling --> AudioDrv
   EnvSampling --> EnvDrv
   AudioProc --> AudioAlgo
   Storage --> MassStorage
   Storage --> Nvs
   Storage --> Record
   Selftest --> Nvs
   Selftest --> MassStorage
   Selftest --> CloudLink
   CloudLink --> SpiTransportS
   CloudLink --> SpiProto
   CloudLink --> Record

   SensorLink --> SpiTransportC
   SensorLink --> SpiProto
   Assembler --> PayloadFormat
   Assembler --> Record
   CloudTx --> HttpClient
   CloudTx --> Retry
   CloudTx --> SpiProto
   HttpClient --> LteM
   @enduml

Open Items and Assumptions
=============================

* ``spi_protocol`` and ``payload_format`` details are not yet defined.
* Only EASNFW-SENSOR is assumed to own persistent storage for failure
  details (``nvs``), consistent with the system architecture, where
  EASNFW-CLOUD's backup storage is still TBD. If EASNFW-CLOUD gains its
  own persistent storage, a corresponding module would be added there.
* ``audio_algo`` is a placeholder pending definition of the audio
  processing algorithm (REQ-006).
* The exact mechanics of the self-test check that verifies data can be
  transmitted to the cloud platform (REQ-001), which necessarily
  involves both components, are TBD.

Audio Interface Decision
========================

The initial acquisition baseline is one external I2S digital microphone
producing 16-bit PCM at 48 kHz directly to the nRF5340. This path does not
require the CS47L63 hardware audio codec. The codec available on the nRF5340
Audio DK remains an experimental alternative for analog input or for acoustic
front ends whose requirements cannot be met by a digital microphone.

The nRF5340's direct PDM peripheral is not the baseline because its hardware
PCM output is limited to 16 kHz. A PDM microphone may still be evaluated when
16-kHz acquisition is acceptable or when routed through a separate decimation
stage such as the CS47L63.

Mass storage is independent from the audio codec. EASNFW-SENSOR accesses the
Audio DK's SD-card holder using the Zephyr SDHC/filesystem stack over SPI.
Uncompressed PCM or WAV data can therefore be stored and retrieved without an
audio codec; compressed formats require a corresponding software encoder or
decoder.

Power Architecture Considerations
=================================

The nPM1100 on the nRF5340 Audio DK is a USB-compatible linear Li-ion/Li-Po
charger and power-path device; it is not a photovoltaic maximum-power-point
tracking controller. During prototype development, photovoltaic input shall
therefore be conditioned by an external solar charger/energy harvester and
presented to the DK as a regulated supply. Direct connection of a solar panel
to nPM1100 VBUS is not assumed to be supported.

The final power architecture remains an open hardware decision and shall be
selected from measured system energy per acquisition/transmission cycle,
panel characteristics, battery chemistry and capacity, required autonomy, and
LTE-M peak-current behavior.
