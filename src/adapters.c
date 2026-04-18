/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * App-local adapters, composed from the framework's
 * ZEPHLET_EVENTS_LISTENER primitive. Both adapters forward to the UI
 * instance's blink RPC.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "zephlet.h"
#include "zlet_tick.h"
#include "zlet_ui.h"
#include "zlet_tampering.h"

LOG_MODULE_REGISTER(adapters, LOG_LEVEL_DBG);

/* Instances are defined (via ZEPHLET_DEFINE) in main.c. */
extern const struct zephlet ui_instance;

static void on_tick_event(const struct tick_events *ev)
{
	LOG_DBG("tick event @%d -> ui_blink", ev->timestamp);
	(void)ui_blink(&ui_instance, K_MSEC(100));
}

static void on_tampering_event(const struct tampering_events *ev)
{
	if (ev->proximity_tamper_detected) {
		LOG_DBG("tampering proximity @%d -> ui_blink", ev->timestamp);
		(void)ui_blink(&ui_instance, K_MSEC(100));
	}
}

ZEPHLET_EVENTS_LISTENER(tick_instance,      tick,      on_tick_event);
ZEPHLET_EVENTS_LISTENER(tampering_instance, tampering, on_tampering_event);
