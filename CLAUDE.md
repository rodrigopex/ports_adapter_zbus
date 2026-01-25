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

# Generate service files from proto
just gen_service_files <service_name>
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

Services are split into infrastructure and implementation for clear separation of concerns:

```
modules/services/<service>/
├── <service>.h              # Public API, data structures, API interface definition
├── <service>.c              # Service infrastructure (channels, dispatcher, registration)
├── <service>_impl.c         # Actual implementation (business logic)
├── <service>.proto          # Protobuf message definitions (nanopb)
├── CMakeLists.txt           # Build configuration
├── Kconfig                  # Feature flags (CONFIG_<SERVICE>_SERVICE)
└── zephyr/module.yml        # Zephyr module metadata
```

**Service Header (`<service>.h`)** defines three key structures:

```c
// 1. Service data structure (thread-safe state)
struct tick_service_data {
    struct k_spinlock lock;              // Protects concurrent access
    struct msg_tick_service_config config;
    struct msg_service_status status;
};

// 2. Service API interface (function pointers)
struct tick_service_api {
    int (*start)(const struct service *service);
    int (*stop)(const struct service *service);
    int (*config)(const struct service *service, const struct msg_tick_service_config *new_config);
    int (*get_status)(const struct service *service);
    int (*get_config)(const struct service *service);
};

// 3. Public inline API functions (client interface)
static inline int tick_service_start(k_timeout_t timeout)
{
    return zbus_chan_pub(&chan_tick_service_invoke,
                         &(struct msg_tick_service_invoke){
                             .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_START_TAG},
                         timeout);
}
```

**Service Infrastructure (`<service>.c`)** provides the framework:

```c
// Global implementation pointer (registered at init)
static const struct service *impl = NULL;

// Registration function (called by implementation)
int tick_service_set_implementation(const struct service *implementation)
{
    int ret = -EBUSY;

    if (impl == NULL) {
        impl = implementation;
        ret = 0;
    } else {
        LOG_ERR("Trying to replace implementation");
    }

    return ret;
}

// Dispatcher routes invoke messages to API functions
static void api_handler(const struct zbus_channel *chan)
{
    struct tick_service_api *api;
    const struct msg_tick_service_invoke *ivk;

    if (impl == NULL || impl->api == NULL) {
        LOG_ERR("Service implementation or API required!");
        goto end;
    }

    api = impl->api;
    ivk = zbus_chan_const_msg(chan);

    switch (ivk->which_tick_invoke) {
    case MSG_TICK_SERVICE_INVOKE_START_TAG:
        if (api->start) {
            api->start(impl);
        }
        break;
    case MSG_TICK_SERVICE_INVOKE_STOP_TAG:
        if (api->stop) {
            api->stop(impl);
        }
        break;
    // ... other cases
    }

end:
    return;
}

ZBUS_LISTENER_DEFINE(lis_tick_service, api_handler);
```

**Service Implementation (`<service>_impl.c`)** contains business logic:

```c
// Implementation of API functions
static int start(const struct service *service)
{
    struct tick_service_data *data = service->data;

    K_SPINLOCK(&data->lock) {
        data->status.is_running = true;
        // Update state atomically
    }

    // Publish status report
    return zbus_chan_pub(&chan_tick_service_report, ...);
}

// Define API implementation
static struct tick_service_api api = {
    .start = start,
    .stop = stop,
    .config = config,
    .get_status = get_status,
    .get_config = get_config,
};

// Define service data
static struct tick_service_data data = {
    .config = MSG_TICK_SERVICE_CONFIG_INIT_ZERO,
    .status = {.is_running = false}
};

// Initialize and register implementation
int tick_service_init_fn(const struct service *self)
{
    int ret;

    ret = tick_service_set_implementation(self);
    printk("   -> %s initialized with%s error\n", self->name, ret == 0 ? " no" : "");

    return ret;
}

// Use SERVICE_DEFINE macro to register with Zephyr
SERVICE_DEFINE(tick_service, tick_service_init_fn, &api, &data);
```

**Shared Service Infrastructure (`service.h`)**:

```c
struct service {
    const char *name;
    struct {
        const struct zbus_channel *invoke;
        const struct zbus_channel *report;
    } channel;
    int (*init_fn)(const struct service *self);
    void *api;           // Points to service-specific API struct
    void *const data;    // Points to service-specific data struct
};

#define SERVICE_DEFINE(_name, _init_fn, _api, _data) \
    const STRUCT_SECTION_ITERABLE(service, _name) = { \
        .name = #_name, \
        .channel = { \
            .invoke = &CONCAT(chan_, _name, _invoke), \
            .report = &CONCAT(chan_, _name, _report), \
        }, \
        .init_fn = _init_fn, \
        .api = _api, \
        .data = _data \
    }
```

This pattern provides:
- **Separation of Concerns**: Infrastructure (routing/registration) vs. implementation (business logic)
- **Pluggable Implementations**: Multiple implementations can be swapped via registration
- **Type Safety**: Compile-time checked API interfaces
- **Thread Safety**: Spinlocks protect shared service data
- **Testability**: Implementation can be tested independently
- **Automatic Registration**: `STRUCT_SECTION_ITERABLE` enables runtime service discovery

### Current Services

- **shared** (`modules/services/shared/`):
  - Provides common protobuf types (`Empty`, `MsgServiceStatus`)
  - Defines `struct service` infrastructure for all services
  - Provides `SERVICE_DEFINE()` macro for service registration

- **tick** (`modules/services/tick/`) - **Reference Implementation**:
  - Periodic timing service using K_TIMER
  - Demonstrates full three-file pattern (`.h`, `.c`, `_impl.c`)
  - Shows thread-safe state management with spinlocks
  - Publishes Events with timestamps at configurable intervals
  - Commands: start, stop, config, get_status, get_config, get_events
  - Reports: status, config, events (with timestamp)

- **ui** (`modules/services/ui/`): UI control service; receives BLINK commands

- **battery** (`modules/services/battery/`) - **Generated Service Example**:
  - Battery monitoring service
  - Fully generated using code generator with RPC-based report mapping
  - Demonstrates custom message types (BatteryState with voltage, percentage, charging status)
  - Commands: start, stop, config, get_status, get_config, get_battery_state, get_events
  - Reports: status, config, battery_state, events

- **tamper_detection** (`modules/services/tamper_detection/`): Security monitoring service

**Note**: Tick service is the canonical reference - other services may use older patterns and should be migrated to the three-file architecture.

### Adapters: Service Composition Without Coupling

Adapters listen to one service's report channel and invoke another service, creating a composition without modifying either service.

Example: `TickService+UiService_adapter.c` listens to tick reports and invokes UI blinks:

```c
static void tick_to_ui_adapter(const struct zbus_channel *chan, const void *msg)
{
    const struct msg_tick_service_report *tick_report = zbus_chan_const_msg(chan);

    switch (tick_report->which_tick_report) {
    case MSG_TICK_SERVICE_REPORT_EVENTS_TAG:
        if (tick_report->events.has_tick) {
            LOG_DBG("Received Tick Service report at %lld, invoking UI Service blink",
                    tick_report->events.tick);
            ui_service_blink(K_NO_WAIT);  // Adapt: Tick → UI
        }
        break;
    default:
        // Ignore other report types
        break;
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
    uint32 delay_ms = 1;
  }

  message Events {
    optional int64 tick = 1;       // Timestamp of tick event
  }

  message Invoke {
    oneof tick_invoke {
      Empty start = 1;          // Start the service
      Empty stop = 2;           // Stop the service
      Empty get_status = 3;     // Request status report
      Config config = 4;        // Set configuration
      Empty get_config = 5;     // Request config report
      Empty get_events = 6;     // Request events stream
    }
  }

  message Report {
    oneof tick_report {
      MsgServiceStatus status = 1;  // Status updates (running/stopped)
      Config config = 2;            // Config updates
      Events events = 3;            // Periodic tick events with timestamps
    }
  }
}
```

Key patterns:

- **Commands**: Use `oneof` for discriminated unions of possible commands
  - Control commands: `start`, `stop`
  - Query commands: `get_status`, `get_config` (trigger report publications)
  - Config commands: `config` (set configuration)
- **Reports**: Use `oneof` for different event types
  - `status`: Lifecycle events (running/stopped)
  - `config`: Configuration updates (in response to get_config or config commands)
  - Domain-specific events: `events` (periodic events with timestamps or custom data)
- **Shared Types**: Import `"service.proto"` for common types (`Empty`, `MsgServiceStatus`)
- **nanopb Options**: Use `option (nanopb_fileopt).anonymous_oneof = true;` to flatten oneof field names in generated C code
- **Query Pattern**: Services respond to `get_status` and `get_config` by publishing corresponding reports

Proto files are collected via `PROTO_FILES_LIST` global property in CMakeLists.txt and compiled by nanopb during build.

## Adding New Services

**Recommended Approach**: Use the code generator to create service infrastructure automatically.

Follow the three-file pattern (infrastructure/implementation separation):

### 1. Create Service Directory
```bash
mkdir -p modules/services/<service>
```

### 2. Define Protobuf Messages (`<service>.proto`)
```protobuf
syntax = "proto3";
import "nanopb.proto";
import "service.proto";
option (nanopb_fileopt).anonymous_oneof = true;

message Msg<Service>Service {
  message Config {
    // Service-specific configuration fields
  }

  message Invoke {
    oneof <service>_invoke {
      Empty start = 1;
      Empty stop = 2;
      Empty get_status = 3;
      Config config = 4;
      Empty get_config = 5;
      // Add service-specific commands
    }
  }

  message Report {
    oneof <service>_report {
      MsgServiceStatus status = 1;
      Config config = 2;
      // Add service-specific events
    }
  }
}
```

### 3. Generate Service Files Using Code Generator

**Use the code generator to automatically create `.h`, `.c`, and `_impl.c` files:**

```bash
cd modules/services/shared/codegen

python3 generate_service.py \
    --proto ../../<service>/<service>.proto \
    --output-dir ../../<service> \
    --service-name <service> \
    --module-dir <service> \
    --generate-impl
```

This will generate:
- `<service>.h`: Service header with data structures, API interface, inline functions
- `<service>.c`: Service infrastructure with channels, dispatcher, registration
- `<service>_impl.c`: Implementation template with TODO comments and proper structure

**Important**: The generator creates infrastructure files marked with "GENERATED FILE - DO NOT EDIT". Only the `.proto` and `_impl.c` files should be edited by developers.

### 4. Complete the Implementation (`<service>_impl.c`)

The generated `_impl.c` template provides:
- Proper function stubs for all API functions
- Context-specific TODO comments explaining what each function should do
- Spinlock usage patterns for thread-safe state access
- WARNING reminders to publish reports when state changes

Example of generated stub (before completion):

```c
/* TODO: Implement start function
 *
 * This function should:
 * 1. Update service state to running
 * 2. Start any timers/resources
 * 3. Report status change via zbus_chan_pub
 */
static int start(const struct service *service)
{
	struct <service>_data *data = service->data;
	int ret = 0;

	/* TODO: Implement function logic here */

	K_SPINLOCK(&data->lock) {
		/* TODO: Access/modify service state here (thread-safe) */
	}

	/* WARNING: If this function changes service state, publish a report... */

	return ret;
}
```

Complete the implementation by:
1. Reading the TODO comments for each function
2. Implementing the business logic
3. Using spinlocks for thread-safe state access
4. Publishing reports via `zbus_chan_pub` when state changes
5. Removing TODO/WARNING comments when done

Example of completed `start` function:

```c
static int start(const struct service *service)
{
	struct <service>_data *data = service->data;
	struct msg_service_status status;

	K_SPINLOCK(&data->lock) {
		data->status.is_running = true;
		status = data->status;
	}

	// Start service-specific resources here (timers, threads, etc.)

	return zbus_chan_pub(&chan_<service>_report,
	                     &(struct msg_<service>_report){
	                         .which_<service>_report = MSG_<SERVICE>_REPORT_STATUS_TAG,
	                         .status = status},
	                     K_MSEC(200));
}
```

**Note**: The generated template already includes the static API struct, data struct, init function, and SERVICE_DEFINE macro. You only need to complete the function implementations.

### 5. Add Build Configuration (`CMakeLists.txt`)
```cmake
if(CONFIG_<SERVICE>_SERVICE)
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
                 "${CMAKE_CURRENT_SOURCE_DIR}/<service>.proto")

    zephyr_interface_library_named(<service>_interface)
    target_include_directories(<service>_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

    zephyr_library()
    zephyr_library_sources(<service>.c)
    zephyr_library_sources(<service>_impl.c)
    zephyr_library_link_libraries(<service>_interface)
endif()
```

### 6. Add Kconfig
```kconfig
config <SERVICE>_SERVICE
    bool "<Service> service"
    default y
    select NANOPB
    select ZBUS
    select ZBUS_ASYNC_LISTENER
    help
      Enable <service> service
```

### 7. Add `zephyr/module.yml`
```yaml
build:
  cmake: .
  kconfig: Kconfig
```

### 8. Register Module
Add to root `CMakeLists.txt`:
```cmake
set(EXTRA_ZEPHYR_MODULES
    "${SERVICES_PATH}/<service>"
    # ... other modules
)
```

### 9. Enable in Configuration
Add to `prj.conf`:
```
CONFIG_<SERVICE>_SERVICE=y
CONFIG_<SERVICE>_LOG_LEVEL_DBG=y
```

### 10. Build and Test
```bash
just clean build run
```

**Summary**: Using the code generator significantly reduces manual work. You only need to:
1. Write the `.proto` file (defines your service API)
2. Run the generator with `--generate-impl`
3. Complete the generated `_impl.c` template
4. Add build configuration files
5. Build and test

Follow the tick service (`modules/services/tick/`) as the reference implementation.

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

- **Service files**:
  - Directory: `modules/services/<service>/`
  - Header: `<service>.h`
  - Infrastructure: `<service>.c`
  - Implementation: `<service>_impl.c`
  - Protobuf: `<service>.proto`

- **Adapter files**: `<OriginService>+<DestinyService>_adapter.c` (e.g., `TickService+UiService_adapter.c`)

- **Channels**: `chan_<service>_<invoke|report>` (e.g., `chan_tick_service_invoke`)

- **Listeners**:
  - Service listeners: `lis_<service>` (sync) or `alis_<service>` (async)
  - Adapter listeners: `lis_<origin>_to_<destiny>_adapter`

- **Messages**: `msg_<service>_<invoke|report>` (generated from proto)

- **Structures**:
  - Service data: `struct <service>_data`
  - Service API: `struct <service>_api`
  - Service instance: Defined via `SERVICE_DEFINE(<service>, ...)`

- **Functions**:
  - Public API: `<service>_<command>(...)` (inline functions in header)
  - API implementation: `static int <command>(const struct service *service, ...)`
  - Init function: `int <service>_init_fn(const struct service *self)`
  - Registration: `int <service>_set_implementation(const struct service *implementation)`

- **Config flags**:
  - Service: `CONFIG_<SERVICE>_SERVICE`
  - Adapter: `CONFIG_<ADAPTER>_ADAPTER`
  - Log level: `CONFIG_<SERVICE>_LOG_LEVEL_DBG`

## Data Flow Example: Tick Service Lifecycle

### Initialization (Boot Time)
```
1. Zephyr calls tick_service_init_fn() during init
   └─> tick_service_impl.c: init_fn calls tick_service_set_implementation()
       └─> tick_service.c: Registers impl pointer (service instance with api + data)

2. Service is now ready to handle commands
```

### Starting the Service
```
3. main.c calls tick_service_start(K_FOREVER)
   └─> tick_service.h: Inline function publishes to chan_tick_service_invoke
       └─> Message: {.which_tick_invoke = START_TAG}

4. tick_service.c: Dispatcher (api_handler) receives message
   └─> Routes to impl->api->start(impl)
       └─> tick_service_impl.c: start() function executes:
           ├─> Acquires spinlock
           ├─> Updates data->status.is_running = true
           ├─> Starts K_TIMER with configured delay
           └─> Publishes STATUS report to chan_tick_service_report
```

### Runtime Operation
```
5. Timer callback fires periodically
   └─> tick_service_impl.c: tick_service_handler() called
       └─> Publishes TICK event to chan_tick_service_report

6. TickService+UiService_adapter.c (async listener) receives TICK
   └─> Checks: if (report->which_tick_report == TICK_TAG)
       └─> Calls ui_service_blink(K_NO_WAIT)
           └─> Publishes BLINK command to chan_ui_service_invoke

7. ui_service.c listener receives BLINK
   └─> Executes blink logic (logs "blink!")

8. main.c status listener observes all report channels
   └─> Prints service status changes when published
```

### Configuration Query
```
9. Client calls tick_service_config_get(K_FOREVER)
   └─> Publishes GET_CONFIG command to chan_tick_service_invoke

10. tick_service.c: Dispatcher routes to impl->api->get_config(impl)
    └─> tick_service_impl.c: get_config() executes:
        ├─> Acquires spinlock
        ├─> Reads data->config
        └─> Publishes CONFIG report to chan_tick_service_report
```

**Key Observations**:
- Commands flow through invoke channel → dispatcher → API implementation
- Reports flow from implementation → report channel → adapters/observers
- Spinlocks ensure thread-safe access to service data
- Adapters compose services without direct coupling

## Important Architectural Principles

- **Loose Coupling**: Services never call each other directly; all communication via zbus channels
- **Separation of Concerns**: Three-file pattern separates infrastructure (`.c`), implementation (`_impl.c`), and interface (`.h`)
- **Single Responsibility**: Each service has one clear domain responsibility
- **Single Return Pattern**: Functions should use a single return statement at the end
  - Declare a `ret` variable for return values
  - Use `goto end;` for error paths in void functions
  - Improves code clarity and makes cleanup easier
- **No Direct Dependencies**: Services don't include each other's headers except for API usage via public inline functions
- **Adapter Pattern**: New compositions are added as adapters, not by modifying services
- **Type Safety**:
  - Protobuf provides structured, validated message types
  - API structs define compile-time checked interfaces
  - Inline functions provide type-safe client APIs
- **Thread Safety**:
  - Service state protected by `k_spinlock` for atomic access
  - Use `K_SPINLOCK(&data->lock) { ... }` for all state modifications
- **Pluggable Implementations**: Services can have multiple implementations registered at runtime
- **Async by Default**: Use `ZBUS_ASYNC_LISTENER_DEFINE()` for adapters to avoid blocking
- **Lifecycle Management**:
  - All services support START/STOP commands and report STATUS changes
  - Services support query commands (get_status, get_config) for observability
- **Automatic Registration**: `STRUCT_SECTION_ITERABLE` enables runtime service discovery without manual registration

## Code Generation

### Protobuf Code Generation (nanopb)

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

Do not manually edit generated nanopb files.

### Service Infrastructure Generator

The project includes a Python-based code generator that creates service infrastructure files (`<service>.h` and `<service>.c`) from protobuf definitions. This eliminates manual synchronization errors and ensures consistency across services.

**Location**: `modules/services/shared/codegen/`

**What Gets Generated**:
- `<service>.h`: Service header with data structures, API interface, and inline client functions
- `<service>.c`: Service infrastructure with channels, dispatcher, and registration
- `private/<service>_priv.h` (with `--generate-impl`): Private header with report helper functions
- `<service>_impl.c` (optional template with `--generate-impl`): Implementation template with TODO comments and proper structure

**What Is Hand-Written** (source of truth):
- `<service>.proto`: Protobuf message definitions (defines API structure)
- `<service>_impl.c`: Business logic implementation (can start from generated template)

#### Installation

Install dependencies from the codegen directory:

```bash
cd modules/services/shared/codegen
pip install -r requirements.txt
```

Dependencies:
- `proto-schema-parser>=0.3.0`: Parses `.proto` files without requiring protoc
- `jinja2>=3.0.0`: Template rendering engine

#### Usage

Generate service infrastructure from a proto file:

```bash
cd modules/services/shared/codegen

# Generate only .h and .c files (for existing services):
python3 generate_service.py \
    --proto ../../tick/tick_service.proto \
    --output-dir ../../tick \
    --service-name tick_service \
    --module-dir tick

# Generate .h, .c, and _impl.c template (for new services):
python3 generate_service.py \
    --proto ../../new_service/new_service.proto \
    --output-dir ../../new_service \
    --service-name new_service \
    --module-dir new_service \
    --generate-impl
```

**Parameters**:
- `--proto`: Path to the `.proto` file
- `--output-dir`: Directory where generated files will be written
- `--service-name`: Service name in snake_case (e.g., `tick_service`)
- `--module-dir`: Module directory name (e.g., `tick` for tick service) - used for include paths
- `--generate-impl`: (optional) Generate `_impl.c` template with TODO comments (only if file doesn't exist)

**Output** (without `--generate-impl`):
```
Parsing ../../tick/tick_service.proto...
Service: tick_service
Invoke fields: ['start', 'stop', 'get_status', 'config', 'get_config']
Config fields: ['delay_ms']
Rendering templates...
Generated: ../../tick/tick_service.h
Generated: ../../tick/tick_service.c

Note: tick_service_impl.c must be written manually
      Implement functions defined in struct tick_service_api
```

**Output** (with `--generate-impl`):
```
Parsing ../../new_service/new_service.proto...
Service: new_service
Invoke fields: ['start', 'stop', 'config']
Report fields: ['status', 'config']
Rendering templates...
Generated: ../../new_service/new_service.h
Generated: ../../new_service/new_service.c
Generated: ../../new_service/private/new_service_priv.h
Generated template: ../../new_service/new_service_impl.c
Note: Complete TODO items in _impl.c
```

**Important**: The generator will NEVER overwrite an existing `_impl.c` file. If you run with `--generate-impl` on an existing service, it will skip the `_impl.c` file generation.

#### Developer Workflow

**For New Services**:

1. **Create Proto File** (`<service>.proto`): Define messages, Invoke, Report, Config

2. **Generate All Files with Template**:
   ```bash
   # Using justfile recipe (recommended):
   just gen_service_files new_service

   # Or manually:
   cd modules/services/shared/codegen
   python3 generate_service.py \
       --proto ../../new_service/new_service_service.proto \
       --output-dir ../../new_service \
       --service-name new_service_service \
       --module-dir new_service \
       --generate-impl
   ```

   This generates:
   - `new_service/new_service_service.h` - Public API and data structures
   - `new_service/new_service_service.c` - Infrastructure (channels, dispatcher)
   - `new_service/private/new_service_service_priv.h` - Report helper functions
   - `new_service/new_service_service_impl.c` - Implementation template

3. **Complete Implementation** (`<service>_impl.c`):
   - Read TODO comments for guidance on each function
   - Implement business logic with proper spinlock usage
   - Use private header helper functions to report state changes (e.g., `<service>_report_status()`)
   - Remove TODO comments when complete

4. **Build and Test**:
   ```bash
   just clean build run
   ```

**For Existing Services** (modifying APIs):

1. **Edit the Proto File** (`<service>.proto`):
   ```protobuf
   message MsgTickService {
     message Config {
       uint32 delay_ms = 1;
       uint32 new_param = 2;  // Add new config field
     }

     message Invoke {
       oneof tick_invoke {
         Empty start = 1;
         Empty stop = 2;
         Empty get_status = 3;
         Config config = 4;
         Empty get_config = 5;
         Empty my_new_command = 6;  // Add new command
       }
     }
   }
   ```

2. **Regenerate Infrastructure**:
   ```bash
   # Using justfile recipe (recommended):
   just gen_service_files tick

   # Or manually (without --generate-impl to avoid overwriting _impl.c):
   cd modules/services/shared/codegen
   python3 generate_service.py \
       --proto ../../tick/tick_service.proto \
       --output-dir ../../tick \
       --service-name tick_service \
       --module-dir tick
   ```

3. **Update Implementation** (`<service>_impl.c`):
   ```c
   // Implement the new API function
   static int my_new_command(const struct service *service)
   {
       struct tick_service_data *data = service->data;
       int ret = 0;

       K_SPINLOCK(&data->lock) {
           // Use new config field: data->config.new_param
           // Implement business logic
       }

       return ret;
   }

   // Add to API struct
   static struct tick_service_api api = {
       .start = start,
       .stop = stop,
       .config = config,
       .get_status = get_status,
       .get_config = get_config,
       .my_new_command = my_new_command,  // Add new function pointer
   };
   ```

4. **Build and Test**:
   ```bash
   just clean build run
   ```

**Key Principles**:
- Infrastructure files (`.h` and `.c`) are marked with `/* GENERATED FILE - DO NOT EDIT */`
- Generated `_impl.c` template is marked with TODO comments; remove header when implementation is complete
- Never edit generated `.h` and `.c` files manually
- Proto file is the single source of truth for data structures and API
- Developer only edits `.proto` and `_impl.c` files
- Regeneration is safe and idempotent (never overwrites existing `_impl.c` files)

#### RPC-Based Report Function Mapping

The generator parses `service` definitions from proto files to automatically determine which report function each implementation should call based on RPC return types. This eliminates manual guesswork and ensures type-safe mapping between service methods and their responses.

**Proto Service Definition**:
```protobuf
service TickService {
  rpc start (Empty) returns (MsgServiceStatus);           // → calls report_status()
  rpc stop (Empty) returns (MsgServiceStatus);            // → calls report_status()
  rpc config (Config) returns (Config);                   // → calls report_config()
  rpc get_events (Empty) returns (stream Events);         // → calls report_events()
}
```

**Automatic Mapping Logic**:
1. **Parse RPC Signatures**: Extract method name, input type, output type, and streaming flag
2. **Map Return Types to Report Fields**:
   - `returns MsgServiceStatus` → maps to Report field `status` → generates call to `report_status()`
   - `returns MsgTickService.Config` → maps to Report field `config` → generates call to `report_config()`
   - `returns MsgTickService.Events` → maps to Report field `events` → generates call to `report_events()`
3. **Generate Smart Implementations**: Standard methods (start, stop, config) get complete implementations; custom methods include hints

**Generator Output Example**:
```
RPC methods: 6 found
  - start(Empty) -> MsgServiceStatus => report_status()
  - stop(Empty) -> MsgServiceStatus => report_status()
  - get_status(Empty) -> MsgServiceStatus => report_status()
  - config(MsgTickService.Config) -> MsgTickService.Config => report_config()
  - get_config(Empty) -> MsgTickService.Config => report_config()
  - get_events(Empty) -> MsgTickService.Events (streaming) => report_events()
```

**Generated Implementation Example**:
```c
static int start(const struct service *service)
{
    struct tick_service_data *data = service->data;
    struct msg_service_status status;

    K_SPINLOCK(&data->lock) {
        data->status.is_running = true;
        status = data->status;
    }

    /* TODO: Start service resources (timers, threads, etc.) */

    // Automatically generated based on RPC return type
    return tick_service_report_status(&status, K_MSEC(250));
}
```

**Benefits**:
- **Type-Safe**: RPC signatures enforce correct report function usage
- **Self-Documenting**: Service definition documents the contract
- **Less Error-Prone**: No manual report function selection
- **Validated**: Generator warns about inconsistencies between RPC methods and Report fields
- **Backward Compatible**: Services without `service` definitions fall back to invoke fields

**Validation**: The generator validates that each RPC method has:
1. A corresponding Invoke oneof field
2. A return type that maps to an existing Report oneof field

If inconsistencies are found, warnings and errors are printed during generation.

#### How It Works

The generator uses three components:

1. **Parser** (`generate_service.py`):
   - Uses `proto-schema-parser` to parse `.proto` files
   - Extracts service name, invoke oneof name, report oneof name, invoke fields, config fields, etc.
   - Uses provided module directory for correct include paths
   - Builds a context dictionary with all extracted data

2. **Templates** (`templates/`):
   - `service.h.jinja`: Generates service header
   - `service.c.jinja`: Generates service infrastructure
   - `service_priv.h.jinja`: Generates private header with report helper functions
   - `service_impl.c.jinja`: Generates implementation template with TODO comments (optional)
   - Uses Jinja2 for conditionals, loops, and filters

3. **Generator Logic**:
   ```python
   # Parse proto file
   context = parse_service_proto(proto_path, service_name, module_dir)

   # Render templates
   header_content, impl_content = render_templates(context, template_dir)

   # Write generated files
   write_file(f"{service_name}.h", header_content)
   write_file(f"{service_name}.c", impl_content)
   ```

#### Generated Code Structure

**From Proto Oneof to Generated API**:

Proto definition:
```protobuf
message MsgTickService {
  message Invoke {
    oneof tick_invoke {
      Empty start = 1;
      Empty stop = 2;
      Config config = 4;
    }
  }
}
```

Generates API structure:
```c
struct tick_service_api {
    int (*start)(const struct service *service);
    int (*stop)(const struct service *service);
    int (*config)(const struct service *service,
                  const struct msg_tick_service_config *config);
};
```

Generates inline client functions:
```c
static inline int tick_service_start(k_timeout_t timeout)
{
    return zbus_chan_pub(&chan_tick_service_invoke,
                         &(struct msg_tick_service_invoke){
                             .which_tick_invoke = MSG_TICK_SERVICE_INVOKE_START_TAG},
                         timeout);
}
```

Generates dispatcher switch case:
```c
switch (ivk->which_tick_invoke) {
case MSG_TICK_SERVICE_INVOKE_START_TAG:
    if (api->start) {
        api->start(impl);
    }
    break;
// ... other cases
}
```

#### Private Header Generation

When using `--generate-impl`, the generator creates a private header file (`private/<service>_priv.h`) containing static inline helper functions for publishing service reports. This provides a clean, type-safe API for reporting state changes without exposing implementation details in the public header.

**Purpose**:
- Simplifies report publishing in implementation code
- Encapsulates zbus channel publishing boilerplate
- Provides compile-time type checking for report messages
- Keeps implementation details private (not visible in public `<service>.h`)

**Generated Helper Functions**:

The generator analyzes the Report message's oneof fields and creates a helper function for each field:

```c
// For shared MsgServiceStatus reports
static inline int <service>_report_status(const struct msg_service_status *status,
                                          k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_<service>_report,
        &(struct msg_<service>_report){
            .which_<report_oneof> = MSG_<SERVICE>_REPORT_STATUS_TAG,
            .status = *status},
        timeout);
}

// For service-specific Config reports
static inline int <service>_report_config(const struct msg_<service>_config *config,
                                          k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_<service>_report,
        &(struct msg_<service>_report){
            .which_<report_oneof> = MSG_<SERVICE>_REPORT_CONFIG_TAG,
            .config = *config},
        timeout);
}

// For Empty reports (events with no data)
static inline int <service>_report_tick(k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_<service>_report,
        &(struct msg_<service>_report){
            .which_<report_oneof> = MSG_<SERVICE>_REPORT_TICK_TAG},
        timeout);
}
```

**Type Mapping**:

The generator handles three categories of report field types:

1. **Shared types** (e.g., `MsgServiceStatus`):
   - Proto: `MsgServiceStatus status = 1;`
   - Parameter: `const struct msg_service_status *status`
   - Usage: `.status = *status`

2. **Service-specific types** (e.g., `Config`):
   - Proto: `Config config = 2;`
   - Parameter: `const struct msg_<service>_config *config`
   - Usage: `.config = *config`

3. **Empty types** (events with no payload):
   - Proto: `Empty tick = 3;`
   - Parameter: None (only `k_timeout_t timeout`)
   - Usage: No payload field in struct literal

4. **Custom CamelCase types** (e.g., `BatteryState`):
   - Proto: `BatteryState battery_state = 4;`
   - Parameter: `const struct msg_<service>_battery_state *battery_state`
   - Usage: `.battery_state = *battery_state`
   - Note: CamelCase proto type names are automatically converted to snake_case to match nanopb naming conventions using the `camel_to_snake` Jinja2 filter

**Usage Example** (from tick service):

Proto definition:
```protobuf
message MsgTickService {
  message Report {
    oneof tick_report {
      MsgServiceStatus status = 1;
      Config config = 2;
      Empty tick = 3;
    }
  }
}
```

Generated helper functions in `private/tick_service_priv.h`:
```c
static inline int tick_service_report_status(const struct msg_service_status *status,
                                             k_timeout_t timeout);
static inline int tick_service_report_config(const struct msg_tick_service_config *config,
                                             k_timeout_t timeout);
static inline int tick_service_report_tick(k_timeout_t timeout);
```

Implementation usage in `tick_service_impl.c`:
```c
#include "tick_service.h"
#include "private/tick_service_priv.h"  // Include private helpers

static int start(const struct service *service)
{
    struct tick_service_data *data = service->data;
    struct msg_service_status status;

    K_SPINLOCK(&data->lock) {
        data->status.is_running = true;
        status = data->status;
    }

    // Use helper instead of raw zbus_chan_pub
    return tick_service_report_status(&status, K_MSEC(250));
}

static int get_config(const struct service *service)
{
    struct tick_service_data *data = service->data;
    struct msg_tick_service_config config;

    K_SPINLOCK(&data->lock) {
        config = data->config;
    }

    // Use helper for config reports
    return tick_service_report_config(&config, K_MSEC(250));
}

static void timer_handler(struct k_timer *timer)
{
    // Use helper for event reports (no parameters)
    tick_service_report_tick(K_NO_WAIT);
}
```

**Benefits**:
- **Cleaner code**: `tick_service_report_status(&status, timeout)` vs. full `zbus_chan_pub` call
- **Type safety**: Compiler checks parameter types at call sites
- **Maintainability**: Changes to report structure only require template updates
- **Discoverability**: IDE autocomplete shows available report functions
- **Privacy**: Implementation helpers not exposed in public API

**File Structure**:
```
modules/services/<service>/
├── <service>.h                    # Public API (generated)
├── <service>.c                    # Infrastructure (generated)
├── private/
│   └── <service>_priv.h          # Private helpers (generated)
└── <service>_impl.c              # Implementation (uses private helpers)
```

The private header is automatically generated whenever `--generate-impl` is used or when using the `just gen_service_files` recipe.

#### nanopb Type Compliance

Generated code uses nanopb type names with the `struct` keyword as required by C:

```c
// CORRECT (generated code)
struct msg_tick_service_config config;           // nanopb struct with typedef
const struct msg_tick_service_invoke *ivk;       // nanopb struct with typedef

// In compound literals
&(struct msg_tick_service_invoke){...}           // Requires struct keyword
```

**Important**: While nanopb generates `typedef struct {...} msg_type;`, the C language still requires the `struct` keyword in most contexts:
- Variable declarations: `struct msg_type var;`
- Pointer declarations: `const struct msg_type *ptr;`
- Compound literals: `&(struct msg_type){...}`

nanopb naming conventions:
- Main message: `MsgTickService.Invoke` → `msg_tick_service_invoke`
- Nested message: `MsgTickService.Config` → `msg_tick_service_config`
- CamelCase types: `MsgBatteryService.BatteryState` → `msg_battery_service_battery_state`
- Enum tags: `MSG_TICK_SERVICE_INVOKE_START_TAG`
- Oneof field names: Based on the oneof name in proto (e.g., `which_tick_invoke` from `oneof tick_invoke`)
- Initializers: `MSG_TICK_SERVICE_INVOKE_INIT_DEFAULT`

**Important**: The code generator uses the `camel_to_snake` filter to convert CamelCase proto type names to snake_case, ensuring generated code matches nanopb's naming conventions. For example:
- Proto type: `BatteryState` → C type: `msg_battery_service_battery_state`
- Proto type: `GPSLocation` → C type: `msg_<service>_g_p_s_location`

#### Template Customization

Templates are located in `modules/services/shared/codegen/templates/`:

- **`service.h.jinja`**: Header template
- **`service.c.jinja`**: Infrastructure template
- **`service_priv.h.jinja`**: Private header template with report helper functions (generated with `--generate-impl`)
- **`service_impl.c.jinja`**: Implementation template (optional, only generated with `--generate-impl`)

Templates use Jinja2 syntax:
```jinja
{%- for field in invoke_fields %}
case MSG_{{ service_name_upper }}_INVOKE_{{ field.name|upper }}_TAG:
    if (api->{{ field.name }}) {
{%- if field.is_empty %}
        api->{{ field.name }}(impl);
{%- else %}
        api->{{ field.name }}(impl, &ivk->{{ field.message_type|lower }});
{%- endif %}
    }
    break;
{%- endfor %}
```

Modify templates to change code structure across all services.

**Custom Jinja2 Filters**:

The generator provides custom filters for type name transformations:

- **`|camel_to_snake`**: Converts CamelCase to snake_case (e.g., `BatteryState` → `battery_state`)
  - Used in `service_priv.h.jinja` for custom message types
  - Ensures generated type names match nanopb naming conventions
  - Example: `{{ field.message_type|camel_to_snake }}` produces correct nanopb types

- **`|upper`**: Built-in Jinja2 filter for UPPER_CASE (e.g., `start` → `START`)
  - Used for enum tag generation
  - Example: `MSG_{{ service_name_upper }}_INVOKE_{{ field.name|upper }}_TAG`

- **`|lower`**: Built-in Jinja2 filter for lowercase (e.g., `Config` → `config`)
  - Used for variable names and simple type references
  - Note: Not suitable for CamelCase type names (use `|camel_to_snake` instead)

**Filter Registration** (in `generate_service.py`):
```python
env = Environment(loader=FileSystemLoader(template_dir))
env.filters['camel_to_snake'] = camel_to_snake
```

#### Generated _impl.c Template Structure

When using `--generate-impl`, the generator creates a template file with the following structure:

**1. Header Comments**:
```c
/* GENERATED FILE - DO NOT EDIT */
/* Complete TODO items and remove this header when implementation is done */
```

**2. Includes and Logging**:
```c
#include "tick_service.h"
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(tick_service, CONFIG_TICK_SERVICE_LOG_LEVEL);
```

**3. TODO Sections for Service-Specific Code**:
```c
/* TODO: Add service-specific helper macros here if needed
 * Example for report messages:
 * #define ALLOCA_MSG_TICK_SERVICE_REPORT_STATUS(...) \
 *     &(struct msg_tick_service_report) { \
 *         .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG, \
 *         .status = (struct msg_service_status){ __VA_ARGS__ } \
 *     }
 */

/* TODO: Add service-specific resources here if needed
 * Examples:
 * - Timers: K_TIMER_DEFINE(timer_tick_service, handler, NULL);
 * - Work queues: K_WORK_DEFINE(work_tick_service, handler);
 * - Threads: K_THREAD_DEFINE(...)
 */
```

**4. Function Stubs with Context-Specific Guidance**:

For `start` function:
```c
/* TODO: Implement start function
 *
 * This function should:
 * 1. Update service state to running
 * 2. Start any timers/resources
 * 3. Report status change via zbus_chan_pub
 */
static int start(const struct service *service)
{
	struct tick_service_data *data = service->data;
	int ret = 0;

	/* TODO: Implement function logic here */

	K_SPINLOCK(&data->lock) {
		/* TODO: Access/modify service state here (thread-safe) */
	}

	/* TODO: Add business logic here */

	/* WARNING: If this function changes service state, publish a report:
	 *
	 * return zbus_chan_pub(
	 *     &chan_tick_service_report,
	 *     &(struct msg_tick_service_report){
	 *         .which_tick_report = MSG_TICK_SERVICE_REPORT_<TYPE>_TAG,
	 *         .<field> = <value>},
	 *     K_MSEC(200));
	 */

	return ret;
}
```

Similar stubs are generated for all invoke fields with appropriate guidance:
- **start**: Start resources, update state to running, report
- **stop**: Stop resources, update state to stopped, report
- **get_status**: Read status (thread-safe), publish report
- **get_config**: Read config (thread-safe), publish report
- **config**: Validate, update config (thread-safe), publish report
- **Other functions**: Generic guidance about business logic and reporting

**5. Static Structures**:
```c
static struct tick_service_api api = {
	.start = start,
	.stop = stop,
	.config = config,
	.get_status = get_status,
	.get_config = get_config,
};

static struct tick_service_data data = {
	.config = MSG_TICK_SERVICE_CONFIG_INIT_ZERO,
	.status = {.is_running = false}
};
```

**6. Init Function and Registration**:
```c
int tick_service_init_fn(const struct service *self)
{
	int err = tick_service_set_implementation(self);

	printk("   -> %s initialed with%s error\n", self->name, err == 0 ? " no" : "");

	return err;
}

SERVICE_DEFINE(tick_service, tick_service_init_fn, &api, &data);
```

**Template Features**:
- ✅ All function signatures match generated API in `.h` file
- ✅ Proper spinlock patterns for thread-safe state access
- ✅ Context-aware TODO comments for each function type
- ✅ WARNING reminders to publish reports on state changes
- ✅ Correct nanopb type usage throughout
- ✅ Ready to compile (returns 0 from all stubs)

**Safety**:
- Template is ONLY created if `_impl.c` doesn't already exist
- Running generator again will skip existing `_impl.c` files
- No risk of overwriting hand-written business logic

#### Future Enhancements

Potential improvements:
- Automatic CMake integration (regenerate on proto changes)
- Generate CMakeLists.txt for new services
- Generate Kconfig files for new services
- Generate unit test stubs
- Add `--watch` mode for development
- Extend to other services beyond tick reference implementation
