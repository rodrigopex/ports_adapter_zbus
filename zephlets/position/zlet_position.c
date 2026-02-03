/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_position_interface.h"

#include "zlet_position.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_position, CONFIG_ZEPHLET_POSITION_LOG_LEVEL);

static struct {
	struct msg_zephlet_status status;
	struct msg_zlet_position_config config;
	struct msg_zlet_position_events events;
} self = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = MSG_ZLET_POSITION_CONFIG_INIT_ZERO,
	.events = MSG_ZLET_POSITION_EVENTS_INIT_ZERO,
};

/* TODO: Add zephlet-specific resources (timers, work queues, threads) */
static int start(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start zephlet resources (timers, threads, etc.) */

	return zlet_position_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	if (!status.is_running) {
		LOG_DBG("Zephlet has not started yet!");
	}

	/* TODO: Stop zephlet resources (timers, threads, etc.) */

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
	}

	return zlet_position_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_position_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_zlet_position_config *config)
{
	struct zlet_position_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to zephlet resources */

	return zlet_position_report_config(config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zlet_position_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_position_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgZletPosition.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zlet_position_events events;

	K_SPINLOCK(&data->lock) {
		events = self.events;
	}

	return zbus_chan_pub(&chan_zlet_position_report, &events, K_MSEC(250));
}

static struct zlet_position_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
};

static struct zlet_position_data data = {
	.config = MSG_ZLET_POSITION_CONFIG_INIT_ZERO,
	.status = {.is_running = false}
};

int zlet_position_init_fn(const struct zephlet *self)
{
	int err = zlet_position_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_position, zlet_position_init_fn, &api, &data);