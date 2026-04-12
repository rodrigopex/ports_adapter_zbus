/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zlet_tick_interface.h"
#include "zlet_ui_interface.h"
#include <zephyr/kernel.h>

int main(void)
{
	printk("Example project running on a %s board.\n", CONFIG_BOARD_TARGET);

	struct tick_report report = zlet_tick_config_set(1000, K_MSEC(500));
	if (ZEPHLET_CALL_OK(report)) {
		printk("Config set ok, delay_ms=%d\n", report.config.delay_ms);
	}

	report = zlet_tick_start(K_MSEC(500));
	if (ZEPHLET_CALL_OK(report)) {
		printk("Tick is %srunning\n",
		       report.status.is_running ? "" : "not ");
	}

	k_sleep(K_SECONDS(10));

	struct ui_report ui_report = zlet_ui_config_set(1000, K_MSEC(500));
	if (ZEPHLET_CALL_OK(ui_report)) {
		printk("UI config set ok\n");
	}

	zlet_tick_stop(K_MSEC(500));

	return 0;
}
