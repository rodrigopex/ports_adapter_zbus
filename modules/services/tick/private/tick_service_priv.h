/* GENERATED FILE - DO NOT EDIT */
/* Private helpers for tick_service implementation */

#include "tick_service.h"

static inline int tick_service_report_status(const struct msg_service_status *status,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG, .status = *status},
		timeout);
}

static inline int tick_service_report_config(const struct msg_tick_service_config *config,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG, .config = *config},
		timeout);
}

static inline int tick_service_report_events(const struct msg_tick_service_events *events,
					     k_timeout_t timeout)
{
	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_EVENTS_TAG, .events = *events},
		timeout);
}