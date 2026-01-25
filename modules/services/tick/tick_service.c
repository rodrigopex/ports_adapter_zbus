#include "service.pb.h"
#include "tick_service.h"
#include "tick/tick_service.pb.h"
#include "zephyr/init.h"
#include "zephyr/spinlock.h"
#include "zephyr/sys/check.h"
#include "zephyr/zbus/zbus.h"
#include "service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sys/errno.h>

LOG_MODULE_REGISTER(tick_service, CONFIG_TICK_SERVICE_LOG_LEVEL);

// #define ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(_is_running) \
// 	&(struct msg_tick_service_report)                                                          \
// 	{                                                                                          \
// 		.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,                           \
// 		.status = (struct msg_service_status)                                              \
// 		{                                                                                  \
// 			.is_running = _is_running                                                  \
// 		}                                                                                  \
// 	}
//
// #define ALLOCA_MSG_TICK_SERVICE_REPORT_CONFIG(...) \
// 	&(struct msg_tick_service_report)                                                          \
// 	{                                                                                          \
// 		.which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG,                           \
// 		.config = (struct msg_tick_service_config)                                         \
// 		{                                                                                  \
// 			__VA_ARGS__                                                                \
// 		}                                                                                  \
// 	}
//
// #define ALLOCA_MSG_TICK_SERVICE_REPORT_TICK() \
// 	&(struct msg_tick_service_report)                                                          \
// 	{                                                                                          \
// 		.which_tick_report = MSG_TICK_SERVICE_REPORT_TICK_TAG                              \
// 	}
//
ZBUS_CHAN_DEFINE(chan_tick_service_invoke, struct msg_tick_service_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(lis_tick_service), MSG_TICK_SERVICE_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_tick_service_report, struct msg_tick_service_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_TICK_SERVICE_REPORT_INIT_DEFAULT);

// static struct tick_service_data data = {.config = MSG_TICK_SERVICE_CONFIG_INIT_ZERO,
// 					.status = {.is_running = false}};
//
// int tick_service_init_fn(const struct service *self)
// {
// 	printk("   -> %s initialed\n", self->name);
// }
//
// SERVICE_DEFINE(tick_service, tick_service_init_fn, NULL, &data);

// void tick_service_handler(struct k_timer *timer_id)
// {
// 	LOG_DBG("tick!");
// 	zbus_chan_pub(&chan_tick_service_report, ALLOCA_MSG_TICK_SERVICE_REPORT_TICK(), K_NO_WAIT);
// }
// K_TIMER_DEFINE(timer_tick_service, tick_service_handler, NULL);

// static void start(const struct service *service)
// {
// 	struct tick_service_data *data = service->data;
// 	struct msg_service_status status;
// 	int delay;
//
// 	K_SPINLOCK(&data->lock) {
// 		delay = data->config.delay_ms;
// 		data->status.is_running = true;
// 		status = data->status;
// 	}
//
// 	k_timer_start(&timer_tick_service, K_MSEC(delay), K_MSEC(delay));
// 	LOG_DBG("Service started with delay %d ms!", delay);
//
// 	zbus_chan_pub(&chan_tick_service_report,
// 		      &(struct msg_tick_service_report){.which_tick_report =
// 								MSG_TICK_SERVICE_REPORT_STATUS_TAG,
// 							.status = status},
// 		      K_MSEC(200));
// }
//
// static void stop(const struct service *service)
// {
// 	struct tick_service_data *data = service->data;
// 	struct msg_service_status status;
//
// 	K_SPINLOCK(&data->lock) {
// 		status = data->status;
// 	}
//
// 	if (!status.is_running) {
// 		LOG_DBG("Service has not started yet!");
// 	}
//
// 	k_timer_stop(&timer_tick_service);
//
// 	K_SPINLOCK(&data->lock) {
// 		data->status.is_running = false;
// 	}
//
// 	LOG_DBG("Service stopped!");
//
// 	zbus_chan_pub(&chan_tick_service_report,
// 		      &(struct msg_tick_service_report){.which_tick_report =
// 								MSG_TICK_SERVICE_REPORT_STATUS_TAG,
// 							.status = status},
// 		      K_MSEC(200));
// }
//
// static void get_status(const struct service *service)
// {
// 	struct tick_service_data *data = service->data;
// 	struct msg_service_status status;
//
// 	bool is_running;
//
// 	K_SPINLOCK(&data->lock) {
// 		status = data->status;
// 		is_running = data->status.is_running;
// 	}
//
// 	zbus_chan_pub(&chan_tick_service_report,
// 		      &(struct msg_tick_service_report){
// 			      .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
// 			      .status = (struct msg_service_status){.is_running = is_running}},
// 		      K_MSEC(200));
// }
// static void config(const struct service *service, const struct msg_tick_service_config
// *new_config)
// {
// 	struct tick_service_data *data = service->data;
//
// 	K_SPINLOCK(&data->lock) {
// 		/* Safely set the service config */
// 		data->config = *new_config;
// 	}
//
// 	zbus_chan_pub(&chan_tick_service_report,
// 		      &(struct msg_tick_service_report){.which_tick_report =
// 								MSG_TICK_SERVICE_REPORT_CONFIG_TAG,
// 							.config = *new_config},
// 		      K_MSEC(200));
// }
//
// static void get_config(const struct service *service)
// {
// 	struct tick_service_data *data = service->data;
// 	struct msg_tick_service_config config;
// 	K_SPINLOCK(&data->lock) {
// 		/* Safely set the service config */
// 		config = data->config;
// 	}
//
// 	zbus_chan_pub(&chan_tick_service_report,
// 		      &(struct msg_tick_service_report){.which_tick_report =
// 								MSG_TICK_SERVICE_REPORT_CONFIG_TAG,
// 							.config = config},
// 		      K_MSEC(200));
// }

static const struct service *impl = NULL;

int tick_service_set_implementation(const struct service *implementation)
{
	int ret = -EBUSY;

	if (impl == NULL) {
		impl = implementation;
		ret = 0;
	} else {
		LOG_ERR("Trying to replace implementation for Tick Service");
	}

	return ret;
}

static void api_handler(const struct zbus_channel *chan)
{
	CHECKIF(impl == NULL) {
		LOG_ERR("Service implementation required!");
		return;
	}

	struct tick_service_api *api = impl->api;

	CHECKIF(api == NULL) {
		LOG_ERR("Service api required!");
		return;
	}

	const struct msg_tick_service_invoke *ivk = zbus_chan_const_msg(chan);

	switch (ivk->which_tick_invoke) {
	case MSG_TICK_SERVICE_INVOKE_START_TAG:
		if (api->start) {
			api->start(impl);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_STOP_TAG:
		if (api->stop) {
			api->stop(impl);
		}
	case MSG_TICK_SERVICE_INVOKE_CONFIG_TAG:
		if (api->config) {
			api->config(impl, &ivk->config);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_GET_STATUS_TAG:
		if (api->get_status) {
			api->get_status(impl);
		}
		break;
	case MSG_TICK_SERVICE_INVOKE_GET_CONFIG_TAG:
		if (api->get_config) {
			api->get_config(impl);
		}
		break;
	}
}

ZBUS_LISTENER_DEFINE(lis_tick_service, api_handler);
