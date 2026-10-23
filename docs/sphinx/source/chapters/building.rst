********************
Building and Signing
********************

This chapter is the authoritative guide to configuring, building, signing, and
running EASNFW images. The repository is validated with nRF Connect SDK v3.4.0
and Zephyr 4.4.0.

Prerequisites
=============

Install nRF Connect SDK v3.4.0 and its matching toolchain using the
`Nordic installation guide
<https://nrfconnectdocs.nordicsemi.com/ncs/3.4.0/nrf/installation/install_ncs.html>`_.
Start a toolchain terminal, enter the repository root, and load the Zephyr
environment:

.. code-block:: console

   source ~/ncs/v3.4.0/zephyr/zephyr-env.sh

On Windows, run ``C:\ncs\v3.4.0\zephyr\zephyr-env.cmd`` from the NCS command
prompt.

Building Firmware
=================

Build the nRF5340 Audio DK SENSOR image:

.. code-block:: console

   west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp \
     fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build

Build the nRF9151 SMA DK CLOUD image:

.. code-block:: console

   west build --pristine -b nrf9151dk/nrf9151/ns \
     fw/easnfw-cloud/app --build-dir fw/easnfw-cloud/app/build

CLOUD uses the non-secure target because the Nordic modem and cellular
date-time libraries require non-secure execution.

Both applications use Zephyr sysbuild and include MCUboot, signed images, and
the default swap-based upgrade mode. The default image type is DEVELOPMENT.
Image types are selected by Kconfig:

* ``PRODUCTION``: business logic without development logging.
* ``DEVELOPMENT``: production behavior with normal logs enabled.
* ``DEBUG``: development behavior with increased logging.
* ``HIL``: SENSOR shell image for CLI-driven hardware verification.

To build the SENSOR HIL image, pass its application-relative Kconfig fragment:

.. code-block:: console

   west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp \
     fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build \
     -- -DEXTRA_CONF_FILE=hil.conf

The ``hil.conf`` file selects the HIL image and enables its serial shell.
It is an extra fragment, not a standalone project configuration. Flash the
normal CLOUD image to the nRF9151 board. Connect UART2 between the development
kits as described in :doc:`interfw_link`; keep the SENSOR USB VCOM connected
for CLI_H, logs, and CLI_C.

Image Signing
=============

MCUboot verifies ECDSA-P256 signatures. The SDK's built-in development key is
selected by default so that a development build is reproducible. It is public
and insecure: **never deploy images signed with the SDK key**. Do not commit a
production private key to the repository.

Provide a protected PEM signing key at build time through sysbuild. For
example, build SENSOR with:

.. code-block:: console

   west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp \
     fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build \
     -- -DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=/secure/path/easnfw-signing.pem

Use the same override when building CLOUD, with its non-secure nRF9151 target.
Keep the matching verification key configured in MCUboot for the production
device; signing the application with a different key does not make it
bootable.

Sysbuild produces signed application artifacts in the domain build directory,
including ``zephyr.signed.bin`` and ``zephyr.signed.hex``. Flash the sysbuild
output rather than an unsigned application artifact:

.. code-block:: console

   west flash -d fw/easnfw-sensor/app/build

An image installed as a test candidate remains unconfirmed until its component
calls the MCUboot confirmation API. SENSOR confirms after local self-tests,
UTC synchronization, and reset-history persistence. CLOUD currently confirms
after its local IFL initialization because its backend-health contract is
undefined. An unconfirmed image is eligible for MCUboot rollback on a later
reset. Firmware does not initiate recovery resets automatically.

HIL Verification
================

Install CLI_C from the repository root:

.. code-block:: console

   python -m pip install -e tools/easncli

Run one test and pass a port explicitly if multiple serial ports are present:

.. code-block:: console

   easncli tc 1
   easncli tc 1 --port /dev/ttyACM0

Run all currently exposed HIL cases, only completely implemented cases, or
cases that do not require human intervention:

.. code-block:: console

   easncli tcfull
   easncli tcfull --implemented-only
   easncli tcfull --no-human
   easncli cases

See :doc:`verification` for result semantics and current test coverage.
CLI_C creates a timestamped result directory with JSON results and logs.

Building Documentation
======================

Install the documentation dependencies from ``docs/requirements.txt`` and
provide Java plus a PlantUML jar:

.. code-block:: console

   python -m pip install -r docs/requirements.txt
   PLANTUML_JAR=/path/to/plantuml.jar make -C docs/sphinx html

The generated HTML site is under ``docs/sphinx/build/html``. The Sphinx source
in ``docs/sphinx/source`` is the primary and authoritative project
documentation; README.md is only a repository entry point and quick reference.
