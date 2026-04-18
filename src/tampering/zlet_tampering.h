#ifndef SRC_ZEPHLETS_TAMPERING_ZLET_TAMPERING_H_
#define SRC_ZEPHLETS_TAMPERING_ZLET_TAMPERING_H_

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "zlet_tampering_interface.h"

/**
 * @file
 * @brief User-owned types and init_fn for the `tampering` zephlet.
 */

struct tampering_data {
	/* ----- Framework-standard fields (keep first, in this order) ----- */
	bool is_running;
	bool is_ready;
	struct tampering_config current_config;
	/* ----- Custom fields below ----- */
};

int tampering_init_fn(const struct zephlet *z);

#endif /* SRC_ZEPHLETS_TAMPERING_ZLET_TAMPERING_H_ */
