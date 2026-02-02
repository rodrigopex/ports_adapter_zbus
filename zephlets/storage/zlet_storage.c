/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_storage_interface.h"

#include "zlet_storage.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(storage_zephlet, CONFIG_ZEPHLET_STORAGE_LOG_LEVEL);

/* TODO: Add zephlet-specific resources (timers, work queues, threads) */
static int start(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	/* TODO: Start zephlet resources (timers, threads, etc.) */

	return zlet_storage_report_status(&status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;
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

	return zlet_storage_report_status(&status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_storage_report_status(&status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_zlet_storage_config *config)
{
	struct zlet_storage_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
	}

	/* TODO: Apply configuration changes to zephlet resources */

	return zlet_storage_report_config(config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zlet_storage_config config;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_storage_report_config(&config, K_MSEC(250));
}

/* RPC returns MsgZephletStatus (status) */
static int store(const struct zephlet *zephlet, const struct msg_zlet_storage_key_value *key_value)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zephlet_status status;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement store logic */
		status = data->status;
	}

	return zlet_storage_report_status(&status, K_MSEC(250));
}

/* RPC returns MsgStorageZephlet.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;

	K_SPINLOCK(&data->lock) {
		/* TODO: Implement get_events logic */
	}
	/* Output-streaming RPC: publish events from async contexts (timer/IRQ) */
	/* Pattern example (see tick_zephlet_impl.c:11-18):
	 *   void timer_handler(struct k_timer *timer) {
	 *       struct msg_zlet_storage_events event = {...};
	 *       zlet_storage_report_events(&event, K_NO_WAIT);
	 *   }
	 */
	/* TODO: Set up K_TIMER_DEFINE/K_WORK_DEFINE and call report helper */
	return 0;
}

static struct zlet_storage_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.store = store,
	.get_events = get_events,
};

static struct zlet_storage_data data = {
	.config = MSG_ZLET_STORAGE_CONFIG_INIT_ZERO,
	.status = {.is_running = false}
};

int zlet_storage_init_fn(const struct zephlet *self)
{
	int err = zlet_storage_set_implementation(self);

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_storage, zlet_storage_init_fn, &api, &data);