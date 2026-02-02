/*
 * PositionZephlet+BatteryZephlet_adapter.c
 * Auto-generated adapter from position to battery
 */

#include "zlet_position_interface.h"
#include "zlet_battery_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

static void position_to_battery_adapter(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_zlet_position_report *report = zbus_chan_const_msg(chan);

	switch (report->which_position_report) {
	case MSG_ZLET_POSITION_REPORT_EVENTS_TAG:
		/* TODO: Handle events report from position_zephlet */
		/* Available battery commands: start, stop, get_status, config, get_config, get_battery_state, get_events */
		/* Example: battery_zephlet_start(K_NO_WAIT); */
		LOG_DBG("Received events report from position_zephlet");
		break;
	default:
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_position_to_battery_adapter, position_to_battery_adapter);
ZBUS_CHAN_ADD_OBS(chan_zlet_position_report, lis_position_to_battery_adapter, 3);