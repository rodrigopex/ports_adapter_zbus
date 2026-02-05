/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_position_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct msg_zlet_position_report last_report;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener - no mocking! */
static void position_report_listener(const struct zbus_channel *chan)
{
	const struct msg_zlet_position_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_position_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, position_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_position_report, test_lis, 2);

/* Reset between tests */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(position_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Start publishes status */
ZTEST(position_integration, test_start)
{
	zlet_position_start(K_MSEC(100));

	/* Wait for listener callback */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status report received");

	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "position should be running");
	zassert_equal(report_count, 1, "Expected 1 report");
}

/* Test 2: Stop stops zephlet */
ZTEST(position_integration, test_stop)
{
	/* Start zephlet */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Stop zephlet */
	zlet_position_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_false(last_report.status.is_running, "position should be stopped");
}

/* Test 3: Get status returns current state */
ZTEST(position_integration, test_get_status)
{
	/* Start zephlet first */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Reset counter */
	report_count = 0;

	/* Get status */
	zlet_position_get_status(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status from get");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Should be running");
}

/* Test 4: Config set publishes config */
ZTEST(position_integration, test_config_set)
{
	zlet_position_config_set(1000, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.update_interval_ms, 1000, "Interval should be 1000ms");
}

/* Test 5: Get config returns current config */
ZTEST(position_integration, test_config_get)
{
	/* First set config */
	zlet_position_config_set(2000, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Reset counter */
	report_count = 0;

	/* Get config */
	zlet_position_get_config(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.update_interval_ms, 2000, "Interval should be 2000ms");
}

/* Test 6: Start-Stop-Start cycle works */
ZTEST(position_integration, test_lifecycle_cycle)
{
	/* Start */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_report.status.is_running, "Should be running");

	/* Stop */
	zlet_position_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_false(last_report.status.is_running, "Should be stopped");

	/* Start again */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No restart status");
	zassert_true(last_report.status.is_running, "Should be running again");
}

/* Test 7: Get position */
ZTEST(position_integration, test_get_position)
{
	/* Start */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Get position */
	zlet_position_get_position(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No position report");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_POSITION_DATA_TAG,
		      "Expected position_data report");
	zassert_true(last_report.position_data.fix_valid, "Fix should be valid");
	zassert_true(last_report.position_data.satellites > 0, "Should have satellites");

	/* Stop */
	zlet_position_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
}

/* Test 8: Position changed events */
ZTEST(position_integration, test_position_changed_events)
{
	/* Start */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Wait for periodic position_changed event (5s default) */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(7)), "No position_changed event");
	zassert_equal(last_report.which_position_report, MSG_ZLET_POSITION_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_true(last_report.events.has_position_changed, "Should have position_changed");

	/* Stop */
	zlet_position_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
}

/* Test 9: Config update interval */
ZTEST(position_integration, test_config_update_interval)
{
	/* Set config */
	zlet_position_config_set(1000, K_MSEC(100)); /* 1s updates */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Start */
	zlet_position_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Should get position_changed within 2s instead of 5s */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(3)), "No quick position_changed event");
	zassert_true(last_report.events.has_position_changed, "Should have position_changed");

	/* Stop */
	zlet_position_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
}
