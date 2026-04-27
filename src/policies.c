/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * App-local policies, composed from the framework's
 * ZEPHLET_EVENTS_LISTENER primitive. Both policies forward to the UI
 * instance's blink RPC.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "zephlet.h"
#include "zlet_tick.h"
#include "zlet_ui.h"
#include "zlet_tampering.h"

LOG_MODULE_REGISTER(policies, LOG_LEVEL_DBG);

/* Instances are defined (via ZEPHLET_NEW) in main.c. */
extern const struct zephlet ui_fake_impl;

#if defined(CONFIG_ZEPHLET_TICK) && defined(CONFIG_ZEPHLET_UI)
static void on_tick_event(const struct zephlet *z, const struct tick_events *ev)
{
	LOG_DBG("from %s: tick event @%d -> ui_blink", z->name, ev->timestamp);
	(void)ui_blink(&ui_fake_impl, K_MSEC(100));
}
ZEPHLET_EVENTS_LISTENER(tick_timer_based_impl, tick, on_tick_event);
#endif

#if defined(CONFIG_ZEPHLET_TAMPERING) && defined(CONFIG_ZEPHLET_UI)
static void on_tampering_event(const struct zephlet *z, const struct tampering_events *ev)
{
	if (ev->proximity_tamper_detected) {
		LOG_DBG("tampering proximity @%d -> ui_blink", ev->timestamp);
		(void)ui_blink(&ui_fake_impl, K_MSEC(100));
	}
}
ZEPHLET_EVENTS_LISTENER(tampering_emul_impl, tampering, on_tampering_event);
#endif
