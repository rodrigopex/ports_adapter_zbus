/*
 * Tampering+Ui_zlet_adapter_impl.c
 * Adapter implementation: zlet_tampering -> zlet_ui
 * Implement callbacks for selected report fields below.
 */

#include "zlet_tampering_interface.h"
#include "zlet_ui_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

void tampering_to_ui_on_report_events(const struct zbus_channel *chan,
				      const struct tampering_report *report)
{
	if (report && report->events.contact_tamper_detected) {
		LOG_DBG("Received tampering event at %d, invoking UI blink",
			report->events.timestamp);
		zlet_ui_blink(K_MSEC(100));
	}
}
