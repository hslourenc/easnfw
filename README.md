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

- `doc/` — project documentation
  - `doc/requirements/` — requirements document (LaTeX source and build)
- `fw/` — firmware source code

## Building the documentation

### Prerequisites

- A TeX distribution with **LuaLaTeX** (e.g. TeX Live) and the LaTeX packages
  used by the documents (`geometry`, `array`, `hyperref`, `graphicx`,
  `indentfirst`).
- **Java** and a **PlantUML jar** are only required to regenerate the diagrams
  from the `.puml` sources. If you only want to build the PDF from the existing
  images, they are not needed.

### Steps

Build the requirements document (PDF):

```sh
make -C doc/requirements
```

The output PDF is written to `doc/requirements/out/requirements.pdf`.

To regenerate the PlantUML diagrams before building, set the `PLANTUML_JAR`
environment variable to the path of the PlantUML jar and run:

```sh
make -C doc/requirements plantuml
```

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
    west build --pristine -b nrf9151dk/nrf9151 fw/easnfw-cloud/app --build-dir fw/easnfw-sensor/app/build
    ```

