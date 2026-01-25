#include "tick_service.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(tick_service, CONFIG_TICK_SERVICE_LOG_LEVEL);

#define ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(_is_running)                                         \
	&(struct msg_tick_service_report)                                                          \
	{                                                                                          \
		.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,                           \
		.status = (struct msg_service_status)                                              \
		{                                                                                  \
			.is_running = _is_running                                                  \
		}                                                                                  \
	}

#define ALLOCA_MSG_TICK_SERVICE_REPORT_CONFIG(...)                                                 \
	&(struct msg_tick_service_report)                                                          \
	{                                                                                          \
		.which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG,                           \
		.config = (struct msg_tick_service_config)                                         \
		{                                                                                  \
			__VA_ARGS__                                                                \
		}                                                                                  \
	}

#define ALLOCA_MSG_TICK_SERVICE_REPORT_TICK()                                                      \
	&(struct msg_tick_service_report)                                                          \
	{                                                                                          \
		.which_tick_report = MSG_TICK_SERVICE_REPORT_TICK_TAG                              \
	}

void tick_service_handler(struct k_timer *timer_id)
{
	LOG_DBG("tick!");
	zbus_chan_pub(&chan_tick_service_report, ALLOCA_MSG_TICK_SERVICE_REPORT_TICK(), K_NO_WAIT);
}

K_TIMER_DEFINE(timer_tick_service, tick_service_handler, NULL);

static int start(const struct service *service)
{
	struct tick_service_data *data = service->data;
	struct msg_service_status status;
	int delay;

	K_SPINLOCK(&data->lock) {
		delay = data->config.delay_ms;
		data->status.is_running = true;
		status = data->status;
	}

	k_timer_start(&timer_tick_service, K_MSEC(delay), K_MSEC(delay));
	LOG_DBG("Service started with delay %d ms!", delay);

	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG, .status = status},
		K_MSEC(200));
}

static int stop(const struct service *service)
{
	struct tick_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	if (!status.is_running) {
		LOG_DBG("Service has not started yet!");
	}

	k_timer_stop(&timer_tick_service);

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
	}

	LOG_DBG("Service stopped!");

	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG, .status = status},
		K_MSEC(200));
}

static int get_status(const struct service *service)
{
	struct tick_service_data *data = service->data;
	struct msg_service_status status;

	bool is_running;

	K_SPINLOCK(&data->lock) {
		status = data->status;
		is_running = data->status.is_running;
	}

	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
			.status = (struct msg_service_status){.is_running = is_running}},
		K_MSEC(200));
}
static int config(const struct service *service, const struct msg_tick_service_config *new_config)
{
	struct tick_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		/* Safely set the service config */
		data->config = *new_config;
	}

	return zbus_chan_pub(&chan_tick_service_report,
			     &(struct msg_tick_service_report){
				     .which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG,
				     .config = *new_config},
			     K_MSEC(200));
}

static int get_config(const struct service *service)
{
	struct tick_service_data *data = service->data;
	struct msg_tick_service_config config;
	K_SPINLOCK(&data->lock) {
		/* Safely set the service config */
		config = data->config;
	}

	return zbus_chan_pub(
		&chan_tick_service_report,
		&(struct msg_tick_service_report){
			.which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG, .config = config},
		K_MSEC(200));
}

static struct tick_service_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
};

static struct tick_service_data data = {.config = MSG_TICK_SERVICE_CONFIG_INIT_ZERO,
					.status = {.is_running = false}};

int tick_service_init_fn(const struct service *self)
{
	int err = tick_service_set_implementation(self);

	printk("   -> %s initialed with%s error\n", self->name, err == 0 ? " no" : "");

	return err;
}

SERVICE_DEFINE(tick_service, tick_service_init_fn, &api, &data);
