#include "tick_service.h"
#include "tick/tick_service.pb.h"
#include "zephyr/zbus/zbus.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tick_service, CONFIG_TICK_SERVICE_LOG_LEVEL);

/* clang-format off */
#define ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(_is_running)                     \
  &(struct msg_tick_service_report) {                                          \
    .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,                   \
    .status = (struct msg_status) {                                            \
      .is_running = _is_running                                                \
    }                                                                          \
  }

#define ALLOCA_MSG_TICK_SERVICE_REPORT_TICK()                                  \
  &(struct msg_tick_service_report) {                                          \
    .which_tick_report = MSG_TICK_SERVICE_REPORT_TICK_TAG                      \
  }
/* clang-format on */

ZBUS_CHAN_DEFINE(chan_tick_service_invoke, struct msg_tick_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(alis_tick_service), MSG_TICK_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_tick_service_report, struct msg_tick_service_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_TICK_SERVICE_REPORT_INIT_DEFAULT);

void tick_service_handler(struct k_timer *timer_id)
{
	LOG_DBG("tick!");
	zbus_chan_pub(&chan_tick_service_report, ALLOCA_MSG_TICK_SERVICE_REPORT_TICK(), K_NO_WAIT);
}
K_TIMER_DEFINE(timer_tick_service, tick_service_handler, NULL);

static void start(const struct msg_tick_service_config *cfg)
{
	k_timer_start(&timer_tick_service, K_MSEC(cfg->delay_ms), K_MSEC(cfg->delay_ms));
	LOG_DBG("Service started with delay %d ms!", cfg->delay_ms);

	zbus_chan_pub(&chan_tick_service_report, ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(true),
		      K_MSEC(200));
}

static void stop(void)
{
	k_timer_stop(&timer_tick_service);

	LOG_DBG("Service stopped!");

	zbus_chan_pub(&chan_tick_service_report, ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(false),
		      K_MSEC(200));
}

static void api_handler(const struct zbus_channel *chan)
{
	const struct msg_tick_service_invoke *ivk = zbus_chan_const_msg(chan);

	switch (ivk->which_tick_invoke) {
	case MSG_TICK_SERVICE_INVOKE_START_TAG:
		start(&ivk->start);
		break;
	case MSG_TICK_SERVICE_INVOKE_STOP_TAG:
		stop();
		break;
	}
}

ZBUS_LISTENER_DEFINE(alis_tick_service, api_handler);
