# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

This project uses `just` as a command runner. Common commands:

```bash
# Build the project
just build                    # or: just b

# Clean and rebuild
just clean build             # or: just c b

# Build and run in QEMU
just build run               # or: just b r

# Clean, build, and run (minimal workflow)
just minimal                 # or: just c b r

# Rebuild without cleaning
just rebuild                 # or: just bb

# Open menuconfig
just config

# Debug with QEMU
just debugserver_qemu        # or: just ds
just attach                  # or: just da (in separate terminal)
```

All commands use the `mps2/an385` board by default (ARM Cortex-M3).

Underlying build system is Zephyr's `west`:

```bash
west build -d ./build -b mps2/an385 . -- -DCONFIG_QEMU_ICOUNT=n
west build -t run
west build -t menuconfig
```

## Architecture Overview

This project implements **Ports & Adapters (Hexagonal Architecture)** on Zephyr RTOS, using **zbus** (Zephyr's message bus) for all inter-component communication.

### Core Architectural Components

1. **Services** (`modules/services/`): Self-contained domain services with no direct dependencies on each other
2. **Adapters** (`adapters/`): Translation layer that composes services by bridging their communication channels
3. **Main Application** (`src/main.c`): Orchestrates service lifecycle and monitors system status

### Communication Pattern: Two-Channel Per Service

Every service exposes exactly two zbus channels:

- **Invoke Channel** (`chan_<service>_invoke`): Receives commands/requests (e.g., START, STOP, CONFIG)
- **Report Channel** (`chan_<service>_report`): Publishes status updates and domain events

Example from tick service:

```c
ZBUS_CHAN_DEFINE(chan_tick_service_invoke, struct msg_tick_service_invoke, ...);
ZBUS_CHAN_DEFINE(chan_tick_service_report, struct msg_tick_service_report, ...);
```

Services listen to their invoke channel using `ZBUS_LISTENER_DEFINE()` (sync) or `ZBUS_ASYNC_LISTENER_DEFINE()` (async) and publish to their report channel.

### Service Structure Pattern

Each service follows this structure:

```
modules/services/<service>/
├── <service>.h              # Public API (inline wrappers for zbus_chan_pub)
├── <service>.c              # Implementation with channel definitions and listener
├── <service>.proto          # Protobuf message definitions (nanopb)
├── CMakeLists.txt           # Build configuration
├── Kconfig                  # Feature flags (CONFIG_<SERVICE>_SERVICE)
└── zephyr/module.yml        # Zephyr module metadata
```

Services expose type-safe inline functions in their headers:

```c
static inline int tick_service_start(uint32_t delay_ms, k_timeout_t timeout)
{
    return zbus_chan_pub(&chan_tick_service_invoke,
                         ALLOCA_MSG_TICK_SERVICE_INVOKE_START(delay_ms),
                         timeout);
}
```

### Current Services

- **shared** (`modules/services/shared/`): Common protobuf types (`Void`, `MsgServiceStatus`) used by all services
- **tick** (`modules/services/tick/`): Periodic timing service using K_TIMER; publishes TICK events
- **ui** (`modules/services/ui/`): UI control service; receives BLINK commands
- **battery** (`modules/services/battery/`): Battery management service
- **tamper_detection** (`modules/services/tamper_detection/`): Security monitoring service

### Adapters: Service Composition Without Coupling

Adapters listen to one service's report channel and invoke another service, creating a composition without modifying either service.

Example: `TickService+UiService_adapter.c` listens to tick reports and invokes UI blinks:

```c
static void tick_to_ui_adapter(const struct zbus_channel *chan, const void *msg)
{
    const struct msg_tick_service_report *tick_report = zbus_chan_const_msg(chan);
    if (tick_report->which_tick_report == MSG_TICK_SERVICE_REPORT_TICK_TAG) {
        ui_service_blink(K_NO_WAIT);  // Adapt: Tick → UI
    }
}

ZBUS_ASYNC_LISTENER_DEFINE(lis_tick_to_ui_adapter, tick_to_ui_adapter);
ZBUS_CHAN_ADD_OBS(chan_tick_service_report, lis_tick_to_ui_adapter, 3);
```

Adapters can be enabled/disabled via Kconfig (e.g., `CONFIG_TICK_TO_UI_ADAPTER`).

### Message Protocol with Protobuf (nanopb)

All messages are defined using protobuf and compiled with nanopb (lightweight protobuf for embedded):

```protobuf
message MsgTickService {
  message Config {
    optional bool should_report = 1;
    uint32 delay_ms = 2;
  }
  message Invoke {
    oneof tick_invoke {
      Void start = 1;
      Void stop = 2;
      Config config = 3;
    }
  }
  message Report {
    oneof tick_report {
      MsgServiceStatus status = 1;
      Void tick = 2;
    }
  }
}
```

Key patterns:

- Use `oneof` for discriminated unions (command variants, event variants)
- Invoke messages contain commands (START, STOP, CONFIG)
- Report messages always include STATUS for lifecycle events plus domain-specific events
- `import "service.proto"` for shared types (`Void`, `MsgServiceStatus`)
- Use `option (nanopb_fileopt).anonymous_oneof = true;` to flatten oneof naming

Proto files are collected via `PROTO_FILES_LIST` global property in CMakeLists.txt and compiled by nanopb during build.

## Adding New Services

1. Create service directory: `modules/services/<service>/`
2. Add `.proto` file with Invoke and Report messages
3. Add `.c` file with:
   - Channel definitions using `ZBUS_CHAN_DEFINE()`
   - Listener for invoke channel
   - Implementation logic
   - Report publishing
4. Add `.h` file with:
   - Channel declarations using `ZBUS_CHAN_DECLARE()`
   - Inline API functions wrapping `zbus_chan_pub()`
5. Add `CMakeLists.txt`:
   - Append proto file to `PROTO_FILES_LIST` global property
   - Conditionally compile based on `CONFIG_<SERVICE>_SERVICE`
   - Create `zephyr_interface_library()` for headers
   - Compile `.c` implementation
6. Add `Kconfig` with feature flag and dependencies
7. Add `zephyr/module.yml` for Zephyr module metadata
8. Register module in root `CMakeLists.txt` in `EXTRA_ZEPHYR_MODULES` list
9. Enable in `prj.conf`: `CONFIG_<SERVICE>_SERVICE=y`

Follow patterns from existing services (tick, ui, battery, tamper_detection).

## Adding New Adapters

1. Create adapter file in `adapters/`: `<OriginService>+<DestinyService>_adapter.c` (e.g., `TickService+UiService_adapter.c`)
2. Define listener function that receives source service reports
3. Use `ZBUS_ASYNC_LISTENER_DEFINE()` for non-blocking operation
4. Use `ZBUS_CHAN_ADD_OBS()` to register listener to source channel
5. Add Kconfig flag in `adapters/Kconfig`:

   ```kconfig
   config <SOURCE>_TO_<TARGET>_ADAPTER
       bool "From <source> to <target> service adapter"
       depends on <SOURCE>_SERVICE && <TARGET>_SERVICE
       default y
   ```

6. Update `adapters/CMakeLists.txt` to conditionally compile adapter
7. Enable in `prj.conf` if not default: `CONFIG_<SOURCE>_TO_<TARGET>_ADAPTER=y`

## Configuration System

Services and adapters are controlled via Kconfig feature flags in `prj.conf`:

```
CONFIG_<SERVICE>_SERVICE=y              # Enable service
CONFIG_<SERVICE>_LOG_LEVEL_DBG=y        # Enable debug logging for service
CONFIG_<ADAPTER>_ADAPTER=y              # Enable adapter
```

Current configuration enables all services and debug logging:

- `CONFIG_TICK_SERVICE=y`
- `CONFIG_UI_SERVICE=y`
- `CONFIG_BATTERY_SERVICE=y`
- `CONFIG_TAMPER_DETECTION_SERVICE=y`
- `CONFIG_LOG=y` with debug levels for all components

## Naming Conventions

- Services: `<name>_service` (e.g., `tick_service`, `ui_service`)
- Adapter files: `<OriginService>+<DestinyService>_adapter.c` (e.g., `TickService+UiService_adapter.c`)
- Channels: `chan_<service>_<invoke|report>` (e.g., `chan_tick_service_invoke`)
- Listeners:
  - Service listeners: `alis_<service>` (async) or `lis_<service>` (sync)
  - Adapter listeners: `lis_<source>_to_<target>_adapter`
- Messages: `msg_<service>_<invoke|report>` (generated from proto)
- Config flags: `CONFIG_<SERVICE>_SERVICE`, `CONFIG_<ADAPTER>_ADAPTER`
- Proto files: `<service>.proto`

## Data Flow Example

```
main.c calls tick_service_start(1000, K_FOREVER)
  └─> publishes START to chan_tick_service_invoke

tick_service.c listener receives START
  └─> starts K_TIMER with 1000ms period

Timer callback fires every 1000ms
  └─> publishes TICK to chan_tick_service_report

TickService+UiService_adapter.c listener receives TICK
  └─> calls ui_service_blink(K_NO_WAIT)

ui_service.c listener receives BLINK
  └─> executes blink logic (currently logs "blink!")

main.c status listener observes all report channels
  └─> prints service status changes
```

## Important Architectural Principles

- **Loose Coupling**: Services never call each other directly; all communication via zbus
- **Single Responsibility**: Each service has one clear domain responsibility
- **No Direct Dependencies**: Services don't include each other's headers except for API usage
- **Adapter Pattern**: New compositions are added as adapters, not by modifying services
- **Type Safety**: Protobuf provides structured, validated message types
- **Async by Default**: Use `ZBUS_ASYNC_LISTENER_DEFINE()` for adapters to avoid blocking
- **Lifecycle Management**: Services support START/STOP commands and report STATUS changes

## Code Generation

Protobuf files are compiled to C code by nanopb during the build process. Generated files appear in:

```
build/modules/services/<service>/<service>.pb.h
build/modules/services/<service>/<service>.pb.c
```

The build system automatically:

1. Collects all `.proto` files from services via `PROTO_FILES_LIST`
2. Invokes `zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})`
3. Generates C structs and serialization functions
4. Includes generated headers in build

Do not manually edit generated files.
