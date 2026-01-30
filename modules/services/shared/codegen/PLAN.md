# Codegen Plan

## Goal
Proto → .h/.c/private/*_priv.h/_impl.c via proto-schema-parser + Jinja2

## Location
`modules/services/shared/codegen/`: generate_service.py, templates/*.jinja

## Usage
```bash
just gen_service_files <service>  # or manual with --generate-impl flag
pip install proto-schema-parser jinja2
```

## Flow
Parse proto → extract (service/invoke/report/config/RPC) → context dict → render templates → write (skip existing _impl.c)

Parser: `Service`/`Method` classes, `MessageType.type/.stream`, MsgTickService→tick_service

## Type Mapping
uint32→uint32_t, Empty→empty, MsgServiceStatus→msg_service_status, Config→msg_<s>_config, CamelCase→snake_case

## Generated Files (DO NOT EDIT except _impl.c)
- .h: data/API structs, inline `<s>_<cmd>(timeout)`→pub invoke chan, registration
- .c: channels, dispatcher (switch/case oneof tags), listener
- private/*_priv.h: `<s>_report_<field>(args,timeout)` helpers wrapping zbus_chan_pub
- _impl.c: template (NEVER OVERWRITES), TODOs, K_SPINLOCK patterns, uses priv.h helpers

## RPC→Report Mapping
Returns: MsgServiceStatus→report_status(), Config→report_config(), (stream)Events→report_events(), Empty→none (exception)
Validates RPC vs Invoke/Report fields (Empty returns skip Report check)

## Stream Semantics
Output streaming: no Invoke field, pub from timer/IRQ (K_NO_WAIT), skips validation
Input streaming: placeholder

## Validation
RPC methods: Invoke fields exist (except output-stream), return types have Report fields (except Empty), type matching

## Workflow
**New:** Write .proto→gen→complete _impl.c (K_SPINLOCK, report helpers)→add CMake/Kconfig/module.yml→enable prj.conf→build
**Modify:** Edit .proto→regen (no --generate-impl)→update _impl.c→build

## Principles
Proto=truth, never edit generated, idempotent regen, _impl.c protected, single-return non-void, nanopb types typedef'd

## Enhancements
✅ _impl.c template, priv.h gen, RPC mapping, CamelCase fix, Empty exception (2026-01-29)

## Future
Auto CMake, gen CMakeLists/Kconfig, test stubs, --watch

## Unresolved
None
