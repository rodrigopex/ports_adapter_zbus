#ifndef _TICK_SERVICE_H_
#define _TICK_SERVICE_H_

#include "tick/tick_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_tick_service_invoke, chan_tick_service_report);

/* clang-format off */
#define ALLOCA_MSG_TICK_SERVICE_INVOKE_START(_delay)                           \
    &(struct msg_tick_service_invoke) {                                        \
        .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_START_TAG, .start = {     \
            .delay_ms = _delay                                                 \
        }                                                                      \
    }

#define ALLOCA_MSG_TICK_SERVICE_INVOKE_STOP()                                  \
    &(struct msg_tick_service_invoke) {                                        \
        .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_STOP_TAG                  \
    }

/* clang-format on */

static inline int tick_service_start(int delay_ms, k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke,
			     ALLOCA_MSG_TICK_SERVICE_INVOKE_START(delay_ms), timeout);
}

static inline int tick_service_stop(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tick_service_invoke, ALLOCA_MSG_TICK_SERVICE_INVOKE_STOP(),
			     timeout);
}

#endif /* #ifndef _TICK_SERVICE_H_ */
