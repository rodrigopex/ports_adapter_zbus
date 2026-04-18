/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "zlet_tick.h"

/* ----- Two tick instances with distinct compile-time configs. -------- */

static struct tick_data tick_fast_data;
static struct tick_data tick_slow_data;

static const struct tick_config tick_fast_cfg = {
	.period_ms = 100,
	.max_period_ms = 60000,
};
static const struct tick_config tick_slow_cfg = {
	.period_ms = 500,
	.max_period_ms = 60000,
};

ZEPHLET_DEFINE(tick, tick_fast, &tick_fast_cfg, &tick_fast_data, tick_init_fn);
ZEPHLET_DEFINE(tick, tick_slow, &tick_slow_cfg, &tick_slow_data, tick_init_fn);

/* ----- Per-instance event observers --------------------------------- */

static atomic_t fast_event_count;
static atomic_t slow_event_count;
static K_SEM_DEFINE(fast_event_sem, 0, 100);
static K_SEM_DEFINE(slow_event_sem, 0, 100);

static void on_fast_tick(const struct tick_events *ev)
{
	ARG_UNUSED(ev);
	atomic_inc(&fast_event_count);
	k_sem_give(&fast_event_sem);
}

static void on_slow_tick(const struct tick_events *ev)
{
	ARG_UNUSED(ev);
	atomic_inc(&slow_event_count);
	k_sem_give(&slow_event_sem);
}

/* Exercises ZEPHLET_EVENTS_LISTENER: one listener per instance's
 * events channel, user callback receives the typed events struct. */
ZEPHLET_EVENTS_LISTENER(tick_fast, tick, on_fast_tick);
ZEPHLET_EVENTS_LISTENER(tick_slow, tick, on_slow_tick);

static void reset_event_counters(void)
{
	atomic_set(&fast_event_count, 0);
	atomic_set(&slow_event_count, 0);
	k_sem_reset(&fast_event_sem);
	k_sem_reset(&slow_event_sem);
}

/* Reset between tests: stop both instances, restore defaults, drain
 * event counters. */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	(void)tick_stop(&tick_fast, NULL, K_MSEC(100));
	(void)tick_stop(&tick_slow, NULL, K_MSEC(100));

	struct tick_config fast_restore = {.period_ms = 100, .max_period_ms = 60000};
	struct tick_config slow_restore = {.period_ms = 500, .max_period_ms = 60000};
	(void)tick_config(&tick_fast, &fast_restore, NULL, K_MSEC(100));
	(void)tick_config(&tick_slow, &slow_restore, NULL, K_MSEC(100));

	reset_event_counters();
}

ZTEST_SUITE(tick_v03, NULL, NULL, reset, NULL, NULL);

/* ----- Single-instance correctness ---------------------------------- */

ZTEST(tick_v03, test_is_ready)
{
	zassert_true(tick_is_ready(&tick_fast), "tick_fast should be ready");
	zassert_true(tick_is_ready(&tick_slow), "tick_slow should be ready");
}

ZTEST(tick_v03, test_start_stop)
{
	struct lifecycle_status st = {0};

	zassert_ok(tick_start(&tick_fast, &st, K_MSEC(100)));
	zassert_true(st.is_running);

	zassert_ok(tick_stop(&tick_fast, &st, K_MSEC(100)));
	zassert_false(st.is_running);
}

ZTEST(tick_v03, test_start_already_running)
{
	zassert_ok(tick_start(&tick_fast, NULL, K_MSEC(100)));
	int rc = tick_start(&tick_fast, NULL, K_MSEC(100));
	zassert_equal(rc, -EALREADY, "second start should return -EALREADY, got %d", rc);
}

ZTEST(tick_v03, test_config_roundtrip)
{
	struct tick_config req = {.period_ms = 250, .max_period_ms = 60000};
	struct tick_config resp = {0};

	zassert_ok(tick_config(&tick_fast, &req, &resp, K_MSEC(100)));
	zassert_equal(resp.period_ms, 250);

	zassert_ok(tick_get_config(&tick_fast, &resp, K_MSEC(100)));
	zassert_equal(resp.period_ms, 250);
}

ZTEST(tick_v03, test_config_invalid_zero_period)
{
	struct tick_config req = {.period_ms = 0, .max_period_ms = 60000};
	int rc = tick_config(&tick_fast, &req, NULL, K_MSEC(100));
	zassert_equal(rc, -EINVAL, "period_ms=0 must be rejected, got %d", rc);
}

ZTEST(tick_v03, test_config_invalid_exceeds_max)
{
	struct tick_config req = {.period_ms = 2000, .max_period_ms = 1000};
	int rc = tick_config(&tick_fast, &req, NULL, K_MSEC(100));
	zassert_equal(rc, -EINVAL, "period_ms > max_period_ms must be rejected, got %d", rc);
}

ZTEST(tick_v03, test_resp_null_discards)
{
	int rc = tick_start(&tick_fast, NULL, K_MSEC(100));
	zassert_ok(rc, "start with NULL resp should succeed, got %d", rc);
}

/* ----- Multi-instance independence ---------------------------------- */

ZTEST(tick_v03, test_instances_have_distinct_default_configs)
{
	struct tick_config fast_cfg = {0};
	struct tick_config slow_cfg = {0};

	zassert_ok(tick_get_config(&tick_fast, &fast_cfg, K_MSEC(100)));
	zassert_ok(tick_get_config(&tick_slow, &slow_cfg, K_MSEC(100)));

	zassert_equal(fast_cfg.period_ms, 100, "tick_fast default period");
	zassert_equal(slow_cfg.period_ms, 500, "tick_slow default period");
}

ZTEST(tick_v03, test_start_one_leaves_other_idle)
{
	struct lifecycle_status fast_st = {0};
	struct lifecycle_status slow_st = {0};

	zassert_ok(tick_start(&tick_fast, &fast_st, K_MSEC(100)));
	zassert_true(fast_st.is_running);

	zassert_ok(tick_get_status(&tick_slow, &slow_st, K_MSEC(100)));
	zassert_false(slow_st.is_running, "tick_slow must stay idle");
}

ZTEST(tick_v03, test_config_one_leaves_other_unchanged)
{
	struct tick_config mutate = {.period_ms = 42, .max_period_ms = 60000};
	zassert_ok(tick_config(&tick_fast, &mutate, NULL, K_MSEC(100)));

	struct tick_config slow_cfg = {0};
	zassert_ok(tick_get_config(&tick_slow, &slow_cfg, K_MSEC(100)));
	zassert_equal(slow_cfg.period_ms, 500, "tick_slow config must be untouched");
}

ZTEST(tick_v03, test_events_per_instance)
{
	reset_event_counters();

	/* Start only tick_fast; tick_slow should produce no events. */
	zassert_ok(tick_start(&tick_fast, NULL, K_MSEC(100)));

	/* Wait for at least one fast tick. */
	zassert_ok(k_sem_take(&fast_event_sem, K_MSEC(500)),
		   "tick_fast should emit within 500 ms");

	/* tick_slow_sem must not have fired. */
	int slow_count = (int)atomic_get(&slow_event_count);
	zassert_equal(slow_count, 0,
		      "tick_slow must not emit when not started; got %d events", slow_count);

	(void)tick_stop(&tick_fast, NULL, K_MSEC(100));
}

ZTEST(tick_v03, test_both_instances_emit_independently)
{
	reset_event_counters();

	zassert_ok(tick_start(&tick_fast, NULL, K_MSEC(100)));
	zassert_ok(tick_start(&tick_slow, NULL, K_MSEC(100)));

	/* Sleep long enough for multiple fast ticks and at least one slow. */
	k_msleep(700);

	int fast_count = (int)atomic_get(&fast_event_count);
	int slow_count = (int)atomic_get(&slow_event_count);

	zassert_true(fast_count >= 3, "tick_fast should emit >=3 in 700 ms (100 ms period); got %d",
		     fast_count);
	zassert_true(slow_count >= 1,
		     "tick_slow should emit >=1 in 700 ms (500 ms period); got %d", slow_count);
	zassert_true(fast_count > slow_count, "tick_fast should emit more than tick_slow");

	(void)tick_stop(&tick_fast, NULL, K_MSEC(100));
	(void)tick_stop(&tick_slow, NULL, K_MSEC(100));
}
