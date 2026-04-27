/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "zlet_tick.h"
#include "zlet_ui.h"
#include "zlet_tampering.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* ----- Instance storage + initial config ------------------------------ */

static struct tick_config tick_cfg = {
	.period_ms = 1000,
	.max_period_ms = 60000,
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
		printk("Tick config: period_ms=%u\n", tick_config_now.period_ms);
	}

	struct tick_config tick_new = {.period_ms = 1000, .max_period_ms = 60000};
	if (tick_config(&tick_timer_based_impl, &tick_new, NULL, K_MSEC(500)) == 0) {
		printk("Tick config updated, period_ms=%u\n", tick_new.period_ms);
	}

	struct lifecycle_status st = {0};

	if (ui_start(&ui_fake_impl, &st, K_MSEC(500)) == 0) {
		printk("UI is %srunning\n", st.is_running ? "" : "not ");
	}

	if (tick_start(&tick_timer_based_impl, &st, K_MSEC(500)) == 0) {
		printk("Tick is %srunning\n", st.is_running ? "" : "not ");
	}

	k_sleep(K_SECONDS(5));

	(void)tick_stop(&tick_timer_based_impl, NULL, K_MSEC(500));

	(void)tampering_start(&tampering_emul_impl, NULL, K_FOREVER);
	(void)tampering_force_tampering(&tampering_emul_impl, K_MSEC(250));
	(void)tampering_stop(&tampering_emul_impl, NULL, K_FOREVER);

	(void)ui_stop(&ui_fake_impl, NULL, K_MSEC(500));
	return 0;
}
