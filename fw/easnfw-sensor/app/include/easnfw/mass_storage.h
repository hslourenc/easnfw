/**
 * @file mass_storage.h
 * @brief SENSOR mass-storage record and filesystem interface.
 *
 * A record is written to a temporary file and is not complete or eligible for
 * transmission until finalization succeeds. Boot recovery validates record
 * files and handles unfinished temporary records.
 */
#ifndef EASNFW_MASS_STORAGE_H
#define EASNFW_MASS_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include <easnfw/time_sync.h>

/** Timestamp source recorded in each ecoacoustic record. */
enum app_mass_storage_timestamp_source {
	/** UTC was synchronized from the cellular network through CLOUD. */
	APP_TIMESTAMP_SOURCE_CELLULAR_NETWORK = 1U,
};

/**
 * @brief Durable metadata stored with a mono PCM16 ecoacoustic record.
 *
 * Environmental values are fixed-point SI units. Audio and environmental
 * timestamps are fixed-size ISO-8601 UTC strings; ``record_id`` is 16 bytes.
 */
struct app_mass_storage_record_metadata {
	uint8_t record_id[16];
	uint32_t sample_rate_hz;
	uint32_t track_duration_seconds;
	int32_t temperature_milli_c;
	uint32_t pressure_pa;
	uint32_t humidity_milli_pct;
	uint32_t gas_resistance_ohms;
	char audio_start_utc[EASNFW_UTC_TIMESTAMP_SIZE];
	char audio_end_utc[EASNFW_UTC_TIMESTAMP_SIZE];
	char environment_sample_utc[EASNFW_UTC_TIMESTAMP_SIZE];
	uint32_t total_sample_count;
	uint32_t processing_algorithm_version;
	uint16_t channels;
	uint16_t bits_per_sample;
	uint16_t schema_version;
	uint8_t timestamp_source;
};

/** Initialize and mount the mass-storage device. */
int app_mass_storage__init(void);

/** Write raw bytes to the mass-storage test file. */
int app_mass_storage__write(const void *data, size_t len);

/** Read raw bytes from the mass-storage test file. */
int app_mass_storage__read(void *data, size_t len);

/** Check whether the mounted filesystem has the requested free space. */
int app_mass_storage__has_space(uint64_t required_bytes);

/** Validate records and promote completed temporary records at boot. */
int app_mass_storage__recover_records(void);

/** Begin writing a record that is not yet eligible for transmission. */
int app_mass_storage__record_begin(
	const struct app_mass_storage_record_metadata *metadata);

/** Append PCM bytes to the active record. */
int app_mass_storage__record_append(const void *data, size_t len);

/** Flush and finalize the active record. */
int app_mass_storage__record_commit(
	const struct app_mass_storage_record_metadata *metadata);

/** Abort the active record while retaining its temporary file. */
int app_mass_storage__record_abort(void);

#endif
