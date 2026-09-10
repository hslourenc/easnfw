#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

#include <ff.h>
#include <easnfw/mass_storage.h>

#define DISK_NAME "SD"
#define MOUNT_POINT "/" DISK_NAME ":"
#define CAPTURE_DIRECTORY MOUNT_POINT "/easn"
#define WAV_HEADER_SIZE 44U

static FATFS fat_fs;
static struct fs_mount_t mount = {
	.type = FS_FATFS, .fs_data = &fat_fs, .mnt_point = MOUNT_POINT,
};
static struct fs_file_t capture_file;
static char temporary_path[64];
static char committed_path[64];
static uint32_t audio_data_bytes;
static uint32_t capture_sample_rate;
static uint16_t capture_channels;
static uint16_t capture_bits_per_sample;
static bool capture_open;

static void le16(uint8_t *out, uint16_t value)
{
	out[0] = value;
	out[1] = value >> 8;
}

static void le32(uint8_t *out, uint32_t value)
{
	out[0] = value;
	out[1] = value >> 8;
	out[2] = value >> 16;
	out[3] = value >> 24;
}

static void wav_header(uint8_t header[WAV_HEADER_SIZE])
{
	uint16_t alignment = capture_channels * (capture_bits_per_sample / 8U);

	memset(header, 0, WAV_HEADER_SIZE);
	memcpy(header, "RIFF", 4);
	le32(&header[4], 36U + audio_data_bytes);
	memcpy(&header[8], "WAVEfmt ", 8);
	le32(&header[16], 16U);
	le16(&header[20], 1U);
	le16(&header[22], capture_channels);
	le32(&header[24], capture_sample_rate);
	le32(&header[28], capture_sample_rate * alignment);
	le16(&header[32], alignment);
	le16(&header[34], capture_bits_per_sample);
	memcpy(&header[36], "data", 4);
	le32(&header[40], audio_data_bytes);
}

static int write_all(const void *data, size_t size)
{
	const uint8_t *cursor = data;

	while (size > 0U) {
		ssize_t written = fs_write(&capture_file, cursor, size);
		if (written <= 0) {
			return written == 0 ? -EIO : (int)written;
		}
		cursor += written;
		size -= written;
	}
	return 0;
}

static int select_paths(void)
{
	struct fs_dirent entry;

	for (uint32_t index = 0U; index <= 9999U; ++index) {
		snprintk(committed_path, sizeof(committed_path),
			CAPTURE_DIRECTORY "/capture_%04u.wav", index);
		snprintk(temporary_path, sizeof(temporary_path),
			CAPTURE_DIRECTORY "/capture_%04u.tmp", index);
		if (fs_stat(committed_path, &entry) == -ENOENT &&
		    fs_stat(temporary_path, &entry) == -ENOENT) {
			return 0;
		}
	}
	return -ENOSPC;
}

int mass_storage_init(void)
{
	int ret = disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_INIT, NULL);
	if (ret != 0) {
		return ret < 0 ? ret : -EIO;
	}
	ret = fs_mount(&mount);
	if (ret < 0) {
		return ret;
	}
	ret = fs_mkdir(CAPTURE_DIRECTORY);
	return ret < 0 && ret != -EEXIST ? ret : 0;
}

int mass_storage_begin_capture(uint32_t rate, uint16_t channels, uint16_t bits)
{
	uint8_t header[WAV_HEADER_SIZE];
	int ret;

	if (capture_open || select_paths() < 0) {
		return -EBUSY;
	}
	capture_sample_rate = rate;
	capture_channels = channels;
	capture_bits_per_sample = bits;
	audio_data_bytes = 0U;
	wav_header(header);
	fs_file_t_init(&capture_file);
	ret = fs_open(&capture_file, temporary_path,
		FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret < 0) {
		return ret;
	}
	capture_open = true;
	ret = write_all(header, sizeof(header));
	if (ret < 0) {
		mass_storage_abort_capture();
	}
	return ret;
}

int mass_storage_append_audio(const void *data, size_t size)
{
	int ret;

	if (!capture_open) {
		return -EBADF;
	}
	if (UINT32_MAX - audio_data_bytes < size) {
		return -EFBIG;
	}
	ret = write_all(data, size);
	if (ret == 0) {
		audio_data_bytes += size;
	}
	return ret;
}

int mass_storage_commit_capture(void)
{
	struct fs_dirent entry;
	uint8_t header[WAV_HEADER_SIZE];
	int ret;

	if (!capture_open) {
		return -EBADF;
	}
	wav_header(header);
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
	if (ret < 0 || fs_stat(temporary_path, &entry) < 0 ||
	    entry.size != WAV_HEADER_SIZE + audio_data_bytes) {
		return ret < 0 ? ret : -EIO;
	}
	return fs_rename(temporary_path, committed_path);
}

int mass_storage_abort_capture(void)
{
	if (capture_open) {
		(void)fs_sync(&capture_file);
		(void)fs_close(&capture_file);
		capture_open = false;
	}
	return 0;
}

const char *mass_storage_capture_path(void)
{
	return committed_path;
}