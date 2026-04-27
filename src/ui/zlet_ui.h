#ifndef SRC_ZEPHLETS_UI_ZLET_UI_H_
#define SRC_ZEPHLETS_UI_ZLET_UI_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "zlet_ui_interface.h"

/**
 * @file
 * @brief User-owned types and init_fn for the `ui` zephlet.
 */

/**
 * @brief Mutable per-instance ui data.
 */
struct ui_data {
	/* ----- Framework-standard fields (keep first, in this order) ----- */
	bool is_running;
	bool is_ready;
	/* ----- Custom fields below ----- */
	uint32_t blink_counter;
};

/**
 * @brief Default init_fn for ui instances.
 */
int ui_init_fn(const struct zephlet *z);

#endif /* SRC_ZEPHLETS_UI_ZLET_UI_H_ */
