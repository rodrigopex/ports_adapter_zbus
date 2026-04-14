# CLAUDE.md

## Plans

- Extreme concision. Kill grammar.
- End w/ unresolved questions list.
- Ignore `.gitignore`d paths unless `@`-referenced.
- Minimize tokens w/o losing clarity. Flag my token-wasteful asks.

## Sibling repo (always in scope)

`../modules/lib/zephlet/` = infra. Co-edited. Proto/codegen/template changes go there; app changes here. Commits that span repos → commit both. Full infra docs: `../modules/lib/zephlet/CLAUDE.md` + `README.md`. West cmds loaded via `west.yml`.

## Commands

West (from infra): `west zephlet new [-n -d -a]`, `west zephlet new-adapter [-o -d -i]`, `west zephlet gen Z` (needs `build/modules/<z>_zephlet`).
Build (`just`, mps2/an385): `just b`, `just c b`, `just b r`, `just c b r`, `just config`, `just ds`, `just da`.
Test: `just test` (twister, src/zephlets).

## Architecture

Ports+Adapters on Zephyr/zbus. Layout: `src/zephlets/` (domain, no deps), `src/adapters/` (compose via channel bridge), `src/main.c` (lifecycle).

### Zephlets

All use result API: correlation_id, return_code, has_result. API fns = `int (struct <z>_context *)`, fill `ctx->response`; interface publishes. Proto reserved ranges validated at build. nanopb opts: `anonymous_oneof=true`, `long_names=false`.

- **tick** = REFERENCE — init sets is_ready; start/stop toggles is_running; timer uses `_async()`; `<z>_context` pattern.
- **ui** — blink cmd; async events + context.

### Adapters

Reference: `Tick+Ui_zlet_adapter.c` — listens tick report, checks has_result, extracts correlation_id+return_code, separates responses from async events.
`base_adapter.c` — `LOG_MODULE_REGISTER(adapter, ...)` shared by all adapters.

## Kconfig (prj.conf)

`CONFIG_ZEPHLET_<Z>=y`, `CONFIG_ZEPHLET_<Z>_LOG_LEVEL_DBG=y`, `CONFIG_<O>_TO_<D>_ADAPTER=y`. Current: all zephlets + debug.
