# Ports & Adapters on Zephyr RTOS

## Overview

Implementation of the Ports & Adapters architectural pattern on Zephyr RTOS using zbus for inter-component communication. Demonstrates loose coupling, domain isolation, and composition through channel bridging.

## Architecture

**Components:**

1. **Zephlets** (`src/zephlets/`): Domain logic modules with no direct dependencies
2. **Adapters** (`src/adapters/`): Compose zephlets via channel bridging
3. **Main** (`src/main.c`): Lifecycle orchestration

**Two-Channel Pattern:**

Each zephlet exposes two zbus channels:

- `chan_<zephlet>_invoke`: Receives commands (START, STOP, CONFIG, get_status, get_config, custom commands)
- `chan_<zephlet>_report`: Publishes status updates and events

Zephlets listen to invoke channels (sync/async), execute logic, and publish to report channels. Adapters listen to report channels and invoke other zephlets, creating composition without direct coupling.

**Code Generation:**

Protobuf definitions (`zlet_<name>.proto`) drive automatic generation of interfaces, channels, dispatchers, and API skeletons via `generate_zephlet.py`. Business logic resides in hand-written `zlet_<name>.c` files.

## Current Components

**Zephlets:**

- `shared`: Common types (`Empty`, `MsgZephletStatus`), `ZEPHLET_DEFINE()` macro
- `tick`: Fully implemented - K_TIMER-based timed events with spinlock protection
- `ui`: Generated template (pending implementation)
- `battery`: Generated template with custom BatteryState type (pending implementation)
- `position`: Generated template (pending implementation)
- `storage`: Generated template (pending implementation)

**Adapters:**

- Base logging: `base_adapter.c` registers shared logging module for all adapters
- `Tick+Ui_zlet_adapter.c`: Fully implemented - listens to tick reports, invokes ui blink
- Additional adapters: Battery+Storage, Position+Battery, Tick+Storage (generated)

## Build Commands

Uses `just` for builds (target: mps2/an385). For zephlet/adapter creation, use `west zephlet` commands directly.

```console
just b              # Build
just c b            # Clean build
just b r            # Build and run
just c b r          # Clean build and run
just config         # Menuconfig
just ds             # Device tree symbols
just da             # Device tree addresses
```

**Code Generation:**

```console
west zephlet gen <zephlet>   # Regenerate interfaces (requires build/modules/<zephlet>_zephlet, run from any directory)
```

**Testing:**

```console
just test_unit                     # Run unit tests
just test_integration             # Run integration tests
just test_zephlet tick            # Run tick zephlet integration tests
```

**Integration Tests:**

Each zephlet includes `tests/integration/` with 6 core lifecycle tests:

- start, stop, get_status, lifecycle cycle
- Config tests (get_config, config) if zephlet has config
- Run via: `just test_zephlet <zephlet_name>`
- Example: `just test_zephlet tick` runs tick's integration tests

## Workflows

**Creating Zephlets:**

```console
west zephlet new [-n NAME] [-d DESC] [-a AUTHOR]
```

Interactive/non-interactive. Edit .proto (Config/Events/RPCs), run `just b` to bootstrap .c, implement TODOs, add to root CMakeLists EXTRA_ZEPHYR_MODULES, enable CONFIG_ZEPHLET_<ZEPHLET>=y, rebuild.

**Creating Adapters:**

```console
west zephlet new-adapter [-o ORIGIN] [-d DEST] [-i]
```

Interactive (-i) field selection or non-interactive (all fields). Auto-generates adapter.c, updates Kconfig/CMakeLists. Implement TODOs, rebuild. Auto-enabled via CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER=y.

## Key Concepts

**Lifecycle Pattern:**

All zephlets support standard commands: START, STOP, get_status, get_config, config. Custom commands extend beyond reserved field numbers (Invoke 7+, Report 4+).

**Protobuf (nanopb):**

Messages follow `MsgZlet<Zephlet>` pattern with Config, Events, Invoke (oneof), Report (oneof) fields. Import "zephlet.proto" for shared types. Use `option (nanopb_fileopt).anonymous_oneof = true`.

**Thread Safety:**

Zephlet state protected by K_SPINLOCK. All state modifications acquire spinlock.

**Naming Conventions:**

| Element           | Pattern                   | Example                          |
|-------------------|---------------------------|----------------------------------|
| Source files      | zlet_<name>.{proto,c}     | zlet_tick.proto, zlet_tick.c     |
| Generated files   | zlet_<name>_interface.h/c | zlet_tick_interface.h            |
| Adapter files     | <Origin>+<Dest>_adapter.c | Tick+Ui_zlet_adapter.c           |
| Channels          | chan_<zephlet>_{invoke\|report} | chan_tick_invoke           |
| Config            | CONFIG_ZEPHLET_<ZEPHLET>  | CONFIG_ZEPHLET_TICK              |

**Data Flow:**

1. **Init:** init_fn registers implementation via `ZEPHLET_DEFINE()`
2. **Start:** Inline function publishes to invoke channel
3. **Dispatch:** Dispatcher routes to API function pointer
4. **Execute:** API function updates state under spinlock, publishes to report channel
5. **Compose:** Adapter listeners observe reports, invoke other zephlets

## Configuration

Enable zephlets in `prj.conf`:

```kconfig
CONFIG_ZEPHLET_<ZEPHLET>=y
CONFIG_ZEPHLET_<ZEPHLET>_LOG_LEVEL_DBG=y
```

**Adapter Dependencies:** Both origin and destination zephlets must be enabled for adapter to function.

**Adapters:** Enabled by default when generated. To disable a specific adapter:

```kconfig
CONFIG_<ORIGIN>_TO_<DEST>_ADAPTER=n
```

**Current Configuration:** All zephlets enabled with debug logging (CONFIG_ZEPHLET_*_LOG_LEVEL_DBG=y). All generated adapters enabled.

## References

See `CLAUDE.md` for detailed architecture documentation, build system internals, code generation details, and development principles.

Underlying build system: `west build -d ./build -b mps2/an385 .`
