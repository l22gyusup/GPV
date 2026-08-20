# `rtl/` — Hand-Written SystemVerilog RTL (Planned)

The thin SV layer that wraps the HLS-generated core to add features
HLS cannot express directly: multi-ID AXI traffic, response reorder,
and boundary-facing performance monitors.

## Status

Not started. The HLS core in `hls/` must exist first (or at least a
placeholder module with the right port list) before this wrapper can
be integrated.

## Planned Layout

```
rtl/
├── gvp_top.sv              # DUT top: instantiates HLS core + wrappers + monitors
├── id_mapper_rd.sv         # Read-side ID mapper + reorder buffer
├── id_mapper_wr.sv         # Write-side ID mapper + completion tracker
├── perf_monitor_rd.sv      # Per-ID FIFO, latency accumulator, MO peak tracker
├── perf_monitor_wr.sv      # Same for write side
└── gvp_regs_pkg.sv         # (Auto-generated later) shared register offsets
```

## Design Contracts

- Reset: `rst_n` active-low, async assert / sync deassert. All state
  cleared during reset; DUT-driven `*VALID` and `*READY` outputs go
  to 0.
- No CDC; single `clk` domain per Section 2 of the RTL spec.
- Fixed AXI-signal contract enforced by assertions living in a bind
  file (see `docs/GVP_VPLAN.md` Section 4.2).

## Reference

- `docs/GVP_RTL_SPEC.md` Sections 3 (interfaces), 6.3 (ID mapper),
  and 4.9 (performance monitors) — the primary requirements this
  layer implements.
- `docs/GVP_GUIDE.md` — SV RTL naming and coding rules.
