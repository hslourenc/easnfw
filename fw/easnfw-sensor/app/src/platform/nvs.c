#include <errno.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>
#include <easnfw/platform.h>

static struct nvs_fs nvs_fs;
static bool nvs_initialized;

int app_nvs__init(void)
{
	struct flash_pages_info page_info;
	int err;

	if (nvs_initialized) {
		return 0;
	}

	nvs_fs.flash_device = PARTITION_DEVICE(storage_partition);
	if (!device_is_ready(nvs_fs.flash_device)) {
		return -ENODEV;
	}

	nvs_fs.offset = PARTITION_OFFSET(storage_partition);
	err = flash_get_page_info_by_offs(nvs_fs.flash_device,
					  nvs_fs.offset, &page_info);
	if (err < 0) {
		return err;
	}

	nvs_fs.sector_size = page_info.size;
	nvs_fs.sector_count = PARTITION_SIZE(storage_partition) /
			      nvs_fs.sector_size;
	if (nvs_fs.sector_count < 2U) {
		return -EINVAL;
	}

	err = nvs_mount(&nvs_fs);
	if (err < 0) {
		return err;
	}

	nvs_initialized = true;
	return 0;
}

int app_nvs__write(uint16_t id, void const *data, size_t len)
{
	int err;

	if (data == NULL || len == 0U) {
		return -EINVAL;
	}

	err = app_nvs__init();
	if (err < 0) {
		return err;
	}

	err = nvs_write(&nvs_fs, id, data, len);
	return err < 0 ? err : 0;
}

int app_nvs__read(uint16_t id, void *data, size_t len)
{
	int err;

	if (data == NULL || len == 0U) {
		return -EINVAL;
	}

	err = app_nvs__init();
	if (err < 0) {
		return err;
	}

	err = nvs_read(&nvs_fs, id, data, len);
	if (err < 0) {
		return err;
	}

	return err == (int)len ? 0 : -EIO;
}