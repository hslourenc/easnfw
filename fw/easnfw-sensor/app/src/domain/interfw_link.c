
#include <easnfw/uart.h>

/** Indicate whether Inter-FW Link is initialized */
static bool ifl_init = false;

bool ifl__is_init(void)
{
	return ifl_init;
}

int ifl__init(void)
{
	int err = uart__ifl_init();

	if (err != 0) {
		return err;
	}

	/** Initialize other Inter-FW Link things here if needed */
	ifl_init = true;
	return 0;
}

int ifl__reset_easnfwcloud_tx(void)
{
	/**
	 * EASNFW_TODO: implement according to function header in
	 * interfw_link.h. Before sending the command, clear the
	 * easnfwcloud_ready flag with reset__easnfwcloud_ready_clear().
	 * 
	 * See also:
	 * - interfw_link_protocol.h
	 */
}

int ifl__easnfwcloud_ready_rx(void)
{
	/**
	 * EASNFW_TODO: implement according to function header in
	 * interfw_link.h. Clear the easnfwcloud_ready flag with
	 * reset__easnfwcloud_ready_clear().
	 *
	 * See also:
	 * - interfw_link_protocol.h
	 */
}
