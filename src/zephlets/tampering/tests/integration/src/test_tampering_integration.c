/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include "zlet_tampering_interface.h"

static int report_count = 0;
static struct tampering_report last_report;
static struct zephlet_result last_result;
static bool last_has_result;

K_SEM_DEFINE(report_sem, 0, 10);

/* Real async listener — no mocking. */
static void tampering_report_listener(const struct zbus_channel *chan)
{
	const struct tampering_report *msg = zbus_chan_const_msg(chan);

	if (chan == &chan_zlet_tampering_report) {
		memcpy(&last_report, msg, sizeof(last_report));
		last_has_result = msg->has_result;
		if (msg->has_result) {
			last_result = msg->result;
		}
		report_count++;
		k_sem_give(&report_sem);
	}
}

ZBUS_LISTENER_DEFINE(test_lis, tampering_report_listener);
ZBUS_CHAN_ADD_OBS(chan_zlet_tampering_report, test_lis, 2);

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	(void)zlet_tampering_stop(K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100));

	report_count = 0;
	memset(&last_report, 0, sizeof(last_report));
	memset(&last_result, 0, sizeof(last_result));
	last_has_result = false;
	k_sem_reset(&report_sem);
}

ZTEST_SUITE(tampering_integration, NULL, NULL, reset, NULL, NULL);

ZTEST(tampering_integration, test_is_ready)
{
	zassert_true(zlet_tampering_is_ready(), "Tampering should be ready after init");
}

ZTEST(tampering_integration, test_start)
{
	struct tampering_report report = zlet_tampering_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Start should succeed");
	zassert_true(report.status.is_running, "tampering should be running");
}

ZTEST(tampering_integration, test_stop)
{
	zlet_tampering_start(K_SECONDS(1));
	struct tampering_report report = zlet_tampering_stop(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Stop should succeed");
	zassert_false(report.status.is_running, "tampering should be stopped");
}

ZTEST(tampering_integration, test_get_status)
{
	zlet_tampering_start(K_SECONDS(1));
	struct tampering_report report = zlet_tampering_get_status(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Get status should succeed");
	zassert_true(report.status.is_running, "Should be running");
}

/* TODO: uncomment and customize once Settings has optional fields. Example:
 *
 * ZTEST(tampering_integration, test_update_settings)
 * {
 *     struct tampering_settings delta = {
 *         .has_some_field = true,
 *         .some_field     = 42,
 *     };
 *     struct tampering_report report =
 *         zlet_tampering_update_settings(&delta, K_SECONDS(1));
 *     zassert_true(ZEPHLET_CALL_OK(report), "update_settings should succeed");
 *     zassert_equal(report.settings.some_field, 42, "Field mismatch");
 * }
 */

ZTEST(tampering_integration, test_lifecycle_cycle)
{
	zlet_tampering_start(K_SECONDS(1));
	zlet_tampering_stop(K_SECONDS(1));
	struct tampering_report report = zlet_tampering_start(K_SECONDS(1));
	zassert_true(ZEPHLET_CALL_OK(report), "Restart should succeed");
}

/* TODO: Add tests for zephlet-specific RPC functions */
