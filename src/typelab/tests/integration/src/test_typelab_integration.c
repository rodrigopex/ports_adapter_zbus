/*
 * Copyright (c) 2026 Rodrigo Peixoto
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "zlet_typelab.h"

/* ----- Test instance -------------------------------------------------- */

static struct typelab_data typelab_test_data;
static struct typelab_config typelab_test_cfg = {0};

ZEPHLET_NEW(typelab, typelab_test, &typelab_test_cfg, &typelab_test_data, typelab_init_fn);

/* ----- Test fixture --------------------------------------------------- */

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	(void)typelab_stop(&typelab_test, NULL, K_MSEC(100));
}

ZTEST_SUITE(typelab_integration, NULL, NULL, reset, NULL, NULL);

/* ----- Tests ---------------------------------------------------------- */

ZTEST(typelab_integration, test_is_ready)
{
	zassert_true(typelab_is_ready(&typelab_test), "typelab should be ready after init");
}

ZTEST(typelab_integration, test_get_status)
{
	struct lifecycle_status st = {0};

	zassert_ok(typelab_get_status(&typelab_test, &st, K_MSEC(100)));
	zassert_true(st.is_ready, "instance should report is_ready");
}

/*
 * Config round trip through the plain C wrapper API (typelab_config()/
 * typelab_get_config()) — one value per nanopb scalar type the `zlet`
 * shell frontend also exercises, but via the generated wrappers
 * directly rather than through the shell's parse/print path.
 */
ZTEST(typelab_integration, test_config_round_trip_all_18_fields)
{
	static const uint8_t bytes_in[4] = {0xDE, 0xAD, 0xBE, 0xEF};
	static const uint8_t fixed_bytes_in[4] = {0xCA, 0xFE, 0xBA, 0xBE};
	struct typelab_config req = {
		.f_uint32 = 4000000000U,
		.f_uint64 = 18000000000000000000ULL,
		.f_fixed32 = 0x12345678U,
		.f_fixed64 = 0x1122334455667788ULL,
		.f_uenum = TYPELAB_UFLAG_ONE,
		.f_int32 = -2000000000,
		.f_int64 = -9000000000000000000LL,
		.f_sint32 = -12345,
		.f_sint64 = -123456789012345LL,
		.f_sfixed32 = -1,
		.f_sfixed64 = -1,
		.f_enum = TYPELAB_SFLAG_NEG,
		.f_float = 3.5f,
		.f_double = -2.25,
		.f_bool = true,
	};
	struct typelab_config resp = {0};
	struct typelab_config readback = {0};

	req.f_bytes.size = sizeof(bytes_in);
	memcpy(req.f_bytes.bytes, bytes_in, sizeof(bytes_in));
	memcpy(req.f_fixed_bytes, fixed_bytes_in, sizeof(fixed_bytes_in));
	strcpy(req.f_string, "hello");

	zassert_ok(typelab_config(&typelab_test, &req, &resp, K_MSEC(100)));
	zassert_ok(typelab_get_config(&typelab_test, &readback, K_MSEC(100)));

	zassert_equal(readback.f_uint32, 4000000000U);
	zassert_equal(readback.f_uint64, 18000000000000000000ULL);
	zassert_equal(readback.f_fixed32, 0x12345678U);
	zassert_equal(readback.f_fixed64, 0x1122334455667788ULL);
	zassert_equal(readback.f_uenum, TYPELAB_UFLAG_ONE);
	zassert_equal(readback.f_int32, -2000000000);
	zassert_equal(readback.f_int64, -9000000000000000000LL);
	zassert_equal(readback.f_sint32, -12345);
	zassert_equal(readback.f_sint64, -123456789012345LL);
	zassert_equal(readback.f_sfixed32, -1);
	zassert_equal(readback.f_sfixed64, -1);
	zassert_equal(readback.f_enum, TYPELAB_SFLAG_NEG);
	zassert_within(readback.f_float, 3.5f, 0.0001f);
	zassert_within(readback.f_double, -2.25, 0.0001);
	zassert_true(readback.f_bool);
	zassert_equal(readback.f_bytes.size, sizeof(bytes_in));
	zassert_mem_equal(readback.f_bytes.bytes, bytes_in, sizeof(bytes_in));
	zassert_mem_equal(readback.f_fixed_bytes, fixed_bytes_in, sizeof(fixed_bytes_in));
	zassert_str_equal(readback.f_string, "hello");
}
