"""Smoke a few CoAP RPCs at a running native_sim demo container.

Intended to be run inside the sibling infra's `zephlet-tester` image
via `just native-coap-smoke`. That recipe shares the demo container's
network namespace (`--network container:<demo>`), so the smoke reaches
the Zephyr CoAP server on `127.0.0.1:5683` with no host-side port
forwarding involved — which keeps the test independent of the
container runtime's UDP forwarding behavior (Docker Desktop vs Colima
vs lima vs bare Linux all give different results).
"""

from __future__ import annotations

import asyncio
import sys

from aiocoap import POST, Context, Message

# Each entry: (uri-path, expected response-code prefix, label).
# Only idempotent read RPCs (`get_status`) are exercised here so the
# smoke result is independent of the demo's start/stop lifecycle in
# main(). Lifecycle-changing methods (start/stop/config) can legitimately
# return 4.09 Conflict depending on current state.
CASES = [
    ("zlet/ui/ui_fake_impl/get_status",               "2.", "ui.get_status"),
    ("zlet/tick/tick_timer_based_impl/get_status",    "2.", "tick.get_status"),
    ("zlet/tampering/tampering_emul_impl/get_status", "4.04", "tampering.get_status (not opted in)"),
]


async def main() -> int:
    ctx = await Context.create_client_context()
    fails = 0
    try:
        for path, expected, label in CASES:
            req = Message(code=POST, uri=f"coap://127.0.0.1:5683/{path}")
            try:
                resp = await asyncio.wait_for(ctx.request(req).response, timeout=5)
                got = str(resp.code)
                ok = got.startswith(expected)
                sigil = "OK  " if ok else "FAIL"
                payload = resp.payload.hex() or "(empty)"
                print(f"{sigil} {label:45s} -> {got}  payload={payload}")
                if not ok:
                    fails += 1
            except Exception as exc:
                print(f"FAIL {label:45s} -> EXC {type(exc).__name__}: {exc}")
                fails += 1
    finally:
        await ctx.shutdown()
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
