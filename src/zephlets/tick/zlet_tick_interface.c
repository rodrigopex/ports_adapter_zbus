#include "zlet_tick_interface.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(zlet_tick, CONFIG_ZEPHLET_TICK_LOG_LEVEL);

/* ----- Weak default handlers (overridden in zlet_tick.c) -------------- */

__weak int tick_on_start(const struct zephlet *z, struct zephlet_status *resp)
{
	ARG_UNUSED(z);
	ARG_UNUSED(resp);
	return -ENOSYS;
}

__weak int tick_on_stop(const struct zephlet *z, struct zephlet_status *resp)
{
	ARG_UNUSED(z);
	ARG_UNUSED(resp);
	return -ENOSYS;
}

__weak int tick_on_get_status(const struct zephlet *z, struct zephlet_status *resp)
{
	ARG_UNUSED(z);
	ARG_UNUSED(resp);
	return -ENOSYS;
}

__weak int tick_on_config(const struct zephlet *z, const struct tick_config *req,
			  struct tick_config *resp)
{
	ARG_UNUSED(z);
	ARG_UNUSED(req);
	ARG_UNUSED(resp);
	return -ENOSYS;
}

__weak int tick_on_get_config(const struct zephlet *z, struct tick_config *resp)
{
	ARG_UNUSED(z);
	ARG_UNUSED(resp);
	return -ENOSYS;
}

/* ----- Trampolines: unpack zephlet_call -> handler --------------------- */

static int trampoline_start(const struct zephlet *z, struct zephlet_call *c)
{
	return tick_on_start(z, (struct zephlet_status *)c->resp);
}

static int trampoline_stop(const struct zephlet *z, struct zephlet_call *c)
{
	return tick_on_stop(z, (struct zephlet_status *)c->resp);
}

static int trampoline_get_status(const struct zephlet *z, struct zephlet_call *c)
{
	return tick_on_get_status(z, (struct zephlet_status *)c->resp);
}

static int trampoline_config(const struct zephlet *z, struct zephlet_call *c)
{
	return tick_on_config(z, (const struct tick_config *)c->req,
			      (struct tick_config *)c->resp);
}

static int trampoline_get_config(const struct zephlet *z, struct zephlet_call *c)
{
	return tick_on_get_config(z, (struct tick_config *)c->resp);
}

/* ----- Method table + api ---------------------------------------------- */

static const struct zephlet_method tick_methods[TICK_M__COUNT] = {
	[TICK_M_RESERVED_0] = {0},
	[TICK_M_START] = {
		.req_desc = NULL,
		.resp_desc = &zephlet_status_t_msg,
		.handler = trampoline_start,
	},
	[TICK_M_STOP] = {
		.req_desc = NULL,
		.resp_desc = &zephlet_status_t_msg,
		.handler = trampoline_stop,
	},
	[TICK_M_GET_STATUS] = {
		.req_desc = NULL,
		.resp_desc = &zephlet_status_t_msg,
		.handler = trampoline_get_status,
	},
	[TICK_M_CONFIG] = {
		.req_desc = &tick_config_t_msg,
		.resp_desc = &tick_config_t_msg,
		.handler = trampoline_config,
	},
	[TICK_M_GET_CONFIG] = {
		.req_desc = NULL,
		.resp_desc = &tick_config_t_msg,
		.handler = trampoline_get_config,
	},
};

const struct zephlet_api tick_api = {
	.methods = tick_methods,
	.num_methods = ARRAY_SIZE(tick_methods),
};

/* ----- Sync listener --------------------------------------------------- */

static void lis_tick_fn(const struct zbus_channel *chan)
{
	const struct zephlet *z = zbus_chan_user_data(chan);
	struct zephlet_call *call = *(struct zephlet_call *const *)zbus_chan_const_msg(chan);

	zephlet_dispatch(z, call);
}
ZBUS_LISTENER_DEFINE(lis_tick, lis_tick_fn);

/* ----- Wrappers -------------------------------------------------------- */

static int tick_sync_pub(const struct zephlet *z, struct zephlet_call *call, k_timeout_t timeout)
{
	struct zephlet_call *p = call;
	int err = zbus_chan_pub(z->channel.rpc, &p, timeout);
	if (err != 0) {
		return err;
	}
	return call->return_code;
}

int tick_start(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout)
{
	struct zephlet_call call = {
		.method_id = TICK_M_START,
		.resp_desc = &zephlet_status_t_msg,
		.resp = resp,
	};
	return tick_sync_pub(z, &call, timeout);
}

int tick_stop(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout)
{
	struct zephlet_call call = {
		.method_id = TICK_M_STOP,
		.resp_desc = &zephlet_status_t_msg,
		.resp = resp,
	};
	return tick_sync_pub(z, &call, timeout);
}

int tick_get_status(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout)
{
	struct zephlet_call call = {
		.method_id = TICK_M_GET_STATUS,
		.resp_desc = &zephlet_status_t_msg,
		.resp = resp,
	};
	return tick_sync_pub(z, &call, timeout);
}

int tick_config(const struct zephlet *z, const struct tick_config *req, struct tick_config *resp,
		k_timeout_t timeout)
{
	struct zephlet_call call = {
		.method_id = TICK_M_CONFIG,
		.req_desc = &tick_config_t_msg,
		.req = req,
		.resp_desc = &tick_config_t_msg,
		.resp = resp,
	};
	return tick_sync_pub(z, &call, timeout);
}

int tick_get_config(const struct zephlet *z, struct tick_config *resp, k_timeout_t timeout)
{
	struct zephlet_call call = {
		.method_id = TICK_M_GET_CONFIG,
		.resp_desc = &tick_config_t_msg,
		.resp = resp,
	};
	return tick_sync_pub(z, &call, timeout);
}

int tick_emit(const struct zephlet *z, const struct tick_events *ev, k_timeout_t timeout)
{
	return zbus_chan_pub(z->channel.events, ev, timeout);
}

bool tick_is_ready(const struct zephlet *z)
{
	struct zephlet_status status = {0};
	int rc = tick_get_status(z, &status, K_MSEC(100));

	return (rc == 0 && status.is_ready);
}
