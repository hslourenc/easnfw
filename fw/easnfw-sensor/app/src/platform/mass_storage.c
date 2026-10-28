#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/disk_access.h>

#include <ff.h>

#include <easnfw/mass_storage.h>
#include <easnfw/nvs.h>
#include "../../../../shared/interfw_link_protocol.h"

LOG_MODULE_REGISTER(mass_storage, LOG_LEVEL_INF);

#define DISK_NAME "SD"
#define MOUNT_POINT "/" DISK_NAME ":"
#define TEST_FILE MOUNT_POINT "/easnfw_selftest.bin"
#define RECORD_DIRECTORY MOUNT_POINT "/records"
#define RECORD_PATH_SIZE 80U
#define RECORD_MAGIC 0x4553414EU
#define RECORD_FOOTER_MAGIC 0x454E4421U
#define RECORD_FORMAT_VERSION 1U
#define RECORD_IO_BUFFER_SIZE 512U

static FATFS fat_fs;
static struct fs_mount_t mount = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};
static bool mass_storage_initialized;
static struct fs_file_t record_file;
static char active_record_temporary_path[RECORD_PATH_SIZE];
static char active_record_committed_path[RECORD_PATH_SIZE];
static bool record_open;
static uint32_t active_record_crc;
static uint32_t active_record_audio_bytes;

struct record_file_header {
	uint32_t magic;
	uint16_t version;
	uint16_t header_size;
	struct app_mass_storage_record_metadata metadata;
	uint32_t header_crc;
};

/* EASNFW-CLOUD identifies records from the shared blob offset. */
BUILD_ASSERT(offsetof(struct record_file_header, metadata.record_id) ==
		     EASN_RECORD_ID_OFFSET,
	     "record_id offset must match the IFL record blob layout");

struct record_file_footer {
	uint32_t magic;
	uint32_t audio_bytes;
	uint32_t audio_crc;
	uint32_t footer_crc;
};

/** Compute a CRC over the defined header fields, excluding its CRC member. */
static uint32_t record_header_crc(const struct record_file_header *header)
{
	return crc32_ieee((const uint8_t *)header,
			  offsetof(struct record_file_header, header_crc));
}

/** Compute a CRC over the defined footer fields, excluding its CRC member. */
static uint32_t record_footer_crc(const struct record_file_footer *footer)
{
	return crc32_ieee((const uint8_t *)footer,
			  offsetof(struct record_file_footer, footer_crc));
}

/** Write an entire buffer, treating a zero-length short write as an error. */
static int write_all(struct fs_file_t *file, const void *data, size_t len)
{
	const uint8_t *cursor = data;

	while (len > 0U) {
		ssize_t written = fs_write(file, cursor, len);

		if (written < 0) {
			return (int)written;
		}
		if (written == 0) {
			return -EIO;
		}
		cursor += written;
		len -= (size_t)written;
	}
	return 0;
}

/** Initialize the SD card and mount its FAT filesystem once. */
int app_mass_storage__init(void)
{
	int err;

	if (mass_storage_initialized) {
		return 0;
	}

	err = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
	if (err != 0) {
		LOG_ERR("SD initialization failed: %d", err);
		return err < 0 ? err : -EIO;
	}

	err = fs_mount(&mount);
	if (err < 0) {
		LOG_ERR("Cannot mount %s: %d", MOUNT_POINT, err);
		return err;
	}

	err = fs_mkdir(RECORD_DIRECTORY);
	if (err < 0 && err != -EEXIST) {
		LOG_ERR("Cannot create %s: %d", RECORD_DIRECTORY, err);
		return err;
	}

	mass_storage_initialized = true;
	LOG_INF("SD card mounted at %s", MOUNT_POINT);
	err = app_mass_storage__recover_records();
	if (err != 0) {
		LOG_ERR("Record recovery failed: %d", err);
		mass_storage_initialized = false;
		(void)fs_unmount(&mount);
		return err;
	}
	return 0;
}

/** Write test data to the self-test file, replacing its previous contents. */
int app_mass_storage__write(const void *data, size_t len)
{
	struct fs_file_t file;
	int err;

	if (!mass_storage_initialized || data == NULL || len == 0U) {
		return -EINVAL;
	}

	fs_file_t_init(&file);
	err = fs_open(&file, TEST_FILE, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (err < 0) {
		return err;
	}
	err = write_all(&file, data, len);
	if (err == 0) {
		err = fs_sync(&file);
	}
	if (fs_close(&file) < 0 && err == 0) {
		err = -EIO;
	}
	return err;
}

/** Read exactly len bytes from the self-test file. */
int app_mass_storage__read(void *data, size_t len)
{
	struct fs_file_t file;
	size_t offset = 0U;
	int err;

	if (!mass_storage_initialized || data == NULL || len == 0U) {
		return -EINVAL;
	}

	fs_file_t_init(&file);
	err = fs_open(&file, TEST_FILE, FS_O_READ);
	if (err < 0) {
		return err;
	}
	while (offset < len) {
		ssize_t read = fs_read(&file, (uint8_t *)data + offset,
				       len - offset);

		if (read < 0) {
			err = (int)read;
			break;
		}
		if (read == 0) {
			err = -EIO;
			break;
		}
		offset += (size_t)read;
		err = 0;
	}
	if (fs_close(&file) < 0 && err == 0) {
		err = -EIO;
	}
	return err;
}

/** Check FAT free-cluster capacity before a full track is acquired. */
int app_mass_storage__has_space(uint64_t required_bytes)
{
	struct fs_statvfs stats;
	uint64_t free_bytes;
	int err;

	if (!mass_storage_initialized) {
		return -ENODEV;
	}
	err = fs_statvfs(MOUNT_POINT, &stats);
	if (err != 0) {
		LOG_ERR("Could not query SD free space: %d", err);
		return err;
	}
	free_bytes = (uint64_t)stats.f_frsize * stats.f_bfree;
	return free_bytes >= required_bytes ? 0 : -ENOSPC;
}

/** Construct temporary and committed filenames from a 128-bit identifier. */
static void record_paths_set(const uint8_t record_id[16])
{
	char id[33];

	for (size_t i = 0U; i < 16U; ++i) {
		snprintk(&id[i * 2U], sizeof(id) - i * 2U, "%02x", record_id[i]);
	}
	id[sizeof(id) - 1U] = '\0';
	snprintk(active_record_temporary_path,
		 sizeof(active_record_temporary_path),
		 RECORD_DIRECTORY "/%s.tmp", id);
	snprintk(active_record_committed_path,
		 sizeof(active_record_committed_path),
		 RECORD_DIRECTORY "/%s.rec", id);
}

/** Read a file range exactly, returning -EIO on a truncated read. */
static int record_read_exact(struct fs_file_t *file, void *data, size_t len)
{
	uint8_t *cursor = data;

	while (len > 0U) {
		ssize_t count = fs_read(file, cursor, len);

		if (count < 0) {
			return (int)count;
		}
		if (count == 0) {
			return -EIO;
		}
		cursor += count;
		len -= (size_t)count;
	}
	return 0;
}

/** Validate a complete record file, including its data and metadata CRCs. */
static int record_file_validate(const char *path)
{
	struct fs_file_t file;
	struct fs_dirent entry;
	struct record_file_header header;
	struct record_file_footer footer;
	uint8_t buffer[RECORD_IO_BUFFER_SIZE];
	uint32_t expected_audio_bytes;
	uint32_t remaining;
	uint32_t crc = 0U;
	int err;

	err = fs_stat(path, &entry);
	if (err != 0) {
		return err;
	}
	if (entry.type != FS_DIR_ENTRY_FILE ||
	    entry.size < sizeof(header) + sizeof(footer)) {
		return -EBADMSG;
	}

	fs_file_t_init(&file);
	err = fs_open(&file, path, FS_O_READ);
	if (err != 0) {
		return err;
	}
	err = record_read_exact(&file, &header, sizeof(header));
	if (err == 0 &&
	    (header.magic != RECORD_MAGIC ||
	     header.version != RECORD_FORMAT_VERSION ||
	     header.header_size != sizeof(header) ||
	     header.metadata.total_sample_count == 0U ||
	     header.metadata.audio_end_utc[0] == '\0' ||
	     header.header_crc != record_header_crc(&header))) {
		err = -EBADMSG;
	}
	if (err == 0 &&
	    header.metadata.total_sample_count > UINT32_MAX / sizeof(int16_t)) {
		err = -EOVERFLOW;
	}
	expected_audio_bytes = 0U;
	if (err == 0) {
		expected_audio_bytes =
			header.metadata.total_sample_count * sizeof(int16_t);
	}
	if (err == 0 &&
	    entry.size != sizeof(header) + expected_audio_bytes + sizeof(footer)) {
		err = -EBADMSG;
	}
	remaining = expected_audio_bytes;
	while (err == 0 && remaining > 0U) {
		size_t count = MIN(remaining, sizeof(buffer));

		err = record_read_exact(&file, buffer, count);
		if (err == 0) {
			crc = crc32_ieee_update(crc, buffer, count);
			remaining -= count;
		}
	}
	if (err == 0) {
		err = record_read_exact(&file, &footer, sizeof(footer));
	}
	if (err == 0 &&
	    (footer.magic != RECORD_FOOTER_MAGIC ||
	     footer.audio_bytes != expected_audio_bytes ||
	     footer.audio_crc != crc ||
	     footer.footer_crc != record_footer_crc(&footer))) {
		err = -EBADMSG;
	}
	if (fs_close(&file) != 0 && err == 0) {
		err = -EIO;
	}
	return err;
}

/** Rename an invalid/incomplete record to a non-transmittable .bad file. */
static int record_quarantine(const char *path, const char *name)
{
	char bad_path[RECORD_PATH_SIZE];
	size_t name_len = strlen(name);

	if (name_len < 4U || name_len - 4U + sizeof(".bad") >
				 sizeof(bad_path) - sizeof(RECORD_DIRECTORY)) {
		return -ENAMETOOLONG;
	}
	snprintk(bad_path, sizeof(bad_path), RECORD_DIRECTORY "/%.*s.bad",
		 (int)(name_len - 4U), name);
	return fs_rename(path, bad_path);
}

/**
 * Validate records on boot, promote complete temporaries, and quarantine
 * incomplete or corrupt files rather than treating them as deliverable.
 */
int app_mass_storage__recover_records(void)
{
	struct fs_dir_t directory;
	struct fs_dirent entry;
	char path[RECORD_PATH_SIZE];
	int err;

	if (!mass_storage_initialized) {
		return -ENODEV;
	}

	fs_dir_t_init(&directory);
	err = fs_opendir(&directory, RECORD_DIRECTORY);
	if (err != 0) {
		return err;
	}
	for (;;) {
		err = fs_readdir(&directory, &entry);
		if (err != 0 || entry.name[0] == '\0') {
			break;
		}
		if (entry.type != FS_DIR_ENTRY_FILE) {
			continue;
		}
		size_t const name_len = strlen(entry.name);

		if (name_len <= 4U ||
		    (strcmp(&entry.name[name_len - 4U], ".tmp") != 0 &&
		     strcmp(&entry.name[name_len - 4U], ".rec") != 0)) {
			continue;
		}
		snprintk(path, sizeof(path), RECORD_DIRECTORY "/%s", entry.name);
		int const validation_err = record_file_validate(path);

		if (validation_err == 0) {
			if (strcmp(&entry.name[name_len - 4U], ".tmp") == 0) {
				char committed_path[RECORD_PATH_SIZE];

				snprintk(committed_path, sizeof(committed_path),
					 RECORD_DIRECTORY "/%.*s.rec",
					 (int)(name_len - 4U), entry.name);
				err = fs_rename(path, committed_path);
				if (err != 0) {
					break;
				}
				LOG_WRN("Recovered complete record %s",
					committed_path);
			}
			continue;
		}
		err = record_quarantine(path, entry.name);
		if (err != 0) {
			LOG_ERR("Could not quarantine invalid record %s (%d)",
				path, err);
			break;
		}
		LOG_WRN("Quarantined incomplete or corrupt record %s (%d)",
			entry.name, validation_err);
	}
	if (fs_closedir(&directory) != 0 && err == 0) {
		err = -EIO;
	}
	return err;
}

/** Begin a record in a temporary file that is not eligible for transmission. */
int app_mass_storage__record_begin(
	const struct app_mass_storage_record_metadata *metadata)
{
	struct record_file_header header;
	struct fs_dirent entry;
	int err;

	if (!mass_storage_initialized || metadata == NULL || record_open) {
		return -EINVAL;
	}
	record_paths_set(metadata->record_id);
	err = fs_stat(active_record_temporary_path, &entry);
	if (err == 0) {
		return -EEXIST;
	}
	if (err != -ENOENT) {
		return err;
	}
	err = fs_stat(active_record_committed_path, &entry);
	if (err == 0) {
		return -EEXIST;
	}
	if (err != -ENOENT) {
		return err;
	}

	fs_file_t_init(&record_file);
	err = fs_open(&record_file, active_record_temporary_path,
		      FS_O_CREATE | FS_O_WRITE);
	if (err < 0) {
		LOG_ERR("Cannot create temporary record %s: %d",
			active_record_temporary_path, err);
		return err;
	}
	record_open = true;
	memset(&header, 0, sizeof(header));
	header.magic = RECORD_MAGIC;
	header.version = RECORD_FORMAT_VERSION;
	header.header_size = sizeof(header);
	header.metadata = *metadata;
	header.header_crc = record_header_crc(&header);
	active_record_crc = 0U;
	active_record_audio_bytes = 0U;
	err = write_all(&record_file, &header, sizeof(header));
	if (err == 0) {
		err = fs_sync(&record_file);
	}
	if (err != 0) {
		(void)app_mass_storage__record_abort();
		return err;
	}
	LOG_INF("Started record %s", active_record_temporary_path);
	return 0;
}

/** Append processed audio bytes to the active temporary record. */
int app_mass_storage__record_append(const void *data, size_t len)
{
	if (!record_open || data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (len > UINT32_MAX - active_record_audio_bytes) {
		return -EFBIG;
	}
	int const err = write_all(&record_file, data, len);

	if (err == 0) {
		active_record_crc = crc32_ieee_update(active_record_crc, data, len);
		active_record_audio_bytes += len;
		int const sync_err = fs_sync(&record_file);

		if (sync_err != 0) {
			return sync_err;
		}
	}
	return err;
}

/** Append integrity footer and rename the complete record to its committed name. */
int app_mass_storage__record_commit(
	const struct app_mass_storage_record_metadata *metadata)
{
	struct record_file_header header;
	struct record_file_footer footer;
	int err;

	if (!record_open || metadata == NULL) {
		return -EBADF;
	}
	memset(&header, 0, sizeof(header));
	memset(&footer, 0, sizeof(footer));
	header.magic = RECORD_MAGIC;
	header.version = RECORD_FORMAT_VERSION;
	header.header_size = sizeof(header);
	header.metadata = *metadata;
	header.header_crc = record_header_crc(&header);
	if (metadata->total_sample_count > UINT32_MAX / sizeof(int16_t) ||
	    active_record_audio_bytes !=
		metadata->total_sample_count * sizeof(int16_t)) {
		(void)app_mass_storage__record_abort();
		return -EMSGSIZE;
	}
	err = fs_seek(&record_file, 0, FS_SEEK_SET);
	if (err == 0) {
		err = write_all(&record_file, &header, sizeof(header));
	}
	if (err == 0) {
		err = fs_seek(&record_file, 0, FS_SEEK_END);
	}
	footer.magic = RECORD_FOOTER_MAGIC;
	footer.audio_bytes = active_record_audio_bytes;
	footer.audio_crc = active_record_crc;
	footer.footer_crc = record_footer_crc(&footer);
	if (err == 0) {
		err = write_all(&record_file, &footer, sizeof(footer));
	}
	if (err != 0) {
		(void)app_mass_storage__record_abort();
		return err;
	}
	err = fs_sync(&record_file);
	if (fs_close(&record_file) < 0 && err == 0) {
		err = -EIO;
	}
	record_open = false;
	if (err != 0) {
		return err;
	}

	/* FAT rename is a best-effort marker; boot recovery validates both CRCs. */
	err = fs_rename(active_record_temporary_path,
			active_record_committed_path);
	if (err != 0) {
		LOG_ERR("Could not commit record %s: %d",
			active_record_temporary_path, err);
		return err;
	}
	LOG_INF("Committed record %s", active_record_committed_path);
	return 0;
}

/** Close an interrupted write but retain its temporary file for recovery. */
int app_mass_storage__record_abort(void)
{
	int err = 0;

	if (record_open) {
		err = fs_sync(&record_file);
		if (fs_close(&record_file) < 0 && err == 0) {
			err = -EIO;
		}
		record_open = false;
	}
	if (active_record_temporary_path[0] != '\0') {
		LOG_WRN("Incomplete record retained at %s",
			active_record_temporary_path);
	}
	return err;
}

static struct fs_file_t reader_file;
static bool reader_open;

/** Open a committed record for transmission and report its size in bytes. */
int app_mass_storage__record_read_open(const uint8_t record_id[16],
				       uint32_t *size)
{
	char path[RECORD_PATH_SIZE];
	struct fs_dirent entry;
	int err;

	if (!mass_storage_initialized || record_id == NULL || size == NULL ||
	    reader_open) {
		return -EINVAL;
	}
	snprintk(path, sizeof(path), RECORD_DIRECTORY "/");
	for (size_t i = 0U; i < 16U; ++i) {
		size_t const used = strlen(path);

		snprintk(&path[used], sizeof(path) - used, "%02x", record_id[i]);
	}
	strncat(path, ".rec", sizeof(path) - strlen(path) - 1U);
	err = fs_stat(path, &entry);
	if (err != 0) {
		return err;
	}
	if (entry.type != FS_DIR_ENTRY_FILE || entry.size > UINT32_MAX) {
		return -EBADF;
	}
	fs_file_t_init(&reader_file);
	err = fs_open(&reader_file, path, FS_O_READ);
	if (err != 0) {
		return err;
	}
	reader_open = true;
	*size = (uint32_t)entry.size;
	return 0;
}

/** Read exactly @p len bytes at @p offset from the open committed record. */
int app_mass_storage__record_read(uint32_t offset, void *data, size_t len)
{
	int err;

	if (!reader_open) {
		return -EINVAL;
	}
	err = fs_seek(&reader_file, offset, FS_SEEK_SET);
	if (err != 0) {
		return err;
	}
	return record_read_exact(&reader_file, data, len);
}

/** Close the committed record opened for transmission. */
void app_mass_storage__record_read_close(void)
{
	if (reader_open) {
		(void)fs_close(&reader_file);
		reader_open = false;
	}
}
