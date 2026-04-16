#include "zlet_tampering.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tampering, CONFIG_ZEPHLET_TAMPERING_LOG_LEVEL);

int zlet_tampering_post_start(const struct zephlet *self)
{
	LOG_DBG("Tampering zephlet start");

	return 0;
}
int zlet_tampering_post_stop(const struct zephlet *self)
{
	LOG_DBG("Tampering zephlet stop");

	return 0;
}

static int force_tampering(struct zlet_tampering_context *ctx)
{
	struct tampering_events delta = {
		.has_proximity_tamper_detected = true,
		.proximity_tamper_detected = true,
	};

	LOG_DBG("tampered by force");

	return tampering_events_update(ctx, &delta);
}

static struct zlet_tampering_api api = {.force_tampering = force_tampering};

int zlet_tampering_init_fn(const struct zephlet *self)
{
	int err = zlet_tampering_set_implementation(self);
	printk("   -> %s %sinitialized\n", self->config.name, err == 0 ? "" : "not ");
	return err;
}

ZEPHLET_DEFINE(zlet_tampering, zlet_tampering_init_fn, &api, NULL);
