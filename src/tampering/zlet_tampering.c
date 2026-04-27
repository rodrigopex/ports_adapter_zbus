#include "zlet_tampering.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tampering, CONFIG_ZEPHLET_TAMPERING_LOG_LEVEL);

int tampering_start_impl(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct tampering_data *d = z->data;

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

int tampering_stop_impl(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct tampering_data *d = z->data;

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

int tampering_get_status_impl(const struct zephlet *z, struct lifecycle_status *resp)
{
	struct tampering_data *d = z->data;

	if (resp != NULL) {
		resp->is_running = d->is_running;
		resp->is_ready = d->is_ready;
	}
	return 0;
}

int tampering_config_impl(const struct zephlet *z, const struct tampering_config *req,
			  struct tampering_config *resp)
{
	struct tampering_config *cfg = z->config;

	*cfg = *req;
	if (resp != NULL) {
		*resp = *cfg;
	}
	return 0;
}

int tampering_get_config_impl(const struct zephlet *z, struct tampering_config *resp)
{
	struct tampering_config *cfg = z->config;

	if (resp != NULL) {
		*resp = *cfg;
	}
	return 0;
}

int tampering_force_tampering_impl(const struct zephlet *z)
{
	struct tampering_events ev = {
		.timestamp = (int32_t)k_uptime_get(),
		.proximity_tamper_detected = true,
	};
	LOG_DBG("%s: force_tampering", z->name);
	return tampering_emit(z, &ev, K_NO_WAIT);
}

int tampering_init_fn(const struct zephlet *z)
{
	struct tampering_data *d = z->data;

	d->is_running = false;
	d->is_ready = true;

	LOG_INF("%s: init", z->name);
	return 0;
}
