#include "tamper_detection/tamper_detection_service.pb.h"
#include "tick_service.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

static void default_adapter(const struct zbus_channel *chan, const void *msg);

ZBUS_ASYNC_LISTENER_DEFINE(alis_debug_adapter, default_adapter);

#if defined(CONFIG_TAMPER_DETECTION_SERVICE)
#include "tamper_detection_service.h"
void tamper_detection_service_on_status_changed(const struct zbus_channel *chan, const void *msg)
{
	if (chan != &chan_tamper_detection_service_report) {
		return;
	}

	const struct msg_tamper_detection_service_report *tamper_detection_report = msg;

	switch (tamper_detection_report->which_tamper_detection_report) {
	case MSG_TAMPER_DETECTION_SERVICE_REPORT_STATUS_TAG:
		LOG_DBG("Tamper Detection service is %s!",
			tamper_detection_report->status.is_running ? "running" : "stopped");
		break;
	default:
		break;
	}
}
ZBUS_CHAN_ADD_OBS(chan_tamper_detection_service_report, alis_debug_adapter, 3);
#else
void tamper_detection_service_on_status_changed(const struct zbus_channel *chan, const void *msg)
{
}
#endif

#if defined(CONFIG_TICK_SERVICE)
#include "tick_service.h"
void tick_service_on_status_changed(const struct zbus_channel *chan, const void *msg)
{
	if (chan != &chan_tick_service_report) {
		return;
	}

	const struct msg_tick_service_report *tick_report = msg;

	switch (tick_report->which_tick_report) {
	case MSG_TICK_SERVICE_REPORT_STATUS_TAG:
		LOG_DBG("Tick service %s!", tick_report->status.is_running ? "running" : "stopped");
		break;
	default:
		break;
	}
}
ZBUS_CHAN_ADD_OBS(chan_tick_service_report, alis_debug_adapter, 3);
#else
void tick_service_on_status_changed(const struct zbus_channel *chan, const void *msg)
{
}
#endif

static void default_adapter(const struct zbus_channel *chan, const void *msg)
{
	tamper_detection_service_on_status_changed(chan, msg);
	tick_service_on_status_changed(chan, msg);
}
