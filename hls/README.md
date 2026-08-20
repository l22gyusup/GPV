# `hls/` — HLS-Ready C++ Sources (Planned)

Vitis HLS sources for the GVP DUT. This directory is a placeholder
until the C reference model in `c_model/` is ported to HLS-friendly
C++.

## Status

Not started. The C reference is stable and its behavior is fully
specified in `docs/GVP_RTL_SPEC.md` and `docs/GVP_VPLAN.md`, so this
directory can be filled in a self-contained work stream.

## Planned Layout

```
hls/
├── src/                # HLS synthesizable C++ top and helpers
│   ├── fft_dma_top.cpp # Top function with AXI-Lite slave + AXI masters
│   ├── fft16_hls.h     # ap_fixed<> versions of fft16 primitives
│   └── ...
├── tb/                 # HLS csim testbench (reuses c_model CSVs)
│   └── fft_dma_tb.cpp
├── configs/            # TCL scripts for MO / burst sweeps
│   ├── mo4.tcl
│   ├── mo8.tcl
│   ├── mo16.tcl
│   └── mo32.tcl
└── scripts/            # Project creation / synth / export helpers
    └── create_project.tcl
```

## Vitis HLS Environment

The tool tree lives at `/opt/Xilinx/2025.2/` but the installer
hard-coded the original installer user (`ubuntu`) into the
`settings64.sh` scripts, so sourcing them out of the box fails on
this machine. A wrapper env script needs to be written before HLS
can be invoked; that is deferred until the first HLS source lands
here.

## Reference

- Section 9 (FFT Details) of `docs/GVP_RTL_SPEC.md` — the algorithm
  and precision the HLS source must implement.
- Section 4.9 (Performance counters) and 4.10 (MAPPER_CTRL) — features
  that need HLS-side hooks (the wrapper in `rtl/` handles the rest).
