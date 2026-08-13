# GVP — Claude Code Working Rules

This project (Gyusup Verification Platform) has strict coding conventions.
**Before writing or editing any source file, read [docs/GVP_GUIDE.md](docs/GVP_GUIDE.md).**
This file is a quick-reference summary; the guide is authoritative.

## Mandatory Rules (Quick Reference)

- **4-space indent**, no tabs, all languages.
- **LF line endings**, UTF-8 (no BOM), no trailing whitespace, final newline.
- **File header** required on every source file (C / C++ / SV). Template:

  ```
  //------------------------------------------------------------------------------
  // File        : <filename>
  // Description : <one-line description>
  // Author      : Gyusup LEE <gyu2910@waric.co.kr>
  // Created     : YYYY-MM-DD
  // Copyright   : (c) YYYY Gyusup LEE. All rights reserved.
  //------------------------------------------------------------------------------
  ```

- **Reset**: `rst_n` (active-low) — project standard.
- **No project prefix** — modules and classes are named without `gvp_`.
- **No exotic suffixes** — use role-based UVM naming (`axi_agent`, `smoke_test`, etc.).
- **Line length**: 100 (soft), 120 (hard).

### Language-Specific Highlights

- **C (`c_model/`)**: C11, snake_case, `UPPER_SNAKE` macros, `_t` suffix for typedefs,
  header guards `#ifndef <FILE>_H_`. Floating point allowed (reference model).
- **HLS C++ (`hls/`)**: C++14, snake_case, `ap_fixed<>` via typedef (`sample_t`),
  interface pragmas grouped at top of function.
- **SV RTL (`rtl/`)**: snake_case modules, `always_ff` / `always_comb` /
  `always_latch` only (never plain `always`), non-blocking `<=` in `always_ff`,
  blocking `=` in `always_comb`, one module per file.
- **SV UVM (`tb/`)**: snake_case classes with role in the name, `<name>_pkg`
  packages, no `m_` prefix on members, one class per file.
- **AXI signals**: Xilinx convention (`m_axi_<bundle>_...`, `s_axi_lite_...`).

## Project Layout

- `c_model/` — pure C golden reference (float + Q2.14 fixed-point)
- `hls/` — HLS-ready C++ (derived from c_model, with pragmas)
- `rtl/` — hand-written SV RTL (wrappers, ID mapper)
- `tb/` — UVM testbench
- `docs/GVP_GUIDE.md` — full naming and coding conventions (authoritative)

## Author Info (for File Headers)

- Author: `Gyusup LEE <gyu2910@waric.co.kr>`
- Copyright: `(c) YYYY Gyusup LEE. All rights reserved.`

## When in Doubt

- Read `docs/GVP_GUIDE.md` for the full rules.
- If a case is not covered by the guide, ask the user before deviating.
- Do not introduce new naming or style patterns without confirming first.
