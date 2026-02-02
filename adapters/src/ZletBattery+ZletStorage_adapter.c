/*
 * BatteryZephlet+StorageZephlet_adapter.c
 * Auto-generated adapter from battery to storage
 */

#include "battery/zlet_battery.pb.h"
#include "zlet_battery_interface.h"
#include "zlet_storage_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(adapter, CONFIG_ADAPTERS_LOG_LEVEL);

static void battery_to_storage_adapter(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_zlet_battery_report *report = zbus_chan_const_msg(chan);

	switch (report->which_battery_report) {
	case MSG_ZLET_BATTERY_REPORT_STATUS_TAG:
		/* TODO: Handle status report from battery_zephlet */
		/* Available storage commands: start, stop, get_status, config, get_config, get_events */
		/* Example: storage_zephlet_start(K_NO_WAIT); */
		LOG_DBG("Received status report from battery_zephlet");
		break;
	case MSG_ZLET_BATTERY_REPORT_CONFIG_TAG:
		/* TODO: Handle config report from battery_zephlet */
		/* Available storage commands: start, stop, get_status, config, get_config, get_events */
		/* Example: storage_zephlet_start(K_NO_WAIT); */
		LOG_DBG("Received config report from battery_zephlet");
		break;
	case MSG_ZLET_BATTERY_REPORT_BATTERY_STATE_TAG:
		/* TODO: Handle battery_state report from battery_zephlet */
		/* Available storage commands: start, stop, get_status, config, get_config, get_events */
		/* Example: storage_zephlet_start(K_NO_WAIT); */
		LOG_DBG("Received battery_state report from battery_zephlet");
		break;
	case MSG_ZLET_BATTERY_REPORT_EVENTS_TAG:
		/* TODO: Handle events report from battery_zephlet */
		/* Available storage commands: start, stop, get_status, config, get_config, get_events */
		/* Example: storage_zephlet_start(K_NO_WAIT); */
		LOG_DBG("Received events report from battery_zephlet");
		break;
	default:
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_battery_to_storage_adapter, battery_to_storage_adapter);
ZBUS_CHAN_ADD_OBS(chan_zlet_battery_report, lis_battery_to_storage_adapter, 3);