#include "zlet_tick_interface.h"
#include "zlet_ui_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

static void tick_to_ui_adapter(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_zlet_tick_report *tick_report = zbus_chan_const_msg(chan);

	switch (tick_report->which_tick_report) {
	case MSG_ZLET_TICK_REPORT_STATUS_TAG:
		LOG_DBG("Received Tick status. Discarding");
		break;

	case MSG_ZLET_TICK_REPORT_CONFIG_TAG:
		LOG_DBG("Received Tick config. Discarding");
		break;

	case MSG_ZLET_TICK_REPORT_EVENTS_TAG:
		/* Events are typically async (no context) */
		if (tick_report->events.has_tick) {
			LOG_DBG("Received Tick event at %d, invoking UI blink",
				tick_report->events.timestamp);
			/* Start new request chain (no correlation) */
			zlet_ui_blink(0, K_NO_WAIT);
		}
		break;

	default:
		LOG_WRN("Received unexpected Tick report type");
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_tick_to_ui_adapter, tick_to_ui_adapter);

ZBUS_CHAN_ADD_OBS(chan_zlet_tick_report, lis_tick_to_ui_adapter, 3);
