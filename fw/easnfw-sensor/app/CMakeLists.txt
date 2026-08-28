cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(EASN_FW)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
	src/main.c
	src/application/sampling.c
	src/application/audio_processing.c
	src/application/storage.c
	src/application/cloud_link.c
	src/domain/audio_sampling.c
	src/domain/env_sampling.c
	src/domain/audio_algo.c
	src/domain/selftest.c
	src/platform/audio_sensor_drv.c
	src/platform/env_sensor_drv.c
	src/platform/mass_storage.c
	src/platform/nvs.c
	src/platform/spi_transport.c
	src/shared/spi_protocol.c
	src/shared/ecoacoustic_record.c
	src/shared/retry.c
)
