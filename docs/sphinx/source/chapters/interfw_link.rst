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

The link uses an 8-N-1 UART; its baud rate and RTS/CTS setting are defined in
the shared protocol header and rendered below. Keep the USB VCOM/UART0
connection for the SENSOR host shell and logs.

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

Protocol definitions from source
--------------------------------

The wire-level declarations below are rendered from the shared protocol
header. Keep frame constants, command identifiers, status values, and payload
layouts in that header; the documentation is generated from those comments on
each Sphinx build.

Wire defaults and limits
^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygendefine:: INTERFW_LINK_UART_BAUDRATE
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_FLOW_CTRL
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_MAX_PAYLOAD
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_ACK_TIMEOUT_MS
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_MAX_RETRIES
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_RX_INTERBYTE_TIMEOUT_MS
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_SOF
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_HDR_SIZE
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_CRC_SIZE
   :project: shared

.. doxygendefine:: INTERFW_LINK_UART_CRC_INIT
   :project: shared

.. doxygendefine:: EASN_FLAG_FRAGMENT
   :project: shared

.. doxygendefine:: EASN_FLAG_LAST
   :project: shared

.. doxygendefine:: EASN_FLAG_ABORT
   :project: shared

.. doxygendefine:: EASN_FLAG_RETRANSMIT
   :project: shared

Frame types
^^^^^^^^^^^

.. doxygenenum:: easn_frame_type_t
   :project: shared

Endpoint roles and state
^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenenum:: easn_role_t
   :project: shared

.. doxygenstruct:: ifl_runtime_state
   :project: shared

Command identifiers and direction ranges
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenenum:: ifl_commands
   :project: shared

Acknowledgement status values
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenenum:: ifl_status_t
   :project: shared

Frame and payload layouts
^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygentypedef:: easn_frame_hdr_t
   :project: shared

.. doxygentypedef:: easn_ack_payload_t
   :project: shared

.. doxygentypedef:: easn_frag_hdr_t
   :project: shared

.. doxygentypedef:: ifl_cmd_payload_cloud_ready_t
   :project: shared

.. doxygentypedef:: ifl_cmd_payload_utc_time_t
   :project: shared

Protocol helpers
^^^^^^^^^^^^^^^^

.. doxygenfunction:: ifl__is_cmd_valid
   :project: shared

.. doxygenfunction:: ifl__is_init
   :project: shared

.. doxygenfunction:: ifl__init
   :project: shared

.. doxygenfunction:: ifl__cmd_tx
   :project: shared

The declarations and their source comments are generated from the shared
protocol header. A link acknowledgement means the peer accepted or rejected a
frame; it is not a durable cloud-commit acknowledgement. See :doc:`open_items`
for record-delivery status.

Frame handling
^^^^^^^^^^^^^^

Each endpoint sends one command at a time and matches the acknowledgement by
command ID and sequence number. The sender retries on ACK timeout using the
same sequence; the SENSOR timeout and retry limit can be configured with
``CONFIG_EASNFW_IFL_UART_ACK_TIMEOUT_MS`` and
``CONFIG_EASNFW_IFL_UART_MAX_RETRIES``. CLOUD uses the shared protocol
defaults. CRC-invalid, oversized, or inter-byte-timeout frames are discarded.
A command flagged ``EASN_FLAG_RETRANSMIT`` that repeats the previous command
and sequence receives the previous acknowledgement status again without
delivering the command to the application a second time. Unflagged commands
are always delivered, because a rebooted peer restarts its sequence numbers.

Both endpoints re-enable UART reception if the driver disables it (for example
after a line error caused by the peer resetting), and SENSOR bounds each frame
transmission to 200 ms so a peer that holds its RTS line cannot keep the
transmitter busy indefinitely.

Fragmented data commands carry ``easn_frag_hdr_t`` followed by the fragment
bytes. Each fragment is acknowledged; CLOUD reports the next expected byte
offset, and an unexpected offset is rejected. ``LAST`` marks the final
fragment, while ``ABORT`` ends an in-progress transfer. Fragment-level
acknowledgements do not imply that a complete record was durably stored by the
cloud backend.

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
