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

#include "service.pb.h"
#include "{{ service_name }}.h"
#include "{{ service_name }}/{{ service_name }}.pb.h"
#include "zephyr/init.h"
#include "zephyr/spinlock.h"
#include "zephyr/sys/check.h"
#include "zephyr/zbus/zbus.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sys/errno.h>

LOG_MODULE_REGISTER({{ service_name }}, CONFIG_{{ service_name_upper }}_LOG_LEVEL);

ZBUS_CHAN_DEFINE(chan_{{ service_name }}_invoke, msg_{{ service_name }}_invoke, NULL, NULL,
		 ZBUS_OBSERVERS(lis_{{ service_name }}), MSG_{{ service_name_upper }}_INVOKE_INIT_DEFAULT);

ZBUS_CHAN_DEFINE(chan_{{ service_name }}_report, msg_{{ service_name }}_report, NULL, NULL,
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
	const msg_{{ service_name }}_invoke *ivk;

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

	switch (ivk->which_{{ service_name }}_invoke) {
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

### Why never generate _impl.c?
- Contains hand-written business logic
- Cannot be automatically derived from proto
- Developer responsibility to implement API contract

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

## Future Enhancements

1. Generate CMakeLists.txt for new services
2. Generate Kconfig for new services
3. Extend to other services (ui, battery, tamper_detection)
4. Add validation for proto structure conventions
5. Generate unit test stubs
6. Add --watch mode for development

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

## Success Criteria

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
