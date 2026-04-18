#ifndef SRC_ZEPHLETS_TICK_ZLET_TICK_H_
#define SRC_ZEPHLETS_TICK_ZLET_TICK_H_

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "zlet_tick_interface.h"

/**
 * @file
 * @brief User-owned types and init_fn for the `tick` zephlet.
 *
 * The interface file (`zlet_tick_interface.h`) is codegen output; it
 * only knows the RPC shape. Per-instance storage (`struct tick_data`)
 * and the init callback (`tick_init_fn`) are user concerns and live
 * here.
 */

/**
 * @brief Mutable per-instance tick data.
 *
 * Application (or test) allocates one of these per `ZEPHLET_DEFINE(tick,
 * ...)` and passes its address as the `_data` arg. Framework treats it
 * as opaque; the tick handlers are the only readers/writers.
 */
struct tick_data {
	/* ----- Framework-standard fields (keep first, in this order) ----- */
	bool is_running;
	bool is_ready;
	struct tick_config current_config;
	/* ----- Custom fields below ----- */
	struct k_timer timer;
};

/**
 * @brief Default init_fn for tick instances.
 *
 * Seeds `current_config` from `z->config` (or hard-coded defaults if
 * NULL), initialises the timer with `z` as user-data, and marks the
 * instance ready. Pass as the `_init` arg of ZEPHLET_DEFINE.
 */
int tick_init_fn(const struct zephlet *z);

#endif /* SRC_ZEPHLETS_TICK_ZLET_TICK_H_ */
