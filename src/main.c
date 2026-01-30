/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #include "battery_service.h"
#include "tick_service.h"
// #include "tamper_detection_service.h"
#include <zephyr/kernel.h>

// static void status_listener_fn(const struct zbus_channel *chan, const void *msg)
// {
// 	if (chan == &chan_tick_service_report) {
// 		const struct msg_tick_service_report *report = msg;
//
// 		if (report->which_tick_report == MSG_TICK_SERVICE_REPORT_STATUS_TAG) {
// 			printk("Tick Service is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// 	if (chan == &chan_tamper_detection_service_report) {
// 		const struct msg_tamper_detection_service_report *report = msg;
//
// 		if (report->which_tamper_detection_report ==
// 		    MSG_TAMPER_DETECTION_SERVICE_REPORT_STATUS_TAG) {
// 			printk("Tamper Detection Service is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// 	if (chan == &chan_battery_service_report) {
// 		const struct msg_battery_service_report *report = msg;
//
// 		if (report->which_battery_report == MSG_BATTERY_SERVICE_REPORT_STATUS_TAG) {
// 			printk("Tamper Detection Service is %s\n",
// 			       report->status.is_running ? "running" : "stopped");
// 		}
// 	}
// }
// ZBUS_ASYNC_LISTENER_DEFINE(alis_status, status_listener_fn);
// ZBUS_CHAN_ADD_OBS(chan_battery_service_report, alis_status, 3);
// ZBUS_CHAN_ADD_OBS(chan_tick_service_report, alis_status, 3);
// ZBUS_CHAN_ADD_OBS(chan_tamper_detection_service_report, alis_status, 3);

int main(void)
{
	printk("Example project running on a %s board.\n", CONFIG_BOARD_TARGET);

	// tamper_detection_service_start(K_FOREVER);
	tick_service_config_set(1000, K_FOREVER);
	tick_service_start(K_FOREVER);

	k_sleep(K_SECONDS(10));

	tick_service_stop(K_FOREVER);
	// tamper_detection_service_stop(K_FOREVER);

	return 0;
}
