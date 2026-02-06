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
		/* Check if this is a response (vs async event) */
		if (tick_report->has_context) {
			uint32_t correlation_id = tick_report->context.correlation_id;
			int return_code = tick_report->context.return_code;

			if (return_code < 0) {
				LOG_WRN("tick status failed: %d", return_code);
			} else {
				LOG_DBG("tick status success (corr_id=%u, running=%d, ready=%d)",
					correlation_id,
					tick_report->status.is_running,
					tick_report->status.is_ready);
			}
		} else {
			LOG_DBG("Async status event from tick");
		}
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
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_tick_to_ui_adapter, tick_to_ui_adapter);

ZBUS_CHAN_ADD_OBS(chan_zlet_tick_report, lis_tick_to_ui_adapter, 3);
