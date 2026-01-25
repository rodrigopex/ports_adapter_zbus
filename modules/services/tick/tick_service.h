#ifndef _TICK_SERVICE_H_
#define _TICK_SERVICE_H_

#include "service.h"
#include "tick/tick_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_tick_service_invoke, chan_tick_service_report);

struct tick_service_data {
	struct k_spinlock lock;
	struct msg_tick_service_config config;
	struct msg_service_status status;
};

struct tick_service_api {
	int (*start)(const struct service *service);                     /*< Reports status */
	int (*stop)(const struct service *service);                      /*< Reports status */
	int (*config)(const struct service *service,                     /*< Reports status */
		      const struct msg_tick_service_config *new_config); /*< Reports config */
	int (*get_status)(const struct service *service);                /*< Reports status */
	int (*get_config)(const struct service *service);                /*< Reports config */
};

static inline int tick_service_start(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     &(struct msg_tick_service_invoke){
				     .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_START_TAG},
			     timeout);
}

static inline int tick_service_stop(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     &(struct msg_tick_service_invoke){
				     .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_STOP_TAG},
			     timeout);
}
static inline int tick_service_get_status(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     &(struct msg_tick_service_invoke){
				     .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_GET_STATUS_TAG},
			     timeout);
}

static inline int tick_service_config_set(uint32_t delay_ms, k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     &(struct msg_tick_service_invoke){
				     .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_CONFIG_TAG,
				     .config = {.delay_ms = delay_ms}},
			     timeout);
}
static inline int tick_service_config_get(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     &(struct msg_tick_service_invoke){
				     .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_GET_CONFIG_TAG},
			     timeout);
}

int tick_service_set_implementation(const struct service *impl);

#endif /* #ifndef _TICK_SERVICE_H_ */
