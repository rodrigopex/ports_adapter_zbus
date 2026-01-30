# Codegen Plan

## Goal
Generate `.h`, `.c`, `private/*_priv.h`, `_impl.c` template from `.proto` via proto-schema-parser.

## Structure
```
tick_service.proto → [parser] → Python generator → .h/.c/priv.h/_impl.c
```

## Generator Location
`modules/services/shared/codegen/`
- `generate_service.py` - Main script
- `templates/` - Jinja2 templates: service.h/c/priv.h/impl.c.jinja

## Dependencies
```bash
pip install proto-schema-parser jinja2
```

## Usage
```bash
# New service
just gen_service_files <service>

# Or manual
python3 generate_service.py \
  --proto ../../<s>/<s>.proto \
  --output-dir ../../<s> \
  --service-name <s> \
  --module-dir <s> \
  --generate-impl
```

## Generator Logic
1. Parse proto (proto-schema-parser)
2. Extract: service_name, invoke/report/config fields, RPC methods
3. Build context dict
4. Render Jinja2 templates
5. Write files (skip existing _impl.c)

## Parsing Strategy
- Service name: MsgTickService → tick_service
- Invoke fields: oneof fields → API functions
- Report fields: oneof fields → report helpers
- Config fields: Config message fields
- RPC methods: service definition → map return types to report functions

## Key Parser Details
- Service defs: `Service` class
- RPC methods: `Method` class (not `Rpc`)
- Method types: `MessageType` objects with `.type`, `.stream`
- Access: `method.input_type.type`, `method.output_type.type`

## Type Mapping
Proto → C:
- `uint32` → `uint32_t`
- `Empty` → `empty`
- `MsgServiceStatus` → `msg_service_status`
- `Config` → `msg_<service>_config`
- CamelCase → snake_case (via camel_to_snake filter)

## Generated Files
- `<service>.h` - Data/API structs, inline client funcs (DO NOT EDIT)
- `<service>.c` - Channels, dispatcher, registration (DO NOT EDIT)
- `private/<service>_priv.h` - Report helpers (DO NOT EDIT, --generate-impl only)
- `<service>_impl.c` - Template with TODOs (NEVER OVERWRITES)

## Header Template
Generates:
- Data struct: `<service>_data` with spinlock, config, status
- API struct: function pointers from Invoke fields
- Inline funcs: `<service>_<cmd>(timeout)` → pub to invoke chan
- Registration: `<service>_set_implementation()`

## Implementation Template
Generates:
- Channels (invoke/report), listener, dispatcher
- Dispatcher switch/case on invoke oneof tags
- API function routing

## Private Header (--generate-impl)
Generates: `private/<service>_priv.h`
- Static inline helpers: `<service>_report_<field>(args, timeout)`
- Wraps zbus_chan_pub boilerplate
- Type-safe, per-report-field functions

Report types:
- `MsgServiceStatus` → `report_status(status*, timeout)`
- `Config` → `report_config(config*, timeout)`
- `Empty` → `report_<field>(timeout)`
- Custom → `report_<field>(type*, timeout)`

## _impl.c Template (--generate-impl)
Generates template with:
- Includes, logging, TODO sections
- Function stubs with correct signatures
- Spinlock patterns: `K_SPINLOCK(&data->lock) { ... }`
- TODO/WARNING comments for guidance
- Static api/data structs
- Init func, SERVICE_DEFINE macro
- Uses report helpers from private header

Safety: Never overwrites existing _impl.c

## RPC-Based Report Mapping
Parses `service` definition → maps RPC return types to report functions:
- `returns MsgServiceStatus` → `report_status()`
- `returns Config` → `report_config()`
- `returns (stream) Events` → `report_events()`
- `returns Empty` → no Report field needed (exception)

Validates RPC methods vs Invoke/Report fields. Fallback to invoke_fields if no service def.

## Stream Semantics
- **Output streaming**: Server pushes events, no Invoke field needed
  - Example: `report_events(stream Empty) returns (stream Events)` (tick_service.proto:44)
  - Pattern: Publish from timer/IRQ handler with K_NO_WAIT
  - Validation: Skips "no Invoke field" warning
- **Input streaming**: Client sends stream (placeholder, not yet used)
- **Flags**: `input_streaming`, `output_streaming` in RPC method context
- **Template**: Generates timer/work queue pattern examples for output streams

## Template Filters
- `camel_to_snake` - BatteryState → battery_state
- `upper`, `lower`

## Validation
- Check RPC methods have Invoke fields
- Check return types have Report fields (except Empty returns)
- Warn on inconsistencies

## CMake Integration
Current: Manual generation, commit files
Future: Custom command to regen on proto changes

## Workflow
### New Service
1. Write `.proto` (messages, Invoke, Report, Config, service def)
2. `just gen_service_files <service>`
3. Complete `_impl.c`: Read TODOs, use K_SPINLOCK, call report helpers
4. Add CMakeLists.txt, Kconfig, module.yml
5. Enable in prj.conf
6. `just c b r`

### Modify Service
1. Edit `.proto`
2. Regenerate (no --generate-impl)
3. Update `_impl.c` (new funcs in api struct)
4. Build/test

## Principles
- Proto = source of truth
- Never edit generated files
- Regeneration safe (idempotent)
- _impl.c never overwritten
- Single-return for non-void (no gotos)
- Multiple returns OK for void
- Nanopb types: no `struct` prefix, typedef'd

## Verification
✅ Parser extracts all proto elements
✅ Generator creates correct files
✅ Generated code compiles
✅ Services function at runtime
✅ Regeneration idempotent
✅ Never overwrites _impl.c
✅ Private header types match nanopb
✅ RPC methods map to report funcs
✅ Templates handle all type categories

## Enhancements Completed
✅ _impl.c template generation (2026-01-25)
✅ Clean includes in service.c
✅ Correct oneof field names
✅ Private header generation
✅ CamelCase→snake_case fix
✅ RPC-based report mapping
✅ Init message typo fix

## Future
- Auto CMake integration
- Generate CMakeLists/Kconfig
- Test stubs
- --watch mode
- Validation for proto conventions

## Success Criteria
✅ All templates render correctly
✅ Generated files build without errors
✅ Services runtime functional
✅ Type names match nanopb
✅ Report helpers work correctly
✅ RPC mappings accurate
✅ Documentation complete
✅ Workflow streamlined via justfile

## Unresolved Questions
None. All features implemented and verified.
