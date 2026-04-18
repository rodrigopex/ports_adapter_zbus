# v0.3 Codegen Oracle

Phase 2 of the v0.3 refactor (see `docs/REFACTOR_V3_PLAN.md`) hand-writes
the files that Phase 3's codegen must reproduce. Those files live here —
**not** under `src/zephlets/<z>/` — because:

1. They are the *output* of a (future) generator, not user source.
2. Placing them in the source tree would invite hand-edits that later
   drift from the generator.

Each per-zephlet subdirectory (`zlet_tick/`, ...) contains the hand-written
`*_interface.h` and `*_interface.c`. The zephlet's `CMakeLists.txt` copies
them into `${CMAKE_BINARY_DIR}/modules/<zlet>/` at configure time and
compiles from there. Phase 3 replaces the copy step with real codegen
that emits to the same build-dir location; this directory then goes away.

**Do not edit these files if the symptom is "my generator is wrong."**
Fix the generator, regenerate, and diff against what lives here. If the
oracle itself needs to change, update the oracle first, confirm the build
still passes, then align the generator.
