# `tb/` — UVM Testbench (Planned)

The SystemVerilog / UVM testbench that exercises the DUT and checks
it against the C reference model. Everything here is driven by
`docs/GVP_VPLAN.md`.

## Status

Not started. The verification plan (VPLAN) is complete and lists
every test, covergroup, and assertion this directory should hold, so
work on individual components can begin without further design
decisions.

## Planned Layout

```
tb/
├── env/                    # UVM environment + config
│   ├── gvp_env.sv
│   └── gvp_env_config.sv
├── agents/                 # AXI4-Lite (master), AXI4 (slave BFM wrap)
│   ├── axi_lite/
│   └── axi_slave/          # Wraps Xilinx AXI VIP for master's memory side
├── sequences/              # One file per test category (C0..C9)
├── tests/                  # One file per named test in VPLAN 5.2
├── coverage/               # Covergroup definitions (per VPLAN 4.1)
├── assertions/             # Bind files with SVAs (per VPLAN 4.2)
├── dpi/                    # DPI-C wrappers for the C reference
│   ├── gvp_dpi.svh
│   └── gvp_dpi.c
├── utils/                  # CSV replay, scoreboard helpers, watchdog
└── run/                    # xsim run scripts, filelists, wave configs
```

## Simulator

- xsim (Vivado 2025.2). Chosen because the Xilinx AXI VIP ships as
  encrypted SV and only runs on xsim / Questa.
- Verilator was considered and ruled out for the same VIP-encryption
  reason and incomplete UVM support at the time.

## Reference

- `docs/GVP_VPLAN.md` — full test list, coverage model, sign-off
  criteria. Every file in this directory should be traceable to a
  section of the VPLAN.
- `docs/GVP_GUIDE.md` — UVM class naming (role-based, no exotic
  suffixes) and file conventions.
