cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(easn_mic_testbed)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
	src/main.c
	src/application/capture.c
	src/domain/audio_payload.c
	src/platform/i2s_mic.c
	src/platform/sd_storage.c
)
