#include "battery_service.h"
#include "battery/battery_service.pb.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(battery_service, CONFIG_BATTERY_SERVICE_LOG_LEVEL);


#define ALLOCA_MSG_BATTERY_SERVICE_REPORT_STATUS(_is_running)         \
  &(struct msg_battery_service_report) {                                  \
    .which_battery_report = MSG_BATTERY_SERVICE_REPORT_STATUS_TAG,    \
    .status = (struct msg_status) {                               \
      .is_running = _is_running                                                          \
    }                                                                          \
  }

static void start(void)
{
	LOG_DBG("start!");
	zbus_chan_pub(&chan_battery_service_report, ALLOCA_MSG_BATTERY_SERVICE_REPORT_STATUS(true),
		      K_MSEC(200));
}

static void stop(void)
{
	LOG_DBG("stop!");
	zbus_chan_pub(&chan_battery_service_report, ALLOCA_MSG_BATTERY_SERVICE_REPORT_STATUS(false),
		      K_MSEC(200));
}


ZBUS_CHAN_DEFINE(chan_battery_service_invoke, struct msg_battery_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(alis_battery_service), MSG_BATTERY_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_battery_service_report, struct msg_battery_service_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_BATTERY_SERVICE_REPORT_INIT_DEFAULT);

static void battery_api_handler(const struct zbus_channel *chan, const void *msg)
{
	const struct msg_battery_service_invoke *ivk = msg;

	switch (ivk->which_battery_invoke) {

	case MSG_BATTERY_SERVICE_INVOKE_START_TAG:
		start();
		break;
	case MSG_BATTERY_SERVICE_INVOKE_STOP_TAG:
		stop();
		break;

	}
}

ZBUS_ASYNC_LISTENER_DEFINE(alis_battery_service, battery_api_handler);
