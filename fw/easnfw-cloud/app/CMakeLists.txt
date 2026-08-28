cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(EASN_FW)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
	src/main.c
	src/application/sensor_link.c
	src/application/payload_assembler.c
	src/application/cloud_tx.c
	src/domain/payload_format.c
	src/domain/selftest.c
	src/platform/spi_transport.c
	src/platform/http_client.c
	src/platform/lte_m.c
	src/shared/spi_protocol.c
	src/shared/ecoacoustic_record.c
	src/shared/retry.c
)
