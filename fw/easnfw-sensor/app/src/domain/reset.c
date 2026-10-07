
/* Indicate whether EASNFW-CLOUD is ready after a reset. */
static bool easnfwcloud_ready = false;

int reset__easnfwcloud_ready_set(void)
{
	easnfwcloud_ready = true;
}

int reset__easnfwcloud_ready_clear(void)
{
	easnfwcloud_ready = false;
}

int reset__easnfw_reset(bool const force)
{
	/**
	 * EASNFW_TODO: implement according to function header (reset.h). Use
	 * interfw_link__reset_fwcloud_tx() to send the reset command to
	 * EASNFW-CLOUD. interfw_link__reset_fwcloud_tx() clears the
	 * easnfwcloud_ready flag before sending the command to EASNFW-CLOUD.
	 * Upon resetting, EASNFW-CLOUD will send the EASNFWCLOUD_READY, which
	 * will be assynchronously handled by
	 * interfw_link__easnfwcloud_ready_rx(), which in turn will set the
	 * easnfwcloud_ready flag again. This is how this function will know
	 * when EASNFW-CLOUD reset is complete.
	 */
}
