# EASNFW

Firmware for the EASN (ecoacoustic sensing node), an ecoacoustic data
acquisition device. EASNFW controls the node hardware to acquire, process and
transmit ecoacoustic data to a cloud platform.

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
- `fw/` — firmware source code

## Building the documentation

### Prerequisites

- Python and the packages listed in `docs/requirements.txt`.
- Java and a PlantUML jar for generating the diagrams.

### Steps

Install the documentation dependencies:

```sh
python -m pip install -r docs/requirements.txt
```

Set `PLANTUML_JAR` to a PlantUML jar and build the HTML site:

```sh
PLANTUML_JAR=/path/to/plantuml.jar make -C docs/sphinx html
```

The output is written to `docs/sphinx/build/html`.

## Building the firmware

1. Install the command line version of nRF Connect SDK as per [Installing the nRF Connect SDK](https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/installation/install_ncs.html).
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

3. At the root of the repository, build EASNFW-SENSOR with:

    ```
    west build --pristine -b nrf5340_audio_dk/nrf5340/cpuapp fw/easnfw-sensor/app --build-dir fw/easnfw-sensor/app/build
    ```
    
    and EASNFW-CLOUD with:
    
    ```
    west build --pristine -b nrf9151dk/nrf9151 fw/easnfw-cloud/app --build-dir fw/easnfw-cloud/app/build
    ```
