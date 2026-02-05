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
	/* Clear storage between tests */
	zlet_storage_clear(K_MSEC(100));
	k_sem_take(&report_sem, K_MSEC(100)); /* Consume clear event */
	/* Reset counters and state */
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

/* Test 4: Config set publishes config */
ZTEST(storage_integration, test_config_set)
{
	zlet_storage_config_set(10, K_MSEC(100));

	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report received");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.max_entries, 10, "Max entries should be 10");
}

/* Test 5: Get config returns current config */
ZTEST(storage_integration, test_config_get)
{
	/* First set config */
	zlet_storage_config_set(10, K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report");

	/* Reset counter */
	report_count = 0;

	/* Get config */
	zlet_storage_get_config(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No config report from get");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_CONFIG_TAG,
		      "Expected config report");
	zassert_equal(last_report.config.max_entries, 10, "Max entries should be 10");
}

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

/* Helper to invoke store */
static int invoke_store(const struct msg_zlet_storage_key_value *kv)
{
	struct msg_zlet_storage_invoke invoke = {
		.which_storage_invoke = MSG_ZLET_STORAGE_INVOKE_STORE_TAG,
		.store = *kv
	};
	return zbus_chan_pub(&chan_zlet_storage_invoke, &invoke, K_MSEC(100));
}

/* Helper to invoke retrieve */
static int invoke_retrieve(const struct msg_zlet_storage_key_value *kv)
{
	struct msg_zlet_storage_invoke invoke = {
		.which_storage_invoke = MSG_ZLET_STORAGE_INVOKE_RETRIEVE_TAG,
		.retrieve = *kv
	};
	return zbus_chan_pub(&chan_zlet_storage_invoke, &invoke, K_MSEC(100));
}

/* Test 7: Store and retrieve */
ZTEST(storage_integration, test_store_retrieve)
{
	struct msg_zlet_storage_key_value kv = {
		.key = "temp",
		.value = {.size = 3, .bytes = {0x32, 0x35, 0x43}}, /* "25C" */
		.value_size = 3
	};

	/* Store */
	invoke_store(&kv);
	/* store() returns Empty - no report expected */

	/* Retrieve */
	invoke_retrieve(&kv);
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No retrieve report");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_STORAGE_ENTRY_TAG,
		      "Expected storage_entry report");
	zassert_true(last_report.storage_entry.found, "Key should be found");
	zassert_equal(last_report.storage_entry.value_size, 3, "Size mismatch");
	zassert_mem_equal(last_report.storage_entry.value.bytes, kv.value.bytes, 3, "Value mismatch");
}

/* Test 8: Retrieve nonexistent */
ZTEST(storage_integration, test_retrieve_missing)
{
	struct msg_zlet_storage_key_value kv = {.key = "missing"};

	invoke_retrieve(&kv);
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No retrieve report");
	zassert_false(last_report.storage_entry.found, "Key should not be found");
}

/* Test 9: Duplicate key rejected */
ZTEST(storage_integration, test_duplicate_rejected)
{
	struct msg_zlet_storage_key_value kv1 = {
		.key = "dup",
		.value = {.size = 1, .bytes = {0x01}},
		.value_size = 1
	};

	/* Store once */
	zassert_ok(invoke_store(&kv1));

	/* Store again - should fail */
	invoke_store(&kv1);
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No error event");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_EVENTS_TAG,
		      "Expected events report");
}

/* Test 10: Clear storage */
ZTEST(storage_integration, test_clear)
{
	struct msg_zlet_storage_key_value kv = {.key = "test", .value = {.size = 1, .bytes = {0xAA}}, .value_size = 1};

	invoke_store(&kv);

	/* Clear */
	zlet_storage_clear(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No clear event");

	/* Verify cleared */
	invoke_retrieve(&kv);
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)));
	zassert_false(last_report.storage_entry.found, "Key should not exist after clear");
}

/* Test 11: Get storage stats */
ZTEST(storage_integration, test_storage_stats)
{
	struct msg_zlet_storage_key_value kv1 = {.key = "k1", .value = {.size = 1, .bytes = {0x11}}, .value_size = 1};
	struct msg_zlet_storage_key_value kv2 = {.key = "k2", .value = {.size = 2, .bytes = {0x22, 0x22}}, .value_size = 2};

	invoke_store(&kv1);
	invoke_store(&kv2);

	zlet_storage_get_storage_stats(K_MSEC(100));
	zassert_ok(k_sem_take(&report_sem, K_SECONDS(1)), "No stats report");
	zassert_equal(last_report.which_storage_report, MSG_ZLET_STORAGE_REPORT_STORAGE_STATS_TAG,
		      "Expected storage_stats report");
	zassert_equal(last_report.storage_stats.total_entries, 2, "Should have 2 entries");
	zassert_equal(last_report.storage_stats.max_entries, 10, "Max should be 10");
	zassert_equal(last_report.storage_stats.total_bytes_used, 3, "Should use 3 bytes");
}
