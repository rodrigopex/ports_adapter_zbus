#ifndef SRC_ZEPHLETS_TICK_ZLET_TICK_INTERFACE_H_
#define SRC_ZEPHLETS_TICK_ZLET_TICK_INTERFACE_H_

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "zephlet.h"
#include "tick/zlet_tick.pb.h"

/**
 * @file
 * @brief Hand-written interface for the `tick` zephlet (v0.3 reference).
 *
 * Codegen is disabled for tick during Phase 2. This file is the oracle
 * the Phase 3 generator must reproduce.
 */

/* Method ids — allocated in .proto declaration order, starting at 1.
 * method_id == 0 is reserved by the v0.3 contract. */
enum tick_method_id {
	TICK_M_RESERVED_0 = 0,
	TICK_M_START = 1,
	TICK_M_STOP = 2,
	TICK_M_GET_STATUS = 3,
	TICK_M_CONFIG = 4,
	TICK_M_GET_CONFIG = 5,
	TICK_M__COUNT = 6,
};

/** Method dispatch table exposed via ZEPHLET_DEFINE(tick, ...). */
extern const struct zephlet_api tick_api;

/**
 * @brief Mutable per-instance tick data.
 *
 * Application (or test) allocates one of these per `ZEPHLET_DEFINE(tick,
 * ...)` and passes its address as the `_data` arg. Framework treats it
 * as opaque; tick is the only reader/writer.
 */
struct tick_data {
	struct k_timer timer;
	bool is_running;
	bool is_ready;
	struct tick_config current_config;
};

/**
 * @brief Default init_fn for tick instances.
 *
 * Seeds `current_config` from `z->config` (or hard-coded defaults if
 * NULL), initialises the timer with `z` as user-data, and marks the
 * instance ready. Pass as the `_init` arg of ZEPHLET_DEFINE.
 */
int tick_init_fn(const struct zephlet *z);

/* ----- Synchronous RPC wrappers ---------------------------------------- */

/**
 * @brief Start the tick timer on instance @p z.
 *
 * Publishes a zephlet_call on z->channel.rpc. Under sync-listener
 * semantics this runs the dispatcher in the caller's thread; by the
 * time zbus_chan_pub() returns, call.return_code is set.
 *
 * @param z        Tick instance.
 * @param resp     ZephletStatus output, or NULL to discard.
 * @param timeout  zbus publish timeout.
 * @return 0 on success, -EALREADY if already running, -ENODEV if not
 *         ready, or a negative zbus error.
 */
int tick_start(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout);

/** @brief Stop the tick timer. */
int tick_stop(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout);

/** @brief Read current lifecycle status. */
int tick_get_status(const struct zephlet *z, struct zephlet_status *resp, k_timeout_t timeout);

/**
 * @brief Replace the current tick config.
 *
 * Validates that period_ms > 0 and (max_period_ms == 0 || period_ms <=
 * max_period_ms). On success the new config is stored; if the timer is
 * running it is restarted with the new period.
 */
int tick_config(const struct zephlet *z, const struct tick_config *req,
		struct tick_config *resp, k_timeout_t timeout);

/** @brief Read the current tick config. */
int tick_get_config(const struct zephlet *z, struct tick_config *resp, k_timeout_t timeout);

/* ----- Events helper --------------------------------------------------- */

/**
 * @brief Publish a tick_events message on z->channel.events.
 *
 * Zbus value-type channel: consumers receive a copy. ISR-safe when
 * called with K_NO_WAIT.
 */
int tick_emit(const struct zephlet *z, const struct tick_events *ev, k_timeout_t timeout);

/* ----- Readiness sugar ------------------------------------------------- */

/**
 * @brief Return true if the instance's get_status reports is_ready.
 *
 * Sugar over tick_get_status(). Blocks up to 100 ms for the sync call.
 */
bool tick_is_ready(const struct zephlet *z);

/* ----- Weak handler contract ------------------------------------------ *
 * Generator emits `__weak` defaults returning -ENOSYS. Developer
 * overrides in zlet_tick.c. Handler signatures follow the four wrapper
 * shapes driven by (req_is_empty, resp_is_empty). */

int tick_on_start(const struct zephlet *z, struct zephlet_status *resp);
int tick_on_stop(const struct zephlet *z, struct zephlet_status *resp);
int tick_on_get_status(const struct zephlet *z, struct zephlet_status *resp);
int tick_on_config(const struct zephlet *z, const struct tick_config *req,
		   struct tick_config *resp);
int tick_on_get_config(const struct zephlet *z, struct tick_config *resp);

#endif /* SRC_ZEPHLETS_TICK_ZLET_TICK_INTERFACE_H_ */
