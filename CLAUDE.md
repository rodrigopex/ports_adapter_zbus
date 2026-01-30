# CLAUDE.md

## Plans

- Extremely concise. Sacrifice grammar.
- List unresolved questions at end.
- Do not consider the folders ignored by the `.gitignore` for analysis and uploads. The only exception is the explicit reference to it using `@`.
- Keep the tokens usage as small as possible without sacrificing clarity.
- Tell me when I am doing something wrong that is using too many tokens.

## Build Commands

Uses `just` (mps2/an385 board):

- `just b` / `just c b` / `just b r` / `just c b r`
- `just config` / `just ds` / `just da`
- `just gen_service_files <service>`
- `just new_adapter_interactive` / `just new_adapter <origin> <dest>`
Underlying: `west build -d ./build -b mps2/an385 .`

## Architecture

**Ports & Adapters** on Zephyr RTOS via **zbus**.

**Components:**

1. Services (`services/`): Domain logic, no direct dependencies
2. Adapters (`adapters/`): Compose services via channel bridging
3. Main (`src/main.c`): Lifecycle orchestration

**Two-Channel Pattern** per service:

- `chan_<service>_invoke`: Receives commands (START/STOP/CONFIG/etc)
- `chan_<service>_report`: Publishes status/events
Services listen (sync/async), publish to report channel.

### Service Structure

**Three files:** _interface.h (data/API/inline funcs), _interface.c (channels/dispatcher/registration), .c (logic). Plus: .proto, CMakeLists, Kconfig, module.yml

**_interface.h:** `<service>_data` (state+spinlock), `<service>_api` (func ptrs), inline `<service>_start(timeout)` → pub invoke chan
**_interface.c:** Dispatcher (switch/case oneof tags), `<service>_set_implementation()`
**.c:** Logic, K_SPINLOCK updates, pub reports, ends with `SERVICE_DEFINE(<service>, init_fn, &api, &data)`
**Shared:** `struct service` + `SERVICE_DEFINE()` → `STRUCT_SECTION_ITERABLE` discovery

### Services

- **shared**: Common types (`Empty`, `MsgServiceStatus`), `SERVICE_DEFINE()` macro
- **tick** (REFERENCE): K_TIMER, full three-file pattern, spinlocks, timed events
- **ui**: BLINK commands
- **battery** (GENERATED): Code gen example, custom BatteryState type
- **tamper_detection**: Security monitoring

### Adapters

Listen to service report → invoke another service. No direct coupling.
Example: `TickService+UiService_adapter.c` listens tick reports → calls `ui_service_blink()`.
Uses `ZBUS_ASYNC_LISTENER_DEFINE()` + `ZBUS_CHAN_ADD_OBS()`. Kconfig toggleable.

### Protobuf (nanopb)

`Msg<Service>Service { Config{}, Events{}, Invoke{oneof}, Report{oneof} }`. Invoke: start/stop/get_status/config/get_config+custom. Report: status/config+events. Import "service.proto" for Empty/MsgServiceStatus. Use `option (nanopb_fileopt).anonymous_oneof = true`. Query: get_status/get_config → reports. PROTO_FILES_LIST → nanopb.

## Build System

**CMakeLists:** Root defines EXTRA_ZEPHYR_MODULES, collects protos, runs nanopb. shared/ exposes `${CMAKE_BINARY_DIR}/services` globally. Each service calls `zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")` after `zephyr_service_generate()`. Self-managed visibility, no manual root updates. Copier template auto-includes pattern.

## Creating Services

**Workflow:** `just new_service_interactive` → Copier creates .proto/.c/CMakeLists/Kconfig/module.yml → Edit .proto (Config/Events/RPCs) → Edit .c (TODOs) → Add to root CMakeLists EXTRA_ZEPHYR_MODULES → Enable CONFIG_<SERVICE>_SERVICE=y → `just c b r`

**Build-time codegen:** .proto changes → auto-regen _interface.h/_interface.c/<service>.h/.pb.h/.pb.c (like nanopb). Manual: `just gen_service_files <service>`

**Files:** VCS: .proto, .c, CMakeLists, Kconfig, module.yml. Build: _interface.h, _interface.c, <service>.h, .pb.h, .pb.c

## Creating Adapters

**Workflow:**
1. `just new_adapter_interactive` → select origin/dest → select report fields
2. Implement TODO comments in generated `<Origin>Service+<Dest>Service_adapter.c`
3. `just c b r` → Enabled by default (CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER=y)

**Non-interactive:** `just new_adapter <origin> <dest>` (generates all report fields)

Auto-generates: adapter.c, Kconfig entry, CMakeLists.txt entry. Parses protos for Report fields (origin) + Invoke fields (dest, adds API suggestions to TODOs). See Code Generation → Adapter Generator for details.

## Configuration

Via Kconfig in `prj.conf`:

- `CONFIG_<SERVICE>_SERVICE=y` / `CONFIG_<SERVICE>_LOG_LEVEL_DBG=y`
- `CONFIG_<ADAPTER>_ADAPTER=y`
Current: All services + debug logging enabled.

## Naming

- **Files:** `<service>_service_interface.h/_interface.c/.c/.proto`, `<Origin>+<Destiny>_adapter.c`
- **Channels:** `chan_<service>_{invoke|report}`
- **Listeners:** `lis_<service>`, `lis_<origin>_to_<destiny>_adapter`
- **Messages:** `msg_<service>_{invoke|report}` (proto-generated)
- **Structs:** `<service>_data`, `<service>_api`, `SERVICE_DEFINE(<service>,...)`
- **Functions:** `<service>_<cmd>()` (inline), `static int <cmd>(service*)`, `<service>_init_fn()`, `<service>_set_implementation()`
- **Config:** `CONFIG_<SERVICE>_SERVICE`, `CONFIG_<ADAPTER>_ADAPTER`, `CONFIG_<SERVICE>_LOG_LEVEL_DBG`

## Data Flow

Init: init_fn → register impl. Start: inline func → invoke chan → dispatcher → api->start() → spinlock update → report chan. Runtime: timer → report → adapter → service. Query: get_config → dispatcher → api → read → pub. Flow: invoke → dispatcher → API → spinlock → report → observers.

## Principles

Loose coupling (zbus only). Separation (.h/.c/_impl.c). Single return (`ret`, `goto end`). No direct deps (except inline API). Composition via adapters. Type safety (proto/structs). Thread safety (K_SPINLOCK). Pluggable (registration). Async (ZBUS_ASYNC_LISTENER). Lifecycle (START/STOP/get_status/get_config). Auto discovery (STRUCT_SECTION_ITERABLE).

## Code Generation

### Protobuf (nanopb)

PROTO_FILES_LIST → zephyr_nanopb_sources() → .pb.h/.pb.c (build dir). Don't edit.

### Service Generator

Script: `services/shared/codegen/generate_service.py`. Proto → _interface.h/_interface.c/<service>.h/.c template. Never overwrites .c.

**Generated:** _interface.h (data/API/inline funcs), _interface.c (channels/dispatcher), <service>.h (report helpers), .c template
**Hand-written:** .proto, .c (business logic)

**Workflow:**
- New: Write .proto → `just gen_service_files <s>` → complete .c → build
- Modify: Edit .proto → regen (no --generate-impl) → update .c → build

**Process:** proto-schema-parser extracts service/invoke/report/config → Jinja2 templates (service.h/c/priv.h/impl.c.jinja) → files. Proto oneof → API func ptrs + inline funcs + dispatcher switch/case.

**RPC mapping:** `returns MsgServiceStatus` → `report_status()`, `returns Config` → `report_config()`, `returns Events (stream)` → `report_events()`. Validates RPC vs Invoke/Report fields.

**Helper header:** `<service>.h` with `<service>_report_*()` helpers. Wraps zbus_chan_pub. Type mapping: MsgServiceStatus→ptr, Config→ptr, Empty→no params, CamelCase→snake_case.

**nanopb types:** Use `struct msg_<service>_{invoke|report|config}`. Naming: `MsgService.Invoke` → `msg_service_invoke`, tags: `MSG_SERVICE_INVOKE_START_TAG`, oneof: `which_<oneof_name>`.

**Templates:** `codegen/templates/` (Jinja2). Filters: `|camel_to_snake`, `|upper`, `|lower`.

### Adapter Generator

Script: `services/shared/codegen/generate_adapter.py`. Parses origin proto (Report fields) + dest proto (Invoke fields) → generates adapter.c + updates Kconfig/CMakeLists.txt.

**Process:** Scans `services/*/` for protos → proto-schema-parser extracts Report/Invoke oneofs → Jinja2 templates (adapter.c.jinja, adapter_kconfig.jinja) → writes files. Duplicate detection, graceful degradation.

**Interactive:** User selects report fields to handle. **Non-interactive:** All fields.
**API suggestions:** Dest Invoke fields → TODO comments (`/* Available ui commands: start, stop */`)

**Templates:** `codegen/templates/` - adapter.c.jinja (listener+switch/case+ZBUS macros), adapter_kconfig.jinja (default y, depends on both services)

**Auto-updates:** Kconfig (before `module =` with proper blank line spacing), CMakeLists.txt (after last zephyr_library_sources). Manual fallback on failure.

**Naming:** File=`<Origin>Service+<Dest>Service_adapter.c`, Config=`CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER`, Listener=`lis_<origin>_to_<dest>_adapter`
