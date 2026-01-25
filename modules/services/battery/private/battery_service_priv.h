/* GENERATED FILE - DO NOT EDIT */
/* Private helpers for battery_service implementation */

#include "battery_service.h"

static inline int battery_service_report_status(const struct msg_service_status *status,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_battery_service_report,
		&(struct msg_battery_service_report){
			.which_battery_report = MSG_BATTERY_SERVICE_REPORT_STATUS_TAG, .status = *status},
		timeout);
}

static inline int battery_service_report_config(const struct msg_battery_service_config *config,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_battery_service_report,
		&(struct msg_battery_service_report){
			.which_battery_report = MSG_BATTERY_SERVICE_REPORT_CONFIG_TAG, .config = *config},
		timeout);
}

static inline int battery_service_report_battery_state(const struct msg_battery_service_battery_state *battery_state,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_battery_service_report,
		&(struct msg_battery_service_report){
			.which_battery_report = MSG_BATTERY_SERVICE_REPORT_BATTERY_STATE_TAG, .battery_state = *battery_state},
		timeout);
}

static inline int battery_service_report_events(const struct msg_battery_service_events *events,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_battery_service_report,
		&(struct msg_battery_service_report){
			.which_battery_report = MSG_BATTERY_SERVICE_REPORT_EVENTS_TAG, .events = *events},
		timeout);
}