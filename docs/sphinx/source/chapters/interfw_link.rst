****************
Inter-FW Link
****************

The Inter-FW Link is the UART connection between EASNFW-SENSOR and
EASNFW-CLOUD. It carries commands, responses and data exchange needed to
coordinate the application across the two firmware components.

Both components treat a command as complete only after receiving an
acknowledgement and the corresponding response when a response is expected.

Hardware connection
===================

The link is a 1 Mbaud, 8-N-1 UART with RTS/CTS hardware flow control. Keep the
USB VCOM/UART0 connection for the SENSOR host shell and logs.

On EASNFW-SENSOR, UART2 is routed as follows:

TXD: P1.09
RXD: P1.08
CTS: P1.10
RTS: P1.11

(UART2, need to ensure Audio DK solder bridge SB20 is intact for UART2
RTS/CTS.)

On the nRF9151 SMA DK (EASNFW-CLOUD), UART2 is routed to the P4 header
(Arduino D2, D3, D6, D7), matching Nordic's external MCU-host routing:

TXD: P0.02
RXD: P0.03
RTS: P0.06
CTS: P0.07

Connect the signals between the boards as follows:

* nRF5340 TXD to nRF9151 RXD
* nRF5340 RXD to nRF9151 TXD
* nRF5340 RTS to nRF9151 CTS
* nRF5340 CTS to nRF9151 RTS

The console, shell and logging stay on UART0/VCOM0, so no Board Configurator
change is needed. Do not mount an Arduino shield that uses D2, D3, D6 or D7.
Both boards must use the same I/O voltage (1.7 V to 3.6 V is allowed by both
SoCs); verify the nRF9151 DK VDD_GPIO setting and the Audio DK I/O voltage
before connecting.

Protocol
========

The protocol is symmetric at the frame level, but commands are assigned to a
receiving component. All multi-byte fields are little-endian.

Frame format
------------

.. list-table:: IFL frame layout
   :header-rows: 1
   :widths: 10 10 10 10 10 10 10 25 10

   * - Field
     - SOF
     - TYPE
     - CMD_ID
     - SEQ
     - FLAGS
     - LEN
     - PAYLOAD
     - CRC16
   * - Size
     - 1 byte
     - 1 byte
     - 1 byte
     - 1 byte
     - 1 byte
     - 2 bytes
     - 0..LEN bytes
     - 2 bytes
   * - Meaning
     - ``0xA5``
     - Command or ACK
     - Command identifier
     - Sequence
     - Fragment/retry flags
     - Payload length
     - Command-specific
     - CRC-16/CCITT-FALSE

The fixed header is 7 bytes. The CRC is calculated over ``TYPE`` through the
last payload byte, excluding ``SOF`` and the CRC itself. CRC parameters are
polynomial ``0x1021``, initial value ``0xFFFF``, no reflection, and no final
XOR. A receiver discards malformed, oversized, timed-out, or CRC-invalid
frames. The shared protocol header defaults to a 2048-byte payload ceiling.

Each command frame receives an ACK containing the echoed command ID and
sequence plus a 6-byte payload: status byte, reserved byte (zero), and
``next_offset`` (little-endian 32-bit; used for fragment resume). Senders use
stop-and-wait retry. Retransmissions retain the original sequence; receivers
cache and resend the previous ACK for a duplicate instead of executing the
command again. Sensor-side ACK timeout/retry values are Kconfig options; CLOUD
uses the protocol header's current defaults.

ACK status codes are ``OK`` (``0x00``), ``UNKNOWN_CMD`` (``0x01``),
``BAD_LEN`` (``0x02``), ``BAD_PARAM`` (``0x03``), ``BUSY`` (``0x04``),
``NOT_SUPPORTED`` (``0x05``), ``BAD_OFFSET`` (``0x06``),
``NO_MEMORY`` (``0x07``), ``TRANSFER_ERR`` (``0x08``), and
``INTERNAL_ERR`` (``0xFF``).

Commands
--------

Command identifiers identify the command supported by the receiver:

.. list-table:: Implemented IFL commands
   :header-rows: 1
   :widths: 12 24 18 46

   * - ID
     - Command
     - Direction
     - Payload and behavior
   * - ``0x01``
     - ``PING``
     - Either component to the other
     - Empty. Receiver acknowledges link liveness.
   * - ``0x10``
     - ``CLOUD_READY``
     - CLOUD to SENSOR
     - Three-byte firmware version; currently sent as ``1.0.0`` during CLOUD
       startup. SENSOR records that CLOUD is ready.
   * - ``0x11``
     - ``UTC_TIME``
     - CLOUD to SENSOR
     - 12 bytes: Unix UTC milliseconds (unsigned 64-bit) followed by CLOUD
       reset-cause bits (unsigned 32-bit). SENSOR validates and anchors UTC.
   * - ``0x80``
     - ``RESET``
     - SENSOR to CLOUD
     - Empty. CLOUD ACKs, delays briefly, then cold-reboots.
   * - ``0x81``
     - ``RECORD_DATA``
     - SENSOR to CLOUD
     - Fragmentable record payload. Each fragment starts with transfer ID
       (8-bit), total length (32-bit), and offset (32-bit), followed by data.
       CLOUD validates offsets and stages the received record in bounded RAM.
       SENSOR's committed-record sender is not yet wired to this path.
   * - ``0x82``
     - ``TIME_SYNC_REQUEST``
     - SENSOR to CLOUD
     - Empty. CLOUD ACKs queueing the request, obtains modem/network UTC
       asynchronously, and sends a separate ``UTC_TIME`` command. An ACK
       confirms request acceptance, not that network time was available.

For fragmented ``RECORD_DATA``, ``FRAGMENT`` marks fragment frames,
``LAST`` marks the final fragment, and ``ABORT`` terminates a transfer.
Fragments are individually acknowledged; the receiver returns the next
expected offset. Out-of-order offsets are rejected with ``BAD_OFFSET``.
``RETRANSMIT`` is informational. This framing capability does not itself
provide an end-to-end cloud commit acknowledgement; see :doc:`open_items`.

Command behavior
----------------

.. _section_handshake_reset_fw_cloud:

reset-fw-cloud
--------------

#. EASNFW-SENSOR sends the reset command to EASNFW-CLOUD.
#. EASNFW-CLOUD acknowledges the command, delays briefly, then cold-reboots.
#. After restarting, EASNFW-CLOUD sends ``CLOUD_READY``; its acknowledgement
   confirms that the peer has completed IFL initialization.

.. note::

   The current implementation exchanges acknowledged control commands,
   including reset and PING. Record delivery is not complete until SENSOR
   streams committed records and CLOUD can acknowledge durable backend
   commitment; see :doc:`open_items`.
