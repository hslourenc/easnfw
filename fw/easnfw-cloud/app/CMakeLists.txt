cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(EASN_FW)

target_include_directories(app PRIVATE
	include
	${CMAKE_CURRENT_LIST_DIR}/../../shared/include
)

target_sources(app PRIVATE
	src/main.c
	src/application/sensor_link.c
	src/application/payload_assembler.c
	src/application/cloud_tx.c
	src/domain/payload_format.c
	src/domain/selftest.c
	src/platform/uart_transport.c
	src/platform/http_client.c
	src/platform/lte_m.c
	${CMAKE_CURRENT_LIST_DIR}/../../shared/src/shared.c
)
