/* GENERATED FILE - DO NOT EDIT */
/* Generated from battery_service.proto */

#include "battery_service.h"
#include <zephyr/zbus/zbus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(battery_service, CONFIG_BATTERY_SERVICE_LOG_LEVEL);

ZBUS_CHAN_DEFINE(chan_battery_service_invoke, struct msg_battery_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(lis_battery_service), MSG_BATTERY_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_battery_service_report, struct msg_battery_service_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_BATTERY_SERVICE_REPORT_INIT_DEFAULT);

static const struct service *impl = NULL;

int battery_service_set_implementation(const struct service *implementation)
{
	int ret = -EBUSY;

	if (impl == NULL) {
		impl = implementation;
		ret = 0;
	} else {
		LOG_ERR("Trying to replace implementation for Battery Service");
	}

	return ret;
}

static void api_handler(const struct zbus_channel *chan)
{
	struct battery_service_api *api;
	const struct msg_battery_service_invoke *ivk;

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

	switch (ivk->which_battery_invoke) {
	case MSG_BATTERY_SERVICE_INVOKE_START_TAG:
		if (api->start) {
			api->start(impl);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_STOP_TAG:
		if (api->stop) {
			api->stop(impl);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_GET_STATUS_TAG:
		if (api->get_status) {
			api->get_status(impl);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_CONFIG_TAG:
		if (api->config) {
			api->config(impl, &ivk->config);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_GET_CONFIG_TAG:
		if (api->get_config) {
			api->get_config(impl);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_GET_BATTERY_STATE_TAG:
		if (api->get_battery_state) {
			api->get_battery_state(impl);
		}
		break;
	case MSG_BATTERY_SERVICE_INVOKE_GET_EVENTS_TAG:
		if (api->get_events) {
			api->get_events(impl);
		}
		break;
	}
}

ZBUS_LISTENER_DEFINE(lis_battery_service, api_handler);