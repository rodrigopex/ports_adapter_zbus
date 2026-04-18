# Zephlet v0.3 — Big-Bang Refactor Plan (v3)

**Status:** Ready to execute pending approval
**Supersedes:** internal v2 draft (the one with a `report` channel)
**Target branch:** `refactor/v0.3-envelope` (both repos)
**Executor:** Claude Code

---

## 0. Purpose

Replace the current per-zephlet `Invoke`/`Report` oneof model with a small,
uniform envelope that carries pointers to per-method request/response protobuf
messages. Make zephlets **non-singleton** (multiple instances per type),
following the Zephyr `struct device` pattern. Drop correlation IDs, semaphores,
sync reply listeners, and `has_result` accounting — zbus sync listeners +
pointer-in-channel give us identity and in-place mutation for free.

**Every zephlet instance owns exactly two channels:**

| Channel | Payload | Observers | Purpose |
|---|---|---|---|
| `rpc` | `struct zephlet_call *` (pointer, caller-stack) | exactly one `lis_<type>` listener | Synchronous RPC via zbus sync-listener semantics |
| `events` | `struct <type>_events` (zbus value-type) | any mix (subscribers, adapters) | Asynchronous fan-out; zbus copies per consumer |

The two channels have different *kinds*, not just different *names*. Pointer-in-channel is safe only for sync listeners; value-in-channel is how zbus natively fans out async.

This is a **big-bang** refactor. Do not support both shapes simultaneously.

---

## 1. Architectural Decisions (locked — do not re-litigate)

1. **Envelope.** `struct zephlet_call` carried on the `rpc` channel as
   `struct zephlet_call *`. Zbus sync listeners let the dispatcher mutate the
   envelope in place, and pointer equality is identity.
2. **Per-method types.** Each RPC has its own request + response protobuf
   types (what the `.proto` service block already expresses). No more
   `Invoke`/`Report` oneofs.
3. **Sync RPC via zbus sync listener only.** No semaphores, no correlation_id,
   no sync reply listener. `zbus_chan_pub()` on the `rpc` channel runs the
   dispatcher synchronously in the caller's thread; by the time `pub()` returns,
   `call->return_code` and `*call->resp` are populated. The wrapper returns
   `call.return_code` directly. **No republish of the reply on any channel.**
4. **Non-singleton zephlets.** Multiple instances of the same type coexist.
   Each instance owns its own `rpc` + `events` channel pair and its own
   `data`/`config`. Pattern mirrors `struct device`.
5. **Single listener per type, not per instance.** Listener uses
   `zbus_chan_user_data(chan)` to recover `const struct zephlet *`.
6. **`resp == NULL` is the discard contract.** Trailing `resp` parameter on
   wrappers; caller passes NULL to signal "don't care." Handlers guard writes
   with `if (resp)`. `req` is never nullable.
7. **Four wrapper shapes** driven by (`req_is_empty`, `resp_is_empty`):
   - `(Empty, Empty)` → `int t_foo(const struct zephlet *z, k_timeout_t timeout)`
   - `(Empty, Resp)`  → `int t_foo(const struct zephlet *z, struct resp *r, k_timeout_t timeout)`
   - `(Req, Empty)`   → `int t_foo(const struct zephlet *z, const struct req *q, k_timeout_t timeout)`
   - `(Req, Resp)`    → `int t_foo(const struct zephlet *z, const struct req *q, struct resp *r, k_timeout_t timeout)`
8. **Handlers via weak symbols.** Generator emits `__weak` defaults returning
   `-ENOSYS`; developer overrides in `.c`. No runtime `set_implementation`.
9. **Events use a zbus value-type channel.** Per-zephlet event type
   `struct <type>_events` published directly via
   `zbus_chan_pub(z->channel.events, &events, timeout)`. Zbus copies into each
   consumer's queue. **No envelope. No pool. No refcount.**
10. **`rpc` channel is listener-only.** Exactly one listener (`lis_<type>`).
    Pointer-in-channel is unsafe otherwise. Enforced by `ZEPHLET_DEFINE` macro
    (only creator) and a runtime assertion in the SYS_INIT walker under
    `CONFIG_ASSERT`.
11. **`ZEPHLET_DEFINE(type, name, cfg, data, init)`** is the user-facing macro.
    No separate `_INST_` variant. Do not introduce devicetree bindings.
12. **`ZEPHLET_CALL_OK` is removed.** Wrappers return `int` directly
    (0 = success, negative errno = failure). `has_result` disappears from the
    envelope; `return_code` is always valid.
13. **`struct zephlet` gains `int init_priority`.** SYS_INIT walker sorts
    ascending before invoking `init_fn`. Default 0. Enables tick→ui init order
    without devicetree.
14. **`<z>_is_ready(z)` becomes sugar over `get_status`.** No separate
    readiness flag in the envelope or in the framework. The zero-arg
    `<z>_is_ready()` helper of the current tree is replaced by
    `<type>_is_ready(const struct zephlet *z)` that invokes `get_status` and
    checks `status.is_ready`.
15. **Events helper.** Generator emits
    `int <type>_emit(const struct zephlet *z, const struct <type>_events *ev, k_timeout_t timeout)`
    which is a thin wrapper over `zbus_chan_pub(z->channel.events, ev, timeout)`.
    Replaces the current `<z>_events_update(ctx, delta)` idiom.

### Rejected alternatives (do not revive)

- Envelope by value on the `rpc` channel. Blocked by zbus `const void *` semantics.
- Correlation-id matching for sync calls. Unnecessary with sync listener.
- Runtime implementation swap. No real use case; weak symbols suffice.
- System-wide single `rpc` channel. Breaks observability, forces every
  zephlet to wake on every call.
- Devicetree-driven zephlet definitions. Premature; no hardware binding.
- A shared `report` channel for both sync replies and async events. UAF:
  pointer-to-caller-stack republish dangles for async observers. **Removed.**
- Pool-backed event envelopes with refcounting. Zbus has no completion signal;
  complexity without payoff once events use value-copy channels.
- Reply republish on any channel. Caller has `call.return_code`; any tracer
  uses log/trace subsystem, not zbus.

---

## 2. The New Shared Contract

These definitions are authoritative. Start Phase 1 by transcribing them.

### `zephlet.h`

```c
#ifndef MODULES_ZEPHLETS_SHARED_ZEPHLET_H
#define MODULES_ZEPHLETS_SHARED_ZEPHLET_H

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <pb.h>

/* Forward decl so the handler typedef can reference struct zephlet. */
struct zephlet;

/* ----- Envelope ---------------------------------------------------------- */

struct zephlet_call {
    uint16_t method_id;             /* RPC method id; 0 reserved */
    int32_t  return_code;           /* callee writes; POSIX errno */
    const pb_msgdesc_t *req_desc;   /* NULL if RPC takes Empty */
    const void         *req;
    const pb_msgdesc_t *resp_desc;  /* NULL if RPC returns Empty */
    void               *resp;       /* may be NULL = caller discards */
};

/* ----- Type-level: API + method table ------------------------------------ */

typedef int (*zephlet_handler_fn)(const struct zephlet *z,
                                   struct zephlet_call *call);

struct zephlet_method {
    const pb_msgdesc_t *req_desc;
    const pb_msgdesc_t *resp_desc;
    zephlet_handler_fn  handler;
};

struct zephlet_api {
    const struct zephlet_method *methods;
    size_t num_methods;
};

/* ----- Instance ---------------------------------------------------------- */

struct zephlet {
    const char *name;
    const struct zephlet_api *api;
    struct {
        const struct zbus_channel *rpc;     /* pointer channel, listener-only */
        const struct zbus_channel *events;  /* value-type, async fan-out */
    } channel;
    const void *config;
    void       *data;
    int (*init_fn)(const struct zephlet *self);
    int init_priority;
};

/* ----- Definition macro --------------------------------------------------
 *
 * Emits:
 *   chan_<name>_rpc    — zbus channel of `struct zephlet_call *`,
 *                        observer list contains exactly lis_<type>.
 *   chan_<name>_events — zbus value-type channel of `struct <type>_events`,
 *                        observers added by users via ZBUS_CHAN_ADD_OBS.
 *   STRUCT_SECTION_ITERABLE(zephlet, <name>) — registered in the linker
 *                        section defined in zephlet_iterables.ld.
 */
#define ZEPHLET_DEFINE(_type, _name, _cfg, _data, _init)                       \
    ZBUS_CHAN_DEFINE(chan_##_name##_rpc,                                       \
                     struct zephlet_call *, NULL,                              \
                     (void *)&_name,                                           \
                     ZBUS_OBSERVERS(lis_##_type),                              \
                     NULL);                                                    \
    ZBUS_CHAN_DEFINE(chan_##_name##_events,                                    \
                     struct _type##_events, NULL,                              \
                     (void *)&_name,                                           \
                     ZBUS_OBSERVERS_EMPTY,                                     \
                     ({0}));                                                   \
    const STRUCT_SECTION_ITERABLE(zephlet, _name) = {                          \
        .name    = #_name,                                                     \
        .api     = &_type##_api,                                               \
        .channel = {                                                           \
            .rpc    = &chan_##_name##_rpc,                                     \
            .events = &chan_##_name##_events,                                  \
        },                                                                     \
        .config         = (_cfg),                                              \
        .data           = (_data),                                             \
        .init_fn        = (_init),                                             \
        .init_priority  = 0,                                                   \
    }

/* Variant that sets init_priority explicitly. */
#define ZEPHLET_DEFINE_PRIO(_type, _name, _cfg, _data, _init, _prio)           \
    /* ...same as ZEPHLET_DEFINE but .init_priority = (_prio) */

/* ----- Core runtime ------------------------------------------------------ */

int zephlet_dispatch(const struct zephlet *z, struct zephlet_call *call);
const struct zephlet *zephlet_get_by_name(const char *name);

#endif /* MODULES_ZEPHLETS_SHARED_ZEPHLET_H */
```

### `zephlet.c` — expected behaviour

- `zephlet_dispatch`: bounds-check `call->method_id` against
  `z->api->num_methods`; if OOB or handler NULL, set
  `call->return_code = -ENOSYS` and return. Otherwise call the handler and
  store its return value in `call->return_code`.
- `zephlet_get_by_name`: linear scan via `STRUCT_SECTION_FOREACH(zephlet, z)`,
  `strcmp` against `z->name`.
- SYS_INIT walker (`APPLICATION`/priority `0`):
  1. Collect pointers from the section.
  2. Sort ascending by `init_priority`.
  3. For each: under `CONFIG_ASSERT`, assert that the `rpc` channel's
     observer list contains exactly one observer (a listener). Then call
     `init_fn` if non-NULL. Log and continue on errors; do not abort boot.

### Generated per-type listener (reference)

```c
static void lis_tick_fn(const struct zbus_channel *chan)
{
    const struct zephlet *z = zbus_chan_user_data(chan);
    struct zephlet_call *call =
        *(struct zephlet_call *const *)zbus_chan_const_msg(chan);

    zephlet_dispatch(z, call);
}
ZBUS_LISTENER_DEFINE(lis_tick, lis_tick_fn);
```

### Generated wrapper (reference — `(Req, Resp)` shape)

```c
int tick_config(const struct zephlet *z,
                const struct tick_config *req,
                struct tick_config *resp,
                k_timeout_t timeout)
{
    struct zephlet_call call = {
        .method_id = TICK_M_CONFIG,
        .req_desc  = &tick_config_msg, .req  = req,
        .resp_desc = &tick_config_msg, .resp = resp,
    };
    struct zephlet_call *p = &call;
    int err = zbus_chan_pub(z->channel.rpc, &p, timeout);
    if (err) { return err; }
    return call.return_code;
}
```

The three other shapes omit `req`, `resp`, or both parameters and set the
corresponding envelope fields to NULL. **No republish on any other channel.**

### Generated events helper (reference)

```c
int tick_emit(const struct zephlet *z,
              const struct tick_events *ev,
              k_timeout_t timeout)
{
    return zbus_chan_pub(z->channel.events, ev, timeout);
}
```

Callers (including timer ISRs) pass `K_NO_WAIT`. Consumers observe
`chan_<instance>_events` directly and receive `struct <type>_events` by value —
no envelope, no demux, no tag filtering.

### Generated trampoline + weak stub (reference)

```c
/* trampoline — <type>_interface.c */
static int tick_trampoline_config(const struct zephlet *z, struct zephlet_call *c)
{
    return tick_on_config(z,
                          (const struct tick_config *)c->req,
                          (struct tick_config *)c->resp);
}

/* weak stub — <type>_interface.c */
__weak int tick_on_config(const struct zephlet *z,
                          const struct tick_config *req,
                          struct tick_config *resp)
{
    ARG_UNUSED(z); ARG_UNUSED(req); ARG_UNUSED(resp);
    return -ENOSYS;
}
```

---

## 3. Execution Phases

Work phase by phase. Do not start Phase N+1 until Phase N's checkpoint passes.
Commit at each checkpoint with message `refactor(v0.3): phase N — <summary>`.

### Phase 0 — Baseline

- Check out `main`, run `just c b r` + `just test` in the app repo; `west build`
  any sample in infra. Capture passing state as the reference. If baseline is
  broken, **stop and report** — do not begin the refactor on a broken tree.
- Create branch `refactor/v0.3-envelope` in both repos.
- Commit this plan (`docs/REFACTOR_V3_PLAN.md`) as the Phase 0 marker.

**Checkpoint:** branches exist, baseline builds in both repos.

### Phase 1 — Shared Contract

- Rewrite `zephlet.h` per §2 in the infra repo. Remove `ZEPHLET_CALL_OK`,
  old `struct zephlet_data` / `struct zephlet_config` shapes, and the old
  `ZEPHLET_IMPL_REGISTER` macro.
- Write new `zephlet.c` with `zephlet_dispatch`, `zephlet_get_by_name`, and
  the SYS_INIT walker (priority sort + rpc-channel observer-count assert +
  `init_fn` dispatch).
- Keep `zephlet_iterables.ld` as-is (linker section already declared).
- Unit tests for `zephlet_dispatch` under `tests/` (or the existing shared
  unit test suite). **Required coverage:**
  1. Successful dispatch → `return_code` set to handler's return value.
  2. Method id out of range → `return_code == -ENOSYS`.
  3. Handler slot NULL → `return_code == -ENOSYS`.
  4. Handler returns non-zero error → envelope reflects it.

**Checkpoint:** infra builds standalone; 4/4 dispatch tests pass.

### Phase 2 — Reference migration: `tick` by hand

Prove the generated shape by writing it manually so Phase 3's codegen has a
concrete target.

- Pick `tick` as the reference zephlet.
- Hand-write, **in-tree, not in the build dir**:
  - `src/zephlets/tick/zlet_tick.proto` — revised to the new schema (see §4).
  - `src/zephlets/tick/zlet_tick_interface.h` — method-id enum, api extern,
    wrapper declarations, weak handler declarations, `tick_emit` declaration,
    `tick_is_ready` declaration.
  - `src/zephlets/tick/zlet_tick_interface.c` — method table, `tick_api`
    instance, `lis_tick` listener, trampolines, weak handler stubs, wrappers,
    `tick_emit`, `tick_is_ready`.
  - `src/zephlets/tick/zlet_tick.c` — strong overrides for the weak handlers.
    State in `struct tick_data` referenced via `z->data`. Timer ISR calls
    `tick_emit(z, &ev, K_NO_WAIT)`.
- Extend the integration test to use
  `ZEPHLET_DEFINE(tick, tick_fast, ...)` and
  `ZEPHLET_DEFINE(tick, tick_slow, ...)` with different configs. Assert
  independent state and independent event streams.
- Disable codegen for tick during this phase via a real CMake flag
  (`ZEPHLET_CODEGEN_SKIP_TICK=ON`) — not hand-edit — to avoid accidental
  regeneration clobber.

**Checkpoint:** two instances of `tick` run concurrently with distinct state
and distinct event streams; integration test passes.

### Phase 3 — Codegen rewrite

- Update infra `zephlet.proto`: keep `Empty`, `ZephletStatus`. Remove
  `ZephletResult`, `Invoke`, `Report`, `has_result`, reserved field numbers
  for the dead oneof slots, `extends_base`.
- Rewrite `codegen/generate_proto.py` as a pass-through of the source `.proto`
  plus standard lifecycle RPCs. No Invoke/Report synthesis.
- Rewrite `codegen/generate_zephlet.py`:
  - Parse the proto's service block; each RPC yields a method entry.
  - Classify each RPC as one of the four shapes by
    (`input == Empty`, `output == Empty`).
  - Drop all code paths for: oneof tags, `has_result`, correlation counter,
    per-zephlet `ctx` struct, `zephlet_data`/`zephlet_context` glue, sync
    listener, blocking helpers, `set_implementation`, events_update merge
    machinery.
  - Emit: method-id enum, `<type>_api`, `lis_<type>`, trampolines, weak stubs,
    four-shape wrappers, `<type>_emit`, `<type>_is_ready`.
- Rewrite templates:
  - `templates/zephlet.h.jinja` — minimal interface header.
  - `templates/zephlet.c.jinja` — method table, trampolines, weak stubs,
    listener, wrappers, `<type>_emit`, `<type>_is_ready`.
  - Delete `templates/zephlet_proto.proto.jinja` (merged into source).
- Regenerate `tick` from its proto. Diff against the Phase 2 hand-written
  files. Target: **semantically equivalent** (not byte-equivalent). Formatting
  / comment-order diffs are not blockers. Functional diffs are — fix the
  generator.

**Checkpoint:** regenerated tick files produce a passing integration test
identical in behaviour to Phase 2.

### Phase 4 — Copier template

Update `codegen/zephyr_zephlet_template/` so `west zephlet new` produces the
new shape:

- `.proto` template: per-method req/resp types; service with standard RPCs;
  no `extends_base`.
- `.c` template: handler stubs that override weak symbols.
- Integration test template: uses `ZEPHLET_DEFINE(<type>, <type>_test, ...)`
  inside the test so the test owns the instance. Uses new wrappers + `_emit`
  directly.
- CMakeLists, Kconfig, module.yml: minor edits as needed.
- **`ZEPHLET_DEFINE` does not live inside the zephlet's `src/zephlets/<z>/`
  folder.** The type is not an instance. The Copier template leaves a
  commented example in the zephlet's README or a `samples/` stub; the
  integration test is the only place the template itself calls
  `ZEPHLET_DEFINE`.

**Checkpoint:** `west zephlet new foo` from a clean sandbox produces a
directory that builds (type only; no instance until the app opts in).

### Phase 5 — Adapter system

Adapters now wire **instances**, not types, and observe value-typed event
channels directly.

- Update `west zephlet new-adapter` to prompt for origin instance and dest
  instance by name. Free-form string input is fine for v0.3.
- Update `codegen/generate_adapter.py` and `templates/adapter.c.jinja`:
  - Listener observes `chan_<origin_instance>_events` (value channel of
    `struct <origin_type>_events`).
  - **No `method_id` guard, no demux, no `which_*_tag` switch.** The listener
    receives the event message directly.
  - Weak callback signature:
    `void <origin>_to_<dest>_on_event(const struct zephlet *dest, const struct <origin_type>_events *ev)`.
- Add or update an adapter test that wires `tick_fast` events to a second
  zephlet's `rpc` channel and verifies event propagation.

**Checkpoint:** adapter connects `tick_fast` events to another zephlet's RPC
surface; events propagate.

### Phase 6 — Schema and symbol cleanup

Repo-wide grep and scrub. Remove:

- `ZephletResult`, `Invoke`, `Report` (as oneof types), `has_result`,
  `correlation_id`, `invoke_tag`, `ZEPHLET_OBSERVE_REPORT`, `wait_report`,
  `ZEPHLET_CALL_OK`, `zephlet_context`, `zephlet_result`.
- Every occurrence of the word `report` that refers to the old channel —
  comments, variable names, doc prose. Keep only intentional "removed in v0.3"
  historical mentions.
- `_set_implementation` symbols and their callers.
- Old blocking-helper templates.
- `<z>_events_update(ctx, delta)` — replaced by `<type>_emit(z, ev, timeout)`.
- Per-zephlet `struct zephlet_data` embedding; data is now opaque to the
  framework.

**Checkpoint:** all greps return only documented historical mentions. Repo
builds; tests pass.

### Phase 7 — Documentation

- Rewrite `CLAUDE.md` (both repos) architecture section around the two-channel
  (rpc + events) model. Remove every reference to `Invoke`/`Report` oneofs,
  `ZephletResult`, correlation IDs, `ZEPHLET_CALL_OK`, spinlock-in-base-data.
- Rewrite `README.md` Architecture section similarly.
- Update `docs/issues/`:
  - `001-explicit-invoke-report-proto.md` → **Superseded by v0.3**.
  - `002-synchronous-call-helpers.md` → **Resolved**.
  - `004-optional-report-channel.md` → **Resolved** (report channel removed
    entirely; events on a value-type channel, RPC on a pointer channel).
- Update `presentation/main.typ` — add a "v0.3 changes" slide at minimum;
  full rewrite deferrable.

**Checkpoint:** `grep -r "Invoke"` and `grep -r "ZEPHLET_CALL_OK"` in docs
return nothing except intentional "removed in v0.3" mentions.

### Phase 8 — Validation

- Run all integration tests in the app repo (`just test`).
- Run Twister across any sample configs.
- Execute the **definition-of-done script**: from a clean sandbox,
  `west zephlet new demo`; in an app, instantiate `demo` twice with distinct
  configs; exercise every standard lifecycle RPC (`start`, `stop`, `get_status`,
  `config`, `get_config`); assert distinct state and distinct event streams.
  Keep the script under `scripts/` or `tests/` as a regression gate.
- If the `ports_adapters_zbus` example app migration is out of scope for this
  PR, note it explicitly — app is expected to be broken against
  `refactor/v0.3-envelope` until its own migration lands.

**Checkpoint:** all tests green; DoD script green; out-of-scope breakage
explicitly flagged.

---

## 4. Proto Schema Changes

Source `.proto` for a zephlet:

```protobuf
syntax = "proto3";

import "nanopb.proto";
import "zephlet.proto";

option (nanopb_fileopt).anonymous_oneof = true;
option (nanopb_fileopt).long_names = false;

message Tick {
  message Config { uint32 period_ms = 1; }
  message Events { int32 timestamp = 1; }
}

service Tick {
  /* Standard lifecycle — always present. */
  rpc start      (Empty)        returns (ZephletStatus);
  rpc stop       (Empty)        returns (ZephletStatus);
  rpc get_status (Empty)        returns (ZephletStatus);
  rpc config     (Tick.Config)  returns (Tick.Config);
  rpc get_config (Empty)        returns (Tick.Config);

  /* Custom RPCs below */
}
```

Key changes from v0.2:

- No `Invoke` or `Report` message generated.
- No `ZephletResult`, no `has_result`, no reserved field numbers.
- The `service` block in the source is authoritative and compiled as-is.
- `method_id`s are assigned in declaration order starting at 1.
- `method_id == 0` is **reserved** (no current use; kept for future in-band
  control signalling).
- `Events` is an ordinary nested message. Instances publish it on the value
  channel via `<type>_emit(z, &ev, timeout)`.
- Streaming RPCs (`stream` keyword): generator emits a clear error. Out of
  scope for v0.3.

---

## 5. File Inventory

### Added

- `shared/zephlet.c` (new logic; replaces existing file entirely)
- `docs/REFACTOR_V3_PLAN.md` (this file)
- `scripts/dod_v03.sh` (Phase 8 definition-of-done script)

### Rewritten

- `shared/zephlet.h`
- `codegen/generate_zephlet.py`
- `codegen/generate_proto.py`
- `codegen/generate_adapter.py`
- `codegen/zephlet.proto`
- `codegen/templates/zephlet.h.jinja`
- `codegen/templates/zephlet.c.jinja`
- `codegen/templates/adapter.c.jinja`
- `codegen/zephyr_zephlet_template/template/**` (Copier)
- `CLAUDE.md` (both repos)
- `README.md` (both repos)

### Deleted

- `codegen/templates/zephlet_proto.proto.jinja`
- Any `*_impl.c` convention files
- Per-zephlet blocking-helper, spinlock-in-base-data, and events-update merge
  helpers

### Untouched (intentional)

- `west/zephlet_commands.py` — command surface stays the same
  (`new`, `new-adapter`, `gen`); internals change but UX does not.
- `zephlet_iterables.ld` — linker section already declared.
- `LICENSE`, `.gitignore`, `west.yml`.

---

## 6. Out of Scope (explicit deferrals)

- Devicetree bindings for zephlet instances.
- Remote/cross-MCU RPC (actual gRPC bridge). The envelope keeps
  `pb_msgdesc_t *` so a wire codec can be added later.
- Streaming RPCs.
- Migration of the `ports_adapters_zbus` example app (may be bundled if
  cheap; otherwise follow-up PR).
- Performance benchmarking.

---

## 7. Working Principles for the Executor

- **Commit at every phase checkpoint.** Small, reviewable commits > one giant
  blob. Sub-commits within a phase are fine.
- **When stuck, stop and summarize.** Do not invent new architectural
  decisions. Surface the blocker and the options.
- **Grep before deleting.** Old symbols (`has_result`, `ZephletResult`,
  `correlation_id`, `invoke_tag`, `report`) must be scrubbed repo-wide, not
  just where obvious.
- **Hand-written Phase 2 is the oracle for Phase 3.** If generator output
  diverges semantically from the hand-written reference, the generator is
  wrong.
- **Tests are the safety net.** Run them at every checkpoint. A silent
  test-suite regression means a phase is not complete.
- **Do not preserve backward compatibility.** Big-bang.

---

## 8. Definition of Done

- `refactor/v0.3-envelope` branches merged to `main` in both repos.
- All integration tests pass.
- `CLAUDE.md` and `README.md` describe the envelope + rpc/events model only.
- `scripts/dod_v03.sh` passes: fresh `west zephlet new demo` scaffold builds,
  is instantiated twice with distinct configs, responds correctly to all
  standard lifecycle RPCs, and produces distinct event streams.
- No references to `Invoke`, `Report` (as oneof), `ZephletResult`,
  `has_result`, `correlation_id`, `ZEPHLET_CALL_OK`,
  `set_implementation`, or a `report` channel remain outside of superseded
  `docs/issues/` files.
