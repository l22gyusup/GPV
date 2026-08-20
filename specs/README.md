# `specs/` — Machine-Readable Specifications

Machine-readable specification files that drive code generation.
Prose specifications live in `docs/`; this directory holds the
structured data behind the tables.

## Files

### `registers.yaml`

Single source of truth for the GVP register map. Consumed by
`scripts/gen_regs.py` to produce language-specific headers and (in
the future) SystemVerilog packages and Python modules.

Schema (informal):

```yaml
device:
  name:       <string>            # short lowercase name, used as macro prefix
  addr_bits:  <int>
  data_bits:  <int>

registers:
  - name:        <UPPER_SNAKE>     # register identifier
    offset:      <hex int>          # byte offset within the register file
    access:      <RO|RW|W1S|W1TC|COR>
    reset:       <hex int>
    description: <string>
    fields:                        # optional; omit for opaque 32-bit regs
      - name:   <lower_snake>
        bits:   "MSB:LSB"          # or single bit "N"
        access: <RO|RW|W1S|W1TC|COR>
        reset:  <int>
    enums:                         # optional; enumerated field values
      <field_name>:
        <ENUM_NAME>: <int>
```

## Editing Rules

1. **Keep in sync with `docs/GVP_RTL_SPEC.md` Section 4.** The prose
   spec is authoritative for the user; this YAML is authoritative for
   the tooling. If they disagree, fix both.
2. Every change requires regenerating consumer artifacts. From the
   repo root:
   ```bash
   python3 scripts/gen_regs.py
   ```
   Or from `c_model/`: `make regs`.
3. Include the register-level `description:` so it can be surfaced in
   generated headers.
4. Do not add hand-edited output files here. Only YAML source.

## Reference

- Consumer: `scripts/gen_regs.py`.
- Prose spec: `docs/GVP_RTL_SPEC.md`.
- Coding conventions: `docs/GVP_GUIDE.md`.
