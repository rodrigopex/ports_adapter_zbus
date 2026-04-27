# Ports & Adapters on Zephyr RTOS (zephlet v0.3 example)

Reference app for the [zephlet](https://github.com/rodrigopex/zephlet) framework. Three domain-isolated zephlets (tick, ui, tampering) talk exclusively over zbus, composed into behavior by a 40-line `src/policies.c`.

## Model

Each zephlet **instance** owns two zbus channels:

- **`command`** — pointer channel, synchronous. `<type>_<cmd>(z, [req], [resp], timeout)` wrappers invoke the dispatcher in the caller's thread and return the handler's rc directly.
- **`events`** — zbus value-typed channel, asynchronous. Producers call `<type>_emit(z, &ev, timeout)`; consumers use the framework's `ZEPHLET_EVENTS_LISTENER(instance, type, callback)` macro (work-queue-backed).

Instances are declared with `ZEPHLET_NEW(type, name, cfg, data, init)` and auto-discovered at boot via `STRUCT_SECTION_ITERABLE`. Multiple instances per type are supported.

## Components

- `src/tick/` — k_timer → periodic `tick_events` with timestamp.
- `src/ui/` — `blink` command increments a counter and emits `ui_events`.
- `src/tampering/` — `force_tampering` command emits `tampering_events` with `proximity_tamper_detected=true`.
- `src/policies.c` — two `ZEPHLET_EVENTS_LISTENER` blocks wiring tick + tampering events to `ui_blink`.
- `src/main.c` — instantiates one of each and drives lifecycle.

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
<dbg> policies: on_tick_event: tick event @1000 -> ui_blink
<inf> zlet_ui: ui_fake_impl: blink #1
...
<dbg> policies: on_tampering_event: tampering proximity @5010 -> ui_blink
<inf> zlet_ui: ui_fake_impl: blink #6
```

## Why look at this

- Pure **domain isolation**: zephlets don't `#include` each other. Wiring happens only in `main.c` (instance definitions) and `policies.c` (event subscriptions).
- **Sync command without gymnastics**: pointer-in-channel + zbus sync-listener gives identity-equals-address and in-place mutation in the caller's thread.
- **No framework-mandated layout**: this app's flat `src/<name>/` zephlet dirs and `src/policies.c` are *this app's* choice; the zephlet framework takes no position.

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

No policies Kconfig — `src/policies.c` is unconditional app code. In projects where the policy targets optional zephlets, the user guards at CMake level (`if(CONFIG_ZEPHLET_X AND CONFIG_ZEPHLET_Y) target_sources(...)`).

## Reference

- Framework: [`../modules/lib/zephlet/`](../modules/lib/zephlet/) (via `west.yml`).
  - [`CLAUDE.md`](../modules/lib/zephlet/CLAUDE.md) — full architecture reference.
  - [`README.md`](../modules/lib/zephlet/README.md) — quick start.
- Full v0.3 refactor plan: [`docs/REFACTOR_V3_PLAN.md`](docs/REFACTOR_V3_PLAN.md).

## License

Apache-2.0
