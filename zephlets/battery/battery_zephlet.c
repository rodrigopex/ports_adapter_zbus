/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "battery_zephlet_interface.h"

#include "battery_zephlet.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(battery_zephlet, CONFIG_BATTERY_ZEPHLET_LOG_LEVEL);

/* TODO: Add zephlet-specific resources (timers, work queues, threads) */
static int start(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start zephlet resources (timers, threads, etc.) */

	return battery_zephlet_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	if (!status.is_running) {
		LOG_DBG("Zephlet has not started yet!");
	}

	/* TODO: Stop zephlet resources (timers, threads, etc.) */

	K_SPINLOCK(&data->lock) {
		data->status.is_running = false;
	}

	return battery_zephlet_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return battery_zephlet_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_battery_zephlet_config *config)
{
	struct battery_zephlet_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to zephlet resources */

	return battery_zephlet_report_config(config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;
	struct msg_battery_zephlet_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return battery_zephlet_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgBatteryZephlet.BatteryState - publish to report field: battery_state */
static int get_battery_state(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_battery_state logic */
	}
	/* TODO: Prepare report data and publish */
	/* return battery_zephlet_report_battery_state(report_data, K_MSEC(250)); */
	return 0;
}

/* RPC returns MsgBatteryZephlet.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct battery_zephlet_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_events logic */
	}
	/* Output-streaming RPC: publish events from async contexts (timer/IRQ) */
	/* Pattern example (see tick_zephlet_impl.c:11-18):
	 *   void timer_handler(struct k_timer *timer) {
	 *       struct msg_battery_zephlet_events event = {...};
	 *       battery_zephlet_report_events(&event, K_NO_WAIT);
	 *   }
	 */
	/* TODO: Set up K_TIMER_DEFINE/K_WORK_DEFINE and call report helper */
	return 0;
}

static struct battery_zephlet_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_battery_state = get_battery_state,
	.get_events = get_events,
};

static struct battery_zephlet_data data = {
	.config = MSG_BATTERY_ZEPHLET_CONFIG_INIT_ZERO,
	.status = {.is_running = false}
};

int battery_zephlet_init_fn(const struct zephlet *self)
{
	int err = battery_zephlet_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(battery_zephlet, battery_zephlet_init_fn, &api, &data);