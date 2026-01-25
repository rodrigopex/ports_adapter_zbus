#include "tick/tick_service.pb.h"
#include "tick_service.h"
#include "ui_service.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

static void tick_to_ui_adapter(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_tick_service_report *tick_report = zbus_chan_const_msg(chan);

	switch (tick_report->which_tick_report) {
	case MSG_TICK_SERVICE_REPORT_STATUS_TAG:
		LOG_DBG("Discarded reported status");
		break;
	case MSG_TICK_SERVICE_REPORT_EVENTS_TAG:
		if (tick_report->events.has_tick) {
			LOG_DBG("Received Tick Service report at %lld, invoking UI Service blink",
				tick_report->events.tick);
			ui_service_blink(K_NO_WAIT);
		}
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_tick_to_ui_adapter, tick_to_ui_adapter);

ZBUS_CHAN_ADD_OBS(chan_tick_service_report, lis_tick_to_ui_adapter, 3);
