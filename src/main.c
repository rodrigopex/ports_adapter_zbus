/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zlet_tick_interface.h"
#include <zephyr/kernel.h>

int main(void)
{
	printk("Example project running on a %s board.\n", CONFIG_BOARD_TARGET);

	// zlet_tamper_detection_start(0, K_FOREVER);
	zlet_tick_config_set(0, 1000, K_FOREVER);
	zlet_tick_start(0, K_FOREVER);

	k_sleep(K_SECONDS(10));

	zlet_tick_stop(0, K_FOREVER);
	// zlet_tamper_detection_stop(K_FOREVER);

	return 0;
}
