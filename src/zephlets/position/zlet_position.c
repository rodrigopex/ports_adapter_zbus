/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_position_interface.h"

#include "zlet_position.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_position, CONFIG_ZEPHLET_POSITION_LOG_LEVEL);

/* Position tracking - random walk simulation */
static void zlet_position_timer_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(timer_zlet_position, zlet_position_timer_handler, NULL);

static struct k_spinlock position_lock;
static struct msg_zlet_position_position_data current_position = {
	.latitude_micro = 37774900,   /* San Francisco: 37.7749° N */
	.longitude_micro = -122419400, /* -122.4194° W */
	.altitude_cm = 1600,           /* 16m */
	.speed_cmps = 0,
	.heading_decideg = 0,
	.satellites = 8,
	.fix_valid = true
};
static uint32_t update_count = 0;
static int32_t update_interval_ms = 5000;

/* Simple PRNG for random walk */
static uint32_t prng_state = 12345;
static uint32_t simple_rand(void)
{
	prng_state = prng_state * 1103515245 + 12345;
	return (prng_state / 65536) % 32768;
}

/* Random walk: step in random direction each update */
static void zlet_position_timer_handler(struct k_timer *timer_id)
{
	struct msg_zlet_position_events events = {0};
	int32_t direction = simple_rand() % 360; /* Random direction 0-359 degrees */
	int32_t step_size = 50 + (simple_rand() % 100); /* 50-150 microdegrees ≈ 5-16m */

	K_SPINLOCK(&position_lock) {
		update_count++;

		/* Random walk - approximate direction to lat/lon offsets */
		/* North/South (0-90 or 180-270): lat changes */
		/* East/West (90-180 or 270-360): lon changes */
		if (direction < 90 || direction >= 270) {
			/* North component */
			current_position.latitude_micro += (direction < 90) ? step_size : -step_size;
		}
		if (direction >= 45 && direction < 225) {
			/* East component */
			current_position.longitude_micro += (direction < 135) ? step_size : -step_size;
		}

		/* Update heading and speed */
		current_position.heading_decideg = direction * 10;
		current_position.speed_cmps = 30 + (simple_rand() % 70); /* 30-100 cm/s walking */

		/* Satellite count variation 6-12 */
		current_position.satellites = 6 + (simple_rand() % 7);

		/* Simulate occasional fix loss (5% chance) */
		if ((simple_rand() % 100) < 5) {
			if (current_position.fix_valid) {
				current_position.fix_valid = false;
				events.has_fix_lost = true;
				events.fix_lost = true;
				events.timestamp = k_uptime_get_32();
			}
		} else if (!current_position.fix_valid) {
			/* Reacquire fix */
			current_position.fix_valid = true;
		}

		/* Emit position_changed event */
		if (current_position.fix_valid) {
			events.has_position_changed = true;
			events.position_changed = true;
			events.timestamp = k_uptime_get_32();
		}
	}

	/* Async event - no context */
	zlet_position_report_events_async(&events, K_NO_WAIT);
}

/* Called by init_fn during SYS_INIT - sets is_ready on success */
static int zlet_position_init(const struct zephlet *zephlet)
{
	struct zlet_position_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		/* Initialize timer (already done by K_TIMER_DEFINE) */
		if (ret == 0) {
			data->status.is_ready = true;
		}
	}

	return ret;
}

static int start(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_ready) {
			ret = -ENODEV;
		} else if (data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = true;
		}
		status = data->status;
	}

	if (ret == 0) {
		/* Start position monitoring timer */
		k_timer_start(&timer_zlet_position, K_MSEC(update_interval_ms),
			      K_MSEC(update_interval_ms));
	}

	return zlet_position_report_status(context, ret, &status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		if (!data->status.is_running) {
			ret = -EALREADY;
		} else {
			data->status.is_running = false;
		}
		status = data->status;
	}

	if (ret == 0) {
		k_timer_stop(&timer_zlet_position);
	}

	return zlet_position_report_status(context, ret, &status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_position_report_status(context, ret, &status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_api_context *context, const struct msg_zlet_position_config *config)
{
	struct zlet_position_data *data = zephlet->data;
	bool is_running;
	int ret = 0;

	/* Validate config */
	if (config->update_interval_ms == 0) {
		ret = -EINVAL;
		goto report;
	}

	K_SPINLOCK(&data->lock) {
		data->config = *config;
		is_running = data->status.is_running;
	}

	/* Apply interval change */
	K_SPINLOCK(&position_lock) {
		update_interval_ms = config->update_interval_ms;
	}

	/* Restart timer with new interval if running */
	if (is_running) {
		k_timer_stop(&timer_zlet_position);
		k_timer_start(&timer_zlet_position, K_MSEC(update_interval_ms),
			      K_MSEC(update_interval_ms));
	}

report:
	return zlet_position_report_config(context, ret, config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zlet_position_config config;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_position_report_config(context, ret, &config, K_MSEC(250));
}

/* RPC returns MsgZletPosition.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_position_data *data = zephlet->data;
	struct msg_zlet_position_events events;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_position_report_events(context, ret, &events, K_MSEC(250));
}

static int get_position(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct msg_zlet_position_position_data position;
	int ret = 0;

	K_SPINLOCK(&position_lock) {
		position = current_position;
	}

	return zlet_position_report_position_data(context, ret, &position, K_MSEC(250));
}

static struct zlet_position_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.get_events = get_events,
	.get_position = get_position,
};

static struct zlet_position_data data = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = {.update_interval_ms = 5000},
	.events = MSG_ZLET_POSITION_EVENTS_INIT_ZERO
};

int zlet_position_init_fn(const struct zephlet *self)
{
	int err;

	/* Initialize zephlet resources */
	err = zlet_position_init(self);

	/* Register implementation */
	if (err == 0) {
		err = zlet_position_set_implementation(self);
	}

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_position, zlet_position_init_fn, &api, &data);
