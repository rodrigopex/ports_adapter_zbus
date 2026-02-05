/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_battery_interface.h"

#include "zlet_battery.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_battery, CONFIG_ZEPHLET_BATTERY_LOG_LEVEL);

/* Battery monitoring timer */
static void zlet_battery_timer_handler(struct k_timer *timer_id);

K_TIMER_DEFINE(timer_zlet_battery, zlet_battery_timer_handler, NULL);

/* Battery state tracking */
static uint32_t monitor_count = 0;
static struct k_spinlock battery_state_lock;
static struct msg_zlet_battery_battery_state battery_state = {
	.voltage = 3300,
	.temperature = 25,
	.is_charging = false
};
static int32_t voltage_operation = 3300;
static int32_t low_battery_threshold = 3000;

static void zlet_battery_timer_handler(struct k_timer *timer_id)
{
	struct msg_zlet_battery_events events = {0};
	int32_t current_voltage;

	k_spinlock_key_t key = k_spin_lock(&battery_state_lock);

	/* Simulate voltage decay: start at voltage_operation, decay by 10mV per tick */
	monitor_count++;
	current_voltage = voltage_operation - (monitor_count * 10);

	/* Update battery state */
	battery_state.voltage = current_voltage;
	battery_state.temperature = 25; /* const temp */
	battery_state.is_charging = false;

	/* Check low battery threshold */
	if (current_voltage < low_battery_threshold) {
		events.has_low_battery = true;
		events.low_battery = true;
		events.timestamp = k_uptime_get_32();
	}

	k_spin_unlock(&battery_state_lock, key);

	if (events.has_low_battery) {
		zlet_battery_report_events(&events, K_NO_WAIT);
	}
}

/* TODO: Add zephlet-specific resources (timers, work queues, threads) */
static int start(const struct zephlet *zephlet)
{
	struct zlet_battery_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* Start battery monitoring timer - 1 second interval */
	k_timer_start(&timer_zlet_battery, K_MSEC(1000), K_MSEC(1000));

	return zlet_battery_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct zlet_battery_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_running) {
			LOG_DBG("Zephlet has not started yet!");
		}

		/* Stop battery monitoring */
		data->status.is_running = false;
		status = data->status;
	}

	k_timer_stop(&timer_zlet_battery);

	return zlet_battery_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct zlet_battery_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_battery_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_zlet_battery_config *config)
{
	struct zlet_battery_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* Apply configuration changes to battery monitoring */
	k_spinlock_key_t key = k_spin_lock(&battery_state_lock);
	voltage_operation = config->voltage_operation;
	low_battery_threshold = config->low_battery_threshold;
	k_spin_unlock(&battery_state_lock, key);

	return zlet_battery_report_config(config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct zlet_battery_data *data = zephlet->data;
	struct msg_zlet_battery_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_battery_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgBatteryZephlet.BatteryState - publish to report field: battery_state */
static int get_battery_state(const struct zephlet *zephlet)
{
	struct msg_zlet_battery_battery_state state;

	k_spinlock_key_t key = k_spin_lock(&battery_state_lock);
	state = battery_state;
	k_spin_unlock(&battery_state_lock, key);

	return zlet_battery_report_battery_state(&state, K_MSEC(250));
}

/* RPC returns MsgZletBattery.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct zlet_battery_data *data = zephlet->data;
	struct msg_zlet_battery_events events;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_battery_report_events(&events, K_MSEC(250));
}

static struct zlet_battery_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_battery_state = get_battery_state,
	.get_events = get_events,
};

static struct zlet_battery_data data = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = {.voltage_operation = 3300, .low_battery_threshold = 3000},
	.events = MSG_ZLET_BATTERY_EVENTS_INIT_ZERO
};

int zlet_battery_init_fn(const struct zephlet *self)
{
	int err = zlet_battery_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_battery, zlet_battery_init_fn, &api, &data);