# Zephlet Infrastructure Review

Based on analysis of tick zephlet and shared infrastructure.

## **Strengths** ✓

1. **Clean architecture**: Ports & Adapters via zbus provides excellent decoupling
2. **Code generation maturity**:
   - RPC method validation against Invoke/Report fields
   - Bootstrap mode (`--impl-only`) generates .c once, never overwrites
   - Template-based consistency across all zephlets
3. **Type safety**: Protobuf messages + compile-time validation
4. **Thread safety**: Correct spinlock usage - held only for state access, blocking calls outside lock
5. **Discovery**: `STRUCT_SECTION_ITERABLE` enables runtime enumeration
6. **Separation**: Generated vs hand-written files cleanly separated
7. **Recent standardization**:
   - Adapter include pattern (b26f246): interface headers only, no .pb.h
   - Tick events refactor (861fa55): timestamp separated from tick flag

## **Drawbacks** ✗

### **Design Issues:**

1. **Type unsafety** (zephlet.h:13-14)
   - `void *api` and `void *data` require manual casting
   - No compile-time type checking or RTTI/type tags
   - Runtime errors possible from incorrect casts

2. **Config apply broken** (zlet_tick.c:68-78)
   - `config()` updates data but doesn't restart timer with new delay
   - Manual stop/config/start sequence required
   - Running timer continues with old delay until restart

3. **Hardcoded timeouts** (zlet_tick.c:36, 53, 65, 77, 89)
   - `K_MSEC(250)` repeated in all report publishing calls
   - Magic number, not configurable
   - Should use Kconfig or constant

4. **No lifecycle cleanup**
   - No deinit path for zephlets
   - Timer lives forever once created (zlet_tick.c:19)
   - Resource leaks possible

### **Code Quality:**

5. **Dispatcher missing default case** (zlet_tick_interface.c:51-82)
   - Switch statement has no `default` case for invalid tags
   - Silently ignores unknown messages
   - Should log warning for unknown invoke tags

6. **Error handling is fire-and-forget** (zlet_tick_interface.c:52-81)
   - Dispatcher doesn't check API function return values
   - API functions return values from `zbus_chan_pub()` but ignored
   - Intentional async messaging design, not a bug, but limits observability

7. **Initialization concerns** (zephlet.c:19)
   - `SYS_INIT` priority 99 hardcoded
   - No inter-zephlet dependency ordering
   - Uses `printk` instead of LOG_* (zephlet.c:7-14, zlet_tick.c:122)

8. **No async response mechanism**
   - All inline functions take `k_timeout_t` but no way to receive responses
   - Query pattern (get_status/get_config) assumes someone listens to report channel
   - Fire-and-forget can't detect failures

## **Recent Architecture Evolution**

### **Tick Events Refactor (861fa55)**

Proto changes in zlet_tick.proto:13-15:
```protobuf
message Events {
  int32 timestamp = 1;        // Dedicated timestamp field
  optional Empty tick = 2;    // Boolean flag (was optional int64)
}
```

Impact:
- Events.tick now acts as pure boolean flag (presence = true)
- Timestamp always present, separate from tick occurrence
- Affects adapter patterns - check `has_tick` instead of tick value

### **Adapter Include Standardization (b26f246)**

Pattern in all adapters:
```c
#include "zlet_<origin>_interface.h"
#include "zlet_<dest>_interface.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
```

- Interface headers only, no .pb.h includes
- Blank line separation between interface/zephyr includes
- Consistent across all generated adapters

### **Code Generation Patterns**

**File Lifecycle:**
- VCS: .proto, .c (after bootstrap), CMakeLists.txt, Kconfig, module.yml
- Generated (build): _interface.h, _interface.c, <zephlet>.h, .pb.h, .pb.c

**Bootstrap Behavior:**
- First build: `--impl-only` generates .c template if missing
- Subsequent builds: regenerate interfaces, never touch .c
- Manual workflow: edit .proto → auto-regen interfaces → update .c → rebuild

**Validation:**
- RPC return types checked against Report fields
- `returns MsgZephletStatus` → `report_status()` helper
- `returns Config` → `report_config()` helper
- `returns stream Events` → `report_events()` helper

## **Thread Safety Analysis**

**CORRECT Implementation** (not broken):

Timer handler (zlet_tick.c:10-17):
- Publishes with `K_NO_WAIT` (non-blocking)
- No spinlock needed (only reads uptime, sets local struct)

API functions (zlet_tick.c:21-90):
- Spinlock held only for data access (K_SPINLOCK macro)
- Data copied to stack before lock release
- Blocking calls (`k_timer_start`, `zbus_chan_pub`) outside spinlock
- Example pattern (start function):
  ```c
  K_SPINLOCK(&data->lock) {
      delay = data->config.delay_ms;    // Copy to stack
      data->status.is_running = true;    // Update state
      status = data->status;             // Copy to stack
  }  // Lock released here

  k_timer_start(&timer_zlet_tick, ...);  // Blocking call outside lock
  return zlet_tick_report_status(&status, K_MSEC(250));
  ```

This is the correct pattern for spinlock usage in Zephyr.

## **Improvement Recommendations**

### **High Priority:**

1. **Fix config apply** (zlet_tick.c:68-78)
```c
static int config(const struct zephlet *zephlet, const struct msg_zlet_tick_config *new_config)
{
    struct zlet_tick_data *data = zephlet->data;
    bool is_running;

    K_SPINLOCK(&data->lock) {
        data->config = *new_config;
        is_running = data->status.is_running;
    }

    // Restart timer with new delay if running
    if (is_running) {
        k_timer_start(&timer_zlet_tick, K_MSEC(new_config->delay_ms),
                      K_MSEC(new_config->delay_ms));
    }

    return zlet_tick_report_config(new_config, K_MSEC(250));
}
```

2. **Add default case in dispatcher template** (zlet_tick_interface.c:51-82)
```c
default:
    LOG_WRN("Unknown invoke tag: %d", ivk->which_tick_invoke);
    break;
```

3. **Type-safe zephlet struct** (avoid void*)
```c
#define ZEPHLET_DEFINE_TYPED(_name, _api_type, _data_type, ...) \
    /* Use container_of or tagged union */
```

### **Medium Priority:**

4. **Make timeouts configurable**
```c
#define ZLET_TICK_REPORT_TIMEOUT K_MSEC(CONFIG_ZLET_TICK_REPORT_TIMEOUT_MS)
```

5. **Add config validation**
```c
if (new_config->delay_ms < 10 || new_config->delay_ms > 60000) {
    return -EINVAL;
}
```

6. **Add lifecycle cleanup**
```c
// In API:
int (*deinit)(const struct zephlet *zephlet);

// In impl:
static int deinit(const struct zephlet *zephlet) {
    k_timer_stop(&timer_zlet_tick);
    return 0;
}
```

### **Low Priority:**

7. **Replace printk with LOG_INF** (zephlet.c:7-14, zlet_tick.c:122)

8. **Consider error propagation in dispatcher**
```c
int ret = api->start(impl);
if (ret < 0) {
    LOG_ERR("start failed: %d", ret);
    // Optionally publish error report
}
```

9. **Add zephlet dependency ordering**
```c
struct zephlet {
    // ...
    const struct zephlet **depends_on;
    size_t depends_count;
};
```

## **Quality Assessment**

**Overall: 7.5/10** - Solid architecture, good production readiness

**Positives:**
- Architecture is sound and extensible
- Code generation dramatically reduces errors and enforces consistency
- Thread safety correctly implemented (spinlock pattern is proper)
- Type safety from protobuf is valuable
- Loose coupling enables testing
- Recent standardization efforts (includes, events refactor) show maturity

**Negatives:**
- Config apply still broken (high impact)
- Type unsafety in zephlet struct (medium impact)
- Missing lifecycle cleanup (medium impact)
- Hardcoded timeouts (low impact)
- Error handling is fire-and-forget (intentional, limits observability)

**Verdict:** Good foundation with correct core patterns (thread safety). Main issues are config apply bug and missing lifecycle management. Code generation maturity and standardization show project is evolving well. Ready for production with fixes to config apply and lifecycle.
