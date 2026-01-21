#ifndef _BATTERY_SERVICE_H_
#define _BATTERY_SERVICE_H_

#include "battery/battery_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_battery_service_invoke, chan_battery_service_report);


/* clang-format off */
#define ALLOCA_MSG_BATTERY_SERVICE_INVOKE_START()                 \
    &(struct msg_battery_service_invoke) {                        \
        .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_START_TAG   \
    }
#define ALLOCA_MSG_BATTERY_SERVICE_INVOKE_STOP()                 \
    &(struct msg_battery_service_invoke) {                        \
        .which_battery_invoke = MSG_BATTERY_SERVICE_INVOKE_STOP_TAG   \
    }

/* clang-format on */

static inline int battery_service_start(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke, ALLOCA_MSG_BATTERY_SERVICE_INVOKE_START(),
			     timeout);
}

static inline int battery_service_stop(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_battery_service_invoke, ALLOCA_MSG_BATTERY_SERVICE_INVOKE_STOP(),
			     timeout);
}


#endif /* #ifndef _BATTERY_SERVICE_H_ */
