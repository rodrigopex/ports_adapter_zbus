# CLAUDE.md

## Plans

- Extreme concision. Kill grammar.
- End w/ unresolved questions list.
- Ignore `.gitignore`d paths unless `@`-referenced.
- Minimize tokens w/o losing clarity. Flag my token-wasteful asks.

## Sibling repo (always in scope)

`../modules/lib/zephlet/` = infra. Co-edited. Proto/codegen/template changes go there; app changes here. Commits that span repos → commit both. Full infra docs: `../modules/lib/zephlet/CLAUDE.md` + `README.md`. West cmds loaded via `west.yml`.

## Commands

West (from infra): `west zephlet new [-o dir] [-n -d -a]`, `west zephlet new-adapter` (prints recipe), `west zephlet gen <zephlet_dir>`.
Build (`just`, mps2/an385): `just b`, `just c b`, `just b r`, `just c b r`, `just config`, `just ds`, `just da`.
Test: `just test` (twister over each zephlet's `tests/integration/`).

## Architecture (v0.3)

Ports+Adapters on Zephyr/zbus.

**Two channels per zephlet instance:**

- `chan_<instance>_rpc` — pointer (`struct zephlet_call *`), exactly one sync listener (`lis_<type>`). Wrapper publishes, listener dispatches in caller thread, wrapper returns `call.return_code` directly.
- `chan_<instance>_events` — zbus value-typed (`struct <type>_events`). Async fan-out via `ZEPHLET_EVENTS_LISTENER` (wraps `ZBUS_ASYNC_LISTENER_DEFINE`).

**Non-singleton.** Instances declared with `ZEPHLET_DEFINE(type, name, cfg, data, init)`. Multiple per type OK.

**Layout is user choice.** The app happens to put zephlets under `src/` and adapters at `src/adapters.c`, but the framework doesn't require it.

### Zephlets

- `src/tick/`   — periodic k_timer → emits `tick_events` with timestamp.
- `src/ui/`     — blink counter (on_blink increments + emits); config carries user_button_long_press_duration.
- `src/tampering/` — `force_tampering` RPC emits `tampering_events` with `proximity_tamper_detected=true`.

Each has `<prefix>.{proto,h,c}` (user) + `<prefix>_interface.{h,c}` (generated in build dir). User `.h` declares `struct <type>_data` with framework-standard fields first (is_running, is_ready, current_config) then custom state.

### Adapters

`src/adapters.c` — one file, two `ZEPHLET_EVENTS_LISTENER` blocks:

- `tick_instance` events → `ui_blink(&ui_instance, K_MSEC(100))`.
- `tampering_instance` events (filtered on `proximity_tamper_detected`) → `ui_blink(...)`.

Callbacks run from the system workqueue so they can safely call back into other zephlets' RPCs with real timeouts. No framework codegen.

## Per-zephlet API shape

- Wrappers: `<type>_<rpc>(z, [req], [resp], timeout)` → int rc (0 or -errno). Four shapes driven by (req_is_empty, resp_is_empty). `resp == NULL` discards.
- Handlers: weak `__weak int <type>_on_<rpc>(...)` returns `-ENOSYS` by default; user overrides in `<prefix>.c`.
- Emit: `<type>_emit(z, &ev, timeout)`.
- Readiness: `<type>_is_ready(z)` sugar over `get_status`.

## Proto (nanopb)

`message <Type> { message Config { ... } message Events { ... } }`. Service `<Type>Api` with lifecycle + custom RPCs. Import `zephlet.proto` for `Empty` + `Lifecycle.Status`. `option (nanopb_fileopt).long_names = false;` — required. No oneofs, no `extends_base`, no reserved field numbers. Service name must differ from the outer message name (protoc collision).

## Kconfig (prj.conf)

`CONFIG_ZEPHLET_TICK=y`, `CONFIG_ZEPHLET_UI=y`, `CONFIG_ZEPHLET_TAMPERING=y`, `CONFIG_ZEPHLET_<Z>_LOG_LEVEL_DBG=y`. No adapter Kconfigs — adapters.c is unconditional app code.
