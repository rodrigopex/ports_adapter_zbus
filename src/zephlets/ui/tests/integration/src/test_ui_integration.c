/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_ui_interface.h"

/* Counter for callbacks */
static int report_count = 0;
static struct ui_report last_report;
static struct zephlet_context last_context;
static bool last_has_context;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener - no mocking! */
static void ui_report_listener(const struct zbus_channel *chan)
{
	const struct ui_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_ui_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		last_has_context = msg->has_context;
		if (msg->has_context) {
			last_context = msg->context;
		}
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, ui_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_ui_report, test_lis, 2);

/* Reset between tests */
static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Stop the zephlet if running */
	zlet_ui_stop(K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));  /* Consume stop report */

	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	memset(&last_context, 0, sizeof(last_context));
	last_has_context = false;
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(ui_integration, NULL, NULL, reset, NULL, NULL);

/* Test 1: Blocking start returns status */
ZTEST(ui_integration, test_start)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_true(report.status.is_running, "ui should be running");
}

/* Test 2: Blocking stop returns status */
ZTEST(ui_integration, test_stop)
{
	/* Start zephlet */
	zlet_ui_start(K_SECONDS(1));

	/* Stop zephlet */
	struct ui_report report = zlet_ui_stop(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "ui should be stopped");
}

/* Test 3: Blocking get status returns current state */
ZTEST(ui_integration, test_get_status)
{
	/* Start zephlet first */
	zlet_ui_start(K_SECONDS(1));

	/* Get status */
	struct ui_report report = zlet_ui_get_status(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
	zassert_true(report.status.is_running, "Should be running");
}

/** Test 4: Config set publishes config */
/** TODO: Uncomment and customize if Config has fields (see battery/tick for examples) */
/*
ZTEST(ui_integration, test_config_set)
{
	struct ui_report report = zlet_ui_config_set(...fields..., K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Config set should succeed");
	// Add assertions for specific config fields
}
*/

/** Test 5: Get config returns current config */
/** TODO: Uncomment and customize if Config has fields (see battery/tick for examples) */
/*
ZTEST(ui_integration, test_config_get)
{
	// First set config
	zlet_ui_config_set(...fields..., K_SECONDS(1));

	// Get config
	struct ui_report report = zlet_ui_get_config(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Get config should succeed");
	// Add assertions for specific config fields
}
*/

/* Test 6: Start-Stop-Start cycle works */
ZTEST(ui_integration, test_lifecycle_cycle)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");
	zassert_true(report.status.is_running, "Should be running");

	report = zlet_ui_stop(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "Should be stopped");

	report = zlet_ui_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Restart should succeed");
	zassert_true(report.status.is_running, "Should be running again");
}

/** Test 7: Blink RPC completes round-trip and publishes events */
ZTEST(ui_integration, test_blink)
{
	/* Call blink RPC - now blocking, returns empty report */
	struct ui_report report = zlet_ui_blink(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Blink should succeed");

	/* Also check that async event was published via listener */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "Should have event report");
	zassert_equal(last_report.events.blink, 1, "Blink count should be 1");

	/* Call again */
	report = zlet_ui_blink(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Second blink should succeed");
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "Should have second event report");
	zassert_equal(last_report.events.blink, 2, "Blink count should be 2");
}

/* Test hook for ENODEV */
extern void zlet_ui_test_set_ready(bool ready);

/* Test 8: Start when already running returns -EALREADY */
ZTEST(ui_integration, test_start_already_running)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");

	/* Try to start again */
	report = zlet_ui_start(K_SECONDS(1));
	zassert_true(report.has_context, "Should have context");
	zassert_equal(report.context.return_code, -EALREADY, "Expected -EALREADY");
}

/* Test 9: Report has invoke_tag set */
ZTEST(ui_integration, test_invoke_tag)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_equal(report.context.invoke_tag, UI_INVOKE_START_TAG,
		      "invoke_tag should indicate start");
}

/* Test 10: Blink async events via listener have has_context=false */
ZTEST(ui_integration, test_blink_async_event)
{
	/* Call blink RPC */
	zlet_ui_blink(K_SECONDS(1));

	/* The listener should see the async events report (has_context=false) */
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No blink events report");
	zassert_equal(last_report.which_ui_report_tag, UI_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_false(last_has_context, "Async events should not have context");
}
