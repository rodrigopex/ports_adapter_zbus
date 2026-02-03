/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_tick_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct msg_zlet_tick_report last_report;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener - no mocking! */
static void tick_report_listener(const struct zbus_channel *chan)
{
	const struct msg_zlet_tick_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_tick_report) {
		memcpy(&last_report, msg, sizeof(last_report));
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
	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(tick_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Start publishes status */
ZTEST(tick_integration, test_start)
{
	zlet_tick_start(K_MSEC(100));

	/* Wait for listener callback */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status report received");

	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Tick should be running");
	zassert_equal(report_count, 1, "Expected 1 report");
}

/* Test 2: Config set publishes config */
ZTEST(tick_integration, test_config_set)
{
	zlet_tick_config_set(2000, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.delay_ms, 2000, "Config delay mismatch");
}

/* Test 3: Get config returns current config */
ZTEST(tick_integration, test_config_get)
{
	/* First set config */
	zlet_tick_config_set(3000, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Reset counter */
	report_count = 0;

	/* Get config */
	zlet_tick_get_config(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.delay_ms, 3000, "Config delay mismatch");
}

/* Test 4: Timer actually fires and publishes events */
ZTEST(tick_integration, test_timer_fires)
{
	/* Set short delay */
	zlet_tick_config_set(100, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Start timer */
	zlet_tick_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Wait for timer event */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(2)), "Timer should fire");
	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_true(last_report.events.has_tick, "Should have tick event");
}

/* Test 5: Stop stops timer */
ZTEST(tick_integration, test_stop)
{
	/* Start timer */
	zlet_tick_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Stop timer */
	zlet_tick_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_false(last_report.status.is_running, "Tick should be stopped");
}

/* Test 6: Get status returns current state */
ZTEST(tick_integration, test_get_status)
{
	/* Start timer first */
	zlet_tick_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Reset counter */
	report_count = 0;

	/* Get status */
	zlet_tick_get_status(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status from get");
	zassert_equal(last_report.which_tick_report, MSG_ZLET_TICK_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Should be running");
}

/* Test 7: Start-Stop-Start cycle works */
ZTEST(tick_integration, test_start_stop_start)
{
	/* Start */
	zlet_tick_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_report.status.is_running, "Should be running");

	/* Stop */
	zlet_tick_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_false(last_report.status.is_running, "Should be stopped");

	/* Start again */
	zlet_tick_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No restart status");
	zassert_true(last_report.status.is_running, "Should be running again");
}
