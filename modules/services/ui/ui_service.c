#include "ui_service.h"
#include "ui/ui_service.pb.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui_service, CONFIG_UI_SERVICE_LOG_LEVEL);

static void blink(void)
{
	LOG_DBG("blink!");
}

ZBUS_CHAN_DEFINE(chan_ui_service_invoke, struct msg_ui_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(alis_ui_service), MSG_UI_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_ui_service_report, struct msg_ui_service_report, NULL, NULL,
		 ZBUS_OBSERVERS(alis_ui_service), MSG_UI_SERVICE_REPORT_INIT_DEFAULT);

void api_handler(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_ui_service_invoke *ivk = msg;

	switch (ivk->which_ui_invoke) {
	case MSG_UI_SERVICE_INVOKE_BLINK_TAG:
		blink();
		break;
	}
}

ZBUS_ASYNC_LISTENER_DEFINE(alis_ui_service, api_handler);
