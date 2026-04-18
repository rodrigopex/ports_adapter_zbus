/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "zlet_tampering.h"

/* ----- Test instance -------------------------------------------------- */

static struct tampering_data tampering_test_data;
static const struct tampering_config tampering_test_cfg = {
	.light_tamper_threshold = 100,
	.proximity_tamper_threshold = 50,
};

ZEPHLET_DEFINE(tampering, tampering_test, &tampering_test_cfg, &tampering_test_data,
	       tampering_init_fn);

/* ----- Event observer ------------------------------------------------- */

static atomic_t event_count;
static struct tampering_events last_event;
static K_SEM_DEFINE(event_sem, 0, 10);

static void on_tampering_event(const struct tampering_events *ev)
{
	last_event = *ev;
	atomic_inc(&event_count);
	k_sem_give(&event_sem);
}
ZEPHLET_EVENTS_LISTENER(tampering_test, tampering, on_tampering_event);

/* ----- Fixture -------------------------------------------------------- */

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	(void)tampering_stop(&tampering_test, NULL, K_MSEC(100));
	atomic_set(&event_count, 0);
	k_sem_reset(&event_sem);
}

ZTEST_SUITE(tampering_v03, NULL, NULL, reset, NULL, NULL);

/* ----- Tests ---------------------------------------------------------- */

ZTEST(tampering_v03, test_is_ready)
{
	zassert_true(tampering_is_ready(&tampering_test));
}

ZTEST(tampering_v03, test_start_stop)
{
	struct lifecycle_status st = {0};

	zassert_ok(tampering_start(&tampering_test, &st, K_MSEC(100)));
	zassert_true(st.is_running);

	zassert_ok(tampering_stop(&tampering_test, &st, K_MSEC(100)));
	zassert_false(st.is_running);
}

ZTEST(tampering_v03, test_get_status)
{
	zassert_ok(tampering_start(&tampering_test, NULL, K_MSEC(100)));
	struct lifecycle_status st = {0};
	zassert_ok(tampering_get_status(&tampering_test, &st, K_MSEC(100)));
	zassert_true(st.is_running);
}

ZTEST(tampering_v03, test_config_roundtrip)
{
	struct tampering_config req = {.light_tamper_threshold = 200,
				       .proximity_tamper_threshold = 75};
	struct tampering_config resp = {0};

	zassert_ok(tampering_config(&tampering_test, &req, &resp, K_MSEC(100)));
	zassert_equal(resp.light_tamper_threshold, 200);
	zassert_equal(resp.proximity_tamper_threshold, 75);
}

ZTEST(tampering_v03, test_force_tampering_emits_event)
{
	zassert_ok(tampering_force_tampering(&tampering_test, K_MSEC(100)));
	zassert_ok(k_sem_take(&event_sem, K_MSEC(100)), "force_tampering must emit an event");
	zassert_true(last_event.proximity_tamper_detected);
}
