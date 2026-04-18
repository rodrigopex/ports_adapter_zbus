#include "zlet_ui.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_ui, CONFIG_ZEPHLET_UI_LOG_LEVEL);

int ui_on_start(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct ui_data *d = z->data;

	if (!d->is_ready) {
		return -ENODEV;
	}
	if (d->is_running) {
		return -EALREADY;
	}
	d->is_running = true;
	if (resp != NULL) {
		resp->is_running = true;
		resp->is_ready = true;
	}
	LOG_DBG("%s: started", z->name);
	return 0;
}

int ui_on_stop(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct ui_data *d = z->data;

	if (!d->is_running) {
		return -EALREADY;
	}
	d->is_running = false;
	if (resp != NULL) {
		resp->is_running = false;
		resp->is_ready = d->is_ready;
	}
	LOG_DBG("%s: stopped", z->name);
	return 0;
}

int ui_on_get_status(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct ui_data *d = z->data;

	if (resp != NULL) {
		resp->is_running = d->is_running;
		resp->is_ready = d->is_ready;
	}
	return 0;
}

int ui_on_config(const struct zephlet *z, const struct ui_config *req, struct ui_config *resp)
{
	struct ui_data *d = z->data;

	d->current_config = *req;
	if (resp != NULL) {
		*resp = d->current_config;
	}
	return 0;
}

int ui_on_get_config(const struct zephlet *z, struct ui_config *resp)
{
	struct ui_data *d = z->data;

	if (resp != NULL) {
		*resp = d->current_config;
	}
	return 0;
}

int ui_on_blink(const struct zephlet *z)
{
	struct ui_data *d = z->data;

	d->blink_counter++;
	struct ui_events ev = {
		.timestamp = (int32_t)k_uptime_get(),
		.blink = d->blink_counter,
	};
	LOG_INF("%s: blink #%u", z->name, d->blink_counter);
	return ui_emit(z, &ev, K_NO_WAIT);
}

int ui_init_fn(const struct zephlet *z)
{
	struct ui_data *d = z->data;
	const struct ui_config *cfg = z->config;

	if (cfg != NULL) {
		d->current_config = *cfg;
	}
	d->is_running = false;
	d->is_ready = true;
	d->blink_counter = 0;

	LOG_INF("%s: init", z->name);
	return 0;
}
