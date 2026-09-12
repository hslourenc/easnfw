************
Introduction
************

Purpose
=======

The purpose of this document is to specify the requirements, architecture and
implementation details of the software product designated as Ecoacoustic
Sensing Node Firmware (EASNFW), which controls the hardware of a device
designated as Ecoacoustic Sensing Node (EASN).

Product Overview
================

Product Functions
-----------------

The main functions of EASNFW are:

* acquiring soundscape (audio) data;
* acquiring environmental data;
* processing the acquired audio data; and
* transmitting the acquired environmental data and the processed audio data to
  a cloud platform.

Product Perspective
-------------------

EASNFW is intended to integrate a distributed ecoacoustic data acquisition
infrastructure. This infrastructure is composed of a set of EASNs and a cloud
platform.

The cloud platform stores the data acquired by the sensing nodes and provides
support for visualizing and analyzing the collected data.

The hardware of EASN is based on the development kits nRF5340 Audio DK and
nRF9151 SMA DK from Nordic Semiconductor.

EASNFW is subdivided into two components:

* *EASNFW-SENSOR* runs on the nRF5340 system on chip (SoC) of the nRF5340 Audio
  DK and is mainly responsible for acquiring audio and environmental data
  through the system sensors and processing the acquired audio data.
* *EASNFW-CLOUD* runs on the nRF9151 system in package (SiP) of the nRF9151 SMA
  DK and is mainly responsible for transmitting the acquired and processed data
  to the cloud.

EASNFW-SENSOR and EASNFW-CLOUD communicate over SPI, with EASNFW-SENSOR as the
controller and EASNFW-CLOUD as the peripheral.

Product Interfaces
------------------

Cloud Interface
^^^^^^^^^^^^^^^

EASNFW-CLOUD communicates with the cloud using HTTPS over LTE-M through the
built-in LTE-M modem of nRF9151. Connections are always initiated by
EASNFW-CLOUD. Besides uploading data, the protocol may later allow
EASNFW-CLOUD to retrieve configuration or maintenance commands in HTTPS
responses without requiring an inbound connection to the sensing node.

USB Interfaces for Host Personal Computers (PCs)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

EASNFW uses the USB interfaces of the nRF5340 Audio DK and nRF9151 SMA DK to 
ommunicate with host personal computers (PCs) for testing and debugging.

USB Command Line Interface (CLI)
""""""""""""""""""""""""""""""""

EASNFW-SENSOR provides a command line interface (CLI) for executing test cases,
verifying data acquisition and processing funcionalities, and quickly
experimenting with different implementation approaches. This interface is
enabled only in the HIL verification image (see :ref:`section_fw_images`).

USB Logging Interface
"""""""""""""""""""""

Both EASNFW-CLOUD and EASNFW-SENSOR log their activities through the USB interface of their
respective boards when logging is enabled.

System Architecture
===================

Figure :numref:`diagram_sys_arch` provides an overview of the system
architecture, showing the main blocks that build the system as well as how they
connect to each other and to external elements. Table
:numref:`table_sys_blocks` provides more detail on the main blocks that EASNFW
interacts with.

.. _diagram_sys_arch:

.. uml:: ../diagrams/sys_arch.puml
   :caption: System architecture overview

.. _table_sys_blocks:

.. list-table:: System blocks
   :header-rows: 1

   * - Block name
     - Part number
     - Connectivity
     - Built-in to the development kits
     - Responsible firmware component
     - Purpose
   * - Audio sensor
     - TBD
     - I2S (initial baseline)
     - No
     - EASNFW-SENSOR
     - Acquire audio data.
   * - Environmental sensor
     - BME688
     - I2C
     - No
     - EASNFW-SENSOR
     - Acquire environmental data: temperature, humidity, pressure and
       volatile organic compounds (VOCs).
   * - Mass storage
     - SD card (exact part TBD)
     - SPI
     - Yes (SD card holder built-in to the nRF5340 Audio DK)
     - EASNFW-SENSOR
     - Store and persist ecoacoustic data.
   * - Non-volatile storage (NVS)
     - Internal nRF5340 flash
     - Memory-mapped
     - Yes
     - EASNFW-SENSOR
     - Store and persist miscellaneous data, such as details about a failure
       that led to a reset.

..
   * - Backup storage
     - GD25WB256E
     - SPI
     - Yes (built-in to the nRF9151 SMA DK)
     - EASNFW-CLOUD
     - Store audio and environmental data after EASNFW-CLOUD receives it from
       EASNFW-SENSOR, in order to persist that data in case a reset happens
       before it is transmitted to the cloud.

..
   User Characteristics
   ~~~~~~~~~~~~~~~~~~~~

   Users of EASN and EASNFW are expected to be familiar with general concepts
   related to embedded systems, the Internet of Things (IoT), wireless
   communication, and digital signal processing (DSP).

   Limitations
   ~~~~~~~~~~~

   EASNFW functions and their implementations are constrained by the computational,
   memory, communication, and energy resources of the target hardware, and by the
   interfaces provided by the cloud platform.

Definitions
===========

Audio track
-----------

An ordered collection of audio samples captured sequentially over a continuous
period of time.

Audio processing algorithm
---------------------------

The algorithm used to process recorded audio tracks.

Processed audio track
---------------------

The output, or collection of outputs, produced by the audio processing
algorithm for a given audio track.

Environmental data
------------------

Temperature, humidity, pressure and volatile organic compounds (VOCs), acquired
through the environmental sensor.

Self-test sequence
------------------

A sequence of tests executed by EASNFW to verify the correct operation of
individual software modules and their integration. It comprises an independent
self-test sequence for each firmware component and a shared self-test sequence
carried out by both components to test functionality that depends on the
interaction between the firmware components.

Payload
-------

A collection of data transmitted by EASNFW to the cloud in a single message.

.. _section_fw_images:

Firmware Images
===============

Build options provide different firmware images depending on intended usage.

Production image
----------------

The image used for deployment. It executes all business logic automatically out
of reset, with logging and other debug functionality disabled.

Development image
-----------------

The image used for development. It is the same as the production image, but
with logging and other debug functionality enabled.

Debug image
-----------

The image used for debugging. It is the same as the development image, but with
increased logging verbosity.

HIL verification image
----------------------

The image used for hardware-in-the-loop (HIL) verification. Instead of
automatically executing business logic out of reset, it runs a CLI that
receives and parses commands through the USB interface of the nRF5340 Audio DK,
triggers specific code paths, and returns results. Its purpose is to support
tests and on-demand data acquisition and processing for verification and
experimentation.

.. _section_data_types:

Data Types
==========

Ecoacoustic Record
------------------

An ecoacoustic record is the canonical, persistent representation of one
acquisition event. It is assembled by EASNFW-SENSOR and comprises a unique
record identifier, the node and firmware identifiers, audio acquisition and
processing configuration, a processed audio track, environmental data,
quality/status flags, and timestamps indicating when acquisition began and
ended. It may also reference a locally stored raw audio track.

The record is complete only after all expected audio blocks and metadata have
been persisted and validated. Complete records are committed atomically to
mass storage and retained until the cloud platform confirms durable storage.

SPI Transfer Frame
------------------

A versioned and checksummed fragment used to transfer a complete ecoacoustic
record or another message between EASNFW-SENSOR and EASNFW-CLOUD. A record may
span multiple frames. Frames carry, at minimum, a message type, record
identifier, fragment index and count, payload length, and integrity check.

Cloud Payload
-------------

A single HTTPS request body transmitted by EASNFW-CLOUD. It contains record
data received from EASNFW-SENSOR and a transport envelope with schema version,
node identity, delivery attempt information, and relevant network metadata.
Cloud payload formatting is a transport concern and does not define the
canonical on-device ecoacoustic record format.

Power-on Log
------------

A power-on log consists of the respective revisions of EASNFW-SENSOR and
EASNFW-CLOUD, the reason why the device was last reset, and the results of the
self-test sequence. Power-on logs are transmitted to the cloud through power-on
log payloads.
