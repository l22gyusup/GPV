# GVP

**Gyusup Verification Platform** — A UVM-based testbench built from scratch to
verify an HLS-generated FFT + DMA accelerator RTL. This repository serves as a
learning and experimental project for practicing scratch-built UVM verification
against a non-trivial, protocol-rich DUT.

## Goals

- Design an HLS-generated RTL that exercises the full range of AXI4 protocol features.
- Build a complete UVM verification environment from scratch (no reused skeleton).
- Apply Coverage-Driven Constrained Random Verification (CDV / CRV) methodology.
- Verify functional correctness (FFT results) and protocol compliance simultaneously.

## DUT Overview

The DUT is an FFT + DMA accelerator. It reads input samples from external memory
via an AXI4 Read Master, performs 16-point radix-2 FFTs on batches of samples,
and writes the results back to external memory via an AXI4 Write Master. A single
activation processes `N` back-to-back FFTs (`N` is runtime configurable), which
generates rich AXI traffic patterns without requiring per-FFT reconfiguration.

### Block Diagram

```
                  +--------------------------------------------------+
                  |                    DUT (Top)                     |
                  |                                                  |
  TB --AXI-Lite-->|  +------------+                                  |
  (Config)        |  | Ctrl Regs  |                                  |
                  |  +------+-----+                                  |
                  |         |                                        |
                  |         v                                        |
                  |  +--------------------------+                    |
                  |  |  HLS Core (fft_dma)      |                    |
                  |  |   - 16-pt radix-2 FFT    |                    |
                  |  |   - Batched N FFTs       |                    |
                  |  |   - Fixed-point compute  |                    |
                  |  +--+--------------------+--+                    |
                  |     | (ID=0)             | (ID=0)                |
                  |     v                    v                       |
                  |  +----------+        +----------+                |
                  |  |  RD ID   |        |  WR ID   |                |
                  |  |  Mapper  |        |  Mapper  |                |
                  |  | (0..3)   |        | (0..3)   |                |
                  |  +----+-----+        +----+-----+                |
                  +-------|-------------------|--------------------- +
                          v (AR/R)            v (AW/W/B)
                  +------------------------------------------+
                  |            AXI Slave BFM                 |
                  |     (Xilinx AXI VIP + virtual memory)    |
                  +------------------------------------------+
```

### AXI Interface Configuration

| Parameter     | Value                                            |
|---------------|--------------------------------------------------|
| Data Width    | 256-bit                                          |
| Address Width | 64-bit                                           |
| ID Width      | 2-bit (IDs 0 – 3)                                |
| Protocol      | AXI4 (full) for data, AXI4-Lite for control      |

### Control Registers (AXI4-Lite)

| Register    | Description                                                        |
|-------------|--------------------------------------------------------------------|
| `CTRL`      | start / done / idle / ap_ready status                              |
| `RD_ADDR`   | Source base address (64-bit)                                       |
| `WR_ADDR`   | Destination base address (64-bit)                                  |
| `NUM_FFTS`  | Number of 16-point FFTs to process in this activation              |
| `MODE`      | Operating mode (copy / read-only / write-only / rw-independent)    |
| `FFT_CFG`   | FFT direction, scaling, etc. (reserved for extension)              |
| `STATUS`    | Runtime status flags                                               |
| `PERF_CNT`  | Performance counters (optional)                                    |

### FFT Kernel

- 16-point, radix-2, decimation-in-time (DIT).
- Hand-written HLS C++ (no external FFT library).
- Fixed-point sample representation (Q2.14 or similar; final choice TBD).
- Precomputed twiddle factors baked in as constants.
- One FFT consumes and produces 16 complex samples per invocation.

## Verification Stack

| Layer          | Choice                                                            |
|----------------|-------------------------------------------------------------------|
| HLS Compiler   | Vitis HLS (Xilinx / AMD)                                          |
| DUT Language   | HLS-generated Verilog + hand-written SystemVerilog wrappers       |
| TB Language    | SystemVerilog + UVM                                               |
| Simulator      | Vivado xsim                                                       |
| AXI VIP        | Xilinx AXI VIP                                                    |
| Methodology    | Coverage-Driven Constrained Random Verification (CDV / CRV)       |

### Rationale

- **xsim** was chosen because the Xilinx AXI VIP ships as encrypted SystemVerilog
  and only runs on xsim / Questa. Verilator was considered but ruled out due to
  VIP incompatibility and incomplete UVM support.
- **UVM** was chosen as the industry-standard methodology for reusability and
  structured verification.
- **Vitis HLS** was chosen to keep the toolchain end-to-end within the Xilinx
  ecosystem (Vitis HLS ↔ Vivado ↔ xsim ↔ AXI VIP).
- **AXI4-Lite (not APB)** for the control interface because Vitis HLS supports
  AXI4-Lite natively; APB would require a bridge and add scope with no benefit.

## Verification Approach

### Randomized at Runtime

| Item                                                | Source                                     |
|-----------------------------------------------------|--------------------------------------------|
| `src_addr`, `dst_addr`, `num_ffts`, `mode`          | UVM sequence via AXI4-Lite                 |
| Input sample data                                   | Sequence fills the AXI slave backing memory|
| Slave AXI ready / response delays (backpressure)    | AXI VIP configuration                      |
| ID mapping policy (round-robin / random / seq.)     | Wrapper mode register (planned)            |

### Fixed at HLS Compile Time

HLS pragmas hard-code certain AXI parameters into the generated RTL. To sweep
these values, multiple HLS builds are produced and treated as coverage bins.

| Item                                                     | Configured via  |
|----------------------------------------------------------|-----------------|
| `num_read_outstanding` / `num_write_outstanding` (MO)    | HLS pragma      |
| `max_read_burst_length` / `max_write_burst_length`       | HLS pragma      |
| Data width (256), Address width (64), ID width (2)       | HLS pragma      |

Multiple builds (e.g., `dut_mo4`, `dut_mo8`, `dut_mo16`, `dut_mo32`) contribute
to coverage bins that span the DUT configuration space.

### Planned Scenario Categories

To be elaborated once the DUT is stable:

- Backpressure tolerance (slave-side ready stalls).
- Multiple Outstanding (MO) utilization.
- Max MO stress.
- Response latency injection.
- Data integrity checking against a golden FFT reference model.
- 4 KB boundary and alignment cases.
- Error injection (SLVERR, DECERR responses).
- Out-of-order response handling using multiple IDs.

## Repository Layout (Planned)

```
gvp/
├── c_model/          # Pure C golden reference (FFT + DMA behavior)
├── hls/              # HLS-ready C++ (derived from c_model, with pragmas)
│   ├── src/          # C++ FFT + DMA sources
│   └── configs/      # TCL scripts for multiple builds (MO / burst sweeps)
├── rtl/              # Hand-written RTL (ID mapper wrapper, top)
├── tb/               # UVM testbench
│   ├── agents/       # AXI, AXI-Lite agents (using VIP)
│   ├── env/          # UVM environment, config
│   ├── sequences/    # Constrained random sequences
│   ├── tests/        # UVM tests
│   └── coverage/     # Coverage groups
├── sim/              # Simulation run scripts
├── scripts/          # Build automation (HLS synthesis, RTL packaging)
└── docs/             # Additional documentation
    └── GVP_GUIDE.md  # Naming and coding conventions
```

## Coding Conventions

Naming and coding conventions are documented in
[`docs/GVP_GUIDE.md`](docs/GVP_GUIDE.md).
