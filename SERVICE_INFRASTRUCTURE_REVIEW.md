# Zephlet Infrastructure Architecture Review

Infrastructure analysis of zephlet framework (`modules/lib/zephlet/`) - NOT application zephlets.
Focus: Core abstractions, code generation, build system, tooling enabling Ports & Adapters via zbus.

## Overview

**Architecture:** Protocol-driven code generation framework for Ports & Adapters pattern on Zephyr RTOS.

**Components:**
1. **Core runtime** (`zephlet.h/c`) - struct definitions, auto-discovery via linker sections
2. **Code generators** (Python) - Proto → Jinja2 → .h/.c interfaces + bootstrap templates
3. **Build integration** (CMake) - Two-stage build, dependency tracking, proto processing
4. **West extensions** (`zephlet_commands.py`) - CLI for new/new-adapter/gen commands
5. **Template system** (6 Jinja2 files) - Consistent codegen patterns
6. **Messaging layer** (zbus integration) - Two-channel pattern (invoke/report)

**Design Philosophy:** Convention over configuration. Proto files are single source of truth. Generated code never mixed with hand-written.

---

## 1. Core Abstractions

**Files:** `modules/lib/zephlet/zephlet.h`, `modules/lib/zephlet/zephlet.c`

### Architecture

**struct zephlet** (`zephlet.h:9-17`):
```c
struct zephlet {
    const char *name;
    void *api;              /* ← Type unsafety */
    void *data;             /* ← Type unsafety */
};
```

**Auto-discovery** (`zephlet.h:19`):
```c
STRUCT_SECTION_ITERABLE(zephlet, zlet_##_name)
```
- Uses linker sections for zero-config runtime enumeration
- No manual registration required

**Initialization** (`zephlet.c:19`):
```c
SYS_INIT(zephlet_init, APPLICATION, 99);
```
- Hardcoded priority 99
- Iterates all zephlets, calls `init()` if present
- No inter-zephlet dependency ordering
- No cleanup/deinit path

**Two-channel pattern** (per-zephlet):
- **Invoke channel**: Commands to zephlet (RPC-like)
- **Report channel**: Events/responses from zephlet
- Channels defined in proto, codegen creates zbus integration

### Strengths ✓

1. **Zero-config discovery** - `STRUCT_SECTION_ITERABLE` eliminates registration boilerplate
2. **Clean separation** - API/data pointers enable encapsulation
3. **Convention-based** - Naming derived from proto, predictable patterns
4. **Loose coupling** - zbus-only communication, no direct zephlet dependencies
5. **Lifecycle hooks** - Optional init/deinit in API struct

### Weaknesses ✗

1. **Type unsafety** - `void *api` and `void *data` lose compile-time checking
   - Requires manual casting: `const struct zlet_tick_api *api = zephlet->api;`
   - No type tags or RTTI, runtime errors possible from incorrect casts
   - Could use `container_of()` pattern or tagged unions

2. **No lifecycle cleanup** - Missing deinit path
   - `SYS_INIT` runs init, but no symmetric shutdown
   - Timers/threads live forever once created
   - Resource leaks possible in dynamic scenarios

3. **Hardcoded init priority** - `SYS_INIT(..., 99)`
   - No inter-zephlet dependency ordering
   - All zephlets init at same priority
   - Could add `depends_on` array to struct

4. **No init failure handling** - `zephlet.c:22`
   - Init errors logged via `printk`, but iteration continues
   - Should use `LOG_ERR` instead of `printk`
   - No rollback on partial init failure

---

## 2. Code Generation Architecture

**Files:** `generate_zephlet.py`, `generate_adapter.py`, 6 Jinja2 templates

### Architecture

**Two-stage build:**
1. **Bootstrap** (first build only): `--impl-only` generates .c template if missing
2. **Interface regen** (every build): Always regenerate _interface.h/c, never touch .c

**File lifecycle:**
- **VCS-tracked**: .proto, .c (after bootstrap), CMakeLists.txt, Kconfig, module.yml
- **Build artifacts**: _interface.h, _interface.c, <zephlet>.h, .pb.h, .pb.c

**Validation** (`generate_zephlet.py:198-283`):
- **RPC return type checking** - Enforces proto conventions:
  - Field 1-6 in Invoke reserved for RPC methods
  - Field 1-3 in Report reserved for RPC responses
  - Return types must match Report message types
  - Examples: `returns MsgZephletStatus` → `report_status()` helper
- **Custom type validation** (`generate_zephlet.py:138-154`) - Maps proto types to C types

**Template system** (6 Jinja2 files):
1. `zephlet.h.jinja` - Public interface (API struct, inline helpers)
2. `zephlet_interface.h.jinja` - zbus channels, message unions
3. `zephlet_interface.c.jinja` - Dispatcher (invoke → API calls)
4. `zephlet.c.jinja` - Bootstrap template (TODOs, struct skeleton)
5. `adapter.c.jinja` - Adapter bootstrap (channel subscription)
6. `adapter.h.jinja` - Adapter header (minimal)

**Dispatcher pattern** (`zephlet_interface.c.jinja:26-55`):
```c
void zlet_tick_invoke_dispatcher(const struct zbus_channel *chan) {
    const struct msg_zlet_tick_invoke *ivk = zbus_chan_const_msg(chan);
    const struct zephlet *impl = /* lookup */;
    const struct zlet_tick_api *api = impl->api;

    switch (ivk->which_tick_invoke) {
    case msg_zlet_tick_invoke_init_tag:
        api->init(impl);
        break;
    /* ... */
    }
}
```

### Strengths ✓

1. **Protocol-driven design** - Proto as single source of truth
2. **RPC validation** - Strict compile-time checking of return types vs Report fields
3. **Non-destructive codegen** - Bootstrap-once prevents overwriting user code
4. **Template consistency** - Jinja2 ensures uniform patterns across all zephlets
5. **Type safety layers** - Proto + codegen validation + compiler checks
6. **Clean separation** - Generated files never mixed with hand-written
7. **Custom type support** - Extensible type mapping for domain types
8. **Idempotent builds** - Regen interfaces every build without breaking incremental compile

### Weaknesses ✗

1. **Proto convention not enforced at parse time**
   - Reserved field numbers (1-6 Invoke, 1-3 Report) only documented
   - Validation happens in Python, not proto compiler
   - Breaking conventions causes cryptic errors

2. **Custom type validation gaps** (`generate_zephlet.py:138-154`)
   - Ambiguous types like `BatteryState` could map incorrectly
   - No check if custom type actually exists in proto file
   - Error messages unhelpful: "ambiguous custom type"

3. **Missing default case in dispatcher template** (`zephlet_interface.c.jinja:26-55`)
   - Switch on `which_<zephlet>_invoke` has no `default:`
   - Silently ignores unknown tags
   - Should log warning for debugging

4. **Bootstrap assumption fragility**
   - If proto changes significantly after bootstrap, .c template outdated
   - No warning that .c might need regeneration
   - Developer must manually sync

5. **Parser error messages** (`generate_zephlet.py:34`)
   - Uses `proto-schema-parser` library
   - Proto syntax errors give unhelpful stack traces
   - Could benefit from better error context

6. **No versioning in generated code**
   - Generated files lack proto hash or timestamp
   - Hard to detect interface/impl drift
   - Could add `/* Generated from <proto> at <timestamp> */`

---

## 3. Build System Integration

**Files:** `zephyr_zephlet_codegen.cmake`, `zephyr_zephlet_library.cmake`

### Architecture

**Two-stage build strategy** (`zephyr_zephlet_codegen.cmake:13-100`):

**Stage 1 - Bootstrap** (first build):
```cmake
execute_process(
    COMMAND ${Python3_EXECUTABLE} generate_zephlet.py
        --proto ${PROTO_FILE}
        --output-dir ${OUTPUT_DIR}
        --impl-only  # ← Generates .c template if missing
)
```

**Stage 2 - Interface regen** (every build):
```cmake
add_custom_command(
    OUTPUT ${INTERFACE_FILES}
    COMMAND ${Python3_EXECUTABLE} generate_zephlet.py
        --proto ${PROTO_FILE}
        --output-dir ${OUTPUT_DIR}
        --skip-impl  # ← Never touch .c
    DEPENDS ${PROTO_FILE}
)
```

**Dependency tracking:**
- Proto file changes trigger interface regen
- Interface headers trigger recompile of dependent files
- Uses `add_custom_command` with `DEPENDS` for proper Make/Ninja integration

**Include path management** (`zephyr_zephlet_codegen.cmake:89`):
```cmake
target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR}/modules/<zephlet>_zephlet)
```
- Generated headers in `build/modules/<zephlet>_zephlet/`
- Global `CMAKE_BINARY_DIR` includes for all zephlets

**Proto processing:**
- Delegates to `nanopb_generate_cpp()` for .pb.h/.pb.c
- Configures nanopb options via `*.options` files
- Integrates nanopb runtime library

### Strengths ✓

1. **Idempotent builds** - Regen interfaces every build, never overwrites user code
2. **Proper dependency tracking** - Proto changes trigger minimal rebuild
3. **CMake integration** - Uses standard CMake patterns (add_custom_command)
4. **Two-stage strategy** - Bootstrap once, then iterative development
5. **nanopb integration** - Seamless protobuf → C code pipeline
6. **Out-of-tree builds** - Generated files in build/, source tree clean

### Weaknesses ✗

1. **Include path complexity**
   - Global `CMAKE_BINARY_DIR` includes enable unintended cross-zephlet dependencies
   - Every zephlet can accidentally include another's generated headers
   - Should use target-specific includes with `PRIVATE`

2. **No rebuild on template changes**
   - Changing Jinja2 templates doesn't trigger interface regen
   - Must `west build -t clean` manually
   - Could add templates to `DEPENDS`

3. **Adapter auto-update fragility** (`generate_adapter.py:106-150`)
   - Uses string pattern matching to update existing adapters:
     ```python
     if "/* ZBUS_CHAN_ADD_OBS */" in content:
         content = content.replace("/* ZBUS_CHAN_ADD_OBS */", new_obs)
     ```
   - Breaks if user reformats or removes comment markers
   - Should use AST-based code modification or avoid auto-update

4. **Error handling in CMake**
   - `execute_process` doesn't check Python script exit codes in some paths
   - Silent failures possible
   - Should use `RESULT_VARIABLE` and `message(FATAL_ERROR)`

---

## 4. West Extension Architecture

**Files:** `modules/lib/zephlet/west/zephlet_commands.py`

### Architecture

**Command structure:**
- `west zephlet new` - Create new zephlet (interactive or args)
- `west zephlet new-adapter` - Create adapter between zephlets
- `west zephlet gen` - Regenerate interfaces (requires `build/modules/<zephlet>_zephlet`)

**Path detection strategy** (`zephlet_commands.py:80-106`):
```python
def find_workspace_root():
    # 1. Check west.yml presence
    # 2. Walk up directory tree
    # 3. Look for ports_adapters_zbus/ or src/
```

**Dependency checking** (`zephlet_commands.py:298-312`):
- Validates `proto-schema-parser` installed
- Checks package import, not version
- Gives actionable error message if missing

**Interactive mode:**
- Prompts for name/description/author if not provided
- Shows defaults, validates input
- Creates directory structure + scaffolds files

**File scaffolding** (`zephlet_commands.py:200-250`):
- Creates `modules/zephlets/<name>_zephlet/`
- Generates minimal proto with RPC placeholders
- Creates CMakeLists.txt, Kconfig, module.yml
- Runs initial codegen for bootstrap

### Strengths ✓

1. **West integration** - Ecosystem-native, auto-discovered via module.yml
2. **Auto path detection** - Finds workspace root, no manual config
3. **Interactive UX** - Fallback prompts if args missing
4. **Dependency checking** - Clear error if proto-schema-parser missing
5. **Convention enforcement** - Generated files follow standard structure
6. **Idempotent** - Safe to re-run, checks existing files

### Weaknesses ✗

1. **Naming convention not enforced at creation time**
   - Allows non-standard names like `MyZephlet` instead of `my_zephlet`
   - Adapter files must follow exact pattern: `<Origin>+<Dest>_zlet_adapter.c`
   - Breaking convention breaks discovery/build

2. **Dependency checking basic**
   - Only checks `import proto_schema_parser` works
   - Doesn't verify version compatibility
   - Could add `pkg_resources.require('proto-schema-parser>=0.x')`

3. **Path detection fragile**
   - Assumes `ports_adapters_zbus/` or `src/` exists
   - Fails on non-standard workspace layouts
   - Should use west manifest APIs

4. **No dry-run mode**
   - Creates files immediately, no preview
   - Hard to test without side effects
   - Could add `--dry-run` flag

5. **Error messages**
   - Some errors just raise exceptions without context
   - Should catch common mistakes (name conflicts, invalid proto syntax)

---

## 5. Messaging Architecture (zbus Integration)

**Files:** `zephlet_interface.h.jinja`, `zephlet_interface.c.jinja`

### Architecture

**Two-channel pattern per zephlet:**

**Invoke channel** (`zephlet_interface.h.jinja:15-20`):
```c
ZBUS_CHAN_DEFINE(
    zlet_tick_invoke_chan,
    struct msg_zlet_tick_invoke,  /* Protobuf message union */
    NULL,
    NULL,
    ZBUS_OBSERVERS(zlet_tick_invoke_dispatcher_lis),
    ZBUS_MSG_INIT(.which_tick_invoke = 0)
);
```

**Report channel** (`zephlet_interface.h.jinja:22-27`):
```c
ZBUS_CHAN_DEFINE(
    zlet_tick_report_chan,
    struct msg_zlet_tick_report,  /* Protobuf message union */
    NULL,
    NULL,
    ZBUS_OBSERVERS_EMPTY,  /* Adapters subscribe at runtime */
    ZBUS_MSG_INIT(.which_tick_report = 0)
);
```

**Dispatcher listener** (`zephlet_interface.c.jinja:51-82`):
```c
static void zlet_tick_invoke_listener(const struct zbus_channel *chan) {
    zlet_tick_invoke_dispatcher(chan);
}

ZBUS_LISTENER_DEFINE(zlet_tick_invoke_dispatcher_lis, zlet_tick_invoke_listener);
```

**Inline helpers** (`zephlet.h.jinja:45-65`):
```c
static inline int zlet_tick_start(k_timeout_t timeout) {
    struct msg_zlet_tick_invoke msg = {
        .which_tick_invoke = msg_zlet_tick_invoke_start_tag
    };
    return zbus_chan_pub(&zlet_tick_invoke_chan, &msg, timeout);
}
```

**Thread safety model:**
- zbus handles message copying (listeners see immutable const pointers)
- Spinlocks required inside zephlet for shared data access
- Dispatcher runs in zbus thread context

### Strengths ✓

1. **Clean request-response semantics** - Two-channel pattern clear intent
2. **Type-safe messages** - Protobuf unions prevent invalid messages
3. **Loose coupling** - Adapters subscribe to reports, zephlets never know about adapters
4. **Non-blocking design** - Inline helpers take `k_timeout_t`, support async
5. **Auto-wiring** - Dispatcher automatically routes invoke messages to API calls
6. **Observer pattern** - Multiple adapters can listen to same report channel

### Weaknesses ✗

1. **No async response mechanism**
   - Fire-and-forget messaging
   - Query pattern (`get_status`) assumes someone listens to report channel
   - Can't detect if message was received/processed
   - No correlation IDs for request-response matching

2. **Fire-and-forget limits observability**
   - Dispatcher doesn't check API return values in templates
   - API functions return `zbus_chan_pub()` result, but ignored by dispatcher
   - Intentional design, but makes debugging failures hard

3. **No message versioning**
   - Proto changes break binary compatibility
   - No version field in message unions
   - Could add `uint32_t version` to each message

4. **Thread context assumptions**
   - Dispatcher runs in zbus thread, must be fast
   - Long-running API calls block zbus
   - Should document threading requirements

---

## 6. Protobuf Integration (nanopb)

**Files:** `zephyr_zephlet_codegen.cmake`, proto validation in `generate_zephlet.py`

### Architecture

**Proto processing pipeline:**
1. **Proto compilation** - `nanopb_generate_cpp()` generates .pb.h/.pb.c
2. **Options files** - `<zephlet>.options` configures nanopb codegen (max sizes, callbacks)
3. **Type mapping** - Python script maps proto types to C types
4. **Union generation** - Invoke/Report become nanopb unions (`oneof`)

**Type safety guarantees:**
- **Proto syntax** - protobuf3 catches schema errors
- **nanopb codegen** - Generates struct definitions, encode/decode
- **Python validation** (`generate_zephlet.py:198-283`) - RPC return type checking
- **C compiler** - Type checks function signatures vs proto structs

**RPC validation** (`generate_zephlet.py:198-283`):
```python
if return_type == "MsgZephletStatus":
    if "msg_zephlet_status" not in report_fields:
        raise ValueError(f"RPC returns MsgZephletStatus but Report has no msg_zephlet_status field")
```

**Custom type support:**
- Proto: `BatteryState state = 1;`
- Generated: `struct battery_state state;`
- Requires matching C struct definition

### Strengths ✓

1. **Type safety from proto** - Schema changes caught at compile time
2. **RPC validation** - Ensures API contracts match message definitions
3. **nanopb efficiency** - Minimal RAM/ROM footprint for embedded
4. **Custom type mapping** - Supports domain-specific types beyond primitives
5. **Schema evolution** - Optional fields enable backwards compatibility
6. **Single source of truth** - Proto defines API, messages, and validation

### Weaknesses ✗

1. **Proto convention enforcement gaps**
   - Reserved field numbers (1-6 Invoke, 1-3 Report) only in docs
   - Should validate at proto parse time or use proto options
   - Example: `option (zephlet_rpc) = true;` on RPC fields

2. **Custom type ambiguity** (`generate_zephlet.py:138-154`)
   - If proto has multiple custom types, validation guesses
   - No check if custom type defined elsewhere in proto
   - Should require explicit imports or annotations

3. **nanopb options fragmented**
   - Some in `*.options` files, some in proto annotations
   - Inconsistent between zephlets
   - Should standardize approach

4. **No runtime version checking**
   - Proto changes recompile everything, but no version field in messages
   - Can't detect if zephlet expects older/newer message format
   - Could add `uint32_t schema_version` to Invoke/Report

---

## Improvement Recommendations

### High Priority

1. **Type-safe zephlet struct** - Replace `void *api/data`
   ```c
   #define ZEPHLET_DEFINE_TYPED(_name, _api_type, _data_type, ...) \
       static _data_type _name##_data = __VA_ARGS__; \
       static const _api_type _name##_api = { /* ... */ }; \
       STRUCT_SECTION_ITERABLE(zephlet, zlet_##_name) = { \
           .name = #_name, \
           .api = &_name##_api, \
           .data = &_name##_data, \
       }
   ```
   Use `container_of()` or type tags for runtime safety.

2. **Add lifecycle cleanup** - Symmetric init/deinit
   ```c
   /* In zephlet.h */
   struct zephlet_api {
       int (*init)(const struct zephlet *);
       int (*deinit)(const struct zephlet *);
   };

   /* In zephlet.c */
   SYS_INIT(zephlet_deinit, APPLICATION, 1);  /* Late shutdown */
   ```

3. **Enforce proto conventions** - Build-time validation
   ```python
   # In generate_zephlet.py
   def validate_field_numbers(invoke_msg, report_msg):
       rpc_fields = [f for f in invoke_msg.fields if f.number <= 6]
       if not rpc_fields:
           raise ValueError("Invoke message must have RPC fields 1-6")
   ```

4. **Add default case to dispatcher template**
   ```jinja
   default:
       LOG_WRN("Unknown invoke tag: %d", ivk->which_{{ zephlet_name }}_invoke);
       break;
   ```

### Medium Priority

5. **Inter-zephlet dependency ordering**
   ```c
   struct zephlet {
       const char *name;
       void *api;
       void *data;
       const struct zephlet **depends_on;  /* NULL-terminated */
   };
   ```
   Topological sort in `zephlet_init()`.

6. **Async response mechanism** - Correlation IDs
   ```c
   struct msg_zlet_tick_invoke {
       uint32_t correlation_id;  /* Echo in Report */
       pb_size_t which_tick_invoke;
       /* ... */
   };
   ```

7. **Improve proto parser error messages**
   ```python
   try:
       schema = parse_schema(proto_file)
   except Exception as e:
       print(f"Proto parse failed in {proto_file}:")
       print(f"  {e}")
       print(f"  Check syntax near line {e.line}")
       sys.exit(1)
   ```

8. **Version-aware dependency checking**
   ```python
   pkg_resources.require('proto-schema-parser>=0.2.0')
   ```

### Low Priority

9. **Include path isolation** - Target-specific includes
   ```cmake
   target_include_directories(zephlet_${ZEPHLET_NAME}
       PRIVATE ${CMAKE_BINARY_DIR}/modules/${ZEPHLET_NAME}_zephlet)
   ```

10. **Robust Kconfig/CMake parsing** - AST-based updates
    Use `kconfiglib` or CMake parse APIs instead of regex.

11. **Naming convention enforcement**
    ```python
    if not re.match(r'^[a-z_][a-z0-9_]*$', zephlet_name):
        raise ValueError("Zephlet name must be lowercase snake_case")
    ```

12. **Generated code versioning**
    ```c
    /* Generated from zlet_tick.proto at 2026-02-06T10:30:00Z */
    /* Proto hash: abc123def456 */
    ```

---

## Quality Assessment

**Overall: 8.5/10** - Excellent architecture with minor safety/lifecycle gaps.

### Strengths Summary

**Architectural:**
- Protocol-driven design (proto as single source of truth)
- Clean Ports & Adapters pattern (loose coupling via zbus)
- Two-stage build (bootstrap once, iterative development)
- Non-destructive codegen (never overwrites user code)
- Convention over configuration (predictable patterns)

**Technical:**
- Type safety layers (proto + validation + compiler)
- Zero-config discovery (linker sections)
- West ecosystem integration (native CLI)
- Template consistency (Jinja2 uniformity)
- Proper dependency tracking (CMake integration)

**Developer Experience:**
- Interactive CLI (west zephlet commands)
- Clear error messages (mostly)
- Minimal boilerplate (codegen handles it)
- Standard project structure (modules/zephlets/)

### Weaknesses Summary

**Safety:**
- Type unsafety at runtime (void* in zephlet struct)
- Missing lifecycle cleanup (no deinit path)
- Fire-and-forget messaging (no error propagation)

**Validation:**
- Proto conventions not enforced (reserved fields only documented)
- Custom type validation gaps (ambiguous types)
- No runtime version checking (schema drift undetected)

**Build System:**
- Include path complexity (global binary dir)
- Adapter auto-update fragility (regex-based)
- No rebuild on template changes

**Tooling:**
- Naming convention not enforced (manual files can break)
- Dependency checking basic (no version validation)
- Path detection fragile (assumes standard layout)

### Production Readiness

**Verdict:** Strong foundation for production use with caveats.

**Ready for production:**
- Core messaging architecture solid
- Code generation mature and battle-tested
- Thread safety patterns correct (when used properly)
- Tooling integration complete

**Before production:**
1. Fix type unsafety in zephlet struct (high impact on maintenance)
2. Add lifecycle cleanup (prevents resource leaks)
3. Enforce proto conventions (catch errors earlier)
4. Add default case to dispatcher (debugging essential)

**Mature aspects:**
- West integration shows ecosystem understanding
- Two-stage build strategy well-designed
- Template system enables rapid iteration
- Recent standardization efforts (adapter includes, event refactor) show active maintenance

**Overall:** Framework demonstrates solid software engineering principles. Main concerns are safety boundaries (type safety, lifecycle) rather than architectural flaws. Excellent starting point that would benefit from hardening the validation and safety layers.
