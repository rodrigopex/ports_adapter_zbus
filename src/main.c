/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #include "zlet_battery_interface.h"
#include "zlet_tick_interface.h"
// #include "zlet_tamper_detection_interface.h"
#include <zephyr/kernel.h>

// static void status_listener_fn(const struct zbus_channel *chan, const void *msg)
// {
// 	if (chan == &chan_zlet_tick_report) {
// 		const struct msg_zlet_tick_report *report = msg;
//
// 		if (report->which_tick_report == MSG_ZLET_TICK_REPORT_STATUS_TAG) {
// 			printk("Tick Zephlet is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// 	if (chan == &chan_zlet_tamper_detection_report) {
// 		const struct msg_zlet_tamper_detection_report *report = msg;
//
// 		if (report->which_tamper_detection_report ==
// 		    MSG_ZLET_TAMPER_DETECTION_REPORT_STATUS_TAG) {
// 			printk("Tamper Detection Zephlet is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// 	if (chan == &chan_zlet_battery_report) {
// 		const struct msg_zlet_battery_report *report = msg;
//
// 		if (report->which_battery_report == MSG_ZLET_BATTERY_REPORT_STATUS_TAG) {
// 			printk("Tamper Detection Zephlet is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// }
// ZBUS_ASYNC_LISTENER_DEFINE(alis_status, status_listener_fn);
// ZBUS_CHAN_ADD_OBS(chan_zlet_battery_report, alis_status, 3);
// ZBUS_CHAN_ADD_OBS(chan_zlet_tick_report, alis_status, 3);
// ZBUS_CHAN_ADD_OBS(chan_zlet_tamper_detection_report, alis_status, 3);

int main(void)
{
	printk("Example project running on a %s board.\n", CONFIG_BOARD_TARGET);

	// zlet_tamper_detection_start(K_FOREVER);
	zlet_tick_config_set(1000, K_FOREVER);
	zlet_tick_start(K_FOREVER);

	k_sleep(K_SECONDS(10));

	zlet_tick_stop(K_FOREVER);
	// zlet_tamper_detection_stop(K_FOREVER);

	return 0;
}
