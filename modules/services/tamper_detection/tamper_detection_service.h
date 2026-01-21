#ifndef _TAMPER_DETECTION_SERVICE_H_
#define _TAMPER_DETECTION_SERVICE_H_

#include "tamper_detection/tamper_detection_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_tamper_detection_service_invoke, chan_tamper_detection_service_report);


/* clang-format off */
#define ALLOCA_MSG_TAMPER_DETECTION_SERVICE_INVOKE_START()                 \
    &(struct msg_tamper_detection_service_invoke) {                        \
        .which_tamper_detection_invoke = MSG_TAMPER_DETECTION_SERVICE_INVOKE_START_TAG   \
    }
#define ALLOCA_MSG_TAMPER_DETECTION_SERVICE_INVOKE_STOP()                 \
    &(struct msg_tamper_detection_service_invoke) {                        \
        .which_tamper_detection_invoke = MSG_TAMPER_DETECTION_SERVICE_INVOKE_STOP_TAG   \
    }

/* clang-format on */

static inline int tamper_detection_service_start(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tamper_detection_service_invoke, ALLOCA_MSG_TAMPER_DETECTION_SERVICE_INVOKE_START(),
			     timeout);
}

static inline int tamper_detection_service_stop(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_tamper_detection_service_invoke, ALLOCA_MSG_TAMPER_DETECTION_SERVICE_INVOKE_STOP(),
			     timeout);
}


#endif /* #ifndef _TAMPER_DETECTION_SERVICE_H_ */
