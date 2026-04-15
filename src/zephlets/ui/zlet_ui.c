#include "zlet_ui.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zlet_ui, CONFIG_ZEPHLET_UI_LOG_LEVEL);

static int blink(struct zlet_ui_context *ctx)
{
	struct ui_events ev = ui_events_clone();
	ev.has_blink = true;
	ev.blink++;
	ev.timestamp = k_uptime_get();

	LOG_INF("LED blink #%u", ev.blink);

	/* Commits to events storage AND publishes async — single call. */
	ui_events_update(&ev, sys_timepoint_timeout(ctx->deadline));
	return 0;
}

static int buzzer_play(struct zlet_ui_context *ctx, enum ui_buzzer_tone buzzer_tone)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(buzzer_tone);

	/* TODO: Implement buzzer playback */
	return 0;
}

static struct zlet_ui_api api = {
	.blink = blink,
	.buzzer_play = buzzer_play,
};

int zlet_ui_pre_stop(const struct zephlet *self)
{
	ARG_UNUSED(self);
	LOG_DBG("Zephlet UI stopped!");
	return 0;
}

int zlet_ui_init_fn(const struct zephlet *self)
{
	int err = zlet_ui_set_implementation(self);
	if (err == 0) {
		zephlet_mark_ready(self);
	}
	printk("   -> %s %sinitialized\n", self->name, err == 0 ? "" : "not ");
	return err;
}

ZEPHLET_DEFINE(zlet_ui, zlet_ui_init_fn, &api);
