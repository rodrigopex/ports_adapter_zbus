/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */

#include "zlet_storage_interface.h"

#include "zlet_storage.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_storage, CONFIG_ZEPHLET_STORAGE_LOG_LEVEL);

/* In-memory key-value store */
#define MAX_STORAGE_ENTRIES 10
static struct k_spinlock storage_lock;
static struct storage_item {
	char key[32];
	uint8_t value[256];
	int32_t value_size;
	bool occupied;
} storage_table[MAX_STORAGE_ENTRIES];
static int32_t entry_count = 0;

/* Simple hash for key distribution */
static uint32_t hash_key(const char *key)
{
	uint32_t hash = 0;
	while (*key) {
		hash = hash * 31 + *key++;
	}
	return hash % MAX_STORAGE_ENTRIES;
}

/* Called by init_fn during SYS_INIT - sets is_ready on success */
static int zlet_storage_init(const struct zephlet *zephlet)
{
	struct zlet_storage_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		/* Initialize storage (already zero-initialized) */
		if (ret == 0) {
			data->status.is_ready = true;
		}
	}

	return ret;
}

static int start(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_storage_data *data = zephlet->data;
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

	return zlet_storage_report_status(context, ret, &status, K_MSEC(250));
}

static int stop(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_storage_data *data = zephlet->data;
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

	return zlet_storage_report_status(context, ret, &status, K_MSEC(250));
}

static int get_status(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zephlet_status status;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		status = data->status;
	}

	return zlet_storage_report_status(context, ret, &status, K_MSEC(250));
}

static int config(const struct zephlet *zephlet, const struct msg_api_context *context, const struct msg_zlet_storage_config *config)
{
	struct zlet_storage_data *data = zephlet->data;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		data->config = *config;
		/* Config is read-only (max_entries) */
	}

	return zlet_storage_report_config(context, ret, config, K_MSEC(250));
}

static int get_config(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zlet_storage_config config;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		config = data->config;
	}

	return zlet_storage_report_config(context, ret, &config, K_MSEC(250));
}

static int store(const struct zephlet *zephlet, const struct msg_api_context *context, const struct msg_zlet_storage_key_value *kv)
{
	struct msg_zlet_storage_events events = {0};
	uint32_t slot = hash_key(kv->key);
	uint32_t start_slot = slot;
	bool stored = false;
	int ret = 0;

	k_spinlock_key_t key = k_spin_lock(&storage_lock);

	/* Linear probing */
	do {
		/* Check for duplicate key */
		if (storage_table[slot].occupied &&
		    strcmp(storage_table[slot].key, kv->key) == 0) {
			/* Reject duplicate */
			k_spin_unlock(&storage_lock, key);
			events.has_error_message = true;
			strncpy(events.error_message, "Key already exists", 63);
			events.timestamp = k_uptime_get_32();
			zlet_storage_report_events_async(&events, K_NO_WAIT);
			return -EEXIST;
		}

		/* Empty slot found */
		if (!storage_table[slot].occupied) {
			strncpy(storage_table[slot].key, kv->key, 31);
			memcpy(storage_table[slot].value, kv->value.bytes, kv->value_size);
			storage_table[slot].value_size = kv->value_size;
			storage_table[slot].occupied = true;
			entry_count++;
			stored = true;
			break;
		}

		/* Try next slot */
		slot = (slot + 1) % MAX_STORAGE_ENTRIES;
	} while (slot != start_slot);

	k_spin_unlock(&storage_lock, key);

	/* Storage full */
	if (!stored) {
		events.has_storage_full = true;
		events.storage_full = true;
		events.timestamp = k_uptime_get_32();
		zlet_storage_report_events_async(&events, K_NO_WAIT);
		ret = -ENOMEM;
	}

	/* RPC returns Empty, no report needed for success case */
	return ret;
}

static int retrieve(const struct zephlet *zephlet, const struct msg_api_context *context, const struct msg_zlet_storage_key_value *kv)
{
	struct msg_zlet_storage_storage_entry entry = {0};
	uint32_t slot = hash_key(kv->key);
	uint32_t start_slot = slot;
	int ret = 0;

	K_SPINLOCK(&storage_lock) {
		/* Linear probing search */
		do {
			if (storage_table[slot].occupied &&
			    strcmp(storage_table[slot].key, kv->key) == 0) {
				/* Found */
				strncpy(entry.key, storage_table[slot].key, 31);
				memcpy(entry.value.bytes, storage_table[slot].value, storage_table[slot].value_size);
				entry.value.size = storage_table[slot].value_size;
				entry.value_size = storage_table[slot].value_size;
				entry.found = true;
				break;
			}

			if (!storage_table[slot].occupied) {
				/* Empty slot - key doesn't exist */
				break;
			}

			slot = (slot + 1) % MAX_STORAGE_ENTRIES;
		} while (slot != start_slot);
	}

	return zlet_storage_report_storage_entry(context, ret, &entry, K_MSEC(250));
}

static int clear(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct msg_zlet_storage_events events = {0};

	K_SPINLOCK(&storage_lock) {
		memset(storage_table, 0, sizeof(storage_table));
		entry_count = 0;
	}

	events.timestamp = k_uptime_get_32();
	zlet_storage_report_events_async(&events, K_NO_WAIT);

	/* RPC returns Empty, operation successful */
	return 0;
}

static int get_storage_stats(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct msg_zlet_storage_storage_stats stats = {0};
	int32_t bytes_used = 0;
	int ret = 0;

	K_SPINLOCK(&storage_lock) {
		stats.total_entries = entry_count;
		stats.max_entries = MAX_STORAGE_ENTRIES;
		for (int i = 0; i < MAX_STORAGE_ENTRIES; i++) {
			if (storage_table[i].occupied) {
				bytes_used += storage_table[i].value_size;
			}
		}
		stats.total_bytes_used = bytes_used;
	}

	return zlet_storage_report_storage_stats(context, ret, &stats, K_MSEC(250));
}

/* RPC returns MsgZletStorage.Events - publish to report field: events */
static int get_events(const struct zephlet *zephlet, const struct msg_api_context *context)
{
	struct zlet_storage_data *data = zephlet->data;
	struct msg_zlet_storage_events events;
	int ret = 0;

	K_SPINLOCK(&data->lock) {
		events = data->events;
	}

	return zlet_storage_report_events(context, ret, &events, K_MSEC(250));
}

static struct zlet_storage_api api = {
	.start = start,
	.stop = stop,
	.get_status = get_status,
	.config = config,
	.get_config = get_config,
	.store = store,
	.retrieve = retrieve,
	.clear = clear,
	.get_storage_stats = get_storage_stats,
	.get_events = get_events,
};

static struct zlet_storage_data data = {
	.status = MSG_ZEPHLET_STATUS_INIT_ZERO,
	.config = {.max_entries = MAX_STORAGE_ENTRIES},
	.events = MSG_ZLET_STORAGE_EVENTS_INIT_ZERO
};

int zlet_storage_init_fn(const struct zephlet *self)
{
	int err;

	/* Initialize zephlet resources */
	err = zlet_storage_init(self);

	/* Register implementation */
	if (err == 0) {
		err = zlet_storage_set_implementation(self);
	}

	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");

	return err;
}

ZEPHLET_DEFINE(zlet_storage, zlet_storage_init_fn, &api, &data);
