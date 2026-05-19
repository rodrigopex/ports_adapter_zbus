#!/usr/bin/env python3
"""Per-zephlet RAM/Flash breakdown from a Zephyr build.

Runs `nm --print-size` on zephyr.elf, classifies symbols by zephlet
instance / type / framework, and emits a markdown table.

Symbol naming (set by ZEPHLET_NEW + zbus internals):

  Per channel:
    chan_<inst>_command            .rodata  struct zbus_channel
    chan_<inst>_command00          .rodata  channel-observer ref
    chan_<inst>_command00_mask     .data    runtime obs mask byte
    _zbus_chan_data_chan_<inst>_*  .data    zbus_channel_data
    _zbus_message_chan_<inst>_*    .bss     payload storage

  Per type:
    lis_<type>, lis_<type>_fn      .rodata + .text
    <type>_api, <type>_methods     .rodata
    <type>_<cmd>, <type>_<cmd>_impl .text
    <type>_*_msg/_fields/_field_info/_submsg_info  .rodata (nanopb)

  Per instance descriptor:
    <inst>                          iterable section (RAM here)

  Framework:
    zephlet_dispatch, zephlet_init_walker, __init_zephlet_init_walker
    _zephlet_list_{start,end}

Usage: size_report.py <build-dir>
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

ZEPHYR_TYPES = ("tick", "ui", "tampering", "sensor")


@dataclass
class Sym:
    addr: int
    size: int
    section: str  # nm letter
    name: str

    @property
    def in_ram(self) -> bool:
        return 0x20000000 <= self.addr < 0x40000000

    @property
    def in_flash(self) -> bool:
        return 0 <= self.addr < 0x20000000


def find_nm() -> str:
    candidates = [
        os.environ.get("ZEPHYR_NM"),
        "/Users/rodrigopeixoto/.local/zephyr-sdk-1.0.0/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm",
        "arm-zephyr-eabi-nm",
    ]
    for c in candidates:
        if not c:
            continue
        try:
            subprocess.run([c, "--version"], capture_output=True, check=True)
            return c
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    sys.exit("error: arm-zephyr-eabi-nm not found (set ZEPHYR_NM env var)")


NM_LINE = re.compile(r"^([0-9a-f]+)\s+([0-9a-f]+)\s+(\S)\s+(.+)$")


def load_symbols(elf: Path) -> list[Sym]:
    out = subprocess.run(
        [find_nm(), "--print-size", str(elf)],
        capture_output=True,
        check=True,
        text=True,
    ).stdout
    syms: list[Sym] = []
    for line in out.splitlines():
        m = NM_LINE.match(line)
        if not m:
            continue
        addr, size, sect, name = m.groups()
        syms.append(Sym(int(addr, 16), int(size, 16), sect, name))
    return syms


@dataclass
class Bucket:
    flash: int = 0
    ram: int = 0
    members: list[tuple[str, int, str]] = field(default_factory=list)

    def add(self, s: Sym, where: str | None = None) -> None:
        if where == "flash" or (where is None and s.in_flash):
            self.flash += s.size
        elif where == "ram" or (where is None and s.in_ram):
            self.ram += s.size
        self.members.append((s.name, s.size, "RAM" if s.in_ram else "FLASH"))


def detect_instances(syms: list[Sym]) -> dict[str, str]:
    """Map instance-name -> type by scanning chan_<inst>_command symbols."""
    out: dict[str, str] = {}
    rx = re.compile(r"^chan_(.+)_command$")
    for s in syms:
        m = rx.match(s.name)
        if not m:
            continue
        inst = m.group(1)
        # Type is the leading word; e.g. tick_timer_based_impl -> tick.
        for t in ZEPHYR_TYPES:
            if inst.startswith(t + "_") or inst == t:
                out[inst] = t
                break
        else:
            out[inst] = inst.split("_", 1)[0]
    return out


def classify(syms: list[Sym], instances: dict[str, str]):
    """Classify each symbol into its bucket. Returns nested dict."""
    by_inst: dict[str, dict[str, Bucket]] = defaultdict(lambda: defaultdict(Bucket))
    by_type: dict[str, dict[str, Bucket]] = defaultdict(lambda: defaultdict(Bucket))
    framework = defaultdict(Bucket)
    other = Bucket()

    inst_re = "|".join(re.escape(i) for i in sorted(instances, key=len, reverse=True)) or "(?!)"
    type_re = "|".join(re.escape(t) for t in ZEPHYR_TYPES)

    pat_chan_rodata = re.compile(rf"^chan_({inst_re})_(command|events)(?:00)?$")
    pat_chan_obs_ref = re.compile(rf"^chan_({inst_re})_(command|events)(?:zz\d+_.+)?(?:00)?(_mask)?$")
    pat_chan_data = re.compile(rf"^_zbus_chan_data_chan_({inst_re})_(command|events)$")
    pat_chan_msg = re.compile(rf"^_zbus_message_chan_({inst_re})_(command|events)$")
    pat_lis = re.compile(rf"^lis_({type_re})(_fn)?$")
    pat_obs_data_lis = re.compile(rf"^_zbus_obs_data_lis_({type_re})$")
    pat_api = re.compile(rf"^({type_re})_api$")
    pat_methods = re.compile(rf"^({type_re})_methods$")
    pat_cmd_impl = re.compile(rf"^({type_re})_([a-z_][a-z0-9_]*)_impl$")
    pat_nanopb_msg = re.compile(rf"^({type_re})_(.+)_(msg|fields|field_info|submsg_info|callback|default)$")
    pat_data_storage = re.compile(rf"^({type_re})_data_storage$")
    pat_cfg = re.compile(rf"^({type_re})_cfg$")
    pat_inst_desc = re.compile(rf"^({inst_re})$")
    pat_adapter_lis = re.compile(rf"^_zephlet_ev_({inst_re})_(.+)_(lis|fn)$")
    pat_adapter_obs_data = re.compile(rf"^_zbus_obs_data__zephlet_ev_({inst_re})_(.+)_lis$")
    pat_adapter_work = re.compile(rf"^_zbus_observer_work(_fifo)?__zephlet_ev_({inst_re})_(.+)_lis$")
    pat_adapter_chan_obs_ref = re.compile(rf"^chan_({inst_re})_eventszz\d+_.+$")

    framework_names = {
        "zephlet_dispatch",
        "zephlet_get_by_name",
        "zephlet_init_walker",
        "__init_zephlet_init_walker",
        "_zephlet_list_start",
        "_zephlet_list_end",
        "_zbus_chan_slock",
    }

    pat_wrapper = re.compile(rf"^({type_re})_([a-z_][a-z0-9_]*)$")

    for s in syms:
        n = s.name
        m = pat_chan_data.match(n)
        if m:
            inst, ch = m.groups()
            by_inst[inst][f"chan-{ch} runtime (.data)"].add(s, "ram")
            continue
        m = pat_chan_msg.match(n)
        if m:
            inst, ch = m.groups()
            by_inst[inst][f"chan-{ch} msg buffer (.bss)"].add(s, "ram")
            continue
        m = pat_chan_rodata.match(n)
        if m:
            inst, ch = m.groups()[:2]
            by_inst[inst][f"chan-{ch} const (.rodata)"].add(s, "flash")
            continue
        m = re.match(rf"^chan_({inst_re})_(command|events)(zz\d+_.+|00_mask)$", n)
        if m:
            inst, ch = m.groups()[:2]
            tag = m.group(3)
            if tag.endswith("_mask"):
                by_inst[inst][f"chan-{ch} obs-mask (.data)"].add(s, "ram")
            else:
                by_inst[inst][f"chan-{ch} obs-ref (.rodata)"].add(s, "flash")
            continue
        m = pat_adapter_obs_data.match(n)
        if m:
            inst = m.group(1)
            by_inst[inst]["adapter obs-data (.data)"].add(s, "ram")
            continue
        m = pat_adapter_work.match(n)
        if m:
            inst = m.group(2)
            by_inst[inst]["adapter work-q (.data)"].add(s, "ram")
            continue
        m = pat_adapter_lis.match(n)
        if m:
            inst, _, kind = m.groups()
            tag = "adapter listener fn (.text)" if kind == "fn" else "adapter listener const (.rodata)"
            by_inst[inst][tag].add(s)
            continue
        m = pat_adapter_chan_obs_ref.match(n)
        if m:
            inst = m.group(1)
            by_inst[inst]["adapter obs-ref (.rodata)"].add(s, "flash")
            continue
        m = pat_inst_desc.match(n)
        if m:
            inst = m.group(1)
            by_inst[inst]["zephlet descriptor (iterable)"].add(s)
            continue

        m = pat_lis.match(n)
        if m:
            t, fn = m.groups()
            tag = "listener fn (.text)" if fn else "listener const (.rodata)"
            by_type[t][tag].add(s)
            continue
        m = pat_obs_data_lis.match(n)
        if m:
            t = m.group(1)
            by_type[t]["listener obs-data (.data)"].add(s, "ram")
            continue
        m = pat_api.match(n)
        if m:
            by_type[m.group(1)]["api struct (.rodata)"].add(s, "flash")
            continue
        m = pat_methods.match(n)
        if m:
            by_type[m.group(1)]["methods table (.rodata)"].add(s, "flash")
            continue
        m = pat_cmd_impl.match(n)
        if m:
            t = m.group(1)
            by_type[t]["handlers (.text)"].add(s, "flash")
            continue
        m = pat_data_storage.match(n)
        if m:
            t = m.group(1)
            by_type[t]["data storage (.bss)"].add(s, "ram")
            continue
        m = pat_cfg.match(n)
        if m:
            t = m.group(1)
            by_type[t]["config storage (.data)"].add(s, "ram")
            continue
        m = pat_nanopb_msg.match(n)
        if m:
            by_type[m.group(1)]["nanopb (.rodata)"].add(s, "flash")
            continue
        m = pat_wrapper.match(n)
        if m and s.section.lower() == "t":
            t = m.group(1)
            by_type[t]["wrappers (.text)"].add(s, "flash")
            continue

        if n in framework_names:
            framework[n].add(s)
            continue

        other.add(s)

    return by_inst, by_type, framework, other


def fmt(b: Bucket) -> str:
    parts = []
    if b.flash:
        parts.append(f"F={b.flash}")
    if b.ram:
        parts.append(f"R={b.ram}")
    return " ".join(parts) or "-"


def total(buckets: dict[str, Bucket]) -> tuple[int, int]:
    f = sum(b.flash for b in buckets.values())
    r = sum(b.ram for b in buckets.values())
    return f, r


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", type=Path, help="Build dir (e.g. build_nrf)")
    ap.add_argument("-v", "--verbose", action="store_true", help="Print every classified symbol")
    args = ap.parse_args()

    elf = args.build_dir / "zephyr" / "zephyr.elf"
    if not elf.exists():
        sys.exit(f"error: {elf} not found")

    syms = load_symbols(elf)
    instances = detect_instances(syms)
    by_inst, by_type, framework, other = classify(syms, instances)

    print(f"# Zephlet footprint — `{elf}`\n")
    print(f"Instances detected: {', '.join(sorted(instances)) or '(none)'}\n")

    print("## Per-instance (channel + descriptor + adapter)\n")
    print("| Instance | Type | Sub-bucket | Flash | RAM |")
    print("|---|---|---|---:|---:|")
    inst_totals_f = inst_totals_r = 0
    for inst in sorted(instances):
        t = instances[inst]
        f_sum = r_sum = 0
        for tag in sorted(by_inst[inst]):
            b = by_inst[inst][tag]
            f_sum += b.flash
            r_sum += b.ram
            print(f"| `{inst}` | {t} | {tag} | {b.flash} | {b.ram} |")
        print(f"| **`{inst}` subtotal** | {t} |  | **{f_sum}** | **{r_sum}** |")
        inst_totals_f += f_sum
        inst_totals_r += r_sum
    print(f"| **All instances** |  |  | **{inst_totals_f}** | **{inst_totals_r}** |\n")

    print("## Per-type (shared by all instances of that type)\n")
    print("| Type | Sub-bucket | Flash | RAM |")
    print("|---|---|---:|---:|")
    type_totals_f = type_totals_r = 0
    for t in sorted(by_type):
        f_sum = r_sum = 0
        for tag in sorted(by_type[t]):
            b = by_type[t][tag]
            f_sum += b.flash
            r_sum += b.ram
            print(f"| {t} | {tag} | {b.flash} | {b.ram} |")
        print(f"| **{t} subtotal** |  | **{f_sum}** | **{r_sum}** |")
        type_totals_f += f_sum
        type_totals_r += r_sum
    print(f"| **All types** |  | **{type_totals_f}** | **{type_totals_r}** |\n")

    print("## Framework constants\n")
    print("| Symbol | Flash | RAM |")
    print("|---|---:|---:|")
    fw_f = fw_r = 0
    for n, b in sorted(framework.items()):
        print(f"| `{n}` | {b.flash} | {b.ram} |")
        fw_f += b.flash
        fw_r += b.ram
    print(f"| **Framework subtotal** | **{fw_f}** | **{fw_r}** |\n")

    grand_f = inst_totals_f + type_totals_f + fw_f
    grand_r = inst_totals_r + type_totals_r + fw_r
    print(f"## Grand total (zephlet-attributable)\n")
    print(f"- Flash: **{grand_f} B**")
    print(f"- RAM:   **{grand_r} B**\n")

    if args.verbose:
        print("\n## Verbose symbol dump\n")
        for inst in sorted(by_inst):
            print(f"\n### {inst}")
            for tag, b in sorted(by_inst[inst].items()):
                print(f"\n  {tag}: {fmt(b)}")
                for n, sz, where in b.members:
                    print(f"    {sz:6d} {where:5} {n}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
