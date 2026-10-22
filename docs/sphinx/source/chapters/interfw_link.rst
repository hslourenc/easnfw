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

(UART2, need to ensure Audio DK solder bridge SB20 is intact for UART2
RTS/CTS.)

On the nRF9151 DK (EASNFW-CLOUD), use UART2 on the P4 header (Arduino D2, D3,
D6, D7), the same pins Nordic uses for an external MCU host in the Serial
Modem application:

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

EASNFW-SENSOR to EASNFW-CLOUD Commands
======================================

.. _section_handshake_reset_fw_cloud:

reset-fw-cloud
--------------

#. EASNFW-SENSOR sends the reset command to EASNFW-CLOUD.
#. EASNFW-CLOUD acknowledges the command and resets.
#. EASNFW-CLOUD sends a reset-complete response after it has restarted.
