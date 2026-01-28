# PKL Adapter Configuration Plan

## Decision
Use pkl with native protobuf support for declarative adapter generation. Coexists with manual adapters - pkl for simple routing, manual C for complex logic.

## Approach
Use `pkl-pantry/protobuf` package to import proto files directly. No intermediate codegen needed.

## Example Syntax

```pkl
// adapters.pkl
amends "package://pkg.pkl-lang.org/pkl-pantry/protobuf@1.0.0#/buf/convert.pkl"

import "modules/services/tick/tick_service.proto"
import "modules/services/ui/ui_service.proto"

adapters {
  new Adapter {
    name = "tick_to_ui"
    enabled = true
    route {
      from = "tick_service.MsgTickReport.tick_event"
      to = "ui_service.MsgUiInvoke.blink"
      map {
        ["duration"] = "interval_ms"  // dest_field -> source_field
      }
      filter = null  // Optional C expression
    }
  }
}
```

## Schema Structure

```pkl
// adapter_schema.pkl
class Adapter {
  name: String
  enabled: Boolean
  route: Route
}

class Route {
  from: String      // "service.MessageType.oneof_field"
  to: String        // "service.MessageType.oneof_field"
  map: Mapping<String, String>?
  filter: String?
}
```

## Generated C Structure

```c
// gen_TickService+UiService_adapter.c
ZBUS_ASYNC_LISTENER_DEFINE(lis_tick_to_ui_adapter, adapter_callback);
ZBUS_CHAN_ADD_OBS(chan_tick_report, lis_tick_to_ui_adapter, 3);

static void adapter_callback(const struct zbus_channel *chan) {
    struct msg_tick_report report;
    zbus_chan_read(&chan_tick_report, &report, K_NO_WAIT);

    if (report.which_report == MSG_TICK_REPORT_TICK_EVENT_TAG) {
        ui_service_blink(report.tick_event.interval_ms);
    }
}
```

## Implementation Steps

1. Install: `pip install pkl-python` in codegen/requirements.txt
2. Create `adapter_schema.pkl` in codegen/
3. Create root-level `adapters.pkl` config file
4. Add `parse_pkl_adapters()` to generate_service.py
5. Create `templates/adapter.c.jinja`
6. Add `generate_adapters.py` script
7. Add `just gen_adapters` command
8. Update adapters/CMakeLists.txt for generated files

## Naming Convention
- Config: `adapters.pkl` (root)
- Generated: `adapters/gen_<Source>+<Dest>_adapter.c`
- Manual: `adapters/<Source>+<Dest>_adapter.c`

## Benefits
- pkl validates proto field existence automatically
- Type checking via protobuf schema
- No proto→pkl codegen step
- Proto changes caught at pkl eval time
- Centralized routing view

## Next Actions
1. Design complete adapter_schema.pkl
2. Implement pkl parser + validator
3. Create Jinja2 template for adapter.c
4. Test with tick→ui example
