#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <mic_testbed/capture.h>

LOG_MODULE_REGISTER(mic_testbed_main, LOG_LEVEL_INF);

int main(void)
{
	int ret;

	LOG_INF("EASN DMM-4026 I2S microphone testbed");
	ret = mic_testbed_capture_run();
	if (ret < 0) {
		LOG_ERR("Capture failed: %d", ret);
	} else {
		LOG_INF("Capture completed successfully");
	}

	LOG_INF("Reset the DK to record another track");
	for (;;) {
		k_sleep(K_FOREVER);
	}

	return ret;
}
