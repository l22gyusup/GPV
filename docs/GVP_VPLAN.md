# GVP Verification Plan (VPLAN)

Verification plan for the GVP DUT. This document is the bridge between
`GVP_RTL_SPEC.md` (what the DUT is required to do) and the UVM testbench
that will be built in `tb/` (how each requirement is exercised and
signed off).

Living document. Sections marked *TBD* are placeholders that will be
filled as the corresponding testbench components come online.

---

## 1. Scope and Objectives

**In scope**

- Functional correctness of the DUT top-level (FFT + DMA) across all
  three operating modes (Section 8 of the spec).
- Bit-accurate match between the DUT's Q2.14 output and the fixed-point
  C reference model (`c_model/fft_dma.c`) for `MODE_FFT`.
- AXI4 / AXI4-Lite protocol compliance at the DUT boundary.
- Contract-compliance for signals fixed by design (`AxBURST`, `AxCACHE`,
  `AxPROT`, `AxQOS`, `AxLOCK`, `AxSIZE`, `WSTRB`, `RID` / `BID` in
  `{0, 1, 2, 3}`).
- Performance-counter correctness against independent testbench
  measurement.
- ID mapper wrapper behavior (all three policies, read-side reorder).
- Error handling per Section 10 (SLVERR / DECERR on R / B).
- Auto-restart and configuration-latch semantics (Section 7).

**Out of scope for this phase**

- Timing / physical closure.
- Post-synthesis netlist verification.
- Multi-clock CDC (single-clock DUT by design; Section 2).
- Cache-coherency modeling (`AxCACHE` treated as an opaque constant).
- Runtime override of HLS-fixed parameters (MO limits, burst length
  ceilings). These are covered by *configuration sweeps*, not runtime
  stimulus.

## 2. Verification Strategy

- **Methodology**: SystemVerilog + UVM 1.2, xsim simulator, Xilinx AXI
  VIP as the slave BFM. See `README.md` for stack rationale.
- **Style**: Coverage-Driven Constrained Random Verification (CDV /
  CRV). Directed sub-tests cover corner cases that random stimulus is
  unlikely to hit within a reasonable simulation budget.
- **Golden reference**: the `c_model` project. UVM scoreboard invokes
  the same fixed-point function that the C tests use (`fft_dma_fx`),
  eliminating a class of divergence bugs between C-model and
  scoreboard.
- **Vectors**: CSV vector reuse from `c_model/vectors/`. The same CSVs
  used to sign off the C model may be replayed against the DUT for
  cross-validation without regenerating goldens.
- **Configuration sweeps**: HLS-fixed parameters (MO limits, burst
  length ceilings, data / addr / ID widths) are treated as
  configuration axes. Multiple HLS builds populate configuration
  coverage bins.
- **Runtime randomization axes**: `NUM_FFTS`, `RD_ADDR`, `WR_ADDR`,
  `MODE`, `MAPPER_CTRL.policy`, sample data, slave-side ready-delay /
  response-delay / response-code profiles.

## 3. Feature-to-Verification Matrix

Each row is one requirement area. Verified-via column notes the primary
mechanism (`FC` = functional covergroup, `AS` = assertion, `SB` =
scoreboard, `CFG` = configuration sweep, `PC` = performance counter
check). Tests column lists the primary tests that exercise the
requirement (test names dropped the `test_` prefix for brevity). Priority
column: `P0` = must-pass to declare feature done, `P1` = important,
`P2` = nice-to-have.

| # | Spec §     | Feature                                    | Verified via        | Primary Tests                                                          | Priority |
|---|------------|--------------------------------------------|---------------------|------------------------------------------------------------------------|----------|
| F01 | 2.2      | Async reset assert / sync deassert         | AS + directed       | smoke_reset_and_regs, reset_mid_run, reset_post_access                 | P0       |
| F02 | 3.4, 4.2 | Interrupt output driven per `GIE`/`IER`    | FC + directed       | ctrl_irq_ack                                                           | P0       |
| F03 | 4.1      | `ap_ctrl_hs` handshake                     | AS + SB             | smoke_single_activation, sanity_multi_activation, ctrl_ap_start_ignored | P0       |
| F04 | 4.2, 4.3 | GIE / IER / ISR interrupt logic            | AS + directed       | ctrl_irq_ack                                                           | P0       |
| F05 | 4.4, 10  | STATUS sticky flags, `ap_start` auto-clear | AS + directed       | err_slverr_read, err_slverr_write, err_continue_on_error               | P0       |
| F06 | 4.5      | 64-bit address assembled from LO/HI        | SB + FC             | smoke_reset_and_regs, addr_alignment_sweep                             | P0       |
| F07 | 4.6      | `NUM_FFTS` boundary values                 | FC + directed       | num_ffts_boundary                                                      | P0       |
| F08 | 4.7      | `MODE` enum values                         | FC                  | sanity_mode_fft / _ro / _wo, mode_fft_directed                         | P0       |
| F09 | 4.9      | Performance counters (all seven)           | PC + SB             | perf_counters, perf_mo_saturation, stress_random_latency               | P0       |
| F10 | 4.10     | `MAPPER_CTRL` policy: SEQ / RR / RANDOM    | FC + AS             | mapper_policy_rr, mapper_policy_seq, multi_id_reorder                  | P0       |
| F11 | 5.1-5.3  | Sample / beat packing (little-endian, LSB) | SB (data compare)   | sanity_mode_fft, mode_fft_directed, mode_wo_directed                   | P0       |
| F12 | 5.4      | Address alignment (32 B, aligned only)     | Constraint + AS     | addr_alignment_sweep                                                   | P0       |
| F13 | 6.1      | Fixed AXI signals contract compliance      | AS + FC             | axi_burst_lengths, stress_backpressure, stress_random_latency          | P0       |
| F14 | 6.2      | 4 KB boundary auto-split                   | FC + directed       | 4kb_boundary                                                           | P1       |
| F15 | 6.3      | ID mapper: reorder buffer, per-ID order    | AS + SB             | multi_id_reorder, stress_max_mo, stress_random_latency                 | P0       |
| F16 | 7.1      | ap_done / ap_ready / ap_idle timing        | AS + SB             | smoke_single_activation, sanity_multi_activation                       | P0       |
| F17 | 7.2      | Config latch at `ap_ready`                 | Directed            | ctrl_config_latch                                                      | P0       |
| F18 | 7.3      | Done condition per mode                    | AS + SB             | sanity_mode_fft / _ro / _wo, mode_ro_directed, mode_wo_directed        | P0       |
| F19 | 7.4      | `auto_restart` flow                        | Directed            | ctrl_auto_restart                                                      | P1       |
| F20 | 8.1      | MODE_FFT full pipeline                     | SB (data compare)   | sanity_mode_fft, mode_fft_directed, mode_fft_random                    | P0       |
| F21 | 8.2      | MODE_READ_ONLY (no write traffic)          | AS + FC             | sanity_mode_ro, mode_ro_directed                                       | P0       |
| F22 | 8.3      | MODE_WRITE_ONLY (counter pattern)          | SB + FC             | sanity_mode_wo, mode_wo_directed                                       | P0       |
| F23 | 9        | FFT numerical correctness (Q2.14 exact)    | SB vs C reference   | sanity_mode_fft, mode_fft_directed, mode_fft_random                    | P0       |
| F24 | 10.1     | SLVERR / DECERR detection (R and B)        | Directed error inj. | err_slverr_read, err_decerr_read, err_slverr_write, err_decerr_write   | P0       |
| F25 | 10.2     | Continue-on-error (no early abort)         | Directed            | err_continue_on_error                                                  | P0       |
| F26 | 10.3     | ISR bits not cleared by `ap_start`         | Directed            | ctrl_irq_ack, err_slverr_read (secondary check)                        | P0       |
| F27 | 11       | Reset in mid-activation                    | Directed            | reset_mid_run                                                          | P1       |
| F28 | Cross    | HLS config sweep coverage                  | CFG                 | Run α/β suite across each HLS build variant                            | P1       |

## 4. Coverage Model

Naming convention: `cg_<domain>` for covergroups, `cp_<name>` for
coverpoints, `bin_<name>` for individual bins. Cross names are
`cx_<a>_x_<b>`.

### 4.1 Functional Coverage — Covergroups

#### 4.1.1 `cg_config` — sampled on every `ap_ready`

| Coverpoint      | Source            | Bin Name       | Values / Range          | Purpose                                          |
|-----------------|-------------------|----------------|-------------------------|--------------------------------------------------|
| `cp_mode`       | `MODE[1:0]`       | `bin_fft`      | 0                       | MODE_FFT exercised                               |
|                 |                   | `bin_ro`       | 1                       | MODE_READ_ONLY exercised                         |
|                 |                   | `bin_wo`       | 2                       | MODE_WRITE_ONLY exercised                        |
| `cp_num_ffts`   | `NUM_FFTS`        | `bin_1`        | 1                       | Minimum activation size                          |
|                 |                   | `bin_2`        | 2                       | Single-pair boundary                             |
|                 |                   | `bin_small`    | `[3:7]`                 | Small batch, single burst                        |
|                 |                   | `bin_medium`   | `[8:31]`                | Two-burst region                                 |
|                 |                   | `bin_large`    | `[32:127]`              | Multi-burst region                               |
|                 |                   | `bin_huge`     | `[128:1023]`            | Many bursts, likely to cross 4 KB                |
|                 |                   | `bin_massive`  | `[1024:$]`              | Extreme batch                                    |
| `cp_src_align`  | `RD_ADDR mod 4KB` | `bin_32`       | `[0x00:0x1F]`           | 32-byte aligned only                             |
|                 |                   | `bin_64`       | multiples of 64         | 64-byte aligned                                  |
|                 |                   | `bin_128`      | multiples of 128        | Cache-line aligned                               |
|                 |                   | `bin_256`      | multiples of 256        | Wide alignment                                   |
|                 |                   | `bin_page`     | 0                       | 4 KB page start                                  |
| `cp_dst_align`  | `WR_ADDR mod 4KB` | same as `cp_src_align`                    | Same intent for write side              |
| `cp_mapper_pol` | `MAPPER_CTRL[1:0]`| `bin_seq`      | 0                       | SEQUENTIAL policy                                |
|                 |                   | `bin_rr`       | 1                       | ROUND_ROBIN                                      |
|                 |                   | `bin_rand`     | 2                       | RANDOM                                           |
| Cross           | —                 | `cx_mode_x_num_ffts`  | full 3 × 7 grid  | Mode × batch size interaction                    |
| Cross           | —                 | `cx_mode_x_pol`       | full 3 × 3 grid  | Mode × mapper policy                             |
| Cross           | —                 | `cx_num_ffts_x_pol`   | full 7 × 3 grid  | Batch size × mapper policy                       |

#### 4.1.2 `cg_axi_rd` — sampled on every AR handshake at the DUT boundary

| Coverpoint       | Source                 | Bin Name          | Values / Range | Purpose                                             |
|------------------|------------------------|-------------------|----------------|-----------------------------------------------------|
| `cp_arlen`       | `ARLEN`                | `bin_1_beat`      | 0              | Single-beat read                                    |
|                  |                        | `bin_2_4_beats`   | `[1:3]`        | Short burst                                         |
|                  |                        | `bin_5_8_beats`   | `[4:7]`        | Medium burst                                        |
|                  |                        | `bin_9_16_beats`  | `[8:15]`       | Max burst (bounded by HLS pragma)                   |
| `cp_arid`        | `ARID`                 | `bin_id0`         | 0              | Mapper policy = SEQ, or RR / RANDOM emitting 0     |
|                  |                        | `bin_id1`         | 1              | RR / RANDOM emitting 1                              |
|                  |                        | `bin_id2`         | 2              | RR / RANDOM emitting 2                              |
|                  |                        | `bin_id3`         | 3              | RR / RANDOM emitting 3                              |
| `cp_cross_4kb`   | derived                | `bin_no`          | intent doesn't cross | Normal burst                                  |
|                  |                        | `bin_yes`         | intent crosses  | Burst that got auto-split (Section 6.2 of spec)    |
| `cp_burst_gap`   | cycles since prev AR   | `bin_b2b`         | 0              | Back-to-back                                        |
|                  |                        | `bin_1`           | 1              | 1-cycle gap                                         |
|                  |                        | `bin_2_4`         | `[2:4]`        | Small gap                                           |
|                  |                        | `bin_5_15`        | `[5:15]`       | Medium gap                                          |
|                  |                        | `bin_16_plus`     | `[16:$]`       | Long gap                                            |
| `cp_outstanding` | AR count in-flight     | `bin_1`           | 1              | No pipelining                                       |
|                  |                        | `bin_2`           | 2              | Minimal pipeline                                    |
|                  |                        | `bin_3_7`         | `[3:7]`        | Moderate                                            |
|                  |                        | `bin_8_15`        | `[8:15]`       | Near HLS ceiling                                    |
|                  |                        | `bin_max`         | `[16:$]`       | At / above ceiling (depends on build)               |
| Cross            | —                      | `cx_arlen_x_arid` | 4 × 4 grid     | Burst length × ID                                   |
| Cross            | —                      | `cx_arlen_x_outstanding` | 4 × 5 grid | Burst length × outstanding                       |

#### 4.1.3 `cg_axi_wr` — sampled on every AW handshake

Same shape as `cg_axi_rd` with `AWLEN` / `AWID` in place of AR
counterparts, plus:

| Coverpoint            | Source            | Bin Name       | Values / Range | Purpose                             |
|-----------------------|-------------------|----------------|----------------|-------------------------------------|
| `cp_wstrb_all_one`    | `WSTRB` (per beat)| `bin_full`     | `'1`           | Every W beat is full-strobe (contract) |

#### 4.1.4 `cg_axi_resp` — sampled on every B handshake and every RLAST beat

| Coverpoint       | Source        | Bin Name       | Values / Range     | Purpose                                    |
|------------------|---------------|----------------|--------------------|--------------------------------------------|
| `cp_bresp`       | `BRESP`       | `bin_okay`     | `2'b00`            | Normal write completion                     |
|                  |               | `bin_slverr`   | `2'b10`            | Slave error injected                        |
|                  |               | `bin_decerr`   | `2'b11`            | Decode error injected                       |
| `cp_rresp_last`  | `RRESP` at RLAST | `bin_okay`  | `2'b00`            | Normal read completion                      |
|                  |               | `bin_slverr`   | `2'b10`            | Injected read error                         |
|                  |               | `bin_decerr`   | `2'b11`            | Injected decode error                       |
| `cp_lat_rd`      | per-txn cycles (AR→RLAST) | `bin_fast`     | `[1:15]`   | Fast slave                                  |
|                  |               | `bin_medium`   | `[16:63]`          | Moderate                                    |
|                  |               | `bin_slow`     | `[64:255]`         | Slow                                        |
|                  |               | `bin_very_slow`| `[256:1023]`       | Very slow                                   |
|                  |               | `bin_extreme`  | `[1024:$]`         | Stress                                      |
| `cp_lat_wr`      | per-txn cycles (AW→B) | same bins as `cp_lat_rd`  | Same intent for write latency       |

#### 4.1.5 `cg_mapper` — sampled on every AR / AW at the DUT boundary

| Coverpoint       | Source        | Bin Name          | Values / Range          | Purpose                                       |
|------------------|---------------|-------------------|-------------------------|-----------------------------------------------|
| `cp_rr_sequence` | ID transition | `bin_0_to_1`      | prev=0, curr=1          | Verify RR emits 0→1                           |
|                  |               | `bin_1_to_2`      | prev=1, curr=2          | Verify RR emits 1→2                           |
|                  |               | `bin_2_to_3`      | prev=2, curr=3          | Verify RR emits 2→3                           |
|                  |               | `bin_3_to_0`      | prev=3, curr=0          | Verify RR wraps                               |
| `cp_random_hist` | ID value under RANDOM | `bin_id0`,`bin_id1`,`bin_id2`,`bin_id3` | 0, 1, 2, 3 | Balance sanity for LFSR-driven ID     |

#### 4.1.6 `cg_control` — sampled at each `ap_done`

| Coverpoint         | Source              | Bin Name          | Values / Range    | Purpose                                       |
|--------------------|---------------------|-------------------|-------------------|-----------------------------------------------|
| `cp_status`        | `STATUS[1:0]`       | `bin_clean`       | 2'b00             | Activation finished with no error             |
|                    |                     | `bin_rd_err`      | 2'b01             | Read-side error only                          |
|                    |                     | `bin_wr_err`      | 2'b10             | Write-side error only                         |
|                    |                     | `bin_both`        | 2'b11             | Both sides had error                          |
| `cp_perf_mo_rd`    | `MO_MAX[15:0]`      | `bin_1`,`bin_2`,`bin_3_7`,`bin_8_15`,`bin_max` | 1, 2, [3:7], [8:15], [16:$] | Observed peak read MO |
| `cp_perf_mo_wr`    | `MO_MAX[31:16]`     | same bins as `cp_perf_mo_rd`         | Observed peak write MO                        |
| `cp_auto_restart`  | `CTRL.auto_restart` counter | `bin_off` | 0 restarts        | Standard flow                                 |
|                    |                     | `bin_on_1`        | 1 restart         | Single re-launch                              |
|                    |                     | `bin_on_2`        | 2 restarts        | Two re-launches                               |
|                    |                     | `bin_on_3plus`    | 3+ restarts       | Sustained auto-restart                        |

### 4.2 Assertion Coverage

Assertions live in a DUT-bind file. Every assertion carries a
matching `cover property` so that "assertion fired successfully N
times" is measurable.

| Assertion ID       | Domain      | Property (simplified)                                         | Priority |
|--------------------|-------------|---------------------------------------------------------------|----------|
| `a_ar_burst_incr`  | Contract    | `ARVALID |-> ARBURST == 2'b01`                                | P0       |
| `a_aw_burst_incr`  | Contract    | `AWVALID |-> AWBURST == 2'b01`                                | P0       |
| `a_ar_size_32b`    | Contract    | `ARVALID |-> ARSIZE == 3'b101`                                | P0       |
| `a_aw_size_32b`    | Contract    | `AWVALID |-> AWSIZE == 3'b101`                                | P0       |
| `a_ar_cache_fixed` | Contract    | `ARVALID |-> ARCACHE == 4'b0011`                              | P0       |
| `a_aw_cache_fixed` | Contract    | `AWVALID |-> AWCACHE == 4'b0011`                              | P0       |
| `a_ar_prot_fixed`  | Contract    | `ARVALID |-> ARPROT  == 3'b000`                               | P0       |
| `a_aw_prot_fixed`  | Contract    | `AWVALID |-> AWPROT  == 3'b000`                               | P0       |
| `a_qos_zero`       | Contract    | `AWVALID or ARVALID |-> {AWQOS,ARQOS} == 0`                   | P0       |
| `a_lock_zero`      | Contract    | `AWVALID or ARVALID |-> {AWLOCK,ARLOCK} == 0`                 | P0       |
| `a_wstrb_full`     | Contract    | `WVALID |-> WSTRB == '1`                                      | P0       |
| `a_axid_range`     | Contract    | `AWVALID or ARVALID |-> {AWID,ARID} inside {0,1,2,3}`         | P0       |
| `a_ap_start_ignored` | ap_ctrl_hs| `!ap_idle && write(ap_start,1) |-> no state change`           | P0       |
| `a_ap_ready_pulse` | ap_ctrl_hs  | `$rose(ap_ready) |-> ##1 !ap_ready`                           | P0       |
| `a_ap_done_sticky` | ap_ctrl_hs  | `ap_done |-> ap_done throughout(!CTRL_read)`                  | P0       |
| `a_status_rd_err`  | STATUS      | `RVALID && RRESP != OKAY |-> ##1 STATUS.rd_error`             | P0       |
| `a_status_wr_err`  | STATUS      | `BVALID && BRESP != OKAY |-> ##1 STATUS.wr_error`             | P0       |
| `a_status_clear`   | STATUS      | `$rose(ap_start) |-> ##1 STATUS == '0`                        | P0       |
| `a_isr_not_cleared`| ISR         | `$rose(ap_start) |-> ISR unchanged`                           | P0       |
| `a_isr_rd_rise`    | ISR         | `$rose(STATUS.rd_error) |-> $rose(ISR.rd_err_int)`            | P0       |
| `a_isr_wr_rise`    | ISR         | `$rose(STATUS.wr_error) |-> $rose(ISR.wr_err_int)`            | P0       |
| `a_align_rd`       | Alignment   | `$rose(ap_start) |-> RD_ADDR[4:0] == 0`                       | P0       |
| `a_align_wr`       | Alignment   | `$rose(ap_start) |-> WR_ADDR[4:0] == 0`                       | P0       |
| `a_no_4kb_cross`   | Protocol    | Emitted burst's byte range does not cross 4 KB                | P0       |
| `a_wlast_matches_awlen` | Protocol | `WLAST` count == `AWLEN` for each transaction                | P0       |
| `a_rlast_matches_arlen` | Protocol | `RLAST` count == `ARLEN` for each transaction                | P0       |

*(Xilinx AXI VIP supplies additional protocol assertions beyond this
DUT-specific list.)*

### 4.3 Configuration Coverage

Populated across multiple HLS builds. Each build embeds its
configuration in a `build_info_pkg` compiled into the testbench.

| Coverpoint         | Values          | Source (HLS pragma)                | Purpose                            |
|--------------------|-----------------|-------------------------------------|------------------------------------|
| `cp_hls_mo_rd`     | 4, 8, 16, 32    | `num_read_outstanding`              | Read MO ceiling per build          |
| `cp_hls_mo_wr`     | 4, 8, 16, 32    | `num_write_outstanding`             | Write MO ceiling per build         |
| `cp_hls_burst_max` | 16, 32, 64, 128 | `max_read/write_burst_length`       | Burst length ceiling               |
| `cp_hls_pipeline`  | II=1, II=2      | `#pragma HLS PIPELINE II=...`       | Compute pipeline depth (if swept)  |
| Cross              | 4 × 4 grid      | `cx_mo_rd_x_burst_max`              | MO / burst interaction             |
| Cross              | 4 × 4 grid      | `cx_mo_wr_x_burst_max`              | MO / burst interaction             |

### 4.4 Code / Toggle Coverage

- Collected automatically by xsim during any simulation run.
- Targets:
  - Line coverage on `rtl/` (hand-written wrappers): 100% (waivers
    per module).
  - Branch coverage on `rtl/`: 100% (waivers per module).
  - Toggle coverage: reported but not gated for sign-off.
  - HLS-generated RTL: reported but not gated (opaque; verified
    through functional matching).

## 5. Test Plan

### 5.1 Test Categories

| Cat | Name         | Purpose                                                        |
|-----|--------------|----------------------------------------------------------------|
| C0  | Smoke        | Reset + register access only; boot smoke                       |
| C1  | Sanity       | Single happy-path activation per mode                          |
| C2  | Functional   | Data-path correctness, per-mode, alignment / boundary sweeps   |
| C3  | Protocol     | AXI protocol, ID mapping, 4 KB boundary                        |
| C4  | Perf         | Performance counter accuracy                                   |
| C5  | Stress       | Backpressure, latency, max MO, long batches                    |
| C6  | Error        | SLVERR / DECERR injection, error propagation                   |
| C7  | Control      | Config latching, auto-restart, IRQ handling                    |
| C8  | Reset        | Reset in mid-activation, post-reset access                     |
| C9  | Full Random  | Large-scale CRV runs                                           |

### 5.2 Test List

| Test                              | Cat | Priority | Notes                                                        |
|-----------------------------------|-----|----------|--------------------------------------------------------------|
| `test_smoke_reset_and_regs`       | C0  | P0       | Reset then read all registers; verify reset values           |
| `test_smoke_single_activation`    | C0  | P0       | Minimal MODE_FFT `NUM_FFTS=1` activation, done observed      |
| `test_sanity_mode_fft`            | C1  | P0       | Single FFT (impulse), bit-exact data check                   |
| `test_sanity_mode_ro`             | C1  | P0       | Single READ_ONLY, no write traffic, dst untouched            |
| `test_sanity_mode_wo`             | C1  | P0       | Single WRITE_ONLY, counter pattern in dst                    |
| `test_sanity_multi_activation`    | C1  | P0       | Two back-to-back activations with different configs          |
| `test_mode_fft_directed`          | C2  | P0       | MODE_FFT scenarios from `fft_dma_vectors.csv`                |
| `test_mode_fft_random`            | C2  | P0       | 200+ random cases, `num_ffts ∈ [1, 1024]`                    |
| `test_mode_ro_directed`           | C2  | P0       | READ_ONLY, `ARVALID` observed, `AWVALID` == 0 asserted       |
| `test_mode_wo_directed`           | C2  | P0       | WRITE_ONLY, counter pattern verified in dst memory           |
| `test_addr_alignment_sweep`       | C2  | P0       | src / dst addresses swept across aligned bins                |
| `test_num_ffts_boundary`          | C2  | P0       | `NUM_FFTS` = 1, 2, 63, 64, 65 (around burst-fill boundaries) |
| `test_axi_burst_lengths`          | C3  | P0       | Constrained-random `NUM_FFTS` to sweep observed `AxLEN`      |
| `test_4kb_boundary`               | C3  | P1       | src / dst positioned so bursts straddle 4 KB                 |
| `test_multi_id_reorder`           | C3  | P0       | `MAPPER_CTRL = RANDOM`, slave replies out-of-order           |
| `test_mapper_policy_rr`           | C3  | P0       | Verify ROUND_ROBIN sequence                                  |
| `test_mapper_policy_seq`          | C3  | P1       | Verify all IDs == 0 in SEQUENTIAL                            |
| `test_perf_counters`              | C4  | P0       | Cross-check DUT counters vs TB scoreboard                    |
| `test_perf_mo_saturation`         | C4  | P1       | Slave stalls to push MO to HLS limit                         |
| `test_stress_backpressure`        | C5  | P0       | Random slave READY stalls, verify no deadlock                |
| `test_stress_random_latency`      | C5  | P0       | Per-transaction random R/B response latency; check functional, perf, no deadlock |
| `test_stress_max_mo`              | C5  | P1       | Sustain max outstanding for long window                      |
| `test_stress_long_batch`          | C5  | P1       | `NUM_FFTS` in the thousands                                  |
| `test_err_slverr_read`            | C6  | P0       | Inject SLVERR on one R response, verify sticky flag and IRQ  |
| `test_err_decerr_read`            | C6  | P0       | Same with DECERR                                             |
| `test_err_slverr_write`           | C6  | P0       | Inject SLVERR on B response                                  |
| `test_err_decerr_write`           | C6  | P0       | Same with DECERR                                             |
| `test_err_continue_on_error`      | C6  | P0       | After error, remaining transactions still complete           |
| `test_ctrl_config_latch`          | C7  | P0       | Change `RD_ADDR` mid-activation, verify no effect this run   |
| `test_ctrl_auto_restart`          | C7  | P1       | Enable auto_restart, count restarts                          |
| `test_ctrl_irq_ack`               | C7  | P0       | IRQ assert, ack via ISR W1TC, drop verified                  |
| `test_ctrl_ap_start_ignored`      | C7  | P1       | Write `ap_start=1` while running, verify ignored             |
| `test_reset_mid_run`              | C8  | P1       | Assert reset during activation, verify clean state           |
| `test_reset_post_access`          | C8  | P0       | Access all registers immediately after reset                 |
| `test_full_random_1k`             | C9  | P1       | 1000 random activations, all modes                           |
| `test_full_random_soak`           | C9  | P2       | Long-running randomized run, all modes                       |

### 5.3 Stages (α / β)

The test list is grouped into two release stages. There is no separate
release-candidate stage; anything not required for α belongs to β.

**α (Alpha) — "Does it work? Does it fail correctly?"** (18 tests)

Basic functional coverage plus full error-handling coverage. α closes
when every test listed here passes.

- All C0 Smoke (2)
- All C1 Sanity (4)
- All C2 Functional (6)
- C3 `test_axi_burst_lengths`, `test_mapper_policy_rr`, `test_multi_id_reorder`
- All C6 Error (5)

**β (Beta) — "Is it robust and complete?"** (18 tests)

Everything not in α. Includes performance, stress, control edge cases,
reset scenarios, and full-random regression. β closes when every β test
passes and the coverage targets in Section 8 are met.

- C3 `test_4kb_boundary`, `test_mapper_policy_seq`
- All C4 Perf (2)
- All C5 Stress (4)
- All C7 Control (4)
- All C8 Reset (2)
- All C9 Full Random (2)

### 5.4 Detailed Test Scenarios

Each scenario lists the requirement IDs it covers, the exact stimulus
the sequence must produce, the checks the scoreboard / assertions
apply, and the covergroups (Section 4.1) that should be hit. Test
`_` prefix `test_` is dropped inside subsection headers for brevity.

#### C0 — Smoke

**`smoke_reset_and_regs`** — Covers F01, F06
- **Stimulus**: Assert `rst_n = 0` for ≥ 16 clock cycles, then
  deassert synchronously. On the first cycle after `rst_n` returns
  high, drive AXI4-Lite reads across every defined offset in
  `[0x00, 0x50]`.
- **Checks**: Each register returns its reset value from Section 4
  of the RTL spec. Assertion `a_align_rd / a_align_wr` are silent
  (no `ap_start` yet). All DUT-driven `*VALID` signals stayed low
  while `rst_n = 0`.
- **Coverage**: none directly (feeds `cg_control` only after later
  activations).

**`smoke_single_activation`** — Covers F03, F16, F18
- **Stimulus**: Program `RD_ADDR = 0x100`, `WR_ADDR = 0x1000`,
  `NUM_FFTS = 1`, `MODE = FFT`. Backing memory pre-filled with zero
  samples. Write `CTRL.ap_start = 1`.
- **Checks**: `ap_ready` observed as a single-cycle pulse.
  `ap_done` observed within a 10 000-cycle watchdog. Assertions
  `a_ap_ready_pulse` and `a_ap_done_sticky` fire without failure.
  Data value at dst is not compared here.
- **Coverage**: `cg_config` bin `bin_fft × bin_1`; `cg_control`
  bin `bin_clean × bin_off`.

#### C1 — Sanity

**`sanity_mode_fft`** — Covers F20, F23
- **Stimulus**: `NUM_FFTS = 1`, `MODE = FFT`, src = impulse
  `[1.0+0j, 0, 0, ...]` in Q2.14.
- **Checks**: dst region matches C-reference output bit-exact
  (`fft_dma_fx` on the same input). Scoreboard reports no diff.
- **Coverage**: `cg_config bin_fft × bin_1`, `cg_axi_rd bin_1_beat
  (or bin_2_4_beats)`, `cg_control bin_clean`.

**`sanity_mode_ro`** — Covers F21
- **Stimulus**: `NUM_FFTS = 1`, `MODE = READ_ONLY`, src filled with
  random data.
- **Checks**: dst region byte-compare identical to pre-activation
  snapshot. Assertion `AWVALID == 0` throughout. Read master emits
  exactly `NUM_FFTS × 2` beats total.
- **Coverage**: `cg_config bin_ro`, `cg_axi_rd` beats observed,
  `cg_axi_wr` not sampled.

**`sanity_mode_wo`** — Covers F22
- **Stimulus**: `NUM_FFTS = 1`, `MODE = WRITE_ONLY`, dst region
  pre-cleared to zero.
- **Checks**: For each sample index `n ∈ [0, 16)` at
  `WR_ADDR + 4·n`, the 32-bit little-endian value equals `n`.
  Assertion `ARVALID == 0` throughout.
- **Coverage**: `cg_config bin_wo`, `cg_axi_wr` sampled, `cg_axi_rd`
  not sampled.

**`sanity_multi_activation`** — Covers F03, F16
- **Stimulus**: Two back-to-back activations. Activation 1:
  `NUM_FFTS = 1`, `RD_ADDR = 0x100`, `WR_ADDR = 0x1000`, `MODE=FFT`.
  Between activations, wait for `ap_done`, read `CTRL` to clear it,
  reprogram `RD_ADDR = 0x800`, `WR_ADDR = 0x2400`.
- **Checks**: Both activations complete independently. Each result
  matches its own C-reference output.
- **Coverage**: `cg_config` sampled twice; `cg_control` sampled
  twice with `bin_off` restart.

#### C2 — Functional

**`mode_fft_directed`** — Covers F08, F11, F20, F23
- **Stimulus**: Replay every MODE_FFT scenario in
  `c_model/vectors/fft_dma_vectors.csv` (10 scenarios).
- **Checks**: Per-scenario dst-region compare against CSV expected.
- **Coverage**: `cg_config bin_fft` combined with the `num_ffts`
  distribution the CSV exercises.

**`mode_fft_random`** — Covers F08, F20, F23
- **Stimulus**: 200 randomized activations with
  `NUM_FFTS ∈ [1, 1024]` (log-uniform), random 32 B-aligned
  addresses, random Q2.14 sample data, `MODE = FFT`.
- **Checks**: DPI to `fft_dma_fx_dpi` for expected memory contents,
  byte compare on dst region.
- **Coverage**: `cg_config` all `bin_fft × cp_num_ffts` bins should
  eventually be hit; `cg_axi_rd`, `cg_axi_wr` bin fill.

**`mode_ro_directed`** — Covers F18, F21
- **Stimulus**: `MODE = READ_ONLY` with `NUM_FFTS ∈ {1, 5, 50}`
  and three different src addresses.
- **Checks**: For each, write master idle assertion holds and dst
  snapshot is preserved. `RD_BEAT_CNT` at `ap_done` equals
  `NUM_FFTS × 2`.
- **Coverage**: `cg_config bin_ro` × three `num_ffts` bins.

**`mode_wo_directed`** — Covers F18, F22
- **Stimulus**: `MODE = WRITE_ONLY` with `NUM_FFTS ∈ {1, 5, 50, 500}`
  and different dst addresses.
- **Checks**: For each, counter pattern verified. Read master idle
  assertion. `WR_BEAT_CNT` = `NUM_FFTS × 2`.
- **Coverage**: `cg_config bin_wo` × four `num_ffts` bins.

**`addr_alignment_sweep`** — Covers F06, F11, F12
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 4`. Sweep `RD_ADDR` and
  `WR_ADDR` across every alignment bin defined in `cp_src_align` /
  `cp_dst_align` (32 B, 64 B, 128 B, 256 B, page start).
- **Checks**: Data correct for each alignment.
- **Coverage**: `cg_config cp_src_align`, `cp_dst_align` all bins
  covered.

**`num_ffts_boundary`** — Covers F07
- **Stimulus**: Directed sweep of `NUM_FFTS` values 1, 2, 63, 64,
  65, 127, 128, 129 with random data.
- **Checks**: Data correct for each `NUM_FFTS`. Assertion on beat
  counts.
- **Coverage**: `cg_config cp_num_ffts` bins `bin_1`, `bin_2`,
  `bin_medium`, `bin_large`.

#### C3 — Protocol

**`axi_burst_lengths`** — Covers F13
- **Stimulus**: Constrained-random `NUM_FFTS` biased to exercise the
  full `cp_arlen` / `cp_awlen` bin set.
- **Checks**: Fixed AXI-signal assertions never fail; every observed
  `AxBURST` = INCR, `AxSIZE` = 32 B, etc.
- **Coverage**: `cg_axi_rd cp_arlen`, `cg_axi_wr cp_awlen` all bins
  filled; assertion coverage on `a_ar_burst_incr` /
  `a_aw_burst_incr`.

**`4kb_boundary`** — Covers F14
- **Stimulus**: `NUM_FFTS = 4`. Place `RD_ADDR = 0x0FE0` and
  `WR_ADDR = 0x1FE0` so the first burst intent (64 B) would cross
  4 KB. Also repeat with `NUM_FFTS = 8` and larger intent bursts.
- **Checks**: No assertion `a_no_4kb_cross` failure. Observed AR /
  AW split into two shorter bursts. `cp_cross_4kb bin_yes` hit.
- **Coverage**: `cg_axi_rd cp_cross_4kb bin_yes`, likewise `cg_axi_wr`.

**`multi_id_reorder`** — Covers F10, F15
- **Stimulus**: `MAPPER_CTRL.policy = RANDOM`. Slave BFM interleaves
  R responses across IDs out of request order.
- **Checks**: Data at dst matches C reference (i.e., mapper reorder
  buffer correctly reassembled). No RLAST out-of-order for a given
  ID (per AXI rule).
- **Coverage**: `cg_axi_rd cp_arid` all four bins; `cg_axi_resp`.

**`mapper_policy_rr`** — Covers F10
- **Stimulus**: `MAPPER_CTRL.policy = ROUND_ROBIN`. Multiple
  activations with `NUM_FFTS` large enough to emit ≥ 8 AR / AW.
- **Checks**: Observed `AxID` sequence is 0, 1, 2, 3, 0, 1, ...
  Cross covergroup `cg_mapper cp_rr_sequence` all four transitions
  hit.
- **Coverage**: `cg_mapper cp_rr_sequence`.

**`mapper_policy_seq`** — Covers F10
- **Stimulus**: `MAPPER_CTRL.policy = SEQUENTIAL`. Multiple
  activations.
- **Checks**: Every observed `AxID == 0`. Assertion may enforce.
- **Coverage**: `cg_axi_rd cp_arid bin_id0` (and only that bin).

#### C4 — Perf

**`perf_counters`** — Covers F09
- **Stimulus**: Multiple activations with varying `NUM_FFTS`. Slave
  BFM injects a reproducible latency profile (fixed seed).
- **Checks**: After `ap_done`, read `CYCLE_CNT`, `RD_BEAT_CNT`,
  `WR_BEAT_CNT`, `RD_LAT_ACC`, `WR_LAT_ACC`, `RD_TXN_CNT`,
  `WR_TXN_CNT`, `MO_MAX`. TB scoreboard has been counting the same
  events independently at the DUT boundary; values must match.
- **Coverage**: `cg_control cp_perf_mo_rd/wr` various bins.

**`perf_mo_saturation`** — Covers F09
- **Stimulus**: Slave holds R responses so outstanding read count
  rises to the HLS build's `num_read_outstanding` ceiling.
- **Checks**: `MO_MAX[15:0]` equals the build's declared ceiling.
- **Coverage**: `cg_control cp_perf_mo_rd bin_max`.

#### C5 — Stress

**`stress_backpressure`** — Covers F13
- **Stimulus**: Random per-cycle `RREADY` / `AWREADY` / `WREADY` /
  `BREADY` stalls in the slave BFM. Multiple activations, mixed
  modes.
- **Checks**: All activations reach `ap_done` within watchdog. Data
  correct. No AXI assertion failure.
- **Coverage**: `cg_axi_rd cp_burst_gap`, `cg_axi_wr cp_burst_gap`
  bins fill (varied gaps observed).

**`stress_random_latency`** — Covers F09, F13, F15
- **Stimulus**: Slave BFM injects per-transaction response latency
  drawn from a wide distribution (e.g., uniform 1–200 cycles) on
  both R and B channels. Mapper policy = RANDOM. Many activations.
- **Checks**: (a) Data correct against C reference. (b) TB-computed
  average latency ≈ `RD_LAT_ACC / RD_TXN_CNT` within tolerance.
  (c) `MO_MAX` varies naturally. (d) No deadlock.
- **Coverage**: `cg_axi_resp cp_lat_rd`, `cp_lat_wr` all bins;
  `cg_control cp_perf_mo_*` variety.

**`stress_max_mo`** — Covers F15
- **Stimulus**: Slave sustains max-MO condition (fully-populated
  outstanding pool) for a long window (≥ 5 000 cycles), then
  releases.
- **Checks**: No reorder buffer overflow. No transaction loss
  (BEAT / TXN counters consistent). Data correct.
- **Coverage**: `cg_control cp_perf_mo_rd bin_max`, `cp_perf_mo_wr
  bin_max` sustained.

**`stress_long_batch`** — Covers F07
- **Stimulus**: `NUM_FFTS ≥ 5000`, single activation.
- **Checks**: Activation completes. Perf counters saturate as
  expected (e.g., `CYCLE_CNT` may hit saturation for extreme
  values). Spot-check dst data (10 random sample positions).
- **Coverage**: `cg_config cp_num_ffts bin_massive`.

#### C6 — Error

**`err_slverr_read`** — Covers F05, F24, F25
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 10`. Slave BFM injects
  `RRESP = SLVERR` on the 5th R response.
- **Checks**: `STATUS.rd_error = 1` (sticky) after activation.
  `ISR.rd_err_int` set on rising edge. `irq_o` asserts if
  `GIE = 1 && IER.rd_err_int = 1`. All remaining transactions
  still issued and completed. `ap_done` normal.
- **Coverage**: `cg_axi_resp cp_rresp_last bin_slverr`;
  `cg_control cp_status bin_rd_err`.

**`err_decerr_read`** — Covers F24
- **Stimulus**: Same as above with `DECERR`.
- **Checks**: Same behavior. `STATUS.rd_error = 1`.
- **Coverage**: `cg_axi_resp cp_rresp_last bin_decerr`.

**`err_slverr_write`** — Covers F05, F24, F25
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 10`. Slave injects
  `BRESP = SLVERR` on the 5th B response.
- **Checks**: `STATUS.wr_error = 1`. `ISR.wr_err_int` set.
  Continue-on-error verified.
- **Coverage**: `cg_axi_resp cp_bresp bin_slverr`;
  `cg_control cp_status bin_wr_err`.

**`err_decerr_write`** — Covers F24
- **Stimulus**: Same with `DECERR`.
- **Checks**: `STATUS.wr_error = 1`.
- **Coverage**: `cg_axi_resp cp_bresp bin_decerr`.

**`err_continue_on_error`** — Covers F25
- **Stimulus**: Inject error in the middle of a batch, log all
  observed AR / AW / B / R events.
- **Checks**: Total observed beat count and TXN count match the
  pre-activation expectation (no early abort). `RD/WR_BEAT_CNT`
  and `RD/WR_TXN_CNT` register values equal TB-observed counts,
  confirming the spec's "erroneous transactions still count" rule.
- **Coverage**: `cg_control cp_status` filled (any error bin).

#### C7 — Control

**`ctrl_config_latch`** — Covers F17
- **Stimulus**: Start activation A with `RD_ADDR = A`. During the
  run, write `RD_ADDR = A'`. Wait for `ap_done`. Immediately
  `ap_start` again without further reprogramming.
- **Checks**: Activation A used address `A` end-to-end. Activation
  B used address `A'`. Neither activation's data crossed the other
  region.
- **Coverage**: sanity, not new bins.

**`ctrl_auto_restart`** — Covers F19
- **Stimulus**: Set `CTRL.auto_restart = 1`, program small
  activation, `ap_start = 1`. After the third restart, clear
  `auto_restart` before the next `ap_done`.
- **Checks**: Exactly four activations occurred (initial +
  three restarts). DUT returns to idle after the fourth.
- **Coverage**: `cg_control cp_auto_restart bin_on_3plus`.

**`ctrl_irq_ack`** — Covers F02, F04, F26
- **Stimulus**: Program `GIE = 1`, `IER.ap_done_int = 1`. Start
  activation. On `irq_o = 1`, read `CTRL` (clears `ap_done`) and
  write 1 to `ISR.ap_done_int` (clears the interrupt).
- **Checks**: `irq_o` deasserts after `ISR` clear. Reading `CTRL`
  alone does NOT deassert `irq_o`.
- **Coverage**: assertion `a_isr_not_cleared` fires.

**`ctrl_ap_start_ignored`** — Covers F03
- **Stimulus**: Mid-activation (`ap_idle = 0`), write `ap_start = 1`.
- **Checks**: No effect. Only one `ap_ready` pulse observed for the
  original activation. Assertion `a_ap_start_ignored` fires.
- **Coverage**: assertion coverage on `a_ap_start_ignored`.

#### C8 — Reset

**`reset_mid_run`** — Covers F27
- **Stimulus**: Start an activation with `NUM_FFTS = 20`. Assert
  reset halfway through (e.g., after `NUM_FFTS/2` completed).
- **Checks**: DUT immediately returns to `ap_idle = 1`. All
  registers back to reset values after deassert. A subsequent
  activation completes cleanly.
- **Coverage**: reset behavior assertion coverage.

**`reset_post_access`** — Covers F01, F06
- **Stimulus**: Assert reset ≥ 16 cycles → deassert. On the next
  cycle after deassert, drive AXI-Lite writes to every RW register,
  then reads to verify.
- **Checks**: All writes accepted, all reads return the written
  value. No AXI-Lite protocol violation.
- **Coverage**: sanity.

#### C9 — Full Random

**`full_random_1k`** — Covers F20, F21, F22, F23
- **Stimulus**: 1 000 randomized activations across all modes,
  address ranges, mapper policies, and slave-timing profiles.
- **Checks**: DPI-based golden for every activation. Assertions
  clean. Scoreboard reports zero mismatches.
- **Coverage**: broad fill across all covergroups.

**`full_random_soak`** — Covers all
- **Stimulus**: 100 000+ activations, extended random space.
  Intended for long-running discovery.
- **Checks**: Same as `full_random_1k`, extended horizon. Failing
  seeds are archived (see Section 9 open questions).
- **Coverage**: closes remaining gaps not hit by `full_random_1k`.

## 6. Golden Reference and Scoreboard

### 6.1 Reference Selection

- The C reference model (`c_model/fft_dma.c`) is the sole golden.
- Divergence between DUT output and the reference is a P0 bug
  regardless of magnitude. **The reference IS the tolerance**; there
  is no fuzz margin at the DUT / reference comparison boundary.

### 6.2 DPI-C Linkage (Planned)

The UVM scoreboard invokes the same fixed-point implementation the
C tests use via DPI-C. Planned prototypes:

```systemverilog
// tb/dpi/gvp_dpi.svh
import "DPI-C" function void fft_dma_fx_dpi(
    input  byte unsigned  mem_bytes[],       // backing memory image
    input  longint unsigned src_addr,
    input  longint unsigned dst_addr,
    input  int unsigned   num_ffts,
    input  int            mode                // 0=FFT, 1=READ_ONLY, 2=WRITE_ONLY
);
```

The C side exposes:

```c
// c_model/fft_dma_dpi.c (thin wrapper around fft_dma_fx)
void fft_dma_fx_dpi(svOpenArrayHandle mem, uint64_t src, uint64_t dst,
                    uint32_t num_ffts, int mode);
```

Comparison is byte-exact on the destination region for `MODE_FFT`
and `MODE_WRITE_ONLY`. For `MODE_READ_ONLY` the check is that the
destination region is untouched relative to its pre-activation
contents.

### 6.3 CSV Replay Path

The same CSV vector files that sign off the C model may be replayed
against the DUT so that no separate goldens are produced for RTL
sign-off.

- Utility: `tb/utils/csv_replay.sv` (planned) — a UVM sequence that
  parses `c_model/vectors/*.csv`, initializes the slave BFM's
  backing memory from `in_re / in_im`, drives one activation per
  case via the AXI-Lite agent, and pushes the expected output to the
  scoreboard.
- Both curated (`fft_dma_vectors.csv`) and random regression files
  (`fft_dma_random_*.csv`) are replayable.
- Seed used to generate a random file is embedded in the file header
  comment; failures reference the CSV path so the exact case is
  reproducible.

### 6.4 Failure Semantics

- Byte mismatch → scoreboard reports (activation id, sample index,
  got, expected) and aborts the test.
- CSV replay records the offending row for post-mortem.
- DPI failures (exception on the C side, allocation failures) are
  routed through `uvm_fatal`.

## 7. Sign-off Criteria

Two stages, per Section 5.3.

**α sign-off**

- Every α test passes (18 tests).
- Assertions in the associated feature areas (functional + error)
  have no unexpected failures.
- Scoreboard reports zero mismatches on all α tests.
- α is the "does it work, does it fail correctly?" gate. Coverage
  targets are not enforced at α closure.

**β sign-off (project sign-off)**

- Every α + β test passes across the full HLS config sweep.
- Weighted functional coverage ≥ 95% overall (per-covergroup
  waivers documented inline in this file).
- Line + branch coverage on `rtl/` ≥ 100% (waivers documented per
  module).

## 8. Traceability

Requirements are traced via the ID column of Section 3 (`F01`, `F02`,
...). Every test in Section 5.2 lists — in its file header comment —
the requirement IDs it covers. Coverage reports emit the requirement
IDs alongside covergroup names so gaps are visible per requirement.

Traceability format inside a test file:

```systemverilog
// Covers: F03, F16, F18 (ap_ctrl_hs handshake and done semantics)
```

## 9. Open Questions

- Whether to run C-model DPI-C linkage as bit-accurate scoreboard
  vs. re-computing goldens in SV. Bit-accurate is preferable but
  needs DPI plumbing; may defer past α.
- Whether HLS config sweep is best driven by a per-config testbench
  build or a single testbench with a compile-time / parameter-driven
  config switch. Deferred until first HLS build is available.
- `NUM_FFTS = 0`: spec marks the behavior undefined. Decide whether
  to add a directed test that pins the DUT's actual behavior (e.g.,
  early `ap_done` with all counters at 0) or to keep this outside
  the covered surface.
- `FFT_CFG` (0x28) is reserved / no-op. Decide whether to add a
  short test that writes the register and reads it back unchanged,
  or to rely on the reset-and-regs smoke to cover it implicitly.
- Failing-seed archiving policy (where to store CSVs that reproduce
  a bug — repository issue, side directory, ignored file?). Defer
  until the first real regression run produces a failure.
- Regression automation mechanism (git hook / CI / cron) is not
  addressed in this document. Deferred until the RTL / TB code
  base is large enough that manual invocation becomes a bottleneck.

## Change Log

- 2026-08-20: Initial version. Written against RTL spec r1
  (skeleton complete). Feature matrix and test list will iterate as
  UVM components come online.
- 2026-08-20: Split Smoke and Sanity, added `test_stress_random_latency`,
  renamed regression tests to `test_full_random_*`, replaced 4-milestone
  ordering with 2-stage α / β structure (errors folded into α).
- 2026-08-20: Removed Section 7 (Regression Strategy) as automation is
  premature. Added Primary Tests column to Feature Matrix, expanded
  Golden Reference section with DPI-C prototype and CSV replay
  details, added open questions on `NUM_FFTS=0`, `FFT_CFG` verify,
  seed archiving, and automation.
- 2026-08-20: Rewrote Section 4 as tables (covergroup → coverpoint →
  named bins → purpose). Added Section 5.4 with per-test detailed
  scenarios (stimulus, checks, coverage points) for all 36 tests.
