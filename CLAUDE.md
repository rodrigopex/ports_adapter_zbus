# CLAUDE.md

## Plans

- Extremely concise. Sacrifice grammar.
- List unresolved questions at end.
- Do not consider the folders ignored by the `.gitignore` for analysis and uploads. The only exception is the explicit reference to it using `@`.
- Keep the tokens usage as small as possible without sacrificing clarity.
- Tell me when I am doing something wrong that is using too many tokens.

## Zephlet Infrastructure

**Infrastructure documentation:** See `modules/lib/zephlet/CLAUDE.md` and `modules/lib/zephlet/README.md`
**West commands:** Provided by zephlet module (loaded via west.yml)

## Commands

**West Extensions** (from zephlet module):
- `west zephlet new [-n NAME] [-d DESC] [-a AUTHOR]` - Create zephlet (interactive if no args)
- `west zephlet new-adapter [-o ORIGIN] [-d DEST] [-i]` - Create adapter (-i for interactive)
- `west zephlet gen ZEPHLET` - Regenerate interface files (needs `build/modules/<zephlet>_zephlet`)

**Build** (`just`, mps2/an385):
- `just b` / `just c b` / `just b r` / `just c b r`
- `just config` / `just ds` / `just da`

## Architecture

**Ports & Adapters** on Zephyr RTOS via **zbus** (infrastructure from zephlet module).

**Components:**

1. Zephlets (`src/zephlets/`): Domain logic, no direct dependencies
2. Adapters (`src/adapters/`): Compose zephlets via channel bridging
3. Main (`src/main.c`): Lifecycle orchestration

### Application Zephlets

**All zephlets use result API** (correlation_id, return_code, has_result flag). API functions return int, fill `ctx->response`. Interface handles publishing. **Proto field validation** enforces reserved ranges at build time. **nanopb options:** `anonymous_oneof = true` + `long_names = false` (shorter C symbols).

- **tick** (REFERENCE): Full implementation - init sets is_ready, start/stop controls is_running, timer uses _async(), `<zephlet>_context` pattern
- **ui**: Blink command, demonstrates async events + context pattern

### Application Adapters

**Reference:** `Tick+Ui_zlet_adapter.c` - listens tick reports, checks has_result flag, extracts correlation_id/return_code, distinguishes responses from async events.
**base_adapter.c:** Registers shared logging module for all adapters.

## Configuration

Via Kconfig in `prj.conf`:

- `CONFIG_ZEPHLET_<ZEPHLET>=y` / `CONFIG_ZEPHLET_<ZEPHLET>_LOG_LEVEL_DBG=y`
- `CONFIG_<ADAPTER>_ADAPTER=y`
  Current: All zephlets + debug logging enabled.
