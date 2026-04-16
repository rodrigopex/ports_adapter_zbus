#include "zlet_tick.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

static void zlet_tick_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	LOG_DBG("tick!");

	struct tick_events delta = {.has_tick = true};

	tick_events_update(NULL, &delta);
}

K_TIMER_DEFINE(timer_zlet_tick, zlet_tick_timer_handler, NULL);

static void tick_timer_arm(const struct tick_settings *s)
{
	k_timer_start(&timer_zlet_tick, K_MSEC(s->delay_ms), K_MSEC(s->delay_ms));
}

/* Strong hook overrides ------------------------------------------------- */

int zlet_tick_post_start(const struct zephlet *self)
{
	ARG_UNUSED(self);
	struct tick_settings s = tick_settings_clone();
	tick_timer_arm(&s);
	LOG_DBG("Zephlet started with delay %u ms!", s.delay_ms);
	return 0;
}

int zlet_tick_pre_stop(const struct zephlet *self)
{
	ARG_UNUSED(self);
	k_timer_stop(&timer_zlet_tick);
	LOG_DBG("Zephlet Tick stopped!");
	return 0;
}

int zlet_tick_validate_settings(const struct tick_settings *merged)
{
	if (merged->delay_ms == 0) {
		return -EINVAL;
	}
	if (merged->max_delay_ms != 0 && merged->delay_ms > merged->max_delay_ms) {
		return -EINVAL;
	}
	return 0;
}

int zlet_tick_post_update_settings(const struct zephlet *self)
{
	ARG_UNUSED(self);
	struct zephlet_status status = tick_status_clone();
	if (status.is_running) {
		k_timer_stop(&timer_zlet_tick);
		struct tick_settings s = tick_settings_clone();
		tick_timer_arm(&s);
	}
	return 0;
}

int zlet_tick_init_fn(const struct zephlet *self)
{
	struct tick_settings defaults = {
		.has_delay_ms = true,
		.delay_ms = 2000,
		.has_max_delay_ms = true,
		.max_delay_ms = 60000,
	};
	tick_settings_init(&defaults);

	int err = zlet_tick_set_implementation(self);
	if (err == 0) {
		zephlet_mark_ready(self);
	}
	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");
	return err;
}

ZEPHLET_DEFINE(zlet_tick, zlet_tick_init_fn, NULL);
