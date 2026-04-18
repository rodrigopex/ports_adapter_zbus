# Ports & Adapters on Zephyr RTOS (zephlet v0.3 example)

Reference app for the [zephlet](https://github.com/rodrigopex/zephlet) framework. Three domain-isolated zephlets (tick, ui, tampering) talk exclusively over zbus, composed into behavior by a 40-line `src/adapters.c`.

## Model

Each zephlet **instance** owns two zbus channels:

- **`rpc`** — pointer channel, synchronous. `<type>_<rpc>(z, [req], [resp], timeout)` wrappers invoke the dispatcher in the caller's thread and return the handler's rc directly.
- **`events`** — zbus value-typed channel, asynchronous. Producers call `<type>_emit(z, &ev, timeout)`; consumers use the framework's `ZEPHLET_EVENTS_LISTENER(instance, type, callback)` macro (work-queue-backed).

Instances are declared with `ZEPHLET_DEFINE(type, name, cfg, data, init)` and auto-discovered at boot via `STRUCT_SECTION_ITERABLE`. Multiple instances per type are supported.

## Components

- `src/zephlets/tick/` — k_timer → periodic `tick_events` with timestamp.
- `src/zephlets/ui/` — `blink` RPC increments a counter and emits `ui_events`.
- `src/zephlets/tampering/` — `force_tampering` RPC emits `tampering_events` with `proximity_tamper_detected=true`.
- `src/adapters.c` — two `ZEPHLET_EVENTS_LISTENER` blocks wiring tick + tampering events to `ui_blink`.
- `src/main.c` — instantiates one of each (`tick_instance`, `ui_instance`, `tampering_instance`) and drives lifecycle.

## Build & run (mps2/an385 / QEMU)

```bash
west init -l .
west update --narrow --fetch-opt=--depth=1
west packages pip --install

just c b r   # clean, build, run under qemu
just test    # twister — tick + ui + tampering integration suites
```

Expected output excerpt:

```
Example project running on a mps2/an385 board.
UI is running
Tick is running
<dbg> adapters: on_tick_event: tick event @1000 -> ui_blink
<inf> zlet_ui: ui_instance: blink #1
...
<dbg> adapters: on_tampering_event: tampering proximity @5010 -> ui_blink
<inf> zlet_ui: ui_instance: blink #6
```

## Why look at this

- Pure **domain isolation**: zephlets don't `#include` each other. Wiring happens only in `main.c` (instance definitions) and `adapters.c` (event subscriptions).
- **Sync RPC without gymnastics**: pointer-in-channel + zbus sync-listener gives identity-equals-address and in-place mutation in the caller's thread.
- **No framework-mandated layout**: the `src/zephlets/` and `src/adapters.c` paths are *this app's* choice; the zephlet framework takes no position.

## Configuration

`prj.conf`:

```
CONFIG_ZEPHLET_TICK=y
CONFIG_ZEPHLET_UI=y
CONFIG_ZEPHLET_TAMPERING=y
CONFIG_LOG=y
CONFIG_ZEPHLET_TICK_LOG_LEVEL_DBG=y
CONFIG_ZEPHLET_UI_LOG_LEVEL_DBG=y
CONFIG_ZEPHLET_TAMPERING_LOG_LEVEL_DBG=y
CONFIG_ASSERT=y
```

No adapter Kconfig — `src/adapters.c` is unconditional app code. In projects where the adapter targets optional zephlets, the user guards at CMake level (`if(CONFIG_ZEPHLET_X AND CONFIG_ZEPHLET_Y) target_sources(...)`).

## Reference

- Framework: [`../modules/lib/zephlet/`](../modules/lib/zephlet/) (via `west.yml`).
  - [`CLAUDE.md`](../modules/lib/zephlet/CLAUDE.md) — full architecture reference.
  - [`README.md`](../modules/lib/zephlet/README.md) — quick start.
- Full v0.3 refactor plan: [`docs/REFACTOR_V3_PLAN.md`](docs/REFACTOR_V3_PLAN.md).

## License

Apache-2.0
