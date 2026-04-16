#include "zlet_tampering.h"
#include "zlet_tampering.pb.h"
#include "zlet_tampering_interface.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_tampering, CONFIG_ZEPHLET_TAMPERING_LOG_LEVEL);

/*
 * Implementation notes
 * --------------------
 *
 * Standard lifecycle RPCs (start/stop/get_status/update_settings/
 * get_settings/get_events) are dispatched directly from the generated
 * api_handler. To attach side effects, provide a strong override for
 * any of the weak hooks declared in zlet_tampering_interface.h:
 *
 *   zlet_tampering_pre_start / _post_start
 *   zlet_tampering_pre_stop  / _post_stop
 *   zlet_tampering_validate_settings
 *   zlet_tampering_post_update_settings
 *
 * Access mutable state via tampering_status_clone(),
 * tampering_settings_clone(), tampering_events_clone(),
 * tampering_settings_init(&defaults),
 * tampering_events_update(ctx_or_NULL, &delta).
 */

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
	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");
	return err;
}

/* Pass &api below once custom RPCs exist, else NULL. */
ZEPHLET_DEFINE(zlet_tampering, zlet_tampering_init_fn, &api);
