# `c_model/` — Pure C Reference Model

The C golden reference for the GVP DUT. Every scenario the DUT is
expected to handle is first captured here, exercised against
byte-exact CSV vectors, and used later as the golden for RTL / UVM
scoreboard checks.

## Contents

| Path                         | Role                                                                 |
|------------------------------|----------------------------------------------------------------------|
| `fft16.h`, `fft16.c`         | 16-point radix-2 DIT FFT. Float and Q2.14 fixed-point implementations |
| `fft_dma.h`, `fft_dma.c`     | Top-level DMA reference: byte-addressed memory + batch FFT + modes    |
| `vector_io.h`, `vector_io.c` | CSV loader used by `test_fft16.c`                                     |
| `test_fft16.c`               | CSV-driven tests for the FFT kernel (Float + Fixed)                   |
| `test_fft_dma.c`             | CSV-driven tests for the DMA reference (MODE_FFT / RO / WO)           |
| `vectors/`                   | Generators and CSV vectors                                            |
| `gvp_regs.h`                 | **Auto-generated** register header (from `specs/registers.yaml`)      |
| `Makefile`                   | Build, generate, test, regress                                        |

## Prerequisites

- `gcc` with C11 support (`gcc -std=c11`).
- Python 3 with `numpy` (vector generation) and `pyyaml` (register
  header generation).

## Quick Start

```bash
cd c_model
make gen      # regenerate the curated CSV vectors
make test     # build binaries and run both suites
```

Expected output (last block):

```
Float : 87 / 87 PASS
Fixed : 87 / 87 PASS

MODE_FFT       : 10 / 10 PASS
MODE_READ_ONLY :  5 /  5 PASS
MODE_WRITE_ONLY:  5 /  5 PASS
```

## Makefile Targets

| Target             | Effect                                                                     |
|--------------------|----------------------------------------------------------------------------|
| `all` (default)    | Build `test_fft16` and `test_fft_dma`                                      |
| `gen`              | Regenerate curated CSVs (`fft16_vectors.csv`, `fft_dma_vectors.csv`)       |
| `test`             | Build + run both curated test suites                                       |
| `regs`             | Regenerate `gvp_regs.h` from `specs/registers.yaml`                        |
| `regs-check`       | Fail if `gvp_regs.h` is out of date with the YAML (CI-style check)         |
| `regress-quick`    | Generate a small random vector set and run it (fast smoke of the flow)     |
| `regress-full`     | Generate a large random vector set and run it                              |
| `clean`            | Remove test binaries                                                       |
| `clean-regress`    | Remove timestamped `*_random_*.csv` files under `vectors/`                 |

### Regression overrides

Both `regress-quick` and `regress-full` accept two optional overrides:

```bash
make regress-quick SEED=42          # reproducible run
make regress-quick CASES=500        # override the case count
make regress-quick SEED=42 CASES=20 # both
```

Every random vector file is written with a timestamped filename such
as `vectors/fft16_random_20260820_161037.csv` so prior runs are
preserved on disk. Random CSVs are ignored by git.

## Vector Files

### Curated vectors (committed)

| File                             | Purpose                                        |
|----------------------------------|------------------------------------------------|
| `vectors/fft16_vectors.csv`      | 87 hand-picked scenarios for the FFT kernel    |
| `vectors/fft_dma_vectors.csv`    | 20 hand-picked scenarios for the DMA reference |

Regenerate with `make gen`.

### Random regression vectors (git-ignored)

| Pattern                                | Written by                            |
|----------------------------------------|---------------------------------------|
| `vectors/fft16_random_<stamp>.csv`     | `vectors/gen_vectors.py --regress`    |
| `vectors/fft_dma_random_<stamp>.csv`   | `vectors/gen_dma_vectors.py --regress`|

The seed used is printed on the generator's stdout and can be pinned
with `--seed`.

### CSV format

Long format, one sample per row. Curated `fft16_vectors.csv`:

```
test_id,test_name,sample_idx,in_re,in_im,exp_out_re,exp_out_im
```

Curated `fft_dma_vectors.csv` (and random `fft_dma_random_*.csv`)
carries the batch metadata on every row:

```
test_id,test_name,mode,num_ffts,src_addr,dst_addr,sample_idx,in_re,in_im,exp_re,exp_im
```

The rationale is to keep parsing trivial and to make each row
self-describing — the same file can be replayed by the future UVM CSV
utility without any side channel.

## Fixed-Point Notes

- Sample format: Q2.14 signed 16-bit (`sample_t`), packed as
  `{imag[15:0], real[15:0]}` in memory (Section 5 of `GVP_RTL_SPEC.md`).
- Twiddle format: Q1.15 signed 16-bit.
- Per-stage `>> 1` block scaling. Total scaling factor 1 / N relative
  to the unscaled DFT — the DUT output equals `DFT(x) / 16`.
- Rounding: truncation (`arithmetic shift right`). No rounding modes
  are exposed. HW simplicity + exact match with the C reference.

## Test Tolerance

- **Float FFT tests**: `1e-3` absolute per real / imag component.
  Observed worst case: ~1e-6.
- **Fixed-point FFT tests**: `96` LSB per real / imag component
  (~0.6% relative). Observed worst case: ~20 LSB.
- Tolerances are chosen loose enough to survive random data with
  wide dynamic range without ever masking a real algorithmic bug.
  The `mode_wo_directed` test path is bit-exact (counter pattern has
  no numerical tolerance).

## Register Auto-Generation

`gvp_regs.h` is regenerated from `specs/registers.yaml` by
`scripts/gen_regs.py`.

```bash
make regs       # regenerate
make regs-check # CI: fail if regenerated content would differ
```

Do not hand-edit `gvp_regs.h`. Update the YAML instead.

## Related Documents

- `docs/GVP_RTL_SPEC.md` — DUT behavior this model must mirror.
- `docs/GVP_VPLAN.md` — Test scenarios and coverage plan that reuse
  these CSVs (see Section 6 CSV Replay Path).
- `docs/GVP_GUIDE.md` — Coding conventions applied to every file in
  this directory.
