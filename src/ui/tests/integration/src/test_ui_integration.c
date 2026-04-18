/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "zlet_ui.h"

/* ----- Test instance -------------------------------------------------- */

static struct ui_data ui_test_data;
static const struct ui_config ui_test_cfg = {
	.user_button_long_press_duration = 1000,
};

ZEPHLET_DEFINE(ui, ui_test, &ui_test_cfg, &ui_test_data, ui_init_fn);

/* ----- Event observer ------------------------------------------------- */

static atomic_t event_count;
static struct ui_events last_event;
static K_SEM_DEFINE(event_sem, 0, 20);

static void on_ui_event(const struct ui_events *ev)
{
	last_event = *ev;
	atomic_inc(&event_count);
	k_sem_give(&event_sem);
}
ZEPHLET_EVENTS_LISTENER(ui_test, ui, on_ui_event);

/* ----- Fixture -------------------------------------------------------- */

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	(void)ui_stop(&ui_test, NULL, K_MSEC(100));
	atomic_set(&event_count, 0);
	k_sem_reset(&event_sem);
}

ZTEST_SUITE(ui_v03, NULL, NULL, reset, NULL, NULL);

/* ----- Tests ---------------------------------------------------------- */

ZTEST(ui_v03, test_is_ready)
{
	zassert_true(ui_is_ready(&ui_test));
}

ZTEST(ui_v03, test_start_stop)
{
	struct lifecycle_status st = {0};

	zassert_ok(ui_start(&ui_test, &st, K_MSEC(100)));
	zassert_true(st.is_running);

	zassert_ok(ui_stop(&ui_test, &st, K_MSEC(100)));
	zassert_false(st.is_running);
}

ZTEST(ui_v03, test_start_already_running)
{
	zassert_ok(ui_start(&ui_test, NULL, K_MSEC(100)));
	int rc = ui_start(&ui_test, NULL, K_MSEC(100));
	zassert_equal(rc, -EALREADY, "second start should return -EALREADY, got %d", rc);
}

ZTEST(ui_v03, test_get_status)
{
	zassert_ok(ui_start(&ui_test, NULL, K_MSEC(100)));
	struct lifecycle_status st = {0};
	zassert_ok(ui_get_status(&ui_test, &st, K_MSEC(100)));
	zassert_true(st.is_running);
}

ZTEST(ui_v03, test_config_roundtrip)
{
	struct ui_config req = {.user_button_long_press_duration = 2500};
	struct ui_config resp = {0};

	zassert_ok(ui_config(&ui_test, &req, &resp, K_MSEC(100)));
	zassert_equal(resp.user_button_long_press_duration, 2500);

	zassert_ok(ui_get_config(&ui_test, &resp, K_MSEC(100)));
	zassert_equal(resp.user_button_long_press_duration, 2500);
}

ZTEST(ui_v03, test_blink_emits_event)
{
	zassert_ok(ui_blink(&ui_test, K_MSEC(100)));
	zassert_ok(k_sem_take(&event_sem, K_MSEC(100)), "blink must emit an event");
	zassert_equal(last_event.blink, 1, "first blink: counter == 1");

	zassert_ok(ui_blink(&ui_test, K_MSEC(100)));
	zassert_ok(k_sem_take(&event_sem, K_MSEC(100)));
	zassert_equal(last_event.blink, 2, "second blink: counter == 2");
}
