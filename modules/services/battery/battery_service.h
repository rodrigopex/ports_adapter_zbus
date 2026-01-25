/* GENERATED FILE - DO NOT EDIT */
/* Generated from battery_service.proto */

#ifndef _BATTERY_SERVICE_H_
#define _BATTERY_SERVICE_H_

#include "service.h"
#include "battery/battery_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_battery_service_invoke, chan_battery_service_report);

struct battery_service_data {
	struct k_spinlock lock;
	struct msg_battery_service_config config;
	struct msg_service_status status;
};

struct battery_service_api {
	int (*start)(const struct service *service);
	int (*stop)(const struct service *service);
	int (*get_status)(const struct service *service);
	int (*config)(const struct service *service,
	                        const struct msg_battery_service_config *config);
	int (*get_config)(const struct service *service);
	int (*get_battery_state)(const struct service *service);
	int (*get_events)(const struct service *service);
};

static inline int battery_service_start(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_START_TAG},
	                     timeout);
}

static inline int battery_service_stop(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_STOP_TAG},
	                     timeout);
}

static inline int battery_service_get_status(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_GET_STATUS_TAG},
	                     timeout);
}

static inline int battery_service_config_set(int32_t voltage_operation, int32_t low_battery_threshold, k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_CONFIG_TAG,
	                             .config = {
	                                     .voltage_operation = voltage_operation,
	                                     .low_battery_threshold = low_battery_threshold,
	                             }},
	                     timeout);
}

static inline int battery_service_get_config(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_GET_CONFIG_TAG},
	                     timeout);
}

static inline int battery_service_get_battery_state(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_GET_BATTERY_STATE_TAG},
	                     timeout);
}

static inline int battery_service_get_events(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke,
	                     &(struct msg_battery_service_invoke){
	                             .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_GET_EVENTS_TAG},
	                     timeout);
}

int battery_service_set_implementation(const struct service *implementation);

#endif /* _BATTERY_SERVICE_H_ */