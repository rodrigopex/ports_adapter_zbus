/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_storage_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct msg_zlet_storage_report last_report;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener - no mocking! */
static void storage_report_listener(const struct zbus_channel *chan)
{
	const struct msg_zlet_storage_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_storage_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, storage_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_storage_report, test_lis, 2);

/* Reset between tests */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(storage_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Start publishes status */
ZTEST(storage_integration, test_start)
{
	zlet_storage_start(K_MSEC(100));

	/* Wait for listener callback */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status report received");

	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "storage should be running");
	zassert_equal(report_count, 1, "Expected 1 report");
}

/* Test 2: Stop stops zephlet */
ZTEST(storage_integration, test_stop)
{
	/* Start zephlet */
	zlet_storage_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Stop zephlet */
	zlet_storage_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_false(last_report.status.is_running, "storage should be stopped");
}

/* Test 3: Get status returns current state */
ZTEST(storage_integration, test_get_status)
{
	/* Start zephlet first */
	zlet_storage_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");

	/* Reset counter */
	report_count = 0;

	/* Get status */
	zlet_storage_get_status(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No status from get");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_STATUS_TAG,
		      "Expected status report");
	zassert_true(last_report.status.is_running, "Should be running");
}

/** Test 4: Config set publishes config */
/** TODO: Uncomment and customize if Config has fields (see battery/tick for examples) */
/*
ZTEST(storage_integration, test_config_set)
{
	struct msg_zlet_storage_config cfg = {
		// Fill in config field values here
	};

	zlet_storage_config(&cfg, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_CONFIG_TAG,
		      "Expected config report");
	// Add assertions for specific config fields
}
*/

/** Test 5: Get config returns current config */
/** TODO: Uncomment and customize if Config has fields (see battery/tick for examples) */
/*
ZTEST(storage_integration, test_config_get)
{
	struct msg_zlet_storage_config cfg = {
		// Fill in config field values here
	};

	// First set config
	zlet_storage_config(&cfg, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	// Reset counter
	report_count = 0;

	// Get config
	zlet_storage_get_config(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_CONFIG_TAG,
		      "Expected config report");
	// Add assertions for specific config fields
}
*/

/* Test 6: Start-Stop-Start cycle works */
ZTEST(storage_integration, test_lifecycle_cycle)
{
	/* Start */
	zlet_storage_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No start status");
	zassert_true(last_report.status.is_running, "Should be running");

	/* Stop */
	zlet_storage_stop(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stop status");
	zassert_false(last_report.status.is_running, "Should be stopped");

	/* Start again */
	zlet_storage_start(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No restart status");
	zassert_true(last_report.status.is_running, "Should be running again");
}

/** Test 7: Custom RPC tests */
/** TODO: Add tests for zephlet-specific RPC functions (see battery for get_battery_state example) */
