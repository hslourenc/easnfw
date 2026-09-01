#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/printk.h>

#include <ff.h>

#include <mic_testbed/sd_storage.h>

LOG_MODULE_REGISTER(mic_testbed_storage, LOG_LEVEL_INF);

#define DISK_NAME "SD"
#define MOUNT_POINT "/" DISK_NAME ":"
#define CAPTURE_DIRECTORY MOUNT_POINT "/easn"
#define WAV_HEADER_SIZE 44U
#define MAX_CAPTURE_INDEX 9999U

static FATFS fat_fs;
static struct fs_mount_t mount = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};
static struct fs_file_t capture_file;
static char temporary_path[64];
static char committed_path[64];
static uint32_t audio_data_bytes;
static uint32_t capture_sample_rate;
static uint16_t capture_channels;
static uint16_t capture_bits_per_sample;
static bool capture_open;

static void write_le16(uint8_t *destination, uint16_t value)
{
	destination[0] = (uint8_t)value;
	destination[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *destination, uint32_t value)
{
	destination[0] = (uint8_t)value;
	destination[1] = (uint8_t)(value >> 8);
	destination[2] = (uint8_t)(value >> 16);
	destination[3] = (uint8_t)(value >> 24);
}

static void build_wav_header(uint8_t header[WAV_HEADER_SIZE])
{
	const uint16_t block_align = capture_channels *
		(capture_bits_per_sample / 8U);
	const uint32_t byte_rate = capture_sample_rate * block_align;

	memset(header, 0, WAV_HEADER_SIZE);
	memcpy(&header[0], "RIFF", 4U);
	write_le32(&header[4], 36U + audio_data_bytes);
	memcpy(&header[8], "WAVE", 4U);
	memcpy(&header[12], "fmt ", 4U);
	write_le32(&header[16], 16U);
	write_le16(&header[20], 1U);
	write_le16(&header[22], capture_channels);
	write_le32(&header[24], capture_sample_rate);
	write_le32(&header[28], byte_rate);
	write_le16(&header[32], block_align);
	write_le16(&header[34], capture_bits_per_sample);
	memcpy(&header[36], "data", 4U);
	write_le32(&header[40], audio_data_bytes);
}

static int write_all(const void *data, size_t size)
{
	const uint8_t *cursor = data;

	while (size > 0U) {
		const ssize_t written = fs_write(&capture_file, cursor, size);

		if (written < 0) {
			return (int)written;
		}
		if (written == 0) {
			return -EIO;
		}
		cursor += written;
		size -= written;
	}

	return 0;
}

static int select_capture_paths(void)
{
	struct fs_dirent entry;

	for (uint32_t index = 0U; index <= MAX_CAPTURE_INDEX; ++index) {
		snprintk(committed_path, sizeof(committed_path),
			 CAPTURE_DIRECTORY "/capture_%04u.wav", index);
		if (fs_stat(committed_path, &entry) == -ENOENT) {
			snprintk(temporary_path, sizeof(temporary_path),
				 CAPTURE_DIRECTORY "/capture_%04u.tmp", index);
			if (fs_stat(temporary_path, &entry) == -ENOENT) {
				return 0;
			}
		}
	}

	return -ENOSPC;
}

int sd_storage_init(void)
{
	int ret;

	ret = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
	if (ret != 0) {
		LOG_ERR("SD initialization failed: %d", ret);
		return ret < 0 ? ret : -EIO;
	}

	ret = fs_mount(&mount);
	if (ret < 0) {
		LOG_ERR("Cannot mount %s: %d", MOUNT_POINT, ret);
		return ret;
	}

	ret = fs_mkdir(CAPTURE_DIRECTORY);
	if ((ret < 0) && (ret != -EEXIST)) {
		LOG_ERR("Cannot create %s: %d", CAPTURE_DIRECTORY, ret);
		return ret;
	}

	LOG_INF("SD card mounted at %s", MOUNT_POINT);
	return 0;
}

int sd_storage_begin_capture(uint32_t sample_rate, uint16_t channels,
	uint16_t bits_per_sample)
{
	uint8_t header[WAV_HEADER_SIZE];
	int ret;

	if (capture_open) {
		return -EBUSY;
	}

	ret = select_capture_paths();
	if (ret < 0) {
		return ret;
	}

	capture_sample_rate = sample_rate;
	capture_channels = channels;
	capture_bits_per_sample = bits_per_sample;
	audio_data_bytes = 0U;
	build_wav_header(header);

	fs_file_t_init(&capture_file);
	ret = fs_open(&capture_file, temporary_path,
		      FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret < 0) {
		LOG_ERR("Cannot open %s: %d", temporary_path, ret);
		return ret;
	}
	capture_open = true;

	ret = write_all(header, sizeof(header));
	if (ret < 0) {
		(void)sd_storage_abort_capture();
		return ret;
	}

	LOG_INF("Recording temporary payload %s", temporary_path);
	return 0;
}

int sd_storage_append_audio(const void *data, size_t size)
{
	int ret;

	if (!capture_open) {
		return -EBADF;
	}
	if ((UINT32_MAX - audio_data_bytes) < size) {
		return -EFBIG;
	}

	ret = write_all(data, size);
	if (ret == 0) {
		audio_data_bytes += size;
	}
	return ret;
}

int sd_storage_commit_capture(void)
{
	struct fs_dirent entry;
	uint8_t header[WAV_HEADER_SIZE];
	const uint32_t expected_file_size = WAV_HEADER_SIZE + audio_data_bytes;
	int ret;

	if (!capture_open) {
		return -EBADF;
	}

	build_wav_header(header);
	ret = fs_seek(&capture_file, 0, FS_SEEK_SET);
	if (ret == 0) {
		ret = write_all(header, sizeof(header));
	}
	if (ret == 0) {
		ret = fs_sync(&capture_file);
	}
	if (fs_close(&capture_file) < 0 && ret == 0) {
		ret = -EIO;
	}
	capture_open = false;
	if (ret < 0) {
		LOG_ERR("Could not finalize WAV header: %d", ret);
		return ret;
	}

	ret = fs_stat(temporary_path, &entry);
	if (ret < 0) {
		LOG_ERR("Could not verify %s: %d", temporary_path, ret);
		return ret;
	}
	if (entry.size != expected_file_size) {
		LOG_ERR("Temporary file has %u bytes; expected %u",
			(uint32_t)entry.size, expected_file_size);
		return -EMSGSIZE;
	}

	ret = fs_rename(temporary_path, committed_path);
	if (ret < 0) {
		LOG_ERR("Could not commit %s: %d", temporary_path, ret);
		return ret;
	}

	LOG_INF("Committed %s (%u audio bytes)", committed_path,
		audio_data_bytes);
	return 0;
}

int sd_storage_abort_capture(void)
{
	if (capture_open) {
		(void)fs_sync(&capture_file);
		(void)fs_close(&capture_file);
		capture_open = false;
	}

	LOG_WRN("Incomplete capture retained as %s", temporary_path);
	return 0;
}

const char *sd_storage_capture_path(void)
{
	return committed_path;
}
