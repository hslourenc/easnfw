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
level. The unresolved algorithm, backend, hardware-validation, and update
decisions are maintained in :doc:`open_items`.

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
  subsystems (sensors, UART, SPI, file system, NVS, modem), isolating the rest
  of the firmware from hardware and Zephyr API specifics.
* **Shared layer** — modules used by both EASNFW-SENSOR and
  EASNFW-CLOUD, mainly data structure and protocol definitions that need
  to stay consistent on both ends of the UART link.

Logging (REQ-040) is not modeled as a separate module: each module
registers its own Zephyr log module (``LOG_MODULE_REGISTER``) and uses
the logging subsystem directly. Power management (REQ-039) is likewise
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
   * - ``uart_protocol``
     - Shared
     - Defines the message/frame types exchanged over the UART link
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
       (REQ-037); kept generic in case other retry needs arise later.
   * - ``fault_state``
     - Shared
     - Defines capability availability, fault classification, and recovery
       notifications exchanged between application modules.

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
       and environmental data per track (REQ-023, REQ-029, REQ-031,
       REQ-032).
   * - ``audio_processing``
     - Processing
     - Consumes audio blocks and runs the audio processing algorithm on
       each (REQ-033).
   * - ``storage``
     - Storage
     - Persists processed blocks and environmental data/timestamps,
       assembles ecoacoustic records,        removes them once durable delivery is
       confirmed, preserves failed records, and
       stores reset history and failure details to NVS (REQ-025, REQ-027,
       REQ-034, REQ-035, REQ-036, REQ-037).
   * - ``cloud_link``
     - Transmission
     - Sends payloads to EASNFW-CLOUD and reports delivery outcomes back
       to ``storage`` (REQ-024, REQ-026, REQ-037).

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
     - Bit-exact PCM pass-through; the final algorithm is tracked in
       :doc:`open_items`. Used by
       ``audio_processing``.
   * - ``selftest``
     - Implements the individual self-test checks and their aggregation
       (REQ-023), including capability checks. Used by
       ``sampling`` during initialization.

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
   * - ``uart_transport``
     - Low-level UART send/receive and framing, used together with
       ``uart_protocol``. Used by ``cloud_link``.

MCUboot and Rollback
====================

Both application sysbuild configurations include MCUboot with ECDSA-P256
signature verification and the default swap-based image mode. The application
images are signed with the SDK development key unless the build overrides
``SB_CONFIG_BOOT_SIGNATURE_KEY_FILE``. Production builds must use a protected
private key kept outside the repository; the SDK key is not a production trust
anchor.

EASNFW-SENSOR confirms a trial image only after local self-tests, UTC
synchronization, and reset-history persistence succeed. EASNFW-CLOUD confirms
after the local inter-firmware link initializes. See :doc:`open_items` for the
remaining backend-health and rollback-validation gaps. An unconfirmed image
can be rolled back by MCUboot on a subsequent reset. The firmware does not
initiate a reset to trigger rollback.

The bootloader and dual-slot layout provide a foundation only. Remaining
FUOTA, rollback, partition-size, and update power-loss decisions are listed in
:doc:`open_items`.

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
     - Receives payloads from EASNFW-SENSOR over UART and relays delivery
       acknowledgements back to it (REQ-024, REQ-026).
   * - ``payload_assembler``
     - Assembling
     - Reassembles and validates UART fragments and adds the transport envelope
       needed by the cloud platform. The canonical record itself is assembled
       by EASNFW-SENSOR (REQ-034).
   * - ``cloud_tx``
     - Transmitting
     - Transmits payloads to the cloud platform, with retry/backoff on
       failure and reports the outcome back to EASNFW-SENSOR
       (REQ-026, REQ-037).

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
   * - ``uart_transport``
     - Low-level UART send/receive and framing, used together with
       ``uart_protocol``. Used by ``sensor_link``.
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
     [uart_protocol] as UartProto
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
       [uart_transport] as UartTransportS
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
       [uart_transport] as UartTransportC
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
   CloudLink --> UartTransportS
   CloudLink --> UartProto
   CloudLink --> Record

   SensorLink --> UartTransportC
   SensorLink --> UartProto
   Assembler --> PayloadFormat
   Assembler --> Record
   CloudTx --> HttpClient
   CloudTx --> Retry
   CloudTx --> UartProto
   HttpClient --> LteM
   @enduml

Implementation Notes
====================

The current architecture uses SENSOR NVS for failure details and retains
FAT-formatted records for direct PC access. The IFL record-delivery path and
the audio algorithm remain at their documented baseline states; the complete
list of pending decisions and validation is maintained in :doc:`open_items`.

Storage deliberately remains FAT so records can be inspected and copied on a
PC without a project-specific reader. Each record has a versioned header,
header CRC, expected PCM length, and a completion footer with byte count and
PCM CRC. Each append is synchronized; at boot, the firmware scans record files,
validates lengths and checksums, promotes a valid complete ``.tmp`` file to
``.rec``, and renames incomplete or corrupt ``.tmp``/``.rec`` files to
``.bad`` so they are not eligible for transmission.

This is application-level validation and recovery, not a transactional FAT
guarantee. ``fs_sync()`` and rename cannot prevent arbitrary power loss from
interrupting FAT metadata or the SD card's internal flash translation layer.
The current track may be lost, and filesystem damage can affect older files.
CRCs detect accidental corruption but do not authenticate data. See
:doc:`open_items` for the required durability validation.

Audio Interface Decision
========================

The initial acquisition baseline is the PUI Audio DMM-4026-B-I2S-R external
I2S digital microphone on Audio DK header P10, producing 16-bit PCM at 48 kHz
directly to the nRF5340. The SENSOR pin routing and I2S settings follow the
known-good ``tools/mic_testbed`` image. This path does not
require the CS47L63 hardware audio codec. The codec available on the nRF5340
Audio DK remains an experimental alternative for analog input or for acoustic
front ends whose requirements cannot be met by a digital microphone.

The current sensor application configures I2S0 for mono, 32-bit microphone
words, converts the upper 16 bits to PCM16, and stores bounded blocks in an
incomplete record file. The processing module is an exact PCM pass-through;
this is a placeholder, not the final scientific algorithm. The algorithm
decision and its validation are tracked in :doc:`open_items`.

UTC time is obtained from the nRF9151 modem's LTE network time using Nordic's
date-time library, then transferred to SENSOR over an acknowledged IFL command.
SENSOR anchors UTC to its monotonic uptime and formats record timestamps as
ISO-8601 UTC strings with ``Z``. Each boot waits for valid UTC before acquiring
data; time unavailability alone does not trigger diagnostic state. Network-time
validation is tracked in :doc:`open_items`.

The nRF5340's direct PDM peripheral is not the baseline because its hardware
PCM output is limited to 16 kHz. A PDM microphone may still be evaluated when
16-kHz acquisition is acceptable or when routed through a separate decimation
stage such as the CS47L63.

The environmental input is configured as an external BME688-compatible device
at the provisional I2C address ``0x76`` on the Audio DK I2C connector. The
address and wiring have not been validated on production hardware. The Zephyr
BME680 driver provides temperature, pressure, humidity, and gas resistance.
Gas resistance is only a raw VOC-related indicator, not a calibrated VOC
concentration. Sensor selection and production validation are tracked in
:doc:`open_items`.

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
