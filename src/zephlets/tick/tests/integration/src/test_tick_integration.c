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
static struct zephlet_context last_context;
static bool last_has_context;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real listener - no mocking! */
static void tick_report_listener(const struct zbus_channel *chan)
{
	const struct tick_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_tick_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		last_has_context = msg->has_context;
		if (msg->has_context) {
			last_context = msg->context;
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
	zlet_tick_stop(0, K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));  /* Consume stop report */

	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	memset(&last_context, 0, sizeof(last_context));
	last_has_context = false;
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(tick_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Start publishes status */
ZTEST(tick_integration, test_start)
{
	zlet_tick_start(123, K_MSEC(100));

	/* Wait for listener callback */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status report received");

	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Tick should be running");
	zassert_equal(report_count, 1, "Expected 1 report");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 123, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test 2: Config set publishes config */
ZTEST(tick_integration, test_config_set)
{
	zlet_tick_config_set(124, 2000, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.delay_ms, 2000, "Config delay mismatch");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 124, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test 3: Get config returns current config */
ZTEST(tick_integration, test_config_get)
{
	/* First set config */
	zlet_tick_config_set(0, 3000, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Reset counter */
	report_count = 0;

	/* Get config */
	zlet_tick_get_config(125, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.delay_ms, 3000, "Config delay mismatch");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 125, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test 4: Timer actually fires and publishes events */
ZTEST(tick_integration, test_timer_fires)
{
	/* Set short delay */
	zlet_tick_config_set(0, 100, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Start timer */
	zlet_tick_start(126, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_has_context, "Start should have context");
	zassert_equal(last_context.correlation_id, 126, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");

	/* Wait for timer event */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_true(last_report.events.has_tick, "Should have tick event");
	zassert_true(last_report.events.timestamp > 0, "Timestamp should be set");
}

/* Test 5: Stop stops timer */
ZTEST(tick_integration, test_stop)
{
	/* Start timer */
	zlet_tick_start(0, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Stop timer */
	zlet_tick_stop(127, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_false(last_report.status.is_running, "Tick should be stopped");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 127, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test 6: Get status returns current state */
ZTEST(tick_integration, test_get_status)
{
	/* Start timer first */
	zlet_tick_start(0, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Reset counter */
	report_count = 0;

	/* Get status */
	zlet_tick_get_status(128, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status from get");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Should be running");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 128, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test 7: Start-Stop-Start cycle works */
ZTEST(tick_integration, test_start_stop_start)
{
	/* Start */
	zlet_tick_start(129, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_report.status.is_running, "Should be running");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 129, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");

	/* Stop */
	zlet_tick_stop(0, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_false(last_report.status.is_running, "Should be stopped");

	/* Start again */
	zlet_tick_start(130, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No restart status");
	zassert_true(last_report.status.is_running, "Should be running again");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 130, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, 0, "Expected success");
}

/* Test hook for ENODEV */
extern void zlet_tick_test_set_ready(bool ready);

/* Test 8: Start when already running returns -EALREADY */
ZTEST(tick_integration, test_start_already_running)
{
	/* Start */
	zlet_tick_start(131, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_equal(last_context.return_code, 0, "First start should succeed");

	/* Try to start again */
	zlet_tick_start(132, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No second start status");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 132, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, -EALREADY, "Expected -EALREADY");
}

/* Test 9: Config with delay_ms=0 returns -EINVAL */
ZTEST(tick_integration, test_config_invalid)
{
	zlet_tick_config_set(133, 0, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");
	zassert_true(last_has_context, "Should have context");
	zassert_equal(last_context.correlation_id, 133, "Correlation ID mismatch");
	zassert_equal(last_context.return_code, -EINVAL, "Expected -EINVAL");
}

/* Test 10: Timer async events have has_context=false */
ZTEST(tick_integration, test_timer_async_event)
{
	/* Set short delay */
	zlet_tick_config_set(0, 100, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Start timer */
	zlet_tick_start(0, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Wait for timer event - async event should have has_context=false */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_report.which_tick_report_tag, TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_false(last_has_context, "Async events should not have context");
}

/* Test 11: Start when not ready returns -ENODEV */
/* Note: This test requires manual initialization control which is not easily */
/* testable in integration tests. Skipping for now - ENODEV path tested in unit tests */

/* Test 12: wait_report receives correct report via OBSERVE_REPORT scope */
ZTEST(tick_integration, test_wait_report)
{
	struct tick_report *report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick) {
		zlet_tick_start(300, K_MSEC(100));
		report = zlet_tick_wait_report(TICK_REPORT_STATUS_TAG, K_SECONDS(1));
	}

	zassert_not_null(report, "Should receive status report");
	zassert_true(report->has_context, "Should have context");
	zassert_equal(report->context.correlation_id, 300, "Correlation ID");
	zassert_equal(report->context.return_code, 0, "Expected success");
	zassert_true(report->status.is_running, "Should be running");
}

/* Test 13: wait_report returns NULL on timeout */
ZTEST(tick_integration, test_wait_report_timeout)
{
	struct tick_report *report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick) {
		/* No invoke — should timeout */
		report = zlet_tick_wait_report(TICK_REPORT_STATUS_TAG, K_MSEC(200));
	}

	zassert_is_null(report, "Should timeout with NULL");
}

/* Test 14: wait_report filters by tag */
ZTEST(tick_integration, test_wait_report_filter)
{
	struct tick_report *report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick) {
		/* start produces STATUS report, but we wait for CONFIG */
		zlet_tick_start(301, K_MSEC(100));
		report = zlet_tick_wait_report(TICK_REPORT_CONFIG_TAG, K_MSEC(500));
	}

	zassert_is_null(report, "Wrong tag should not match");
}

/* Test 15: wait_report with filter_tag=-1 accepts any report */
ZTEST(tick_integration, test_wait_report_no_filter)
{
	struct tick_report *report = NULL;

	ZEPHLET_OBSERVE_REPORT(zlet_tick) {
		zlet_tick_start(302, K_MSEC(100));
		report = zlet_tick_wait_report(-1, K_SECONDS(1));
	}

	zassert_not_null(report, "Should receive any report");
	zassert_equal(report->which_tick_report_tag, TICK_REPORT_STATUS_TAG,
		      "First report should be status");
}
