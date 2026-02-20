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

	struct msg_zlet_tick_report *report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick)
	{
		zlet_tick_config_set(100, 1000, K_FOREVER);
		report = zlet_tick_wait_report(MSG_ZLET_TICK_REPORT_CONFIG_TAG, K_MSEC(500));
	}

	if (report != NULL && report->has_context) {
		printk("Context: id=%d, return code=%d\n", report->context.correlation_id,
		       report->context.return_code);
	}

	report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick)
	{
		zlet_tick_start(200, K_FOREVER);
		report = zlet_tick_wait_report(MSG_ZLET_TICK_REPORT_STATUS_TAG, K_MSEC(500));
	}

	if (report != NULL && report->has_context) {
		printk("Context: id=%d, return code=%d\n", report->context.correlation_id,
		       report->context.return_code);
		printk("Main knows the zephlet tick is %srunning\n",
		       report->status.is_running ? "" : "not ");
	}

	k_sleep(K_SECONDS(10));

	ZEPHLET_OBSERVE_REPORT(zlet_ui)
	{
		zlet_ui_config_set(100, 1000, K_FOREVER);
		struct msg_zlet_ui_report *ui_report =
			zlet_ui_wait_report(MSG_ZLET_UI_REPORT_CONFIG_TAG, K_MSEC(500));
		if (ui_report != NULL && ui_report->has_context) {
			printk("Context: id=%d, return code=%d\n",
			       ui_report->context.correlation_id, ui_report->context.return_code);
		}
	}

	zlet_tick_stop(0, K_FOREVER);

	return 0;
}
