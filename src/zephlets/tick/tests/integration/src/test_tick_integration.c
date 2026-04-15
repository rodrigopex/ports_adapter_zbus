/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_tick_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct tick_report last_report;
static struct tick_report last_async_report;
static struct zephlet_result last_result;
static bool last_has_result;

K_SEM_DEFINE(report_sem, 0, 10);
K_SEM_DEFINE(async_report_sem, 0, 10);

/* Real listener - no mocking! */
static void tick_report_listener(const struct zbus_channel *chan)
{
	const struct tick_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_tick_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		last_has_result = msg->has_result;
		if (msg->has_result) {
			last_result = msg->result;
		} else {
			memcpy(&last_async_report, msg, sizeof(last_async_report));
			k_sem_give(&async_report_sem);
		}
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, tick_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_tick_report, test_lis, 2);

/* Reset between tests: stop, restore defaults, drain reports. */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	zlet_tick_stop(K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));

	struct tick_settings restore = {
		.has_delay_ms = true,
		.delay_ms = 1000,
		.has_max_delay_ms = true,
		.max_delay_ms = 60000,
	};
	zlet_tick_update_settings(&restore, K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));

	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	memset(&last_async_report, 0, sizeof(last_async_report));
	memset(&last_result, 0, sizeof(last_result));
	last_has_result = false;
	k_sem_reset(&report_sem);
	k_sem_reset(&async_report_sem);
}

ZTEST_SUITE(tick_integration, NULL, NULL, reset, NULL, NULL);

ZTEST(tick_integration, test_start)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_true(report.status.is_running, "Tick should be running");
}

ZTEST(tick_integration, test_update_settings)
{
	struct tick_settings delta = {
		.has_delay_ms = true,
		.delay_ms = 2000,
	};
	struct tick_report report = zlet_tick_update_settings(&delta, K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "update_settings should succeed");
	zassert_equal(report.settings.delay_ms, 2000, "delay_ms mismatch");
}

ZTEST(tick_integration, test_get_settings)
{
	struct tick_settings delta = {
		.has_delay_ms = true,
		.delay_ms = 3000,
	};
	zlet_tick_update_settings(&delta, K_SECONDS(1));

	struct tick_report report = zlet_tick_get_settings(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "get_settings should succeed");
	zassert_equal(report.settings.delay_ms, 3000, "delay_ms mismatch");
}

ZTEST(tick_integration, test_timer_fires)
{
	struct tick_settings delta = {
		.has_delay_ms = true,
		.delay_ms = 100,
	};
	zlet_tick_update_settings(&delta, K_SECONDS(1));

	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");

	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_async_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_true(last_async_report.events.has_tick, "Should have tick event");
	zassert_true(last_async_report.events.timestamp > 0, "Timestamp should be set");
}

ZTEST(tick_integration, test_stop)
{
	zlet_tick_start(K_SECONDS(1));

	struct tick_report report = zlet_tick_stop(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "Tick should be stopped");
}

ZTEST(tick_integration, test_get_status)
{
	zlet_tick_start(K_SECONDS(1));

	struct tick_report report = zlet_tick_get_status(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
	zassert_true(report.status.is_running, "Should be running");
}

ZTEST(tick_integration, test_start_stop_start)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");

	report = zlet_tick_stop(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");

	report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Restart should succeed");
}

ZTEST(tick_integration, test_start_already_running)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");

	report = zlet_tick_start(K_SECONDS(1));
	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EALREADY, "Expected -EALREADY");
}

ZTEST(tick_integration, test_update_settings_invalid)
{
	/* delay_ms = 0 is rejected by validate_settings. */
	struct tick_settings delta = {
		.has_delay_ms = true,
		.delay_ms = 0,
	};
	struct tick_report report = zlet_tick_update_settings(&delta, K_SECONDS(1));

	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EINVAL, "Expected -EINVAL");
}

/*
 * Regression for cross-field validation with a single-field delta
 * (Scenario C from the design discussion).
 *
 * Stored defaults: delay_ms=1000, max_delay_ms=60000.
 * Delta sets ONLY max_delay_ms to 500 → merged {1000, 500} violates
 * delay_ms <= max_delay_ms. A delta-only validator would pass it;
 * the merged-state validator catches it.
 */
ZTEST(tick_integration, test_update_settings_cross_field)
{
	struct tick_settings delta = {
		.has_max_delay_ms = true,
		.max_delay_ms = 500,
	};
	struct tick_report report = zlet_tick_update_settings(&delta, K_SECONDS(1));

	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EINVAL,
		      "Merged-state validator should reject delay_ms > max_delay_ms");

	/* Stored settings must be unchanged. */
	report = zlet_tick_get_settings(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "get_settings should succeed");
	zassert_equal(report.settings.max_delay_ms, 60000,
		      "max_delay_ms must be untouched after validation failure");
}

ZTEST(tick_integration, test_timer_async_event)
{
	struct tick_settings delta = {
		.has_delay_ms = true,
		.delay_ms = 100,
	};
	zlet_tick_update_settings(&delta, K_SECONDS(1));

	zlet_tick_start(K_SECONDS(1));

	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_async_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_false(last_async_report.has_result, "Async events should not have result");
}

ZTEST(tick_integration, test_invoke_tag)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_equal(report.result.invoke_tag, TICK_INVOKE_START_TAG,
		      "invoke_tag should indicate start");
}

ZTEST(tick_integration, test_blocking_timeout)
{
	struct tick_report report = zlet_tick_get_status(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
}
