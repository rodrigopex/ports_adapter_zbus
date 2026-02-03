/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

/* Wait for semaphore with timeout */
#define WAIT_FOR_SEM(sem, timeout_ms) \
	zassert_ok(k_sem_take(sem, K_MSEC(timeout_ms)), \
		   "Semaphore not signaled within %dms", timeout_ms)

/* Assert zbus operation succeeded */
#define ASSERT_ZBUS_OK(ret) \
	zassert_ok(ret, "zbus operation failed: %d", ret)

/* Wait for async listener callback */
#define WAIT_FOR_CALLBACK(sem, timeout_ms) \
	zassert_ok(k_sem_take(sem, K_MSEC(timeout_ms)), \
		   "Callback not triggered within %dms", timeout_ms)

#endif /* TEST_HELPERS_H */
