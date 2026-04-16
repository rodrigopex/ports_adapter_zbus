#include "zlet_tick.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

struct tick_instance_data {
	struct k_timer timer;
};

static struct tick_instance_data inst_data;

static void tick_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	LOG_DBG("tick!");

	struct tick_events delta = {.has_tick = true};

	tick_events_update(NULL, &delta);
}

/* Strong hook overrides ------------------------------------------------- */

int zlet_tick_post_start(const struct zephlet *self)
{
	struct tick_instance_data *inst_data = self->instance_data;
	struct tick_settings s = tick_settings_clone();

	k_timer_start(&inst_data->timer, K_MSEC(s.delay_ms), K_MSEC(s.delay_ms));
	LOG_DBG("Zephlet started with delay %u ms!", s.delay_ms);
	return 0;
}

int zlet_tick_pre_stop(const struct zephlet *self)
{
	struct tick_instance_data *inst_data = self->instance_data;

	k_timer_stop(&inst_data->timer);
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
	struct tick_instance_data *inst_data = self->instance_data;
	struct zephlet_status status = tick_status_clone();

	if (status.is_running) {
		k_timer_stop(&inst_data->timer);
		struct tick_settings s = tick_settings_clone();
		k_timer_start(&inst_data->timer, K_MSEC(s.delay_ms), K_MSEC(s.delay_ms));
	}
	return 0;
}

static int tick_init_fn(const struct zephlet *self)
{
	struct tick_settings defaults = {
		.has_delay_ms = true,
		.delay_ms = 2000,
		.has_max_delay_ms = true,
		.max_delay_ms = 60000,
	};

	int err = zlet_tick_set_implementation(self);
	if (err != 0) {
		printk("   -> %s not initialized\n", self->config.name);
		return err;
	}

	struct tick_instance_data *inst_data = self->instance_data;

	tick_settings_init(&defaults);
	k_timer_init(&inst_data->timer, tick_timer_handler, NULL);

	printk("   -> %s initialized\n", self->config.name);
	return 0;
}

ZEPHLET_IMPL_REGISTER(tick, tick_init_fn, NULL, &inst_data);
