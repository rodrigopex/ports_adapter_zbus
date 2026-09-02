/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <nanopb_textformat.h>

#include "zephlet_textformat.h"
#include "zlet_tick.h"
#include "zlet_ui.h"
#include "zlet_tampering.h"
#include "zlet_typelab.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* ----- Printing proto structs ------------------------------------------
 *
 * Rather than hand-writing a format string per message -- which has to be
 * kept in step with the .proto by hand, and quietly goes stale when a
 * field is added -- print the struct as protobuf text format. The
 * descriptors come from codegen (`PB_TF_DEFINE` per message in
 * `<prefix>_interface.c`, declared in its header); `lifecycle_status_t_tf`
 * is the shared zephlet.proto one, from `zephlet_textformat.h`.
 *
 * Output style is the library's own Kconfig choice; this app picks compact
 * (see prj.conf), so a message prints on one line.
 * `pb_tf_print_buf()` is preferred over
 * `pb_tf_print()` with a character sink: it terminates the buffer even when
 * a message prints nothing, and reports truncation as PB_TF_ERR_NO_SPACE
 * rather than a flag to remember to check.
 */
static void print_proto(const char *label, const struct pb_tf_msg *tf, const void *msg)
{
	/* Static, not a local: main's stack is generous but the library's own
	 * guidance is to keep the output buffer off it. Only ever called from
	 * this thread, sequentially. */
	static char text[192];
	enum pb_tf_err err = pb_tf_print_buf(tf, msg, text, sizeof(text));

	if (err != PB_TF_OK) {
		printk("%s: cannot print: %s\n", label, pb_tf_strerror(err));
		return;
	}

	/* Style is a compile-time choice, and the two need different framing:
	 * multi-line already terminates its last field with a newline, while
	 * compact emits one line and no terminator. The unused branch compiles
	 * out. */
	if (IS_ENABLED(CONFIG_NANOPB_TEXTFORMAT_PRINT_COMPACT)) {
		printk("%s: %s\n", label, text);
	} else {
		printk("%s:\n%s", label, text);
	}
}

/* ----- Instance storage + initial config ------------------------------ */

static struct tick_config tick_cfg = {
	.duration_ms = 600,
	.period_ms = 1000,
};
static struct tick_data tick_data_storage;
ZEPHLET_NEW(tick, tick_timer_based_impl, &tick_cfg, &tick_data_storage, tick_init_fn);

static struct ui_config ui_cfg = {
	.user_button_long_press_duration = 1000,
};
static struct ui_data ui_data_storage;
ZEPHLET_NEW(ui, ui_fake_impl, &ui_cfg, &ui_data_storage, ui_init_fn);

static struct tampering_config tampering_cfg = {
	.light_tamper_threshold = 100,
	.proximity_tamper_threshold = 50,
};
static struct tampering_data tampering_data_storage;
ZEPHLET_NEW(tampering, tampering_emul_impl, &tampering_cfg, &tampering_data_storage,
	    tampering_init_fn);

/* Bench for exercising every shell-supported nanopb scalar type via
 * `zlet typelab_bench config ...` / `get_config` — see
 * src/typelab/zlet_typelab.proto. No policies wiring: it's a standalone
 * type-exercise fixture, not part of the tick/ui/tampering event chain. */
static struct typelab_config typelab_cfg;
static struct typelab_data typelab_data_storage;
ZEPHLET_NEW(typelab, typelab_bench, &typelab_cfg, &typelab_data_storage, typelab_init_fn);

int main(void)
{
	printk("Example project running on a %s board.\n", CONFIG_BOARD_TARGET);

	if (!tick_is_ready(&tick_timer_based_impl)) {
		LOG_ERR("Tick not ready");
		return -ENODEV;
	}
	if (!ui_is_ready(&ui_fake_impl)) {
		LOG_ERR("UI not ready");
		return -ENODEV;
	}
	if (!tampering_is_ready(&tampering_emul_impl)) {
		LOG_ERR("Tampering not ready");
		return -ENODEV;
	}

	struct tick_config tick_config_now = {0};
	if (tick_get_config(&tick_timer_based_impl, &tick_config_now, K_MSEC(500)) == 0) {
		print_proto("Tick config", &tick_config_t_tf, &tick_config_now);
	}

	struct tick_config tick_new = {.duration_ms = 3000, .period_ms = 1000};
	if (tick_config(&tick_timer_based_impl, &tick_new, NULL, K_MSEC(500)) == 0) {
		print_proto("Tick config updated", &tick_config_t_tf, &tick_new);
	}

	struct lifecycle_status st = {0};

	if (ui_start(&ui_fake_impl, &st, K_MSEC(500)) == 0) {
		print_proto("UI status", &lifecycle_status_t_tf, &st);
	}

	if (tick_alt_start(&tick_timer_based_impl, &st, K_MSEC(500)) == 0) {
		print_proto("Tick status", &lifecycle_status_t_tf, &st);
	}

	k_sleep(K_SECONDS(10));

	(void)tick_alt_stop(&tick_timer_based_impl, NULL, K_MSEC(500));

	(void)tampering_start(&tampering_emul_impl, NULL, K_FOREVER);
	(void)tampering_force_tampering(&tampering_emul_impl, K_MSEC(250));
	(void)tampering_stop(&tampering_emul_impl, NULL, K_FOREVER);

	(void)ui_stop(&ui_fake_impl, NULL, K_MSEC(500));
	return 0;
}
