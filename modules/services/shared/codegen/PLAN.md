# Code Generation Plan: Generate Service Infrastructure from Protobuf

## Goal
Generate `tick_service.h` and `tick_service.c` infrastructure files from `tick_service.proto` using the `proto-schema-parser` Python library.

## Scope
- **Focus**: Tick service only (reference implementation)
- **Generate**: Infrastructure boilerplate (`tick_service.h`, `tick_service.c`)
- **Preserve**: Hand-written business logic (`tick_service_impl.c`)
- **Approach**: Python script using proto-schema-parser library

## Architecture Overview

```
tick_service.proto (hand-written)
        ↓
  [proto-schema-parser]
        ↓
  Python generator script
        ↓
  ├── tick_service.h (generated)
  ├── tick_service.c (generated)
  └── tick_service_impl.c (template/stub - only if doesn't exist)
```

## Implementation Steps

### Step 1: Install Dependencies
```bash
pip install proto-schema-parser
pip install copier
```

**Dependencies:**
- `proto-schema-parser`: Parse .proto files without protoc
- `copier`: Template rendering engine (uses Jinja2)

Add to project requirements or document in README.

### Step 2: Create Directory Structure

**Generator Location**: `modules/services/shared/codegen/`

```
modules/services/shared/codegen/
├── generate_service.py          # Main generator script
└── templates/                   # Copier/Jinja2 templates
    ├── service.h.jinja          # Header template
    └── service.c.jinja          # Implementation template
```

### Step 3: Create Code Generator Script

**Location**: `modules/services/shared/codegen/generate_service.py`

**Responsibilities**:
1. Parse `tick_service.proto` using proto-schema-parser
2. Extract:
   - Service name from message name (MsgTickService → tick_service)
   - Invoke oneof fields (start, stop, get_status, config, get_config)
   - Report oneof fields (status, config, tick)
   - Config message fields (delay_ms: uint32)
3. Build context dictionary with all extracted data
4. Use Copier to render templates with context
5. Write generated files to output directory

**Key Functions**:
```python
def parse_proto(proto_file_path):
    """Parse proto file and return structured data"""
    # Returns: dictionary with service_name, invoke_fields, config_fields, etc.

def render_templates(context, output_dir):
    """Use Copier to render Jinja2 templates"""
    # Renders service.h.jinja and service.c.jinja with context
```

### Step 4: Create Jinja2 Templates

**Header Template** (`modules/services/shared/codegen/templates/service.h.jinja`):
```jinja
/* GENERATED FILE - DO NOT EDIT */
/* Generated from {{ service_name }}.proto */

#ifndef _{{ service_name_upper }}_H_
#define _{{ service_name_upper }}_H_

#include "service.h"
#include "{{ service_name }}/{{ service_name }}.pb.h"
#include <zephyr/zbus/zbus.h>

ZBUS_CHAN_DECLARE(chan_{{ service_name }}_invoke, chan_{{ service_name }}_report);

/* Data structure - must use nanopb generated types */
struct {{ service_name }}_data {
	struct k_spinlock lock;
{%- if has_config %}
	{{ config_type }} config;
{%- endif %}
	msg_service_status status;
};

/* API structure - fully generated from proto Invoke message */
struct {{ service_name }}_api {
{%- for field in invoke_fields %}
{%- if field.is_empty %}
	int (*{{ field.name }})(const struct service *service);
{%- else %}
	int (*{{ field.name }})(const struct service *service,
	                        const msg_{{ service_name }}_{{ field.message_type|lower }} *{{ field.message_type|lower }});
{%- endif %}
{%- endfor %}
};

/* Inline API functions - generated from Invoke oneof fields */
{%- for field in invoke_fields %}
{%- if field.is_empty %}
static inline int {{ service_name }}_{{ field.name }}(k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_{{ service_name }}_invoke,
	                     &(msg_{{ service_name }}_invoke){
	                             .which_{{ service_name }}_invoke = MSG_{{ service_name_upper }}_INVOKE_{{ field.name|upper }}_TAG},
	                     timeout);
}
{%- else %}
{%- if field.message_type|lower == 'config' and config_fields %}
static inline int {{ service_name }}_{{ field.name }}_set({% for cfg in config_fields %}{{ cfg.type }} {{ cfg.name }}{{ ", " if not loop.last else "" }}{% endfor %}, k_timeout_t timeout)
{
	return zbus_chan_pub(&chan_{{ service_name }}_invoke,
	                     &(msg_{{ service_name }}_invoke){
	                             .which_{{ service_name }}_invoke = MSG_{{ service_name_upper }}_INVOKE_{{ field.name|upper }}_TAG,
	                             .config = {
{%- for cfg in config_fields %}
	                                     .{{ cfg.name }} = {{ cfg.name }},
{%- endfor %}
	                             }},
	                     timeout);
}
{%- endif %}
{%- endif %}

{%- endfor %}

int {{ service_name }}_set_implementation(const struct service *implementation);

#endif /* _{{ service_name_upper }}_H_ */
```

**Implementation Template** (`modules/services/shared/codegen/templates/service.c.jinja`):
```jinja
/* GENERATED FILE - DO NOT EDIT */
/* Generated from {{ service_name }}.proto */

#include "{{ service_name }}.h"
#include <zephyr/zbus/zbus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER({{ service_name }}, CONFIG_{{ service_name_upper }}_LOG_LEVEL);

ZBUS_CHAN_DEFINE(chan_{{ service_name }}_invoke, struct msg_{{ service_name }}_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(lis_{{ service_name }}), MSG_{{ service_name_upper }}_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_{{ service_name }}_report, struct msg_{{ service_name }}_report, NULL, NULL,
		 ZBUS_OBSERVERS_EMPTY, MSG_{{ service_name_upper }}_REPORT_INIT_DEFAULT);

static const struct service *impl = NULL;

int {{ service_name }}_set_implementation(const struct service *implementation)
{
	int ret = -EBUSY;

	if (impl == NULL) {
		impl = implementation;
		ret = 0;
	} else {
		LOG_ERR("Trying to replace implementation for {{ service_name_camel }} Service");
	}

	return ret;
}

static void api_handler(const struct zbus_channel *chan)
{
	struct {{ service_name }}_api *api;
	const struct msg_{{ service_name }}_invoke *ivk;

	if (impl == NULL) {
		LOG_ERR("Service implementation required!");
		return;
	}

	if (impl->api == NULL) {
		LOG_ERR("Service API required!");
		return;
	}

	api = impl->api;
	ivk = zbus_chan_const_msg(chan);

	switch (ivk->which_{{ invoke_oneof_name }}) {
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
	}
}

ZBUS_LISTENER_DEFINE(lis_{{ service_name }}, api_handler);
```

### Step 5: CMake Integration

**Option A: Pre-build generation** (Recommended for future)
Add custom target in `modules/services/tick/CMakeLists.txt`:

```cmake
if(CONFIG_TICK_SERVICE)
    # Path to shared generator script
    set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/modules/services/shared/generate_service.py")

    # Generate service infrastructure if proto changed
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tick_service.h
               ${CMAKE_CURRENT_SOURCE_DIR}/tick_service.c
        COMMAND python3 ${GENERATOR_SCRIPT}
                --proto ${CMAKE_CURRENT_SOURCE_DIR}/tick_service.proto
                --output-dir ${CMAKE_CURRENT_SOURCE_DIR}
                --service-name tick_service
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tick_service.proto
                ${GENERATOR_SCRIPT}
        COMMENT "Generating tick service infrastructure from proto"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Proto file registration (existing)
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
                 "${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service.proto")

    # Build configuration (existing)
    zephyr_interface_library_named(tick_service)
    target_include_directories(tick_service INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

    zephyr_library()
    zephyr_library_sources(tick_service.c tick_service_impl.c)
    zephyr_library_link_libraries(tick_service)
endif()
```

**Option B: Manual generation** (Recommended for initial implementation)
Run generator manually after editing proto:
```bash
cd modules/services/shared/codegen
python3 generate_service.py \
    --proto ../../tick/tick_service.proto \
    --output-dir ../../tick \
    --service-name tick_service
```

Then commit generated files to source control.

**Recommendation**: Start with Option B (manual). Developer workflow:
1. Edit `tick_service.proto` to add/modify messages
2. Run generator script
3. Edit `tick_service_impl.c` to implement business logic
4. Build and test

### Step 6: Generator Script Implementation Details

**Main Script Structure**:
```python
#!/usr/bin/env python3
import argparse
from proto_schema_parser import Parser
from jinja2 import Environment, FileSystemLoader
import os

def parse_service_proto(proto_path):
    """Parse proto and return context dictionary"""
    # ... parsing logic
    return context

def render_templates(context, output_dir):
    """Render Jinja2 templates using context"""
    template_dir = os.path.join(os.path.dirname(__file__), 'templates')
    env = Environment(loader=FileSystemLoader(template_dir))

    # Render header
    header_template = env.get_template('service.h.jinja')
    header_content = header_template.render(**context)

    # Render implementation
    impl_template = env.get_template('service.c.jinja')
    impl_content = impl_template.render(**context)

    return header_content, impl_content

def main():
    # Parse arguments
    # Parse proto file
    context = parse_service_proto(args.proto)
    # Render templates
    header_content, impl_content = render_templates(context, args.output_dir)
    # Write files
    # ...
```

**Parsing Strategy with proto-schema-parser**:
```python
from proto_schema_parser import Parser

def parse_service_proto(proto_path):
    parser = Parser()
    with open(proto_path, 'r') as f:
        proto_content = f.read()

    schema = parser.parse(proto_content)

    # Find the service message (e.g., MsgTickService)
    service_msg = None
    for message in schema.messages:
        if message.name.startswith('Msg') and message.name.endswith('Service'):
            service_msg = message
            break

    # Extract service name: MsgTickService -> tick_service
    service_name = camel_to_snake(service_msg.name.replace('Msg', '').replace('Service', ''))

    # Find Invoke, Report, and Config nested messages
    invoke_msg = None
    report_msg = None
    config_msg = None

    for nested in service_msg.messages:
        if nested.name == 'Invoke':
            invoke_msg = nested
        elif nested.name == 'Report':
            report_msg = nested
        elif nested.name == 'Config':
            config_msg = nested

    # Extract invoke fields from oneof (these become API functions)
    invoke_fields = []
    for field in invoke_msg.fields:
        if field.oneof:
            invoke_fields.append({
                'name': field.name,           # 'start', 'stop', 'config', etc.
                'tag': field.number,          # Field number
                'type': field.type,           # 'Empty', 'Config', etc.
                'is_empty': field.type == 'Empty',
                'message_type': field.type if field.type != 'Empty' else None
            })

    # Extract config fields
    # Note: Config message becomes msg_tick_service_config via nanopb
    # We use the entire nanopb-generated type in tick_service_data
    config_fields = []
    if config_msg:
        for field in config_msg.fields:
            config_fields.append({
                'name': field.name,           # 'delay_ms'
                'type': map_proto_type_to_c(field.type),  # 'uint32_t'
                'is_optional': field.cardinality == 'optional'
            })

        # Generate nanopb type name for the Config message
        config_type = f"msg_{service_name}_config"  # e.g., msg_tick_service_config

    return {
        'service_name': service_name,
        'service_name_upper': service_name.upper(),  # TICK_SERVICE
        'invoke_fields': invoke_fields,
        'report_fields': extract_report_fields(report_msg),
        'config_fields': config_fields,
        'config_type': f"msg_{service_name}_config" if config_msg else None,
        'has_config': config_msg is not None
    }

def map_proto_type_to_c(proto_type):
    """Map protobuf types to C types

    Note: For message types (Config, etc.), use nanopb generated names:
    - MsgTickService.Config -> msg_tick_service_config (lowercase, underscores)
    - No 'struct' prefix needed - nanopb uses typedef
    """
    mapping = {
        'uint32': 'uint32_t',
        'uint64': 'uint64_t',
        'int32': 'int32_t',
        'int64': 'int64_t',
        'bool': 'bool',
        'string': 'char*',
        'bytes': 'uint8_t*',
        'Empty': 'empty',           # nanopb generated: typedef struct empty { ... } empty;
        'MsgServiceStatus': 'msg_service_status',  # nanopb generated type
    }
    return mapping.get(proto_type, proto_type)
```

**Template Best Practices**:
- Use Jinja2 filters: `|upper`, `|lower` for name transformations
- Use conditional blocks: `{% if has_config %}...{% endif %}`
- Use loops: `{% for field in invoke_fields %}...{% endfor %}`
- Follow single-return pattern for non-void functions
- Allow multiple returns in void functions (no gotos)
- Use nanopb generated type names (no `struct` prefix)
- Use tabs for indentation (matching existing code)
- Mark files with "GENERATED FILE - DO NOT EDIT" header
- Keep templates readable and maintainable

### Step 7: Handle Hand-Written Code

**tick_service_impl.c Strategy**:
1. **Never generate or overwrite** - This file contains hand-written business logic
2. **Developer creates manually** based on generated API structure
3. **Generator only outputs** `.h` and `.c` files

**Developer Workflow**:
```python
# In generate_service.py - explicitly skip _impl.c
def main(args):
    parse_and_generate(args.proto, args.output_dir, args.service_name)

    print(f"Generated: {args.service_name}.h")
    print(f"Generated: {args.service_name}.c")
    print(f"Note: {args.service_name}_impl.c must be written manually")
    print(f"      Implement functions defined in struct {args.service_name}_api")
```

**Key Principle**: Proto file is single source of truth for data structures and API.
Developers never edit generated files - only `.proto` and `_impl.c`.

## Critical Files

### Files to Create
- `modules/services/shared/codegen/generate_service.py` - Code generator script (~300-400 lines)
- `modules/services/shared/codegen/templates/service.h.jinja` - Header template (~80 lines)
- `modules/services/shared/codegen/templates/service.c.jinja` - Implementation template (~60 lines)

### Files to Modify (Optional - for CMake integration)
- `modules/services/tick/CMakeLists.txt` - Add custom command (optional, for Option A)

### Files to Generate (Output - overwritten on each run)
- `modules/services/tick/tick_service.h` - Generated header (DO NOT EDIT)
- `modules/services/tick/tick_service.c` - Generated infrastructure (DO NOT EDIT)

### Files to Hand-Write (NEVER generated)
- `modules/services/tick/tick_service.proto` - Proto definition (source of truth)
- `modules/services/tick/tick_service_impl.c` - Business logic implementation

## Verification Strategy

### Step 1: Test Generator
```bash
cd modules/services/tick
python3 generate_service.py --proto tick_service.proto --output-dir /tmp/generated
diff /tmp/generated/tick_service.h tick_service.h
diff /tmp/generated/tick_service.c tick_service.c
```

### Step 2: Build Verification
```bash
just clean build
# Should compile successfully with generated files
```

### Step 3: Runtime Verification
```bash
just clean build run
# Verify tick service still functions:
# - Service initialization message
# - Tick events every 1000ms
# - Status changes on start/stop
```

### Step 4: Functional Testing
- Verify service registration succeeds
- Verify start command triggers timer
- Verify stop command stops timer
- Verify status queries work
- Verify config commands work

## Nanopb Type Compliance

### Understanding nanopb Generated Types

Nanopb generates C code from proto files with specific naming conventions:

**Proto Definition:**
```protobuf
message MsgTickService {
  message Config {
    uint32 delay_ms = 1;
  }
}
```

**Nanopb Generated Code** (`tick_service.pb.h`):
```c
// Nested message becomes: msg_<parent>_<nested> (lowercase with underscores)
typedef struct msg_tick_service_config {
    uint32_t delay_ms;
} msg_tick_service_config;

// Main message
typedef struct msg_tick_service_invoke {
    pb_size_t which_tick_invoke;
    union {
        empty start;                      // Empty type from service.proto
        empty stop;
        empty get_status;
        msg_tick_service_config config;   // Nested Config type
        empty get_config;
    };
} msg_tick_service_invoke;
```

**Key Points:**
- nanopb uses `typedef` - no `struct` prefix needed
- Nested messages: `msg_<parent_snake>_<nested_snake>`
- Empty type: `empty` (from shared service.proto)
- Standard types: `uint32_t`, `bool`, etc.

**Generator Must:**
1. Use nanopb type names exactly: `msg_tick_service_config` not `struct msg_tick_service_config`
2. Reference nanopb-generated constants: `MSG_TICK_SERVICE_INVOKE_START_TAG`
3. Use nanopb initializers: `MSG_TICK_SERVICE_INVOKE_INIT_DEFAULT`

## Design Decisions

### Why proto-schema-parser?
- Pure Python (no protoc dependency)
- Simple API for parsing .proto files
- Returns structured Python objects
- Lightweight and easy to install

### Why use Copier/Jinja2 templates?
- Separates logic from output format
- Templates are readable and maintainable
- Easy to modify generated code structure
- Standard Python templating (Jinja2)
- Supports conditionals, loops, filters
- No string concatenation mess

### Why centralized generator in shared/codegen/?
- Single script serves all services
- Shared templates for consistency
- Easier to maintain and update
- Can add common utilities and helpers

### Why fully generate data/API structures?
- Proto file is single source of truth
- Eliminates manual synchronization errors
- Forces developers to think in terms of messages/APIs
- Regeneration is safe and deterministic

### Why never edit generated files?
- Clear separation: proto + impl.c = complete service
- Regeneration doesn't require manual merging
- Forces good architecture (data-driven design)
- Generated files can be .gitignored in future

### Why generate _impl.c as optional template (not required)?
- ✅ **UPDATED**: Now supports optional _impl.c template generation via `--generate-impl` flag
- Template provides proper structure and TODO comments for faster development
- Never overwrites existing _impl.c files (safe to run multiple times)
- Contains hand-written business logic that cannot be fully derived from proto
- Developer responsibility to complete implementation based on TODO guidance

### Why single-return pattern (for non-void functions)?
- User requirement for code style
- Easier cleanup and error handling
- Matches existing code conventions
- Relaxed for void functions (multiple returns acceptable)
- No gotos in generated code (use early returns for void functions)

## Risks and Mitigation

### Risk: proto-schema-parser parsing errors
**Mitigation**: Validate proto structure, provide clear error messages

### Risk: Overwriting hand-written code
**Mitigation**: Never generate _impl.c at all; only .h and .c are generated (clearly marked DO NOT EDIT)

### Risk: Proto changes breaking generated code
**Mitigation**: Regenerate after proto changes, test build

### Risk: Build system integration complexity
**Mitigation**: Start with manual generation, add CMake automation incrementally

## Completed Enhancements

1. ✅ **_impl.c Template Generation** (2026-01-25): Optional template generation with `--generate-impl` flag
2. ✅ **Cleaned Up Includes** (2026-01-25): Removed unnecessary includes from service.c.jinja template
3. ✅ **Correct Oneof Field Names** (2026-01-25): Use `invoke_oneof_name` variable instead of hardcoded `service_name` in switch statement
4. ✅ **Improved Template Implementations** (2026-01-25): Replaced verbose TODOs with actual working implementations for standard functions
5. ✅ **Private Header Generation** (2026-01-25): Generate `private/<service>_priv.h` with static inline report helper functions

## Future Enhancements

1. Generate CMakeLists.txt for new services
2. Generate Kconfig for new services
3. Extend to other services (ui, battery, tamper_detection)
4. Add validation for proto structure conventions
5. Generate unit test stubs
6. Add --watch mode for development
7. Automatic CMake integration (regenerate on proto changes)

## Developer Workflow Summary

```
1. Developer edits tick_service.proto
   ├─> Add new Invoke command: Empty my_command = 6;
   └─> Add new Config field: uint32 my_param = 2;

2. Developer runs generator
   $ cd modules/services/shared/codegen
   $ python3 generate_service.py \
       --proto ../../tick/tick_service.proto \
       --output-dir ../../tick \
       --service-name tick_service

3. Generator produces:
   ├─> tick_service.h (with new API function pointer and inline function)
   ├─> tick_service.c (with new switch case in dispatcher)
   └─> Note: Implement my_command() in tick_service_impl.c

4. Developer edits tick_service_impl.c
   ├─> Add my_command() implementation
   ├─> Update api struct to include .my_command = my_command
   └─> Use new config field (data->config.my_param)

5. Build and test
   $ just clean build run
```

## Success Criteria (Original Implementation)

✅ proto-schema-parser correctly parses tick_service.proto
✅ Generator extracts service name, invoke fields, config fields
✅ Generated tick_service.h defines correct data/API structures from proto
✅ Generated tick_service.c creates correct dispatcher with all invoke cases
✅ Generated files have "DO NOT EDIT" warning at top
✅ struct tick_service_data uses nanopb generated type (msg_tick_service_config)
✅ struct tick_service_api includes all function pointers from Invoke oneof
✅ API function signatures use nanopb types correctly (no struct prefix)
✅ Generated code follows single-return for non-void functions
✅ Generated code uses multiple returns for void functions (no gotos)
✅ Inline API functions generated for all Invoke fields with correct signatures
✅ Project builds successfully with generated files
✅ Tick service functions correctly at runtime
✅ Generator can be run multiple times (regeneration is idempotent)
✅ Generator script located in modules/services/shared/codegen/
✅ Templates use Jinja2 for clean code generation
✅ Templates properly handle conditionals and loops

---

## Refinement: Cleaned Up service.c.jinja Includes

**Completed: 2026-01-25**

### Changes
Removed unnecessary includes from `service.c.jinja` template:
- ❌ Removed: `"service.pb.h"` (not needed, types come through service header)
- ❌ Removed: `"{{ service_name }}/{{ service_name }}.pb.h"` (included via service header)
- ❌ Removed: `"zephyr/init.h"` (not used)
- ❌ Removed: `"zephyr/spinlock.h"` (not used in infrastructure file)
- ❌ Removed: `"zephyr/sys/check.h"` (not used)
- ❌ Removed: `<sys/errno.h>` (not needed, only -EBUSY used)

**Result**: Cleaner generated code with only necessary includes:
- ✅ `"{{ service_name }}.h"` - Service interface
- ✅ `<zephyr/zbus/zbus.h>` - zbus types and macros
- ✅ `<zephyr/kernel.h>` - Zephyr kernel types
- ✅ `<zephyr/logging/log.h>` - Logging macros

**Additional Fix**: Changed `which_{{ service_name }}_invoke` to `which_{{ invoke_oneof_name }}` to use the correct oneof field name extracted from the proto.

---

## Enhancement: _impl.c Template Generation

**Completed: 2026-01-25**

### Goal
Add optional template generation for `<service>_impl.c` files to accelerate developer workflow when creating new services.

### Problem Statement
Developers had to manually create `_impl.c` files from scratch, which was time-consuming and error-prone. A template would:
- Ensure consistent structure across all services
- Provide proper function signatures matching the generated API
- Include spinlock usage patterns for thread safety
- Remind developers to publish reports when state changes
- Reduce boilerplate coding

### Implementation Summary

#### Files Created
1. **`templates/service_impl.c.jinja`** - New template with:
   - Proper includes and logging declarations
   - TODO comments for helper macros and service-specific resources
   - Function stubs with correct signatures for all API functions
   - Context-specific guidance for common functions (start, stop, config, get_status, get_config)
   - Spinlock usage patterns (`K_SPINLOCK(&data->lock) { ... }`)
   - WARNING comments reminding to publish reports on state changes
   - Static API and data structures
   - Init function and SERVICE_DEFINE macro

#### Files Modified
2. **`generate_service.py`**:
   - Added extraction of `report_oneof_name` from Report message oneof
   - Added `report_oneof_name` to context dictionary
   - Added `--generate-impl` command-line flag
   - Added conditional generation logic (only creates `_impl.c` if it doesn't exist)
   - Updated docstring with usage examples for both modes

3. **`CLAUDE.md`** (project root):
   - Updated "What Gets Generated" section to include `_impl.c` template
   - Updated "What Is Hand-Written" section to clarify template usage
   - Added usage examples for both with/without `--generate-impl`
   - Split "Developer Workflow" into "New Services" and "Existing Services"
   - Updated "Key Principles" to mention template behavior
   - Updated "How It Works" section with new template and parser capabilities

### Usage

#### For New Services
```bash
cd modules/services/shared/codegen

python3 generate_service.py \
    --proto ../../new_service/new_service.proto \
    --output-dir ../../new_service \
    --service-name new_service \
    --module-dir new_service \
    --generate-impl
```

Output:
```
Parsing ../../new_service/new_service.proto...
Service: new_service
Invoke fields: ['start', 'stop', 'config']
Rendering templates...
Generated: ../../new_service/new_service.h
Generated: ../../new_service/new_service.c
Generated template: ../../new_service/new_service_impl.c
Note: Complete TODO items and remove WARNING comments
```

#### For Existing Services (Modifying APIs)
```bash
python3 generate_service.py \
    --proto ../../tick/tick_service.proto \
    --output-dir ../../tick \
    --service-name tick_service \
    --module-dir tick
    # Note: No --generate-impl flag to avoid attempting to overwrite existing impl
```

### Template Structure

The generated `_impl.c` template includes:

1. **Header Comments**: Marks file as generated template with TODO reminder
2. **Includes**: Proper service header and logging includes
3. **TODO Sections**:
   - Helper macros (for report message construction)
   - Service-specific resources (timers, work queues, threads)
4. **Function Stubs**: For each invoke field with:
   - Correct function signature matching API
   - Context-specific TODO comments explaining what the function should do
   - Spinlock usage pattern for thread-safe state access
   - WARNING comment with example of how to publish reports
5. **Static Structures**: API struct, data struct with proper initialization
6. **Init Function**: Standard initialization with implementation registration
7. **SERVICE_DEFINE**: Macro to register service with Zephyr

### Template TODO Comments

Three types of guidance:

1. **Section TODOs** - Optional areas for service-specific code
2. **Function TODOs** - Guidance for each API function:
   - `start`: Update state to running, start resources, report status
   - `stop`: Stop resources, update state to stopped, report status
   - `get_status`: Read current status, publish report
   - `get_config`: Read current config, publish report
   - `config`: Validate new config, update state, publish report
   - Others: Implement business logic, use spinlocks, report changes
3. **Inline WARNINGs** - Critical reminders within function bodies to publish reports

### Safety Guarantees

- **Never Overwrites**: If `<service>_impl.c` exists, generator skips it with message
- **Idempotent**: Safe to run multiple times; existing implementations preserved
- **Explicit Opt-In**: Only generates template when `--generate-impl` flag is provided
- **Clear Marking**: Template includes header indicating it needs completion

### Verification Results

✅ New template file `service_impl.c.jinja` created with proper structure
✅ Generator extracts `report_oneof_name` from proto Report message
✅ `--generate-impl` flag added and working correctly
✅ Generator creates `_impl.c` ONLY if file doesn't exist
✅ Generated `_impl.c` includes all function stubs with correct signatures
✅ Generated `_impl.c` includes 23 TODO comments providing guidance
✅ Generated `_impl.c` includes 5 WARNING comments about reporting
✅ Generated `_impl.c` includes spinlock usage patterns
✅ Generated `_impl.c` includes static structures (api, data)
✅ Generated `_impl.c` includes init function and SERVICE_DEFINE
✅ Existing `_impl.c` files are never overwritten
✅ Generated template compiles successfully (with stub returns)
✅ Tested with test service: all files generated correctly
✅ Tested with existing tick service: `_impl.c` correctly preserved

### Developer Workflow (New Services)

1. **Create Proto File**: Define messages, Invoke, Report, Config
2. **Generate All Files**:
   ```bash
   python3 generate_service.py ... --generate-impl
   ```
3. **Complete Implementation**:
   - Read TODO comments for guidance
   - Implement business logic with proper spinlock usage
   - Add zbus_chan_pub calls to report state changes
   - Remove TODO/WARNING comments when complete
4. **Build and Test**:
   ```bash
   just clean build run
   ```

### Benefits

1. **Faster Development**: Start with proper structure instead of blank file
2. **Consistency**: All services follow the same patterns
3. **Guidance**: TODO comments explain what each function should do
4. **Safety**: Template includes spinlock patterns and thread-safety reminders
5. **Best Practices**: Reminds developers to report state changes
6. **No Risk**: Never overwrites existing implementations
7. **Reduced Errors**: Proper signatures and structures from the start

---

## Enhancement: Private Header Generation

**Completed: 2026-01-25**

### Goal
Generate `private/<service>_priv.h` with static inline helper functions for publishing service reports, providing a clean API for implementation code while keeping these helpers private (not exposed in public `<service>.h`).

### Problem Statement
Implementation files manually wrote verbose `zbus_chan_pub` calls for every report:
```c
// Before: Repetitive, error-prone
return zbus_chan_pub(
    &chan_tick_service_report,
    &(struct msg_tick_service_report){
        .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
        .status = *status},
    timeout);
```

This led to:
- Code duplication across multiple report sites
- Easy to introduce typos in channel names or field names
- Hard to maintain when report structure changes
- Verbose code that obscures business logic

### Solution
Generate a private header with static inline helper functions, one per Report oneof field:
```c
// After: Clean, type-safe API
return tick_service_report_status(&status, K_MSEC(250));
```

### Implementation Summary

#### Files Created
1. **`templates/service_priv.h.jinja`** - New template that:
   - Includes the service header (`<service>.h`)
   - Generates static inline functions for each Report oneof field
   - Handles three categories of report types:
     - **Shared types** (MsgServiceStatus): Uses `msg_service_status`
     - **Service-specific types** (Config): Uses `msg_<service>_config`
     - **Empty types**: No parameters except timeout
   - Provides proper function signatures with type safety
   - Encapsulates zbus_chan_pub boilerplate

#### Files Modified
2. **`generate_service.py`**:
   - Added extraction of `report_fields` from Report message oneof
   - Each report field includes: name, tag, type, is_empty flag, message_type
   - Creates `private/` subdirectory in output directory
   - Generates `private/<service>_priv.h` when using `--generate-impl`
   - Added `report_fields` to context dictionary for template rendering

3. **`templates/service_impl.c.jinja`**:
   - Added `#include "private/<service>_priv.h"` to access helpers
   - Updated function implementations to use helper functions
   - Replaced direct `zbus_chan_pub` calls with helper function calls
   - Examples:
     - `start()`: Uses `<service>_report_status(&status, timeout)`
     - `stop()`: Uses `<service>_report_status(&status, timeout)`
     - `get_config()`: Uses `<service>_report_config(&config, timeout)`
     - `get_status()`: Uses `<service>_report_status(&status, timeout)`

4. **`CLAUDE.md`** (project root):
   - Added comprehensive "Private Header Generation" section with:
     - Purpose and benefits of private headers
     - Generated helper function structure
     - Type mapping (Shared/Service-specific/Empty)
     - Full usage example from tick service
     - File structure showing `private/` directory
   - Updated "What Gets Generated" to list `private/<service>_priv.h`
   - Updated Templates section to include `service_priv.h.jinja`
   - Updated workflow examples to show private header in output

5. **`justfile`** (project root):
   - Added `gen_service_files` recipe for convenient service generation:
     ```just
     gen_service_files service_name:
         python3 modules/services/shared/codegen/generate_service.py \
         --proto modules/services/{{service_name}}/{{service_name}}_service.proto \
         --output-dir modules/services/{{service_name}} \
         --service-name {{service_name}}_service \
         --module-dir {{service_name}} \
         --generate-impl
     ```

### Generated Private Header Structure

For a service with Report message like:
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

Generates `private/tick_service_priv.h`:
```c
#ifndef _TICK_SERVICE_PRIV_H_
#define _TICK_SERVICE_PRIV_H_

#include "tick_service.h"

/* Helper: Report status (shared type) */
static inline int tick_service_report_status(const struct msg_service_status *status,
                                             k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_tick_service_report,
        &(struct msg_tick_service_report){
            .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
            .status = *status},
        timeout);
}

/* Helper: Report config (service-specific type) */
static inline int tick_service_report_config(const struct msg_tick_service_config *config,
                                             k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_tick_service_report,
        &(struct msg_tick_service_report){
            .which_tick_report = MSG_TICK_SERVICE_REPORT_CONFIG_TAG,
            .config = *config},
        timeout);
}

/* Helper: Report tick (empty type - no payload) */
static inline int tick_service_report_tick(k_timeout_t timeout)
{
    return zbus_chan_pub(
        &chan_tick_service_report,
        &(struct msg_tick_service_report){
            .which_tick_report = MSG_TICK_SERVICE_REPORT_TICK_TAG},
        timeout);
}

#endif /* _TICK_SERVICE_PRIV_H_ */
```

### Type Mapping Logic

The generator handles three categories of Report message types:

1. **Shared Types** (e.g., `MsgServiceStatus`):
   - Proto field type: `MsgServiceStatus`
   - C parameter type: `const struct msg_service_status *`
   - Usage: `.status = *status`

2. **Service-Specific Types** (e.g., `Config`):
   - Proto field type: `Config`
   - C parameter type: `const struct msg_<service>_config *`
   - Usage: `.config = *config`

3. **Empty Types** (events with no payload):
   - Proto field type: `Empty`
   - C parameter: None (only `k_timeout_t timeout`)
   - Usage: No payload field in struct literal

### Usage Example

**Before** (manual zbus_chan_pub):
```c
static int start(const struct service *service)
{
    struct tick_service_data *data = service->data;
    struct msg_service_status status;

    K_SPINLOCK(&data->lock) {
        data->status.is_running = true;
        status = data->status;
    }

    /* TODO: Start service resources */

    return zbus_chan_pub(
        &chan_tick_service_report,
        &(struct msg_tick_service_report){
            .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
            .status = status},
        K_MSEC(250));
}
```

**After** (with private header helpers):
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

    /* TODO: Start service resources */

    return tick_service_report_status(&status, K_MSEC(250));
}
```

### Benefits

1. **Cleaner Code**: Reduces verbose `zbus_chan_pub` calls to simple function calls
2. **Type Safety**: Compiler checks parameter types at call sites
3. **Maintainability**: Report structure changes only require template updates
4. **Discoverability**: IDE autocomplete shows available report functions
5. **Privacy**: Implementation helpers not exposed in public API header
6. **Error Prevention**: Can't typo channel names or field names
7. **Consistency**: All services follow same helper pattern
8. **Refactoring Safety**: Changing report structure updates all helpers automatically

### File Structure

```
modules/services/<service>/
├── <service>.h                    # Public API (generated)
├── <service>.c                    # Infrastructure (generated)
├── private/
│   └── <service>_priv.h          # Private helpers (generated with --generate-impl)
└── <service>_impl.c              # Implementation (uses private helpers)
```

### Verification Results

✅ New template file `service_priv.h.jinja` created
✅ Generator extracts `report_fields` from Report message oneof
✅ Generator creates `private/` subdirectory automatically
✅ Private header generated when using `--generate-impl` flag
✅ Helper functions handle three type categories correctly:
   - ✅ Shared types (MsgServiceStatus)
   - ✅ Service-specific types (Config)
   - ✅ Empty types (no parameters)
✅ Generated helpers have correct function signatures
✅ Generated helpers use correct nanopb type names
✅ `_impl.c` template includes private header
✅ `_impl.c` template uses helper functions in all standard functions
✅ Code compiles successfully with generated helpers
✅ tick_service successfully migrated to use private header pattern
✅ Justfile recipe `gen_service_files` simplifies generation workflow
✅ CLAUDE.md updated with comprehensive private header documentation

### Developer Workflow (Using justfile)

**Generate new service with all files**:
```bash
just gen_service_files new_service
```

This single command:
1. Parses `new_service_service.proto`
2. Generates `new_service_service.h` (public API)
3. Generates `new_service_service.c` (infrastructure)
4. Generates `private/new_service_service_priv.h` (report helpers)
5. Generates `new_service_service_impl.c` (implementation template using helpers)

**Developer then**:
1. Reviews generated files
2. Completes TODO items in `_impl.c`
3. Uses helper functions: `new_service_report_status(&status, timeout)`
4. Builds and tests: `just clean build run`

### Comparison: Before vs After

**Lines of Code per Report** (tick service example):

Before (manual):
```c
// 7 lines per report call
return zbus_chan_pub(
    &chan_tick_service_report,
    &(struct msg_tick_service_report){
        .which_tick_report = MSG_TICK_SERVICE_REPORT_STATUS_TAG,
        .status = status},
    K_MSEC(250));
```

After (with helper):
```c
// 1 line per report call
return tick_service_report_status(&status, K_MSEC(250));
```

**Result**: 85% reduction in boilerplate code per report call.

### Future Enhancements

Potential improvements:
- Automatic CMake integration (regenerate on proto changes)
- Generate CMakeLists.txt for new services
- Generate Kconfig files for new services
- Generate unit test stubs
- Add `--watch` mode for development
- Extend template to include service-specific patterns (timers, work queues)
