/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "ui_service.h"

#include "private/ui_service_priv.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ui_service, CONFIG_UI_SERVICE_LOG_LEVEL);

/* TODO: Add service-specific resources (timers, work queues, threads) */
static int start(const struct service *service)
{
	struct ui_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start service resources (timers, threads, etc.) */

	return ui_service_report_status(&status, K_MSEC(250));
}

static int stop(const struct service *service)
{
	struct ui_service_data *data = service->data;
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

	return ui_service_report_status(&status, K_MSEC(250));
}

static int get_status(const struct service *service)
{
	struct ui_service_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return ui_service_report_status(&status, K_MSEC(250));
}

static int config(const struct service *service, const struct msg_ui_service_config *config)
{
	struct ui_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to service resources */

	return ui_service_report_config(config, K_MSEC(250));
}

static int get_config(const struct service *service)
{
	struct ui_service_data *data = service->data;
	struct msg_ui_service_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return ui_service_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgUIService.Events - publish to report field: events */
static int get_events(const struct service *service)
{
	struct ui_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_events logic */
	}
	/* Output-streaming RPC: publish events from async contexts (timer/IRQ) */
	/* Pattern example (see tick_service_impl.c:11-18):
	 *   void timer_handler(struct k_timer *timer) {
	 *       struct msg_ui_service_events event = {...};
	 *       ui_service_report_events(&event, K_NO_WAIT);
	 *   }
	 */
	/* TODO: Set up K_TIMER_DEFINE/K_WORK_DEFINE and call report helper */
	return 0;
}

/* RPC returns Empty - publish to report field: empty */
static int blink(const struct service *service)
{
	struct ui_service_data *data = service->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement blink logic */
	}
	/* Request-response RPC */
	LOG_DBG("blink!");
	/* TODO: Prepare report data and publish */
	/* return ui_service_report_empty(report_data, K_MSEC(250)); */
	return 0;
}

static struct ui_service_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
	.blink = blink,
};

static struct ui_service_data data = {.config = MSG_UI_SERVICE_CONFIG_INIT_ZERO,
				      .status = {.is_running = false}};

int ui_service_init_fn(const struct service *self)
{
	int err = ui_service_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

SERVICE_DEFINE(ui_service, ui_service_init_fn, &api, &data);
