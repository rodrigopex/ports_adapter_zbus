#include "zlet_tick.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

static void tick_timer_handler(struct k_timer *timer_id)
{
	const struct zephlet *z = k_timer_user_data_get(timer_id);
	struct tick_events ev = {
		.timestamp = (int32_t)k_uptime_get(),
	};

	(void)tick_emit(z, &ev, K_NO_WAIT);
}

static int validate_config(const struct tick_config *c)
{
	if (c->period_ms == 0) {
		return -EINVAL;
	}
	if (c->max_period_ms != 0 && c->period_ms > c->max_period_ms) {
		return -EINVAL;
	}
	return 0;
}

/* ----- Strong handler overrides --------------------------------------- */

int tick_on_start(const struct zephlet *z, struct zephlet_status *resp)
{
	struct tick_data *d = z->data;

	if (!d->is_ready) {
		if (resp != NULL) {
			resp->is_running = false;
			resp->is_ready = false;
		}
		return -ENODEV;
	}
	if (d->is_running) {
		if (resp != NULL) {
			resp->is_running = true;
			resp->is_ready = true;
		}
		return -EALREADY;
	}

	k_timer_start(&d->timer, K_MSEC(d->current_config.period_ms),
		      K_MSEC(d->current_config.period_ms));
	d->is_running = true;

	if (resp != NULL) {
		resp->is_running = true;
		resp->is_ready = true;
	}
	LOG_DBG("%s: started (period=%u ms)", z->name, d->current_config.period_ms);
	return 0;
}

int tick_on_stop(const struct zephlet *z, struct zephlet_status *resp)
{
	struct tick_data *d = z->data;

	if (!d->is_running) {
		if (resp != NULL) {
			resp->is_running = false;
			resp->is_ready = d->is_ready;
		}
		return -EALREADY;
	}

	k_timer_stop(&d->timer);
	d->is_running = false;

	if (resp != NULL) {
		resp->is_running = false;
		resp->is_ready = d->is_ready;
	}
	LOG_DBG("%s: stopped", z->name);
	return 0;
}

int tick_on_get_status(const struct zephlet *z, struct zephlet_status *resp)
{
	struct tick_data *d = z->data;

	if (resp != NULL) {
		resp->is_running = d->is_running;
		resp->is_ready = d->is_ready;
	}
	return 0;
}

int tick_on_config(const struct zephlet *z, const struct tick_config *req,
		   struct tick_config *resp)
{
	struct tick_data *d = z->data;
	int err = validate_config(req);

	if (err != 0) {
		if (resp != NULL) {
			*resp = d->current_config;
		}
		return err;
	}

	d->current_config = *req;

	if (d->is_running) {
		k_timer_stop(&d->timer);
		k_timer_start(&d->timer, K_MSEC(d->current_config.period_ms),
			      K_MSEC(d->current_config.period_ms));
	}

	if (resp != NULL) {
		*resp = d->current_config;
	}
	LOG_DBG("%s: reconfigured (period=%u ms)", z->name, d->current_config.period_ms);
	return 0;
}

int tick_on_get_config(const struct zephlet *z, struct tick_config *resp)
{
	struct tick_data *d = z->data;

	if (resp != NULL) {
		*resp = d->current_config;
	}
	return 0;
}

/* ----- Init fn --------------------------------------------------------- */

int tick_init_fn(const struct zephlet *self)
{
	struct tick_data *d = self->data;
	const struct tick_config *cfg = self->config;

	if (cfg != NULL) {
		d->current_config = *cfg;
	} else {
		d->current_config.period_ms = 1000;
		d->current_config.max_period_ms = 60000;
	}

	k_timer_init(&d->timer, tick_timer_handler, NULL);
	k_timer_user_data_set(&d->timer, (void *)self);

	d->is_running = false;
	d->is_ready = true;

	LOG_INF("%s: init (period=%u ms)", self->name, d->current_config.period_ms);
	return 0;
}
