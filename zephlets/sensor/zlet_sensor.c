/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_sensor_interface.h"

#include "zlet_sensor.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_sensor, CONFIG_ZEPHLET_SENSOR_LOG_LEVEL);

/* TODO: Add zephlet-specific resources (timers, work queues, threads) */
static int start(const struct zephlet *zephlet)
{
	struct zlet_sensor_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start zephlet resources (timers, threads, etc.) */

	return zlet_sensor_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct zlet_sensor_data *data = zephlet->data;
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

	return zlet_sensor_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct zlet_sensor_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_sensor_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_zlet_sensor_config *config)
{
	struct zlet_sensor_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to zephlet resources */

	return zlet_sensor_report_config(config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct zlet_sensor_data *data = zephlet->data;
	struct msg_zlet_sensor_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_sensor_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgZletSensor.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct zlet_sensor_data *data = zephlet->data;
	struct msg_zlet_sensor_events events;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_sensor_report_events(&events, K_MSEC(250));
}

static struct zlet_sensor_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
};

static struct zlet_sensor_data data = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = MSG_ZLET_SENSOR_CONFIG_INIT_ZERO,
	.events = MSG_ZLET_SENSOR_EVENTS_INIT_ZERO
};

int zlet_sensor_init_fn(const struct zephlet *self)
{
	int err = zlet_sensor_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_sensor, zlet_sensor_init_fn, &api, &data);