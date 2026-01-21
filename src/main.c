/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "battery_service.h"
#include "tick_service.h"
#include "tamper_detection_service.h"
#include "zephyr/kernel.h"

static void status_listener_fn(const struct zbus_channel *chan, const void *msg)
{
}
ZBUS_ASYNC_LISTENER_DEFINE(alis_status, status_listener_fn);

int main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD_TARGET);

	tamper_detection_service_start(K_FOREVER);
	tick_service_start(2000, K_FOREVER);

	k_sleep(K_SECONDS(10));

	tick_service_stop(K_FOREVER);
	tamper_detection_service_stop(K_FOREVER);

	return 0;
}
