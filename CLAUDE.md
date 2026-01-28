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
Underlying: `west build -d ./build -b mps2/an385 .`

## Architecture

**Ports & Adapters** on Zephyr RTOS via **zbus**.

**Components:**

1. Services (`modules/services/`): Domain logic, no direct dependencies
2. Adapters (`adapters/`): Compose services via channel bridging
3. Main (`src/main.c`): Lifecycle orchestration

**Two-Channel Pattern** per service:

- `chan_<service>_invoke`: Receives commands (START/STOP/CONFIG/etc)
- `chan_<service>_report`: Publishes status/events
Services listen (sync/async), publish to report channel.

### Service Structure: Three-File Pattern

```
modules/services/<service>/
├── <service>.h         # Data structs, API interface, inline functions
├── <service>.c         # Channels, dispatcher, registration
├── <service>_impl.c    # Business logic
├── <service>.proto     # Protobuf (nanopb)
├── CMakeLists.txt / Kconfig / zephyr/module.yml
```

**Key structures in `.h`:**

1. `<service>_data`: State with `k_spinlock` for thread safety
2. `<service>_api`: Function pointers (start/stop/config/get_status/get_config)
3. Inline client functions: `<service>_start(timeout)` → publishes to invoke channel

**Infrastructure `.c`:** Dispatcher routes invoke messages to API functions via switch/case on oneof tags. Registration via `<service>_set_implementation()`.

**Implementation `_impl.c`:** Business logic. Updates data under `K_SPINLOCK`, publishes reports. Ends with `SERVICE_DEFINE(<service>, init_fn, &api, &data)`.

**Shared (`service.h`):** `struct service` + `SERVICE_DEFINE()` macro enables runtime discovery via `STRUCT_SECTION_ITERABLE`.

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

Structure: `Msg<Service>Service { Config{}, Events{}, Invoke{oneof}, Report{oneof} }`

**Invoke oneof:** start/stop/get_status/config/get_config + custom commands
**Report oneof:** status/config + domain events

Import `"service.proto"` for `Empty`/`MsgServiceStatus`. Use `option (nanopb_fileopt).anonymous_oneof = true;`.
Query pattern: get_status/get_config trigger reports. Proto files collected via `PROTO_FILES_LIST`, compiled by nanopb.

## Adding Services

Use code generator:

1. Create `modules/services/<service>/` + write `<service>.proto`
2. Run: `just gen_service_files <service>` (generates `.h`, `.c`, `private/<service>_priv.h`, `_impl.c` template)
3. Complete `_impl.c`: Read TODO comments, implement logic with K_SPINLOCK, use `<service>_report_*()` helpers from private header, publish reports
4. Add `CMakeLists.txt` (append to PROTO_FILES_LIST, zephyr_library_sources)
5. Add `Kconfig` (CONFIG_<SERVICE>_SERVICE, select NANOPB/ZBUS)
6. Add `zephyr/module.yml` + register in root CMakeLists.txt
7. Enable in `prj.conf`: CONFIG_<SERVICE>_SERVICE=y
8. Build: `just c b r`

Generator never overwrites existing `_impl.c`. Reference: tick service.

## Adding Adapters

1. Create `adapters/<Origin>+<Destiny>_adapter.c` with listener function
2. Use `ZBUS_ASYNC_LISTENER_DEFINE()` + `ZBUS_CHAN_ADD_OBS()`
3. Add Kconfig + update adapters/CMakeLists.txt
4. Enable in prj.conf if needed

## Configuration

Via Kconfig in `prj.conf`:

- `CONFIG_<SERVICE>_SERVICE=y` / `CONFIG_<SERVICE>_LOG_LEVEL_DBG=y`
- `CONFIG_<ADAPTER>_ADAPTER=y`
Current: All services + debug logging enabled.

## Naming

- **Files:** `<service>.h/.c/_impl.c/.proto`, `<Origin>+<Destiny>_adapter.c`
- **Channels:** `chan_<service>_{invoke|report}`
- **Listeners:** `lis_<service>`, `lis_<origin>_to_<destiny>_adapter`
- **Messages:** `msg_<service>_{invoke|report}` (proto-generated)
- **Structs:** `<service>_data`, `<service>_api`, `SERVICE_DEFINE(<service>,...)`
- **Functions:** `<service>_<cmd>()` (inline), `static int <cmd>(service*)`, `<service>_init_fn()`, `<service>_set_implementation()`
- **Config:** `CONFIG_<SERVICE>_SERVICE`, `CONFIG_<ADAPTER>_ADAPTER`, `CONFIG_<SERVICE>_LOG_LEVEL_DBG`

## Data Flow

Init: Zephyr calls init_fn → registers impl → ready
Start: Client calls inline func → pub to invoke chan → dispatcher → api->start() → updates state under spinlock → pub to report chan
Runtime: Timer fires → pub event to report → adapter listens → calls another service
Query: get_config → dispatcher → api->get_config() → reads state → pub config report
Flow: invoke chan → dispatcher → API → state update (spinlock) → report chan → observers

## Principles

- **Loose Coupling:** Services communicate only via zbus channels
- **Separation:** Three files: .h (interface), .c (infra), _impl.c (logic)
- **Single Return:** Use `ret` variable, `goto end;` for errors
- **No Direct Deps:** Services don't include each other (except inline API)
- **Adapters:** Composition via adapters, not service modification
- **Type Safety:** Proto messages, API structs, inline functions
- **Thread Safety:** `K_SPINLOCK(&data->lock)` for all state access
- **Pluggable:** Multiple impls via registration
- **Async:** `ZBUS_ASYNC_LISTENER_DEFINE()` for adapters
- **Lifecycle:** All services: START/STOP/get_status/get_config
- **Auto Registration:** `STRUCT_SECTION_ITERABLE` for discovery

## Code Generation

### Protobuf (nanopb)

Build auto-collects `.proto` via `PROTO_FILES_LIST` → `zephyr_nanopb_sources()` → generates `.pb.h/.pb.c` in build dir. Don't edit.

### Service Generator

Python codegen (`modules/services/shared/codegen/`) creates `.h`, `.c`, `private/*_priv.h`, `_impl.c` from `.proto`.

**Generated:** .h (data/API/inline funcs), .c (channels/dispatcher), private header (report helpers), _impl.c template (TODO comments)
**Hand-written:** .proto (API def),_impl.c (business logic)

**Install:** `cd codegen && pip install -r requirements.txt` (proto-schema-parser, jinja2)

**Usage:** `just gen_service_files <service>` or manually:

```bash
python3 generate_service.py --proto ../../<s>/<s>.proto --output-dir ../../<s> --service-name <s> --module-dir <s> --generate-impl
```

Never overwrites existing _impl.c.

#### Workflow

**New service:** Write .proto → `just gen_service_files <s>` → complete _impl.c (read TODOs, use K_SPINLOCK, call `<s>_report_*()` helpers) → build

**Modify service:** Edit .proto → regenerate (without --generate-impl) → update _impl.c (add new funcs to api struct) → build

**Principles:** Never edit generated .h/.c. Proto is source of truth. Regeneration safe (never overwrites _impl.c).

#### RPC-Based Report Mapping

Generator parses `service` definitions to map RPC return types to report functions:

- `returns MsgServiceStatus` → `report_status()`
- `returns Config` → `report_config()`
- `returns Events (stream)` → `report_events()`

Validates RPC methods match Invoke/Report fields. Type-safe, self-documenting. Falls back to invoke fields if no service def.

#### How It Works

Parser (proto-schema-parser) → extracts service/invoke/report/config → builds context → Jinja2 templates (service.h/c/priv.h/impl.c.jinja) → write files.

#### Generated Structure

Proto oneof → API function pointers + inline client functions + dispatcher switch/case. Invoke fields become api struct methods, inline wrappers, and dispatcher routes.

#### Private Header

`--generate-impl` creates `private/<service>_priv.h` with inline report helpers: `<service>_report_status/config/events()`. Wraps `zbus_chan_pub` boilerplate.

**Type mapping:** MsgServiceStatus→status ptr, Config→config ptr, Empty→no params, CamelCase→snake_case (via camel_to_snake filter).

**Usage:** `#include "private/<service>_priv.h"` in _impl.c → call helpers instead of raw zbus_chan_pub. Cleaner, type-safe, private API.

#### nanopb Types

Use `struct` keyword: `struct msg_<service>_{invoke|report|config}`, compound literals need struct.

**Naming:** `MsgService.Invoke` → `msg_service_invoke`, CamelCase → snake_case (via camel_to_snake), tags: `MSG_SERVICE_INVOKE_START_TAG`, oneof: `which_<oneof_name>`.

#### Templates

Located: `codegen/templates/` (service.h/c/priv.h/impl.c.jinja). Jinja2 syntax. Custom filters: `|camel_to_snake` (BatteryState→battery_state), `|upper`, `|lower`. Modify templates to change all services.

#### _impl.c Template

Generated with TODO comments: includes, logging, resource definitions, function stubs (start/stop/config/get_status/get_config), static api/data structs, init_fn, SERVICE_DEFINE. Context-aware guidance for each function. Compiles immediately (stubs return 0). Never overwrites existing _impl.c.

#### Future

Auto CMake regen, generate CMakeLists/Kconfig, test stubs, --watch mode.
