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

	struct tick_settings tick_delta = {0};

	struct tick_report tick_report = zlet_tick_get_settings(K_MSEC(500));
	if (ZEPHLET_CALL_OK(tick_report)) {
		tick_delta = tick_report.settings;
		printk("Tick settings: delay_ms=%u\n", tick_report.settings.delay_ms);
	}

	tick_delta.has_delay_ms = true;
	tick_delta.delay_ms = 1000;

	tick_report = zlet_tick_update_settings(&tick_delta, K_MSEC(500));
	if (ZEPHLET_CALL_OK(tick_report)) {
		printk("Tick settings updated, delay_ms=%u\n", tick_report.settings.delay_ms);
	}

	struct ui_settings ui_delta = {
		.has_user_button_long_press_duration = true,
		.user_button_long_press_duration = 1000,
	};

	struct ui_report ui_report = zlet_ui_update_settings(&ui_delta, K_MSEC(500));
	if (ZEPHLET_CALL_OK(ui_report)) {
		printk("UI settings updated\n");
	}

	ui_report = zlet_ui_start(K_MSEC(500));
	if (ZEPHLET_CALL_OK(ui_report)) {
		printk("UI is %srunning\n", ui_report.status.is_running ? "" : "not ");
	}

	tick_report = zlet_tick_start(K_MSEC(500));
	if (ZEPHLET_CALL_OK(tick_report)) {
		printk("Tick is %srunning\n", tick_report.status.is_running ? "" : "not ");
	}

	k_sleep(K_SECONDS(10));

	zlet_tick_stop(K_MSEC(500));
	zlet_ui_stop(K_MSEC(500));

	return 0;
}
