# EASNFW

Firmware for the EASN (ecoacoustic sensing node), an ecoacoustic data
acquisition device. EASNFW controls the node hardware to acquire, process and
transmit ecoacoustic data to a cloud platform.

The Sphinx documentation under `docs/sphinx/source` is the authoritative
project documentation. This README is a repository entry point and quick
reference; consult the Sphinx [introduction](docs/sphinx/source/chapters/introduction.rst),
[build and signing guide](docs/sphinx/source/chapters/building.rst),
[verification chapter](docs/sphinx/source/chapters/verification.rst),
[Inter-FW Link protocol](docs/sphinx/source/chapters/interfw_link.rst), and
[open-items register](docs/sphinx/source/chapters/open_items.rst) for current
details.

EASNFW is subdivided into two components:

- **EASNFW-SENSOR** — runs on the nRF5340 SoC (nRF5340 Audio DK) and is
  responsible for interfacing with the system sensors, and for processing,
  storing and retrieving the acquired data.
- **EASNFW-CLOUD** — runs on the nRF9151 SiP (nRF9151 SMA DK) and is
  responsible for interfacing with the cloud over HTTP over LTE-M.

## Repository layout

- `docs/sphinx/source/` — source for requirements, architecture, design,
  verification, and cloud-backend documentation
- `docs/sphinx/build/html/` — locally generated documentation site
- `fw/easnfw-sensor/app/` — nRF5340 Audio DK firmware, including acquisition,
  processing, SD/NVS storage, inter-FW link, and HIL host CLI
- `fw/easnfw-cloud/app/` — nRF9151 DK firmware, including LTE-M/cloud-facing
  functionality
- `fw/shared/` — protocol definitions shared by both firmware images
- `tools/easncli/` — Python CLI_C that drives the sensor HIL shell
- `tools/mic_testbed/` — known-good I2S microphone and SD-card capture example

## Building the documentation

For the full and authoritative build instructions, see the
[Building and Signing](docs/sphinx/source/chapters/building.rst) chapter.
Requirements include Python packages from `docs/requirements.txt`, Java, and a
PlantUML jar.

Quick build:

```sh
python -m pip install -r docs/requirements.txt
PLANTUML_JAR=/path/to/plantuml.jar make -C docs/sphinx html
```

The output is written to `docs/sphinx/build/html`.

## Building the firmware

1. Install the command line version of nRF Connect SDK v3.4.0 as per
   [Installing the nRF Connect
   SDK](https://nrfconnectdocs.nordicsemi.com/ncs/3.4.0/nrf/installation/install_ncs.html).
2. Start the toolchain environment with (also from [Installing the nRF Connect SDK](https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/installation/install_ncs.html)):

    **Linux:**

    ```
    nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 --shell

    ```

    **macOS:**

    ```
    nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 --shell
    ```

    **Windows:**

    ```
    nrfutil sdk-manager toolchain launch --ncs-version v3.4.0 --terminal
    ```

3. Run the script to define the required environment variables (assuming the
   default locations were used to install nRF Connect SDK):

    **Linux:**

    ```
    source ~/ncs/v3.4.0/zephyr/zephyr-env.sh

    ```

    **macOS:**

    ```
    source /opt/nordic/ncs/v3.4.0/zephyr/zephyr-env.sh

    ```

    **Windows:**

    ```
    C:\ncs\v3.4.0\zephyr\zephyr-env.cmd
    ```

4. At the root of the repository, build EASNFW-SENSOR with:

    ```
    west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build
    ```

    and EASNFW-CLOUD with:

    ```
    west build --pristine -b nrf9151dk/nrf9151/ns fw/easnfw-cloud/app --build-dir fw/easnfw-cloud/app/build
    ```

    The CLOUD image uses the non-secure nRF9151 target because the LTE modem
    library and network date-time service require non-secure execution.

    The default image type is the development image. To build a different
    image, create a Kconfig fragment and pass it using
    `-DEXTRA_CONF_FILE=/path/relative/to/the/application/fragment.conf`; for
    example, the sensor HIL image uses `hil.conf` as described below.
    Image choices are `PRODUCTION`, `DEVELOPMENT` (the default), `DEBUG`, and
    `HIL`.

Both applications enable MCUboot with ECDSA-P256 image verification and
swap-based rollback. The SDK's development signing key is used by default;
never ship an image built with that key. Keep the production private key outside
the repository and select it at build time, for example:

```sh
west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp \
  fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build \
  -- -DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE=/secure/path/easnfw-signing.pem
```

The CLOUD build uses the same `SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` override
with its nRF9151 non-secure board target. SENSOR confirms a candidate image
after its self-tests, UTC synchronization, and reset-history update succeed;
CLOUD currently confirms after its local IFL initialization because its backend
health contract is not yet defined. An unconfirmed candidate is eligible for
MCUboot rollback on a subsequent reset; firmware does not initiate recovery
resets automatically. See [Building and Signing](docs/sphinx/source/chapters/building.rst)
for signing-key and artifact details. Network download, update authorization,
and coordinated SENSOR/CLOUD rollout remain unimplemented; see the Sphinx
[open-items register](docs/sphinx/source/chapters/open_items.rst).

## Building the HIL verification image

`hil.conf` is an additional Zephyr Kconfig fragment, not a standalone
configuration. Build it with `EXTRA_CONF_FILE` (from the repository root):

```sh
west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp \
  fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build \
  -- -DEXTRA_CONF_FILE=hil.conf
```

In the HIL image, logs and shell output share the shell's UART backend on
UART0/VCOM0. This avoids competing UART transmitters that can scramble the
console output. Open VCOM0 at 115200 baud, 8-N-1, with no flow control; this is
also the setting used by CLI_C. The Audio DK board controller enables
hardware flow control on VCOM0, so the SENSOR application and MCUboot enable
it in their devicetree (`app.overlay`, `sysbuild/mcuboot.overlay`); without it
console characters are lost. If VCOM0 is garbled or unresponsive regardless,
power-cycle the Audio DK to restore the board controller's serial bridge.

Flash the normal EASNFW-CLOUD image to the nRF9151 DK and connect the two
boards using the UART2 wiring in
[`interfw_link.rst`](docs/sphinx/source/chapters/interfw_link.rst). The sensor
DK USB VCOM remains the CLI/log interface.

For first bring-up, make all inter-board signal and ground connections with
both boards unpowered; do not connect their power rails together. Verify that
the I/O voltage levels match. Flash both boards, connect both to USB, then boot
SENSOR before CLOUD so its IFL receiver is ready for CLOUD's startup handshake.
Keep both powered and connected while running HIL tests; CLI_C coordinates
resets between test cases.

## Running HIL test cases

Install the host CLI from `tools/easncli`:

```sh
python -m pip install -e tools/easncli
easncli tc 1
```

CLI_C currently exposes TC-001 through TC-013, TC-020, TC-042, TC-046, and
TC-048. Use the following primary commands to run suites or inspect coverage:

```sh
easncli tcfull
easncli tcfull --implemented-only
easncli tcfull --no-human
easncli tcfull --no-env-sensor --no-sim
easncli cases
```

`--implemented-only` skips cases that are not completely implemented,
`--no-human` skips cases requiring human intervention, `--no-env-sensor` skips
cases requiring the environmental sensor, and `--no-sim` skips cases requiring
a SIM. See the Sphinx
[verification chapter](docs/sphinx/source/chapters/verification.rst) for the
coverage catalog, meanings, and aggregate-result behavior. The CLI
automatically selects the only serial port visible to the host; use
`easncli tc 1 --port /dev/ttyACM0` when more than one serial port is present.
Each run creates a timestamped directory containing `result.json` and device
and CLI logs.

The current bench has no external environmental sensor or SIM. TC-007 depends
on the environmental sensor, and TC-048 needs cellular service and a SIM to
complete UTC synchronization; both are implemented but cannot be fully verified
on this setup. Run
`easncli tcfull --implemented-only --no-human --no-env-sensor --no-sim` to
exclude them. TC-048 can still be run individually, but without
cellular service it ends inconclusively.

Use `--pristine` when changing image types so that the previous configuration
is removed.

The current microphone, environmental-sensor, and SD-card connections are
documented in the Sphinx [hardware overview](docs/sphinx/source/chapters/introduction.rst).
The IFL pins and wire protocol are documented in
[Inter-FW Link](docs/sphinx/source/chapters/interfw_link.rst). Remaining
scientific, durability, backend, and update questions are centralized in the
[open-items register](docs/sphinx/source/chapters/open_items.rst).
