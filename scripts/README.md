# `scripts/` — Cross-Project Utilities

Small helper scripts that operate across the repository. Anything
scoped to a single subtree (for example, `c_model/vectors/`) lives
next to its data; anything shared lives here.

## Scripts

### `gen_regs.py`

Generates language-specific register headers from the YAML source of
truth (`specs/registers.yaml`).

```bash
# Regenerate the C header (default output c_model/gvp_regs.h)
python3 scripts/gen_regs.py

# CI-style drift check (exit 1 if output would change)
python3 scripts/gen_regs.py --check

# Override paths
python3 scripts/gen_regs.py --yaml specs/registers.yaml \
                            --c-out c_model/gvp_regs.h
```

Also invokable from `c_model/Makefile`:

```bash
cd c_model
make regs        # regenerate
make regs-check  # CI drift check
```

Consumers today: `c_model/gvp_regs.h`.
Consumers planned: SystemVerilog package in `rtl/gvp_regs_pkg.sv`
(auto-generated later) and any driver code.

**Do not hand-edit generator outputs.** Update the YAML and rerun.

## Dependencies

- Python 3
- `pyyaml`

## Adding a New Script

Keep scripts self-contained where possible: one file, argparse-based
CLI, `--check` mode where regeneration is idempotent. Follow the file
header convention from `docs/GVP_GUIDE.md`.
