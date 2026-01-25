/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "battery_service.h"

#include "private/battery_service_priv.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(battery_service, CONFIG_BATTERY_SERVICE_LOG_LEVEL);

/* TODO: Add service-specific resources (timers, work queues, threads) */
static int start(const struct service *service)
{
	struct battery_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start service resources (timers, threads, etc.) */

	return battery_service_report_status(&status, K_MSEC(250));
}

static int stop(const struct service *service)
{
	struct battery_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	if (!status.is_running) {
		LOG_DBG("Service has not started yet!");
	}

	/* TODO: Stop service resources (timers, threads, etc.) */

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
	}

	return battery_service_report_status(&status, K_MSEC(250));
}

static int get_status(const struct service *service)
{
	struct battery_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return battery_service_report_status(&status, K_MSEC(250));
}

static int config(const struct service *service, const struct msg_battery_service_config *config)
{
	struct battery_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to service resources */

	return battery_service_report_config(config, K_MSEC(250));
}

static int get_config(const struct service *service)
{
	struct battery_service_data *data = service->data;
	struct msg_battery_service_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return battery_service_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgBatteryService.BatteryState - publish to report field: battery_state */
static int get_battery_state(const struct service *service)
{
	struct battery_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_battery_state logic */
	}
	/* TODO: Prepare report data and publish */
	/* return battery_service_report_battery_state(report_data, K_MSEC(250)); */
	return 0;
}

/* RPC returns MsgBatteryService.Events - publish to report field: events */
static int get_events(const struct service *service)
{
	struct battery_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_events logic */
	}
	/* Streaming RPC - publish events as they occur */
	/* Example: return battery_service_report_events(event_data, K_NO_WAIT); */
	return 0;
}

static struct battery_service_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_battery_state = get_battery_state,
	.get_events = get_events,
};

static struct battery_service_data data = {
	.config = MSG_BATTERY_SERVICE_CONFIG_INIT_ZERO,
	.status = {.is_running = false}
};

int battery_service_init_fn(const struct service *self)
{
	int err = battery_service_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

SERVICE_DEFINE(battery_service, battery_service_init_fn, &api, &data);