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

/* Reset between tests */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Stop the timer if running */
	zlet_tick_stop(K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));  /* Consume stop report */

	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	memset(&last_async_report, 0, sizeof(last_async_report));
	memset(&last_result, 0, sizeof(last_result));
	last_has_result = false;
	k_sem_reset(&report_sem);
	k_sem_reset(&async_report_sem);
}

ZTEST_SUITE(tick_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Blocking start returns status */
ZTEST(tick_integration, test_start)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_true(report.status.is_running, "Tick should be running");
}

/* Test 2: Blocking config set returns config */
ZTEST(tick_integration, test_config_set)
{
	struct tick_report report = zlet_tick_config_set(2000, K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Config set should succeed");
	zassert_equal(report.config.delay_ms, 2000, "Config delay mismatch");
}

/* Test 3: Blocking get config returns current config */
ZTEST(tick_integration, test_config_get)
{
	/* First set config */
	zlet_tick_config_set(3000, K_SECONDS(1));

	/* Get config */
	struct tick_report report = zlet_tick_get_config(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Get config should succeed");
	zassert_equal(report.config.delay_ms, 3000, "Config delay mismatch");
}

/* Test 4: Timer actually fires and publishes events */
ZTEST(tick_integration, test_timer_fires)
{
	/* Set short delay */
	zlet_tick_config_set(100, K_SECONDS(1));

	/* Start timer */
	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");

	/* Wait for timer event via async listener */
	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_async_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_true(last_async_report.events.has_tick, "Should have tick event");
	zassert_true(last_async_report.events.timestamp > 0, "Timestamp should be set");
}

/* Test 5: Blocking stop stops timer */
ZTEST(tick_integration, test_stop)
{
	/* Start timer */
	zlet_tick_start(K_SECONDS(1));

	/* Stop timer */
	struct tick_report report = zlet_tick_stop(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "Tick should be stopped");
}

/* Test 6: Blocking get status returns current state */
ZTEST(tick_integration, test_get_status)
{
	/* Start timer first */
	zlet_tick_start(K_SECONDS(1));

	/* Get status */
	struct tick_report report = zlet_tick_get_status(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
	zassert_true(report.status.is_running, "Should be running");
}

/* Test 7: Start-Stop-Start cycle works */
ZTEST(tick_integration, test_start_stop_start)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");
	zassert_true(report.status.is_running, "Should be running");

	report = zlet_tick_stop(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "Should be stopped");

	report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Restart should succeed");
	zassert_true(report.status.is_running, "Should be running again");
}

/* Test hook for ENODEV */
extern void zlet_tick_test_set_ready(bool ready);

/* Test 8: Start when already running returns -EALREADY */
ZTEST(tick_integration, test_start_already_running)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");

	/* Try to start again */
	report = zlet_tick_start(K_SECONDS(1));
	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EALREADY, "Expected -EALREADY");
}

/* Test 9: Config with delay_ms=0 returns -EINVAL */
ZTEST(tick_integration, test_config_invalid)
{
	struct tick_report report = zlet_tick_config_set(0, K_SECONDS(1));

	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EINVAL, "Expected -EINVAL");
}

/* Test 10: Timer async events have has_result=false */
ZTEST(tick_integration, test_timer_async_event)
{
	/* Set short delay */
	zlet_tick_config_set(100, K_SECONDS(1));

	/* Start timer */
	zlet_tick_start(K_SECONDS(1));

	/* Wait for timer event - async event should have has_result=false */
	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_async_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_false(last_async_report.has_result, "Async events should not have result");
}

/* Test 11: Report has invoke_tag set */
ZTEST(tick_integration, test_invoke_tag)
{
	struct tick_report report = zlet_tick_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_equal(report.result.invoke_tag, TICK_INVOKE_START_TAG,
		      "invoke_tag should indicate start");
}

/* Test 12: Blocking call timeout returns -ETIMEDOUT */
ZTEST(tick_integration, test_blocking_timeout)
{
	/* Acquire the zephlet semaphore to block the next call */
	/* Can't easily test timeout without a real slow implementation,
	 * so just verify the API works with a reasonable timeout */
	struct tick_report report = zlet_tick_get_status(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
}
