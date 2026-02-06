/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_ui_interface.h"

#include "zlet_ui.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_ui, CONFIG_ZEPHLET_UI_LOG_LEVEL);

/* Blink counter */
static uint32_t blink_count = 0;
static struct k_spinlock blink_lock;

/* Called by init_fn during SYS_INIT - sets is_ready on success */
static int zlet_ui_init(const struct zephlet *zephlet)
{
	struct zlet_ui_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		/* TODO: Initialize zephlet resources (one-time setup) */
		if (ret == 0) {
			data->status.is_ready = true;
		}
	}

	return ret;
}

static int start(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_ui_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_ready) {
			ret = -ENODEV;
		} else if (data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = true;
			/* TODO: Start zephlet resources (timers, threads, etc.) */
		}
		status = data->status;
	}

	return zlet_ui_report_status(context, ret, &status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_ui_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = false;
			/* TODO: Stop zephlet resources (timers, threads, etc.) */
		}
		status = data->status;
	}

	return zlet_ui_report_status(context, ret, &status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_ui_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_ui_report_status(context, ret, &status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_api_context *context, const struct msg_zlet_ui_config *config)
{
	struct zlet_ui_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
		/* TODO: Apply configuration changes to zephlet resources */
	}

	return zlet_ui_report_config(context, ret, config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_ui_data *data = zephlet->data;
	struct msg_zlet_ui_config config;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_ui_report_config(context, ret, &config, K_MSEC(250));
}

/* RPC returns MsgZletUi.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_ui_data *data = zephlet->data;
	struct msg_zlet_ui_events events;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_ui_report_events(context, ret, &events, K_MSEC(250));
}

/* RPC returns Empty - triggered by adapter (no direct response needed) */
static int blink(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct msg_zlet_ui_events events = {0};

	K_SPINLOCK(&blink_lock) {
		blink_count++;
		events.blink = blink_count;
	}

	LOG_INF("LED blink #%u", events.blink);

	/* Async event report (no context correlation) */
	return zlet_ui_report_events_async(&events, K_MSEC(250));
}

static struct zlet_ui_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
	.blink = blink,
};

static struct zlet_ui_data data = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = MSG_ZLET_UI_CONFIG_INIT_ZERO,
	.events = MSG_ZLET_UI_EVENTS_INIT_ZERO
};

int zlet_ui_init_fn(const struct zephlet *self)
{
	int err;

	/* Initialize zephlet resources */
	err = zlet_ui_init(self);

	/* Register implementation */
	if (err == 0) {
		err = zlet_ui_set_implementation(self);
	}

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_ui, zlet_ui_init_fn, &api, &data);
