# GVP Guide — Naming and Coding Conventions

This document defines the naming and coding conventions used throughout the GVP
(Gyusup Verification Platform) project. All contributions should follow these
rules to keep the codebase consistent across languages (C, HLS C++,
SystemVerilog RTL, SystemVerilog UVM).

---

## 1. Common Conventions (All Languages)

| Item                | Rule                                               |
|---------------------|----------------------------------------------------|
| Indentation         | 4 spaces (never tabs)                              |
| Line length         | 100 columns (soft), 120 (hard)                     |
| Line endings        | LF (Unix)                                          |
| File encoding       | UTF-8, no BOM                                      |
| Trailing whitespace | Not allowed                                        |
| Final newline       | Every file must end with a newline                 |

---

## 2. File Header

Every source file (C, C++, SystemVerilog) must begin with the following header.
Use `//` comments uniformly across all languages.

```
//------------------------------------------------------------------------------
// File        : <filename>
// Description : <one-line description>
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : YYYY-MM-DD
// Copyright   : (c) YYYY Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------
```

Notes:
- `<filename>` matches the actual file name (e.g., `fft16.c`).
- `Created` uses ISO date format (`YYYY-MM-DD`).
- Copyright year is the file's original creation year (do not update on edits;
  git history tracks changes).

---

## 3. File and Directory Naming

| Item          | Rule                                | Example                         |
|---------------|-------------------------------------|---------------------------------|
| Directories   | snake_case, lowercase               | `c_model/`, `id_mapper/`        |
| Source files  | snake_case, matches primary object  | `fft16.c`, `id_mapper.sv`       |
| Header files  | snake_case                          | `fft16.h`, `fft_types.hpp`      |
| Documentation | UPPER_SNAKE or PascalCase           | `README.md`, `GVP_GUIDE.md`     |

- **No project prefix** on modules or classes (i.e., no `gvp_` prefix).
- Simple, descriptive names are preferred.

---

## 4. C — `c_model/` (Golden Reference)

| Item              | Rule                                      | Example                    |
|-------------------|-------------------------------------------|----------------------------|
| Standard          | C11                                       |                            |
| Functions         | snake_case                                | `fft16_forward()`          |
| Variables         | snake_case                                | `sample_count`             |
| Macros            | UPPER_SNAKE                               | `FFT_POINTS`               |
| Constants         | UPPER_SNAKE                               | `const int MAX_SIZE = 16;` |
| Struct / typedef  | `_t` suffix                               | `cplx_t`, `fft_ctx_t`      |
| Header guard      | `#ifndef <FILE>_H_ / #define / #endif`    | `#ifndef FFT16_H_`         |
| Floating point    | Allowed (this is the reference model)     |                            |

---

## 5. HLS C++ — `hls/` (Synthesizable)

| Item              | Rule                                    | Example                             |
|-------------------|-----------------------------------------|-------------------------------------|
| Standard          | C++14 (Vitis HLS supported subset)      |                                     |
| Functions         | snake_case                              | `load_samples()`                    |
| Variables         | snake_case                              | `beat_count`                        |
| Classes           | PascalCase (rarely used in HLS)         | `FftEngine`                         |
| Fixed-point type  | Alias via `typedef`                     | `typedef ap_fixed<16, 2> sample_t;` |
| Complex type      | Struct or `hls::x_complex`, aliased     | `typedef struct { sample_t re, im; } cplx_t;` |
| Interface pragmas | Grouped at top of function body         |                                     |
| Top function      | Descriptive name, matches file          | `fft_dma()`                         |

---

## 6. SystemVerilog RTL — `rtl/`

| Item                | Rule                                                                          | Example        |
|---------------------|-------------------------------------------------------------------------------|----------------|
| Module name         | snake_case, matches file                                                      | `id_mapper`    |
| Signals             | snake_case                                                                    | `data_valid`   |
| Clock               | `clk` (single) / `<domain>_clk` (multiple domains)                            | `clk`, `axi_clk` |
| Reset               | `rst_n` (active-low) — project standard                                       | `rst_n`        |
| Active-low signals  | `_n` suffix (semantic marker for polarity)                                    | `enable_n`     |
| Parameter           | UPPER_SNAKE                                                                   | `DATA_WIDTH`   |
| Localparam          | UPPER_SNAKE                                                                   | `IDLE`         |
| Always blocks       | Explicit: `always_ff` / `always_comb` / `always_latch` (never plain `always`) |                |
| Assignments         | Non-blocking `<=` in `always_ff` only; blocking `=` in `always_comb` only     |                |
| One module per file | Required                                                                      |                |

---

## 7. SystemVerilog UVM — `tb/`

Role-based class naming. Class names carry the UVM role (agent, driver, monitor,
sequencer, sequence, transaction, config, test) without exotic prefixes or
suffixes.

| Item              | Rule                                        | Example                         |
|-------------------|---------------------------------------------|---------------------------------|
| Class             | snake_case; role is the class name          | `axi_agent`, `fft_driver`       |
| Package           | `<name>_pkg` (SV convention)                | `axi_pkg`, `fft_env_pkg`        |
| Config class      | `<domain>_config`                           | `axi_config`                    |
| Transaction item  | `<domain>_transaction`                      | `axi_transaction`               |
| Sequence          | `<scenario>_sequence`                       | `smoke_sequence`                |
| Test              | `<scenario>_test`                           | `smoke_test`                    |
| Members           | No `m_` prefix — plain descriptive names    | `env`, `agent`, `driver`        |
| `uvm_info` ID     | Class name (usually `get_type_name()`)      |                                 |
| One class per file| Required                                    |                                 |

---

## 8. AXI Signal Naming (Xilinx Convention)

To interoperate with Xilinx AXI VIP and HLS-generated interfaces:

```
m_axi_<bundle>_awvalid       // e.g., m_axi_gmem_rd_awvalid
m_axi_<bundle>_arready
s_axi_lite_awvalid           // control interface uses s_axi_lite_ prefix
```

Bundle names should reflect purpose (e.g., `gmem_rd`, `gmem_wr`), not just
sequential naming.

---

## 9. Comments

- Default to writing no comments. Well-named identifiers document intent.
- Add comments only when the **why** is non-obvious: hidden constraints, subtle
  invariants, workarounds, or surprising behavior.
- Do not explain **what** the code does when the code speaks for itself.

---

## 10. Commit Messages

No strict format required. Guidelines:
- Use imperative mood: "add", "fix", "update" (not "added", "fixed").
- One-line summary under 72 characters.
- Add a blank line and longer body if needed for context.

---

## 11. Deviations

If a specific situation requires deviating from these conventions, document the
reason in a comment near the deviation, or in a PR description.
