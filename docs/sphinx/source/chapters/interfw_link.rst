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

On EASNFW-SENSOR:

TXD: P1.09
RXD: P1.08
CTS: P1.10
RTS: P1.11

(UART2)

EASNFW-SENSOR to EASNFW-CLOUD Commands
======================================

.. _section_handshake_reset_fw_cloud:

reset-fw-cloud
--------------

#. EASNFW-SENSOR sends the reset command to EASNFW-CLOUD.
#. EASNFW-CLOUD acknowledges the command and resets.
#. EASNFW-CLOUD sends a reset-complete response after it has restarted.
