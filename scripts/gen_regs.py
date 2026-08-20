#!/usr/bin/env python3
# ----------------------------------------------------------------------------
# File        : gen_regs.py
# Description : Generate register-map artifacts from specs/registers.yaml.
#               Currently emits a C header for use by the C reference model
#               (and later by any C driver code). SystemVerilog / Python
#               emitters can be added by extending the generator without
#               changing the YAML.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-20
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
# ----------------------------------------------------------------------------

import argparse
import os
import sys

import yaml


REPO_ROOT   = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
YAML_PATH   = os.path.join(REPO_ROOT, "specs", "registers.yaml")
C_OUT_PATH  = os.path.join(REPO_ROOT, "c_model", "gvp_regs.h")


def parse_bits(spec):
    s = str(spec).strip()
    if ":" in s:
        msb, lsb = (int(x) for x in s.split(":"))
    else:
        msb = lsb = int(s)
    if msb < lsb:
        raise ValueError(f"Field bits '{spec}' has MSB < LSB")
    width = msb - lsb + 1
    mask  = ((1 << width) - 1) << lsb
    return lsb, width, mask


def load_spec(path):
    with open(path, "r") as f:
        spec = yaml.safe_load(f)

    device = spec["device"]
    regs   = spec["registers"]

    seen_offsets = {}
    for r in regs:
        off = int(r["offset"])
        if off in seen_offsets:
            raise ValueError(
                f"Register '{r['name']}' offset 0x{off:X} collides with "
                f"'{seen_offsets[off]}'"
            )
        seen_offsets[off] = r["name"]
        if "fields" in r:
            for fld in r["fields"]:
                parse_bits(fld["bits"])
    return device, regs


def emit_c_header(device, regs):
    lines = []
    push = lines.append

    dev_upper = device["name"].upper()
    guard     = f"{dev_upper}_REGS_H_"

    push("//" + "-" * 76)
    push("// File        : gvp_regs.h")
    push("// Description : Auto-generated register map for the GVP DUT.")
    push("//               DO NOT EDIT. Regenerate via scripts/gen_regs.py")
    push("//               from specs/registers.yaml.")
    push("// Author      : Gyusup LEE <gyu2910@waric.co.kr>")
    push("// Created     : 2026-08-20")
    push("// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.")
    push("//" + "-" * 76)
    push("")
    push(f"#ifndef {guard}")
    push(f"#define {guard}")
    push("")
    push("#include <stdint.h>")
    push("")
    push(f"#define {dev_upper}_ADDR_BITS  {device['addr_bits']}")
    push(f"#define {dev_upper}_DATA_BITS  {device['data_bits']}")
    push("")

    push("// -------------------------------------------------------------------")
    push("// Register offsets")
    push("// -------------------------------------------------------------------")
    for r in regs:
        macro = f"{dev_upper}_REG_{r['name']}"
        push(f"#define {macro:40s} 0x{int(r['offset']):03X}u")
    push("")

    push("// -------------------------------------------------------------------")
    push("// Field masks and shifts")
    push("// -------------------------------------------------------------------")
    for r in regs:
        if "fields" not in r:
            continue
        push(f"// {r['name']} (0x{int(r['offset']):03X}) : {r.get('description', '')}")
        for fld in r["fields"]:
            lsb, width, mask = parse_bits(fld["bits"])
            base = f"{dev_upper}_{r['name']}_{fld['name'].upper()}"
            push(f"#define {base + '_LSB':40s} {lsb}u")
            push(f"#define {base + '_WIDTH':40s} {width}u")
            push(f"#define {base + '_MASK':40s} 0x{mask:08X}u")
        push("")

    # Enums (from `enums:` block on a register)
    any_enum = any("enums" in r for r in regs)
    if any_enum:
        push("// -------------------------------------------------------------------")
        push("// Enumerated field values")
        push("// -------------------------------------------------------------------")
        for r in regs:
            if "enums" not in r:
                continue
            for fld_name, mapping in r["enums"].items():
                push(f"// {r['name']}.{fld_name}")
                for enum_name, enum_val in mapping.items():
                    macro = f"{dev_upper}_{r['name']}_{fld_name.upper()}_{enum_name}"
                    push(f"#define {macro:40s} {int(enum_val)}u")
                push("")

    push(f"#endif  // {guard}")
    push("")
    return "\n".join(lines)


def write_if_changed(path, content):
    if os.path.exists(path):
        with open(path, "r") as f:
            if f.read() == content:
                return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(content)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--yaml", default=YAML_PATH,
                        help=f"YAML spec path (default: {YAML_PATH})")
    parser.add_argument("--c-out", default=C_OUT_PATH,
                        help=f"C header output path (default: {C_OUT_PATH})")
    parser.add_argument("--check", action="store_true",
                        help="Fail if outputs would change (CI mode)")
    args = parser.parse_args()

    device, regs = load_spec(args.yaml)
    c_content    = emit_c_header(device, regs)

    if args.check:
        with open(args.c_out, "r") as f:
            current = f.read()
        if current != c_content:
            print(f"C header at {args.c_out} is out of date. "
                  f"Regenerate with scripts/gen_regs.py.", file=sys.stderr)
            return 1
        print("Register map artifacts are up to date.")
        return 0

    changed = write_if_changed(args.c_out, c_content)
    if changed:
        print(f"Wrote {args.c_out}")
    else:
        print(f"{args.c_out} already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
