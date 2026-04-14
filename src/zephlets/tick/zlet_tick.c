#include "zlet_tick_interface.h"

#include "zlet_tick.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

void zlet_tick_timer_handler(struct k_timer *timer_id)
{
	LOG_DBG("tick!");

	struct tick_events events = {.timestamp = k_uptime_get(), .has_tick = true};

	/* Async event - no result */
	zlet_tick_report_events_async(&events, K_NO_WAIT);
}

K_TIMER_DEFINE(timer_zlet_tick, zlet_tick_timer_handler, NULL);

#ifdef CONFIG_ZTEST
static bool test_is_ready = true;

void zlet_tick_test_set_ready(bool ready)
{
	test_is_ready = ready;
}
#endif

/* Called by init_fn during SYS_INIT - sets is_ready on success */
static int zlet_tick_init(const struct zephlet *zephlet)
{
	struct zlet_tick_data *data = zephlet->data;

	int ret = 0;

	K_SPINLOCK(&data->lock) {
		/* Initialize timer (already done by K_TIMER_DEFINE) */
#ifdef CONFIG_ZTEST
		data->status.is_ready = test_is_ready;
#else
		data->status.is_ready = true;
#endif
	}

	return ret;
}

static int start(struct zlet_tick_context *ctx)
{
	struct zlet_tick_data *data = ctx->zephlet->data;
	int delay;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_ready) {
			ret = -ENODEV;
		} else if (data->status.is_running) {
			ret = -EALREADY;
		} else {
			delay = data->config.delay_ms;
		}
		ctx->response.status = data->status;
	}

	if (ret == 0) {
		k_timer_start(&timer_zlet_tick, K_MSEC(delay), K_MSEC(delay));

		K_SPINLOCK(&data->lock) {
			data->status.is_running = true;
			ctx->response.status = data->status;
		}

		LOG_DBG("Zephlet started with delay %d ms!", delay);
	}

	return ret;
}

static int stop(struct zlet_tick_context *ctx)
{
	struct zlet_tick_data *data = ctx->zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = false;
		}
		ctx->response.status = data->status;
	}

	if (ret == 0) {
		k_timer_stop(&timer_zlet_tick);
		LOG_DBG("Zephlet stopped!");
	}

	return ret;
}

static int get_status(struct zlet_tick_context *ctx)
{
	struct zlet_tick_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.status = data->status;
	}

	return 0;
}

static int config(struct zlet_tick_context *ctx, const struct tick_config *new_config)
{
	struct zlet_tick_data *data = ctx->zephlet->data;

	/* Validate config */
	if (new_config->delay_ms == 0) {
		return -EINVAL;
	}

	K_SPINLOCK(&data->lock) {
		data->config = *new_config;
	}

	ctx->response.config = *new_config;
	return 0;
}

static int get_config(struct zlet_tick_context *ctx)
{
	struct zlet_tick_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.config = data->config;
	}

	return 0;
}

/* RPC returns Tick.Events - publish to report field: events */
static int get_events(struct zlet_tick_context *ctx)
{
	struct zlet_tick_data *data = ctx->zephlet->data;

	K_SPINLOCK(&data->lock) {
		ctx->response.events = data->events;
	}

	return 0;
}

static struct zlet_tick_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
};

static struct zlet_tick_data data = {.config = {.delay_ms = 1000},  /* Default 1s */
				     .status = ZEPHLET_STATUS_INIT_ZERO,
				     .events = TICK_EVENTS_INIT_ZERO};

int zlet_tick_init_fn(const struct zephlet *self)
{
	/* Initialize zephlet resources */
	int ret = zlet_tick_init(self);

	/* Register implementation */
	if (ret == 0) {
		ret = zlet_tick_set_implementation(self);
	}

	printk("   -> %s %sinitialized\n", self->name, ret == 0 ? "" : "not ");

	return ret;
}

ZEPHLET_DEFINE(zlet_tick, zlet_tick_init_fn, &api, &data);
