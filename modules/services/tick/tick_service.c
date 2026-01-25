/* GENERATED FILE - DO NOT EDIT */
/* Generated from tick_service.proto */

#include "service.pb.h"
#include "tick_service.h"
#include "tick/tick_service.pb.h"
#include "zephyr/init.h"
#include "zephyr/spinlock.h"
#include "zephyr/sys/check.h"
#include "zephyr/zbus/zbus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sys/errno.h>

LOG_MODULE_REGISTER(tick_service, CONFIG_TICK_SERVICE_LOG_LEVEL);

ZBUS_CHAN_DEFINE(chan_tick_service_invoke, struct msg_tick_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(lis_tick_service), MSG_TICK_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_tick_service_report, struct msg_tick_service_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_TICK_SERVICE_REPORT_INIT_DEFAULT);

static const struct service *impl = NULL;

int tick_service_set_implementation(const struct service *implementation)
{
	int ret = -EBUSY;

	if (impl == NULL) {
		impl = implementation;
		ret = 0;
	} else {
		LOG_ERR("Trying to replace implementation for Tick Service");
	}

	return ret;
}

static void api_handler(const struct zbus_channel *chan)
{
	struct tick_service_api *api;
	const struct msg_tick_service_invoke *ivk;

	if (impl == NULL) {
		LOG_ERR("Service implementation required!");
		return;
	}

	if (impl->api == NULL) {
		LOG_ERR("Service API required!");
		return;
	}

	api = impl->api;
	ivk = zbus_chan_const_msg(chan);

	switch (ivk->which_tick_invoke) {
	case MSG_TICK_SERVICE_INVOKE_START_TAG:
		if (api->start) {
			api->start(impl);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_STOP_TAG:
		if (api->stop) {
			api->stop(impl);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_GET_STATUS_TAG:
		if (api->get_status) {
			api->get_status(impl);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_CONFIG_TAG:
		if (api->config) {
			api->config(impl, &ivk->config);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_GET_CONFIG_TAG:
		if (api->get_config) {
			api->get_config(impl);
		}
		break;
	}
}

ZBUS_LISTENER_DEFINE(lis_tick_service, api_handler);