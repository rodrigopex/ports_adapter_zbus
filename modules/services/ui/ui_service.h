#ifndef _UI_SERVICE_H_
#define _UI_SERVICE_H_

#include "ui/ui_service.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_ui_service_invoke, chan_ui_service_report);

/* clang-format off */
#define ALLOCA_MSG_UI_SERVICE_INVOKE_BLINK()                 \
    &(struct msg_ui_service_invoke) {                        \
        .which_ui_invoke = MSG_UI_SERVICE_INVOKE_BLINK_TAG   \
    }
/* clang-format on */

static inline int ui_service_blink(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_ui_service_invoke, ALLOCA_MSG_UI_SERVICE_INVOKE_BLINK(),
			     timeout);
}

#endif /* #ifndef _UI_SERVICE_H_ */
