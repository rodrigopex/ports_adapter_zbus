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
static struct ui_report last_async_report;
static struct zephlet_result last_result;
static bool last_has_result;

K_SEM_DEFINE(report_sem, 0, 10);
K_SEM_DEFINE(async_report_sem, 0, 10);

/* Real async listener - no mocking! */
static void ui_report_listener(const struct zbus_channel *chan)
{
	const struct ui_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_ui_report) {
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
	memset(&last_async_report, 0, sizeof(last_async_report));
	memset(&last_result, 0, sizeof(last_result));
	last_has_result = false;
	k_sem_reset(&report_sem);
	k_sem_reset(&async_report_sem);
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

ZTEST(ui_integration, test_update_settings)
{
	struct ui_settings delta = {
		.has_user_button_long_press_duration = true,
		.user_button_long_press_duration = 1500,
	};
	struct ui_report report = zlet_ui_update_settings(&delta, K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "update_settings should succeed");
	zassert_equal(report.settings.user_button_long_press_duration, 1500,
		      "Field mismatch");
}

ZTEST(ui_integration, test_get_settings)
{
	struct ui_settings delta = {
		.has_user_button_long_press_duration = true,
		.user_button_long_press_duration = 2500,
	};
	zlet_ui_update_settings(&delta, K_SECONDS(1));

	struct ui_report report = zlet_ui_get_settings(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "get_settings should succeed");
	zassert_equal(report.settings.user_button_long_press_duration, 2500,
		      "Field mismatch");
}

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
	/* Call blink RPC - blocking, returns empty report */
	struct ui_report report = zlet_ui_blink(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Blink should succeed");

	/* Also check that async event was published via listener */
	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(1)), "Should have event report");
	zassert_equal(last_async_report.events.blink, 1, "Blink count should be 1");

	/* Call again */
	report = zlet_ui_blink(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Second blink should succeed");
	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(1)), "Should have second event report");
	zassert_equal(last_async_report.events.blink, 2, "Blink count should be 2");
}

/* Test 8: Start when already running returns -EALREADY */
ZTEST(ui_integration, test_start_already_running)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "First start should succeed");

	/* Try to start again */
	report = zlet_ui_start(K_SECONDS(1));
	zassert_true(report.has_result, "Should have context");
	zassert_equal(report.result.return_code, -EALREADY, "Expected -EALREADY");
}

/* Test 9: Report has invoke_tag set */
ZTEST(ui_integration, test_invoke_tag)
{
	struct ui_report report = zlet_ui_start(K_SECONDS(1));

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_equal(report.result.invoke_tag, UI_INVOKE_START_TAG,
		      "invoke_tag should indicate start");
}

/* Test 10: Blink async events via listener have has_result=false */
ZTEST(ui_integration, test_blink_async_event)
{
	/* Call blink RPC */
	zlet_ui_blink(K_SECONDS(1));

	/* The listener should see the async events report (has_result=false) */
	zassert_ok(k_sem_take(&async_report_sem, K_SECONDS(1)), "No blink events report");
	zassert_equal(last_async_report.which_ui_report_tag, UI_REPORT_EVENTS_TAG,
		      "Expected events report");
	zassert_false(last_async_report.has_result, "Async events should not have result");
}

/* Test 11: Timeout is an upper bound — blocking call returns well before it expires */
ZTEST(ui_integration, test_timeout_propagation)
{
	const k_timeout_t budget = K_SECONDS(5);
	int64_t t0 = k_uptime_get();

	struct ui_report report = zlet_ui_start(budget);

	int64_t elapsed = k_uptime_get() - t0;

	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	/* If the timeout weren't honored as an upper bound, a lost report would
	 * hang forever or wait the full budget; a prompt return proves the
	 * blocking layer is waiting under the caller's timeout. */
	zassert_true(elapsed < 500, "Elapsed %lldms should be << 5000ms budget", elapsed);
}

