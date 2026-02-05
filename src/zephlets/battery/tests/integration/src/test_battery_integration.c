/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_battery_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct msg_zlet_battery_report last_report;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener - no mocking! */
static void battery_report_listener(const struct zbus_channel *chan)
{
	const struct msg_zlet_battery_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_battery_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, battery_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_battery_report, test_lis, 2);

/* Reset between tests */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(battery_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Start publishes status */
ZTEST(battery_integration, test_start)
{
	zlet_battery_start(K_MSEC(100));

	/* Wait for listener callback */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status report received");

	zassert_equal(last_report.which_battery_report, MSG_ZLET_BATTERY_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "battery should be running");
	zassert_equal(report_count, 1, "Expected 1 report");
}

/* Test 2: Stop stops zephlet */
ZTEST(battery_integration, test_stop)
{
	/* Start zephlet */
	zlet_battery_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Stop zephlet */
	zlet_battery_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_equal(last_report.which_battery_report, MSG_ZLET_BATTERY_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_false(last_report.status.is_running, "battery should be stopped");
}

/* Test 3: Get status returns current state */
ZTEST(battery_integration, test_get_status)
{
	/* Start zephlet first */
	zlet_battery_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Reset counter */
	report_count = 0;

	/* Get status */
	zlet_battery_get_status(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status from get");
	zassert_equal(last_report.which_battery_report, MSG_ZLET_BATTERY_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Should be running");
}

/** Test 4: Config set publishes config */
ZTEST(battery_integration, test_config_set)
{
	zlet_battery_config_set(3700, 20, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_battery_report, MSG_ZLET_BATTERY_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.voltage_operation, 3700, "Voltage mismatch");
	zassert_equal(last_report.config.low_battery_threshold, 20, "Threshold mismatch");
}

/** Test 5: Get config returns current config */
ZTEST(battery_integration, test_config_get)
{
	/** First set config */
	zlet_battery_config_set(3600, 15, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/** Reset counter */
	report_count = 0;

	/** Get config */
	zlet_battery_get_config(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_battery_report, MSG_ZLET_BATTERY_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.voltage_operation, 3600, "Voltage mismatch");
	zassert_equal(last_report.config.low_battery_threshold, 15, "Threshold mismatch");
}

/* Test 6: Start-Stop-Start cycle works */
ZTEST(battery_integration, test_lifecycle_cycle)
{
	/* Start */
	zlet_battery_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_report.status.is_running, "Should be running");

	/* Stop */
	zlet_battery_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_false(last_report.status.is_running, "Should be stopped");

	/* Start again */
	zlet_battery_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No restart status");
	zassert_true(last_report.status.is_running, "Should be running again");
}

/** Test 7: Custom RPC tests */
/** TODO: Add tests for zephlet-specific RPC functions (see battery for get_battery_state example) */
