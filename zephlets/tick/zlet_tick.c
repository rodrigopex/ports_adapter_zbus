#include "zlet_tick_interface.h"

#include "zlet_tick.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

void zlet_tick_timer_handler(struct k_timer *timer_id)
{
	LOG_DBG("tick!");

	struct msg_zlet_tick_events events = {.timestamp = k_uptime_get(), .has_tick = true};

	zlet_tick_report_events(&events, K_NO_WAIT);
}

K_TIMER_DEFINE(timer_zlet_tick, zlet_tick_timer_handler, NULL);

static int start(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int delay;

	K_SPINLOCK(&data->lock) {
		delay = data->config.delay_ms;
		data->status.is_running = true;
		status = data->status;
	}

	k_timer_start(&timer_zlet_tick, K_MSEC(delay), K_MSEC(delay));
	LOG_DBG("Zephlet started with delay %d ms!", delay);

	return zlet_tick_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
		status = data->status;
	}

	k_timer_stop(&timer_zlet_tick);

	LOG_DBG("Zephlet stopped!");

	return zlet_tick_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_tick_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_zlet_tick_config *new_config)
{
	struct zlet_tick_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* Safely set the zephlet config */
		data->config = *new_config;
	}

	return zlet_tick_report_config(new_config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;
	struct msg_zlet_tick_config config;
	K_SPINLOCK(&data->lock) {
		/* Safely set the zephlet config */
		config = data->config;
	}

	return zlet_tick_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgZletTick.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;
	struct msg_zlet_tick_events events;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_tick_report_events(&events, K_MSEC(250));
}

static struct zlet_tick_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
};

static struct zlet_tick_data data = {.config = MSG_ZLET_TICK_CONFIG_INIT_ZERO,
				     .status = MSG_ZEPHLET_STATUS_INIT_ZERO,
				     .events = MSG_ZLET_TICK_EVENTS_INIT_ZERO};

int zlet_tick_init_fn(const struct zephlet *self)
{
	int err = zlet_tick_set_implementation(self);

	printk("   -> %s initialed with%s error\n", self->name, err == 0 ? " no" : "");

	return err;
}

ZEPHLET_DEFINE(zlet_tick, zlet_tick_init_fn, &api, &data);
