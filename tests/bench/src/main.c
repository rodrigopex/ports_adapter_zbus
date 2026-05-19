/*
 * Zephlet latency benchmark — nrf52833dk_nrf52833.
 *
 * Measures four cases over N=1024 iterations each. Reports min / median
 * / p95 / max in cycles and ns at 64 MHz (15.625 ns/cyc).
 *
 *   #1 tick_get_status_impl(z, &st)         direct C call (floor)
 *   #2 tick_get_status(z, &st, K_NO_WAIT)   wrapper (zbus + dispatch + impl)
 *   #3 tick_config(z, &in, &out, K_NO_WAIT) wrapper w/ req+resp payload
 *   #4 zbus_chan_pub(&bare_chan, ...)       raw zbus floor (no zephlet)
 *
 * GPIO P0.13 (LED1) toggles around each measured call so a scope can
 * cross-check the cycle counter.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/zbus/zbus.h>

#include "zlet_tick.h"

/* ----- DWT cycle counter (Cortex-M3/M4/M7) ---------------------------- */

#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)
#define DEMCR       (*(volatile uint32_t *)0xE000EDFCUL)

#define DEMCR_TRCENA_Msk       (1UL << 24)
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)

static inline void dwt_enable(void)
{
	DEMCR |= DEMCR_TRCENA_Msk;
	DWT_CYCCNT = 0;
	DWT_CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t dwt_now(void)
{
	return DWT_CYCCNT;
}

/* ----- GPIO probe ----------------------------------------------------- */

static const struct gpio_dt_spec probe =
	GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), probe_gpios, 0);

/* ----- Tick zephlet instance ------------------------------------------ */

static struct tick_config tick_cfg = {
	.duration_ms = 1000,
	.period_ms   = 1000,
};
static struct tick_data tick_data_storage;
ZEPHLET_NEW(tick, bench_tick, &tick_cfg, &tick_data_storage, tick_init_fn);

/* ----- Bare zbus baseline channel ------------------------------------- */

static void noop_lis_fn(const struct zbus_channel *chan)
{
	(void)chan;
}
ZBUS_LISTENER_DEFINE(noop_lis, noop_lis_fn);

struct bench_msg {
	uint32_t a;
};

ZBUS_CHAN_DEFINE(bare_chan, struct bench_msg, NULL, NULL,
		 ZBUS_OBSERVERS(noop_lis), ZBUS_MSG_INIT(0));

/* ----- Sample buffer + stats ----------------------------------------- */

#define ITERS 1024
#define WARMUP 16
#define EFFECTIVE (ITERS - WARMUP)

static uint32_t samples[ITERS];

static int cmp_u32(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : (x > y);
}

struct stats {
	uint32_t min, median, p95, max;
};

static struct stats compute_stats(void)
{
	qsort(samples + WARMUP, EFFECTIVE, sizeof(samples[0]), cmp_u32);
	struct stats s = {
		.min    = samples[WARMUP],
		.median = samples[WARMUP + EFFECTIVE / 2],
		.p95    = samples[WARMUP + (EFFECTIVE * 95) / 100],
		.max    = samples[ITERS - 1],
	};
	return s;
}

/* ----- Per-iteration shape -------------------------------------------- */

#define MEASURE(call) do {                                                \
	for (int i = 0; i < ITERS; ++i) {                                  \
		gpio_pin_set_dt(&probe, 1);                                \
		uint32_t t0 = dwt_now();                                   \
		call;                                                      \
		uint32_t t1 = dwt_now();                                   \
		gpio_pin_set_dt(&probe, 0);                                \
		samples[i] = t1 - t0;                                      \
	}                                                                  \
} while (0)

/* ----- Result reporting ----------------------------------------------- */

#define NS_PER_CYC_X1000 15625UL  /* 15.625 ns at 64 MHz, ×1000 */

static uint32_t cyc_to_ns(uint32_t cyc)
{
	return (uint32_t)(((uint64_t)cyc * NS_PER_CYC_X1000) / 1000UL);
}

static void print_row(const char *label, struct stats s)
{
	printk("| %-44s | %5u | %5u | %5u | %5u | %6u | %6u | %6u | %6u |\n",
	       label,
	       s.min, s.median, s.p95, s.max,
	       cyc_to_ns(s.min), cyc_to_ns(s.median),
	       cyc_to_ns(s.p95), cyc_to_ns(s.max));
}

/* ----- Compiler barrier + sink to defeat DCE -------------------------- */

static volatile int sink;
#define BARRIER() __asm__ __volatile__("" ::: "memory")

int main(void)
{
	if (!gpio_is_ready_dt(&probe)) {
		printk("ERR: probe GPIO not ready\n");
		return -ENODEV;
	}
	gpio_pin_configure_dt(&probe, GPIO_OUTPUT_INACTIVE);

	dwt_enable();

	/* Wait for the SYS_INIT walker to settle and zephlet to be ready. */
	k_sleep(K_MSEC(50));
	if (!tick_is_ready(&bench_tick)) {
		printk("ERR: bench_tick not ready\n");
		return -ENODEV;
	}

	struct lifecycle_status st = {0};
	struct tick_config cfg_in = {.duration_ms = 1000, .period_ms = 1000};
	struct tick_config cfg_out = {0};
	struct bench_msg msg = {.a = 0};

	const struct zephlet *z = &bench_tick;

	unsigned int key = irq_lock();

	/* #1 — direct C call into the impl. */
	MEASURE({
		BARRIER();
		sink = tick_get_status_impl(z, &st);
		BARRIER();
	});
	struct stats s1 = compute_stats();

	/* #2 — full wrapper path: zbus pub → sync listener → dispatch → impl. */
	MEASURE({
		BARRIER();
		sink = tick_get_status(z, &st, K_NO_WAIT);
		BARRIER();
	});
	struct stats s2 = compute_stats();

	/* #3 — wrapper with req+resp payload. */
	MEASURE({
		BARRIER();
		sink = tick_config(z, &cfg_in, &cfg_out, K_NO_WAIT);
		BARRIER();
	});
	struct stats s3 = compute_stats();

	/* #4 — bare zbus_chan_pub to a sync listener with no-op fn. */
	MEASURE({
		BARRIER();
		sink = zbus_chan_pub(&bare_chan, &msg, K_NO_WAIT);
		BARRIER();
	});
	struct stats s4 = compute_stats();

	irq_unlock(key);

	printk("\n");
	printk("# Zephlet latency benchmark — nrf52833dk @ 64 MHz, DWT, IRQs locked\n");
	printk("Iters=%d (warmup %d dropped), GPIO probe=P0.13 (LED1)\n\n",
	       ITERS, WARMUP);
	printk("|                                              |       cycles            |          ns @ 64 MHz          |\n");
	printk("| measurement                                  |   min |   med |   p95 |   max |    min |    med |    p95 |    max |\n");
	printk("|----------------------------------------------|-------|-------|-------|-------|--------|--------|--------|--------|\n");
	print_row("#1 tick_get_status_impl(z, &st)  [C floor]", s1);
	print_row("#2 tick_get_status(z, &st, K_NO_WAIT)",     s2);
	print_row("#3 tick_config(z, &in, &out, K_NO_WAIT)",   s3);
	print_row("#4 zbus_chan_pub(&bare_chan, &msg)",        s4);
	printk("\n");
	printk("Derived (median):\n");
	printk("  zephlet+zbus overhead = #2 - #1 = %u cyc (%u ns)\n",
	       s2.median - s1.median,
	       cyc_to_ns(s2.median - s1.median));
	printk("  zephlet layer over plain zbus = #2 - #4 = %d cyc (%d ns)\n",
	       (int)(s2.median - s4.median),
	       (int)cyc_to_ns(s2.median > s4.median ? s2.median - s4.median : 0));
	printk("  payload-handling cost = #3 - #2 = %d cyc (%d ns)\n",
	       (int)(s3.median - s2.median),
	       (int)cyc_to_ns(s3.median > s2.median ? s3.median - s2.median : 0));
	printk("\n");
	(void)sink;
	return 0;
}
