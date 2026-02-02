#include "tick/tick_zephlet.pb.h"
#include "tick_zephlet_interface.h"

#include "tick_zephlet.h"
#include "zephyr/kernel.h"
#include "zephyr/zbus/zbus.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(tick_zephlet, CONFIG_TICK_ZEPHLET_LOG_LEVEL);

void tick_zephlet_handler(struct k_timer *timer_id)
{
	LOG_DBG("tick!");

	struct msg_tick_zephlet_events events = {.has_tick = true, .tick = k_uptime_get()};

	tick_zephlet_report_events(&events, K_NO_WAIT);
}

K_TIMER_DEFINE(timer_tick_zephlet, tick_zephlet_handler, NULL);

static int start(const struct zephlet *zephlet)
{
	struct tick_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int delay;

	K_SPINLOCK(&data->lock) {
		delay = data->config.delay_ms;
		data->status.is_running = true;
		status = data->status;
	}

	k_timer_start(&timer_tick_zephlet, K_MSEC(delay), K_MSEC(delay));
	LOG_DBG("Zephlet started with delay %d ms!", delay);

	return tick_zephlet_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct tick_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
		status = data->status;
	}

	k_timer_stop(&timer_tick_zephlet);

	LOG_DBG("Zephlet stopped!");

	return tick_zephlet_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct tick_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return tick_zephlet_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_tick_zephlet_config *new_config)
{
	struct tick_zephlet_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* Safely set the zephlet config */
		data->config = *new_config;
	}

	return tick_zephlet_report_config(new_config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct tick_zephlet_data *data = zephlet->data;
	struct msg_tick_zephlet_config config;
	K_SPINLOCK(&data->lock) {
		/* Safely set the zephlet config */
		config = data->config;
	}

	return tick_zephlet_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgBatteryZephlet.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct tick_zephlet_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_events logic */
	}
	/* TODO: Set up K_TIMER_DEFINE/K_WORK_DEFINE and call report helper */
	return 0;
}

static struct tick_zephlet_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
};

static struct tick_zephlet_data data = {.config = MSG_TICK_ZEPHLET_CONFIG_INIT_ZERO,
					.status = {.is_running = false}};

int tick_zephlet_init_fn(const struct zephlet *self)
{
	int err = tick_zephlet_set_implementation(self);

	printk("   -> %s initialed with%s error\n", self->name, err == 0 ? " no" : "");

	return err;
}

ZEPHLET_DEFINE(tick_zephlet, tick_zephlet_init_fn, &api, &data);
