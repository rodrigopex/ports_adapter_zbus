#include "zlet_ui_interface.h"

#include "zlet_ui.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_ui, CONFIG_ZEPHLET_UI_LOG_LEVEL);

static uint32_t blink_count;

#ifdef CONFIG_ZTEST
static bool test_is_ready = true;

void zlet_ui_test_set_ready(bool ready)
{
	test_is_ready = ready;
}
#endif

/* Called by init_fn during SYS_INIT - sets is_ready on success */
static int zlet_ui_init(const struct zephlet *zephlet)
{
	struct zlet_ui_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
#ifdef CONFIG_ZTEST
		data->status.is_ready = test_is_ready;
#else
		data->status.is_ready = true;
#endif
	}

	return ret;
}

static int start(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_ready) {
			ret = -ENODEV;
		} else if (data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = true;
		}
		ctx->response.status = data->status;
	}

	return ret;
}

static int stop(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = false;
		}
		ctx->response.status = data->status;
	}

	return ret;
}

static int get_status(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.status = data->status;
	}

	return 0;
}

static int config(struct zlet_ui_context *ctx, const struct ui_config *new_config)
{
	struct zlet_ui_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *new_config;
	}

	ctx->response.config = *new_config;
	return 0;
}

static int get_config(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.config = data->config;
	}

	return 0;
}

/* RPC returns Ui.Events - publish to report field: events */
static int get_events(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.events = data->events;
	}

	return 0;
}

static int blink(struct zlet_ui_context *ctx)
{
	struct zlet_ui_data *data = ctx->zephlet->data;

	++blink_count;

	struct ui_events events = {
		.has_blink = true, .blink = blink_count, .timestamp = k_uptime_get()};

	K_SPINLOCK(&data->lock) {
		data->events = events;
	}

	LOG_INF("LED blink #%u", events.blink);

	/* Async event for observers — use caller's remaining timeout */
	zlet_ui_report_events_async(&events, sys_timepoint_timeout(ctx->timeout));

	return 0;
}

static int buzzer_play(struct zlet_ui_context *ctx, enum ui_buzzer_tone buzzer_tone)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(buzzer_tone);

	/* TODO: Implement buzzer playback */

	return 0;
}

static struct zlet_ui_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
	.blink = blink,
	.buzzer_play = buzzer_play,
};

static struct zlet_ui_data data = {.status = ZEPHLET_STATUS_INIT_ZERO,
				   .config = UI_CONFIG_INIT_ZERO,
				   .events = UI_EVENTS_INIT_ZERO};

int zlet_ui_init_fn(const struct zephlet *self)
{
	int err = zlet_ui_init(self);

	if (err == 0) {
		err = zlet_ui_set_implementation(self);
	}

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_ui, zlet_ui_init_fn, &api, &data);
