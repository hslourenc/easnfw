#include <errno.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

#include <ff.h>

#include <easnfw/platform.h>

LOG_MODULE_REGISTER(mass_storage, LOG_LEVEL_INF);

#define DISK_NAME "SD"
#define MOUNT_POINT "/" DISK_NAME ":"
#define TEST_FILE MOUNT_POINT "/easnfw_selftest.bin"

static FATFS fat_fs;
static struct fs_mount_t mount = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};
static bool mass_storage_initialized;

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

	mass_storage_initialized = true;
	LOG_INF("SD card mounted at %s", MOUNT_POINT);
	return 0;
}

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
	}

	if (fs_close(&file) < 0 && err == 0) {
		err = -EIO;
	}

	return err;
}
