/*
 * Tick+Ui_zlet_adapter_impl.c
 * Adapter implementation: zlet_tick -> zlet_ui
 */

#include "zlet_tick_interface.h"
#include "zlet_ui_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

void tick_to_ui_on_report_events(const struct zbus_channel *chan, const struct tick_report *report)
{
	if (report && report->events.has_tick) {
		LOG_DBG("Received Tick event at %d, invoking UI blink", report->events.timestamp);
		zlet_ui_blink(K_MSEC(100));
	}
}

void tick_to_ui_on_report_status(const struct zbus_channel *chan, const struct tick_report *report)
{
	LOG_DBG("Tick status: running=%d, ready=%d", report->status.is_running,
		report->status.is_ready);
}
