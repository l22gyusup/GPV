# GVP RTL Specification

Specification for the DUT (FFT + DMA block) of the Gyusup Verification
Platform. This document is filled in section-by-section as design decisions are
made. Sections marked `TBD` are not yet decided.

---

## 1. Top-Level Overview

The DUT is an FFT + DMA block generated primarily by Xilinx Vitis HLS,
wrapped by a small amount of hand-written SystemVerilog RTL to add
features that HLS cannot produce on its own (multi-ID AXI traffic,
performance monitoring). The block is controlled by a host through an
AXI4-Lite slave interface and moves data through two AXI4 master
interfaces, one for reads and one for writes.

**Function.** On each activation, the DUT:

1. Reads `NUM_FFTS * 16` complex samples from external memory starting
   at `RD_ADDR` via the read master.
2. Performs one 16-point radix-2 forward FFT per 16-sample chunk, in
   Q2.14 fixed-point arithmetic with block-scaling `>> 1` per stage.
3. Writes the results to external memory starting at `WR_ADDR` via
   the write master.

Read-only and write-only modes are also provided for isolating
verification of a single master (Section 8).

**External interfaces.** A single clock / active-low reset pair
(Section 2), an AXI4-Lite slave for configuration and status
(Section 3.1), two AXI4 masters for data (Sections 3.2 and 3.3), and
a level-sensitive interrupt output (Section 3.4).

**Hierarchy.** The DUT top wrapper contains:

- The HLS core (`fft_dma`), which emits AXI transactions with
  `ARID = 0` / `AWID = 0`.
- Two hand-written ID mapper wrappers (one per master) that re-label
  AXI IDs according to the `MAPPER_CTRL` policy and, on the read
  side, reorder responses back into request order for the HLS core.
- Two hand-written performance monitors sitting between the mappers
  and the DUT boundary, feeding the `PERF_CNT` register block.

**Configuration model.** All configuration is via AXI4-Lite registers.
Values are latched at `ap_ready`; mid-activation writes affect only
subsequent activations (Section 7.2).

**Verification stance.** Certain AXI signals (`AxBURST`, `AxCACHE`,
`AxPROT`, `AxQOS`, `AxLOCK`, `AxSIZE`, `WSTRB`) are fixed by design.
The verification plan treats these as contract-compliance items
(assertions + coverage), while the axes of true stimulus variation
are `NUM_FFTS`, `RD_ADDR` / `WR_ADDR`, `MODE`, `MAPPER_CTRL.policy`,
sample data, and slave-side timing.

## 2. Clock and Reset

### 2.1 Clock Domain

- Single clock domain across the entire DUT.
- All interfaces (AXI4-Lite slave, AXI4 read master, AXI4 write master) and
  the internal FFT compute share the same clock.
- Signal name: `clk`.
- Rationale: HLS-generated blocks are typically synthesized to a single clock
  domain; keeping the top level single-domain removes CDC design and
  verification burden. Async FIFO wrappers (if ever needed) belong to the
  integrating SoC, not to the DUT.

### 2.2 Reset

- Signal name: `rst_n`.
- Polarity: active-low.
- Style: asynchronous assertion, synchronous deassertion (Xilinx AXI
  convention).
- Minimum assertion: 16 clock cycles (per AXI recommendation).
- All registers, outstanding transaction state, and FSMs must be cleared
  during reset. No outputs may be driven X after reset deassertion.

## 3. Interface Summary

The DUT exposes three AXI interfaces plus a single interrupt output. All
interfaces share the single `clk` / `rst_n` pair defined in Section 2.

| Interface        | Type            | Signal Prefix         | Notes                       |
|------------------|-----------------|-----------------------|-----------------------------|
| Control          | AXI4-Lite slave | `s_axi_lite_`         | Register access             |
| Data read        | AXI4 master     | `m_axi_gmem_rd_`      | AR / R channels only        |
| Data write       | AXI4 master     | `m_axi_gmem_wr_`      | AW / W / B channels only    |
| Interrupt        | Single wire     | `irq_o`               | Level-sensitive, active-high|

### 3.1 AXI4-Lite Slave (Control)

- Data width: 32 bits.
- Address width: 12 bits (4 KB register space, room for future expansion).
- Standard AXI4-Lite channels: AW, W, B, AR, R. No sideband signals.
- Signal prefix: `s_axi_lite_`.
- Rationale: 32-bit data is the AXI4-Lite convention and the HLS default;
  12-bit address matches Xilinx IP conventions and leaves headroom.

### 3.2 AXI4 Master #1 (Read)

- Data width: 256 bits.
- Address width: 64 bits.
- ID width: 2 bits (IDs 0 through 3).
- Channels: AR, R (read-only master; AW / W / B are not present).
- Sideband signals (`ARUSER`, `RUSER`, etc.): not used.
- Signal prefix: `m_axi_gmem_rd_`.

### 3.3 AXI4 Master #2 (Write)

- Data width: 256 bits.
- Address width: 64 bits.
- ID width: 2 bits (IDs 0 through 3).
- Channels: AW, W, B (write-only master; AR / R are not present).
- Sideband signals (`AWUSER`, `WUSER`, `BUSER`): not used.
- Signal prefix: `m_axi_gmem_wr_`.

### 3.4 Interrupt / Status Outputs

- Single interrupt line `irq_o`.
- Polarity: active-high, level-sensitive (Xilinx AXI IP convention).
- Asserts when the current activation reaches the `done` state and remains
  asserted until the `done` flag is cleared through the AXI-Lite register
  interface (see Section 7).
- Polling via the `STATUS`/`CTRL` registers is also supported; interrupt use
  is optional for the integrator and testbench.
- No other status wires: all state is exposed through registers.

## 4. Register Map

All registers are 32 bits wide and word-aligned (4-byte offsets). The layout
follows the Xilinx HLS `ap_ctrl_hs` convention so that HLS-generated slave
logic can be reused directly.

| Offset | Register     | Access | Reset | Description                                  |
|--------|--------------|--------|-------|----------------------------------------------|
| 0x00   | `CTRL`       | RW     | 0     | Control / handshake (see 4.1)                |
| 0x04   | `GIE`        | RW     | 0     | Global interrupt enable                      |
| 0x08   | `IER`        | RW     | 0     | IP interrupt enable                          |
| 0x0C   | `ISR`        | W1TC   | 0     | IP interrupt status (write-1-to-clear)       |
| 0x10   | `RD_ADDR_LO` | RW     | 0     | Read base address, bits `[31:0]`             |
| 0x14   | `RD_ADDR_HI` | RW     | 0     | Read base address, bits `[63:32]`            |
| 0x18   | `WR_ADDR_LO` | RW     | 0     | Write base address, bits `[31:0]`            |
| 0x1C   | `WR_ADDR_HI` | RW     | 0     | Write base address, bits `[63:32]`           |
| 0x20   | `NUM_FFTS`   | RW     | 0     | Number of 16-point FFTs per activation       |
| 0x24   | `MODE`       | RW     | 0     | 0=`FFT`, 1=`READ_ONLY`, 2=`WRITE_ONLY`       |
| 0x28   | `FFT_CFG`    | RW     | 0     | Reserved for future FFT configuration        |
| 0x2C   | `STATUS`     | RO     | 0     | Error / status flags (see 4.4)               |
| 0x30   | `CYCLE_CNT`  | RO     | 0     | Cycles between `ap_start` and `ap_done`      |
| 0x34   | `RD_BEAT_CNT`| RO     | 0     | Total R beats accepted this activation       |
| 0x38   | `WR_BEAT_CNT`| RO     | 0     | Total W beats issued this activation         |
| 0x3C   | `RD_LAT_ACC` | RO     | 0     | Accumulated read latency (see 4.9)           |
| 0x40   | `WR_LAT_ACC` | RO     | 0     | Accumulated write latency (see 4.9)          |
| 0x44   | `MO_MAX`     | RO     | 0     | Max outstanding: `[15:0]`=read, `[31:16]`=wr |
| 0x48   | `RD_TXN_CNT` | RO     | 0     | Number of read transactions (RLAST beats)    |
| 0x4C   | `WR_TXN_CNT` | RO     | 0     | Number of write transactions (B handshakes)  |
| 0x50   | `MAPPER_CTRL`| RW     | 0     | ID mapper policy (see 4.10)                  |

Unlisted offsets in `[0x00, 0xFFF]` are reserved. Writes are ignored; reads
return 0.

### 4.1 `CTRL` (0x00) — Control / Handshake

Follows the Xilinx HLS `ap_ctrl_hs` protocol.

| Bit  | Name           | Access | Reset | Description                                                        |
|------|----------------|--------|-------|--------------------------------------------------------------------|
| 0    | `ap_start`     | W1S    | 0     | Write 1 to launch an activation. Hardware clears on `ap_ready`.    |
| 1    | `ap_done`      | COR    | 0     | Set when the current activation completes. Cleared on read.        |
| 2    | `ap_idle`      | RO     | 1     | 1 while the DUT is idle (no activation in progress).               |
| 3    | `ap_ready`     | RO     | 0     | 1 for one cycle when the DUT has accepted `ap_start`.              |
| 6:4  | Reserved       | RO     | 0     |                                                                    |
| 7    | `auto_restart` | RW     | 0     | If 1, hardware re-asserts `ap_start` automatically after `ap_done`.|
| 31:8 | Reserved       | RO     | 0     |                                                                    |

Access legend: `W1S` = write 1 to set; `COR` = clear on read; `RO` = read-only.

### 4.2 `GIE` (0x04) — Global Interrupt Enable

| Bit  | Name    | Access | Reset | Description                                          |
|------|---------|--------|-------|------------------------------------------------------|
| 0    | `gie`   | RW     | 0     | 1 = interrupt output enabled; 0 = `irq_o` forced 0.  |
| 31:1 | Reserved| RO     | 0     |                                                      |

### 4.3 `IER` / `ISR` (0x08 / 0x0C) — IP Interrupt Enable / Status

| Bit  | Name           | IER Access | ISR Access | Description                                              |
|------|----------------|------------|------------|----------------------------------------------------------|
| 0    | `ap_done_int`  | RW         | W1TC       | Interrupt on activation done.                            |
| 1    | `ap_ready_int` | RW         | W1TC       | Interrupt on `ap_ready`.                                 |
| 2    | `rd_err_int`   | RW         | W1TC       | Interrupt on a read-side response error (`STATUS.rd_error` rising). |
| 3    | `wr_err_int`   | RW         | W1TC       | Interrupt on a write-side response error (`STATUS.wr_error` rising). |
| 31:4 | Reserved       | RO         | RO         |                                                          |

`irq_o = GIE.gie & |(IER & ISR)`.

The `rd_err_int` / `wr_err_int` ISR bits are set on the cycle
`STATUS.rd_error` / `STATUS.wr_error` transitions from 0 to 1
(i.e., the first erroneous response of the activation). They are
cleared only by a W1TC write to `ISR`; auto-clear of `STATUS`
flags on `ap_start` does **not** clear the ISR bits.

### 4.4 `STATUS` (0x2C) — Error / Status Flags

| Bit  | Name       | Access | Reset | Description                                              |
|------|------------|--------|-------|----------------------------------------------------------|
| 0    | `rd_error` | RO     | 0     | Read master received `SLVERR` or `DECERR`. Sticky within an activation. |
| 1    | `wr_error` | RO     | 0     | Write master received `SLVERR` or `DECERR`. Sticky within an activation. |
| 2    | `overflow` | RO     | 0     | Reserved for compute-overflow flag (Section 9.6).        |
| 31:3 | Reserved   | RO     | 0     |                                                          |

Sticky flags are cleared automatically on `ap_start`, matching the
performance counter policy. See Section 10 for full error-handling
semantics.

### 4.5 Address Register Semantics

- `RD_ADDR_LO` and `RD_ADDR_HI` together form a 64-bit read base address.
  The concatenation is `{RD_ADDR_HI, RD_ADDR_LO}`.
- `WR_ADDR_LO` / `WR_ADDR_HI` are analogous.
- Both halves must be programmed before `ap_start` is written.
- Alignment requirements are defined in Section 5.3.

### 4.6 `NUM_FFTS` (0x20)

- Unsigned 32-bit count of 16-point FFTs to process in one activation.
- Must be non-zero at `ap_start`; behavior for `NUM_FFTS = 0` is undefined
  (may be tightened later, e.g., early-`ap_done`).

### 4.7 `MODE` (0x24)

- `[1:0]` selects the operating mode; upper bits reserved.
- `0` = `MODE_FFT`, `1` = `MODE_READ_ONLY`, `2` = `MODE_WRITE_ONLY`.
- Detailed semantics in Section 8.

### 4.8 `FFT_CFG` (0x28)

- Reserved. All bits ignored by hardware. Reads return the last written value.

### 4.9 Performance Counters (0x30 – 0x4C)

All performance counters are 32-bit read-only. They are cleared automatically
on every `ap_start` write and stop updating when `ap_done` asserts, so a
read after `ap_done` reflects the values for the most recently completed
activation.

**Measurement location.** Performance monitors sit between the ID mapper
wrappers (Section 6.3) and the DUT's AXI boundary. This is the point where
the real AXI protocol is exposed: IDs 0–3 all appear here and out-of-order
responses are possible. Measuring here captures the true slave-facing
behavior (latency, backpressure, MO utilization) rather than the
in-order / single-ID view seen inside the HLS core.

**ID tracking.** For each master (read / write), a per-ID FIFO records the
handshake cycle of every outstanding request. On the matching response
handshake for that ID, the FIFO is popped and the elapsed cycle count is
accumulated into `RD_LAT_ACC` / `WR_LAT_ACC`. Because the AXI ordering
rule guarantees in-order responses within the same ID, a per-ID FIFO is
sufficient to match requests to responses without any ID field in the FIFO
entry. FIFO depth per ID must be at least the master's `num_*_outstanding`
setting.

Counter definitions:

- **`CYCLE_CNT`**: counts `clk` cycles from `ap_start` (inclusive) to
  `ap_done` (inclusive). Saturates at `0xFFFF_FFFF` if the activation is
  longer than 2^32 - 1 cycles.
- **`RD_BEAT_CNT`**: number of R-channel data-beat handshakes
  (`RVALID & RREADY`) observed at the DUT boundary.
- **`WR_BEAT_CNT`**: number of W-channel data-beat handshakes
  (`WVALID & WREADY`) at the DUT boundary.
- **`RD_LAT_ACC`**: per-transaction latency, summed. Per-transaction
  latency is the cycle count from the AR handshake to the matching RLAST
  beat (same ID). Aggregate across all IDs; per-ID latency is not exposed.
  Saturates at `0xFFFF_FFFF`.
- **`WR_LAT_ACC`**: per-transaction latency, summed. Per-transaction
  latency is the cycle count from the AW handshake to the matching B
  handshake (same ID). Aggregate across all IDs. Saturates at
  `0xFFFF_FFFF`.
- **`MO_MAX`**: peak observed outstanding transaction counts during the
  activation. `[15:0]` is the read master's peak (AR handshakes not yet
  matched by an RLAST, summed across all IDs); `[31:16]` is the write
  master's peak. Each half saturates at `0xFFFF`.
- **`RD_TXN_CNT`**: number of completed read transactions during the
  activation (equivalent to the number of RLAST beats observed).
- **`WR_TXN_CNT`**: number of completed write transactions during the
  activation (equivalent to the number of B handshakes observed).

Derived quantities the testbench can compute off-chip:

- Average read latency: `RD_LAT_ACC / RD_TXN_CNT`.
- Average write latency: `WR_LAT_ACC / WR_TXN_CNT`.
- Read throughput (bytes / cycle): `RD_BEAT_CNT * 32 / CYCLE_CNT`.
- Write throughput (bytes / cycle): `WR_BEAT_CNT * 32 / CYCLE_CNT`.

Notes:
- Counter saturation instead of wrap keeps post-run analysis unambiguous.
- Because counters clear on `ap_start`, software must read them after
  `ap_done` and before the next activation.

### 4.10 `MAPPER_CTRL` (0x50) — ID Mapper Policy

Controls how the ID mapper wrapper assigns AXI IDs to transactions emitted
by the HLS core (which itself uses only ID 0). The same policy applies to
both read and write masters.

| Bit  | Name       | Access | Reset | Description                                     |
|------|------------|--------|-------|-------------------------------------------------|
| 1:0  | `policy`   | RW     | 0     | 0=`SEQUENTIAL`, 1=`ROUND_ROBIN`, 2=`RANDOM`     |
| 31:2 | Reserved   | RO     | 0     |                                                 |

Policies:

- `SEQUENTIAL`: mapper always emits ID 0 (equivalent to a pass-through).
  Useful for correlating pre- and post-mapper behavior.
- `ROUND_ROBIN`: mapper assigns IDs 0, 1, 2, 3, 0, 1, 2, 3, ... in the
  order of transaction requests, independent of read vs. write.
- `RANDOM`: mapper assigns an ID uniformly at random from {0, 1, 2, 3}
  per transaction, using an internal LFSR seeded at reset.

Policy is sampled on each transaction request. It may be reprogrammed at
any time; the change takes effect on subsequent transactions. Software
should typically set the policy while the DUT is idle to avoid mid-stream
policy changes during a single activation.

## 5. Memory Data Layout

### 5.1 Sample Format

- Each sample is a complex value: `(real, imag)`, both Q2.14 signed
  fixed-point, 16 bits each. Total sample width: 32 bits.
- In-word layout: `sample[15:0] = real`, `sample[31:16] = imag`.
- Byte order in memory: little-endian, matching the AXI4 convention.
  Real occupies bytes `+0, +1`; imag occupies bytes `+2, +3`.

### 5.2 Beat Packing

- AXI data width is 256 bits = 32 bytes = 8 samples per beat.
- Samples are packed by ascending index into ascending bit positions:

  | Bit range | Sample index |
  |-----------|--------------|
  | `[ 31:  0]` | 0          |
  | `[ 63: 32]` | 1          |
  | `[ 95: 64]` | 2          |
  | `[127: 96]` | 3          |
  | `[159:128]` | 4          |
  | `[191:160]` | 5          |
  | `[223:192]` | 6          |
  | `[255:224]` | 7          |

- One 16-point FFT therefore occupies exactly two beats: samples 0–7 in
  the first beat, samples 8–15 in the second beat.

### 5.3 Batching Layout

FFTs within a batch are packed contiguously with no padding between them.
FFT `k` occupies the byte range `[base + k * 64, base + (k + 1) * 64)`,
where `base` is `RD_ADDR` (input) or `WR_ADDR` (output). Because each FFT
is exactly 64 bytes (2 beats), it is naturally aligned to the 32-byte beat
size regardless of `base`, provided `base` itself satisfies Section 5.4.

Total data transferred per activation:

- Read side: `NUM_FFTS * 2` beats = `NUM_FFTS * 64` bytes.
- Write side: `NUM_FFTS * 2` beats = `NUM_FFTS * 64` bytes.

### 5.4 Address Alignment Requirements

- `RD_ADDR` and `WR_ADDR` must be 32-byte aligned (bits `[4:0]` = 0). The
  DUT does not implement partial-beat handling; alignment is the software
  contract.
- Behavior on misalignment is undefined. Verification must not exercise
  misaligned bases through directed stimulus.

### 5.5 4 KB AXI Boundary

AXI4 forbids a single burst from crossing a 4 KB boundary. When the master
would emit a burst that crosses such a boundary, the HLS-generated master
automatically splits it into two bursts. No software padding or alignment
beyond Section 5.4 is required to satisfy this rule.

## 6. Transaction Behavior

### 6.1 AXI Burst Parameters

Values below apply to both the read master (AR channel) and the write
master (AW / W channels) unless noted otherwise. Fixed values are enforced
by design and shall be checked via assertions and coverage in the
testbench (contract-compliance verification).

| Field                 | Value              | Notes                                                |
|-----------------------|--------------------|------------------------------------------------------|
| `AWSIZE` / `ARSIZE`   | `0b101` (32 B)     | Matches 256-bit data width. No narrow transfers.     |
| `AWBURST` / `ARBURST` | `0b01` (INCR)      | Only INCR is supported. FIXED / WRAP not used.       |
| `AWLEN` / `ARLEN`     | 0 – 15 (1–16 beats)| Upper bound = 16 beats per burst. Actual value is chosen by the HLS master; the 4 KB rule may split into shorter bursts. |
| `AWCACHE` / `ARCACHE` | `0b0011`           | Modifiable + Bufferable, non-cacheable.              |
| `AWPROT` / `ARPROT`   | `0b000`            | Data, Secure, Unprivileged.                          |
| `AWQOS` / `ARQOS`     | `0b0000`           | QoS not used.                                        |
| `AWLOCK` / `ARLOCK`   | `0b0`              | Normal access; no exclusive access.                  |
| `AWID` / `ARID`       | 0 – 3              | Assigned by ID mapper wrapper (Section 6.3).         |
| `WSTRB`               | All ones (`0xFF..F`) | Full-beat writes only; no partial writes.          |
| `WLAST`               | Asserted on final beat of each write burst | Per AXI spec.                    |
| `RLAST`               | Consumed on final beat of each read burst  | Per AXI spec.                    |

Rationale for fixed values: the DUT is an FFT + DMA block accessing
contiguous memory. INCR is the only semantically meaningful burst type;
CACHE / PROT / QOS / LOCK are all set to standard non-cacheable data
access values consistent with the Xilinx HLS defaults.

### 6.2 4 KB Boundary Handling

- AXI4 forbids a single burst from crossing a 4 KB boundary. The
  HLS-generated master automatically splits any burst that would cross
  such a boundary into two consecutive bursts.
- Splitting is internal to the master; upstream logic (FFT compute path)
  is unaware of it.
- The testbench monitor may observe (via coverage) whether requests that
  would cross a boundary are actually split, and whether the resulting
  sub-bursts are correctly aligned.

### 6.3 AXI ID Usage

**Inside the HLS core.** The HLS m_axi bundle emits every request with
`ARID = 0` (read master) or `AWID = 0` (write master). This is the fixed
behavior of Xilinx HLS.

**ID mapper wrapper.** A thin hand-written wrapper (in `rtl/`) sits
between the HLS core and the DUT boundary and re-labels the ID of every
outgoing request according to the current policy (`MAPPER_CTRL.policy`,
Section 4.10):

- `SEQUENTIAL`: passes ID 0 through unchanged.
- `ROUND_ROBIN`: cycles through IDs 0 → 1 → 2 → 3 → 0 → ...
- `RANDOM`: uniformly random ID from {0, 1, 2, 3}, LFSR-based.

Policies may be reprogrammed at runtime; changes take effect on
subsequent transactions.

**Read reorder buffer.** The read side of the wrapper contains a reorder
buffer sized to at least `num_read_outstanding`. It reassembles
out-of-order R responses (allowed for different IDs at the slave) so that
the HLS core sees only in-order, ID 0 responses.

**Write completion.** The write side does not need to reorder data (W
channel is always in the order the master issues it), but does need to
track outstanding AW → B pairs so that the completion notification
returned to the HLS core respects the master's request order.

**Interaction with performance monitors.** All performance counters
(Section 4.9) observe the AXI signals **outside** the mapper (i.e., at
the DUT boundary), so they see the full range of IDs 0–3 and any
out-of-order behavior induced by the slave.

## 7. Control Flow (Activation Protocol)

### 7.1 Start / Done / Idle Semantics

The DUT follows the Xilinx HLS `ap_ctrl_hs` protocol. One activation
proceeds through the following phases:

1. **Idle** — `ap_idle = 1`, `ap_start = 0`, `ap_done = 0`. The DUT is
   waiting for a new request.
2. **Configuration** — Software programs `RD_ADDR_LO/HI`, `WR_ADDR_LO/HI`,
   `NUM_FFTS`, `MODE`, and (optionally) `MAPPER_CTRL`.
3. **Start** — Software writes `CTRL.ap_start = 1` (W1S).
4. **Accept** — On the next cycle the DUT satisfies the start condition,
   `ap_ready` is asserted for exactly one clock cycle, `ap_start` is
   auto-cleared by hardware, and `ap_idle` drops to 0. Configuration
   registers are latched on this cycle (see 7.2).
5. **Running** — The DUT issues read / compute / write traffic.
   `ap_idle = 0`, `ap_done = 0` throughout.
6. **Completion** — When the last outstanding response for this activation
   has been received (see 7.3), `ap_done` is asserted and `ap_idle`
   returns to 1.
7. **Done acknowledgement** — Software reads `CTRL`; the read auto-clears
   `ap_done` (COR). This does **not** clear `ISR.ap_done_int`; the
   interrupt status flag must be cleared explicitly via a W1TC write to
   `ISR`.

Signal semantics:

- **`ap_ready`**: single-cycle pulse asserted on the accept cycle.
- **`ap_done`**: sticky until read (COR). While set, `irq_o` may be
  asserted (subject to `GIE` / `IER`).
- **`ap_done` and `ISR.ap_done_int` are independent**. Reading `CTRL`
  clears only `ap_done`. `ISR.ap_done_int` is cleared only by a W1TC
  write to `ISR`. This lets the interrupt service routine acknowledge the
  interrupt without racing with a status read.
- **Writes to `ap_start` while `ap_idle = 0`**: ignored. No effect on
  the in-flight activation.

### 7.2 Configuration Latching

Configuration registers (`RD_ADDR_*`, `WR_ADDR_*`, `NUM_FFTS`, `MODE`,
`FFT_CFG`, `MAPPER_CTRL`) may be written at any time, but their values
are **sampled by hardware only on the `ap_ready` cycle**. Mid-run writes
therefore do not disturb the current activation and take effect on the
next `ap_start`.

Software should still program these registers while `ap_idle = 1` when
possible to avoid ambiguity in coverage / debugging.

### 7.3 Done Condition

`ap_done` asserts once the activation's data traffic has fully retired:

- `MODE_FFT`: the last B response of the last write burst has been
  received **and** the last RLAST beat of the last read burst has been
  consumed.
- `MODE_READ_ONLY`: the last RLAST beat of the last read burst has been
  consumed. (No writes are issued in this mode.)
- `MODE_WRITE_ONLY`: the last B response of the last write burst has
  been received. (No reads are issued in this mode.)

Performance counters (Section 4.9) stop updating on the same cycle
`ap_done` asserts.

### 7.4 Restart Handshake

Two restart flows are supported:

**Software-driven restart.** The default flow.

1. Wait for `ap_done = 1` (or an interrupt).
2. Read `CTRL` to clear `ap_done`; W1TC-clear `ISR.ap_done_int` if used.
3. Optionally reprogram configuration registers.
4. Write `CTRL.ap_start = 1` to launch the next activation.

**Auto-restart.** Setting `CTRL.auto_restart = 1` causes hardware to
re-assert `ap_start` automatically on the same cycle it would set
`ap_done`, using the current (latched) configuration values. This
produces back-to-back activations with no software intervention.

- Auto-restart uses the configuration latched at the most recent
  `ap_ready`; register writes between activations are only visible if
  they occurred before the next `ap_ready` cycle.
- Clearing `auto_restart` before an in-flight activation's `ap_done`
  causes the DUT to return to Idle after that activation, restoring the
  normal software-driven flow.

## 8. Operating Modes

The `MODE` register (0x24) selects one of three operating modes. `MODE` is
latched at `ap_ready` along with the other configuration registers
(Section 7.2). Mid-run writes to `MODE` do not affect the in-flight
activation.

### 8.1 MODE_FFT (`MODE = 0`)

Full pipeline: read → compute → write.

- Read master issues `NUM_FFTS * 2` beats from `RD_ADDR` (Section 5.3).
- Each 16-sample chunk is passed through the 16-point FFT (Section 9).
- Write master issues `NUM_FFTS * 2` beats to `WR_ADDR`.
- Read and write may overlap freely, subject to the outstanding limits of
  each master.
- `ap_done` asserts after the last B response **and** the last RLAST
  beat have both been observed.
- Performance counters: both read and write counters accumulate.

### 8.2 MODE_READ_ONLY (`MODE = 1`)

Read master only. FFT compute and write master are held disabled.

- Read master issues `NUM_FFTS * 2` beats from `RD_ADDR`.
- Received data is discarded at the FFT compute input; the FFT pipeline
  is not clocked / activated, and no internal state changes based on
  the data.
- Write master emits no traffic (`AWVALID`, `WVALID` remain 0).
- `ap_done` asserts after the last RLAST beat.
- Performance counters: only read counters accumulate. All write
  counters read as 0 at `ap_done`.
- Purpose: stress the read side (AR / R channels) in isolation from any
  write-side backpressure or FFT-pipeline coupling.

### 8.3 MODE_WRITE_ONLY (`MODE = 2`)

Write master only, using a deterministic index pattern for the payload.
Read master and FFT compute are held disabled.

- Write master issues `NUM_FFTS * 2` beats to `WR_ADDR`.
- **Data pattern**: for each 32-bit sample slot at zero-based sample
  index `n` from the start of the write region, the emitted value equals
  `n` (unsigned 32-bit). Concretely, if the write region contains
  `NUM_FFTS * 16` samples, then the sample at byte offset
  `WR_ADDR + 4 * n` holds the little-endian 32-bit value `n`.
- The `(real, imag)` interpretation of the 32-bit sample (Section 5.1)
  is preserved on the wire (real in `[15:0]`, imag in `[31:16]`), but
  the pattern is chosen for checker simplicity, not for its arithmetic
  meaning.
- Read master emits no traffic (`ARVALID` remains 0).
- `ap_done` asserts after the last B response.
- Performance counters: only write counters accumulate. All read
  counters read as 0 at `ap_done`.
- Purpose: stress the write side (AW / W / B channels) in isolation
  from any read-side latency or FFT-pipeline coupling.

### 8.4 Mode Change Timing

`MODE` follows the standard configuration-latch rule (Section 7.2). To
switch modes cleanly, program the new `MODE` value while the DUT is idle
(or between activations) and then assert `ap_start`.

## 9. FFT Details

### 9.1 Algorithm and Point Size

- Point size: 16 (fixed).
- Radix: 2.
- Structure: Decimation-in-Time (DIT), 4 stages.
- Direction: forward only. Inverse FFT is out of scope for this spec;
  `FFT_CFG` (0x28) is reserved for a future direction bit.
- Implementation: hand-written in HLS C++. No external FFT library is
  used.
- Bit-reversal is applied on the **input** side: sample `i` is placed at
  position `bit_reverse_4(i)` before butterfly stages. Output is in
  natural order (bin 0 → bin 15).

### 9.2 Fixed-Point Format

- Sample representation: Q2.14 signed, 16 bits per real / imaginary
  part.
  - 1 sign bit, 1 integer bit, 14 fractional bits.
  - Numeric range: `[-2, 2 - 2^-14]`.
  - Real value = `raw / 16384`.
- Complex sample width: 32 bits (`real` in `[15:0]`, `imag` in `[31:16]`)
  as defined in Section 5.1.

### 9.3 Scaling Policy

- Per-stage arithmetic right shift by 1 (block scaling). Applied
  independently to real and imaginary components after each butterfly.
- Four stages × `>> 1` = total scale factor of `1 / 16` relative to the
  unscaled DFT. The DUT output therefore equals `DFT(x) / N`.
- Rounding on the shift: **truncation** (arithmetic right shift; floor
  toward −∞ for signed values). Chosen for HW simplicity and exact
  match to the C model. Rounding-mode changes (convergent, half-up) are
  not planned.
- Block scaling mathematically prevents overflow at the sample slot when
  inputs remain in Q2.14 range; see Section 9.6 for the overflow flag
  policy.

### 9.4 Twiddle Precision

- Twiddle representation: Q1.15 signed, 16 bits per real / imaginary
  part.
  - Numeric range: `[-1, 1 - 2^-15]`.
  - `+1.0` is approximated by `0x7FFF ≈ 0.99997` to keep the LUT
    symmetric. `-1.0` is likewise represented as `-0x7FFF` (rather than
    `INT16_MIN = -0x8000`) so that negation cannot overflow.
- Storage: 8-entry lookup table `W16[0..7]` hard-coded as constants
  (only the first half is stored; the second half is derived by
  butterfly symmetry).
- No runtime twiddle computation (no CORDIC / no ROM addressing beyond
  the 8-entry LUT).

### 9.5 Output Ordering

- Natural order. Output bin `k` corresponds to the DFT frequency index
  `k` for `k = 0 .. 15`. Because bit-reversal is applied to the input
  (Section 9.1), no output permutation is required.

### 9.6 Butterfly Datapath Precision

- Complex multiplication `W · b`:
  - Format: Q1.15 × Q2.14 → product in Q3.29, held in a signed 32-bit
    intermediate.
  - Normalization to Q3.14: arithmetic right shift by 15
    (**truncation**, no rounding).
- Butterfly sum / difference:
  - `a + Wb` and `a − Wb` computed in signed 32-bit to avoid mid-stage
    overflow.
  - Right-shifted by 1 (Section 9.3) before storing back into a Q2.14
    slot.
- Overflow flag (`STATUS.overflow`, bit 2 of `STATUS`): reserved,
  hardwired to 0 in this revision. With Q2.14 inputs and the fixed
  block-scaling policy, arithmetic overflow at the sample slot is
  mathematically prevented, so no runtime detection is instantiated. If
  the scaling policy is relaxed in a future revision, the flag will be
  activated then.

## 10. Error Handling

### 10.1 Detection

- Read master: on every accepted R-channel beat, the DUT samples
  `RRESP[1:0]`. Any value other than `OKAY` (`0b00`) — i.e., `SLVERR`
  (`0b10`) or `DECERR` (`0b11`) — is treated as an error.
- Write master: on every accepted B-channel handshake, the DUT samples
  `BRESP[1:0]`. Any value other than `OKAY` is treated as an error.
- `EXOKAY` (`0b01`) is not expected because the DUT never asserts
  exclusive access (`AWLOCK` / `ARLOCK` = 0). Receipt of `EXOKAY` is
  treated identically to `OKAY` for status purposes; assertions may
  additionally flag it as an unexpected response.
- Error detection observes signals **at the DUT boundary** (outside the
  ID mapper wrapper), matching the performance-monitor placement.

### 10.2 Response Behavior

- The DUT does **not** abort on error. All remaining reads and writes
  planned for the current activation are still issued.
- `STATUS.rd_error` / `STATUS.wr_error` are latched to 1 on the first
  erroneous response of that type and remain sticky for the duration of
  the activation.
- On the 0 → 1 transition of `STATUS.rd_error`, `ISR.rd_err_int` is
  set. Likewise for `STATUS.wr_error` and `ISR.wr_err_int`. Interrupts
  are asserted subject to `GIE` and `IER`.
- `ap_done` asserts normally after all outstanding transactions retire
  (as defined in Section 7.3). Software must inspect `STATUS` after
  `ap_done` to determine whether the activation completed cleanly.

### 10.3 Status Clearing

- `STATUS.rd_error` and `STATUS.wr_error` are cleared automatically on
  every `ap_start`, matching the performance-counter clear policy. This
  makes the status reflect only the most recently completed activation.
- `ISR.rd_err_int` and `ISR.wr_err_int` are **not** cleared by
  `ap_start`; they must be cleared explicitly via W1TC to `ISR`,
  following the standard interrupt-acknowledge flow (Section 4.3).

### 10.4 Interaction with Performance Counters

Erroneous transactions are still counted in the standard performance
counters (Section 4.9). `RD_BEAT_CNT`, `WR_BEAT_CNT`, `RD_LAT_ACC`,
`WR_LAT_ACC`, `RD_TXN_CNT`, and `WR_TXN_CNT` include contributions
from transactions whose response was `SLVERR` or `DECERR`. Error
occurrence is signaled separately through `STATUS` and `ISR`.

### 10.5 Interaction with Auto-Restart

If `CTRL.auto_restart = 1` and an error occurs during an activation,
the DUT still asserts `ap_done` and re-launches the next activation
on the same cycle. The sticky `STATUS` bits are cleared by the
auto-generated `ap_start`, so software using `auto_restart` should
rely on `ISR` (which is not auto-cleared) to detect errors between
activations.

## 11. Reset Behavior

Reset signaling itself (polarity, style, minimum assertion) is defined in
Section 2.2. This section specifies the internal effects of reset.

### 11.1 State Cleared on Reset Assertion

While `rst_n = 0`, the DUT is forced into a fully idle state:

- All memory-mapped registers (0x00 – 0x50) return to their reset values
  in Section 4.
- All internal FSMs return to `IDLE`.
- Outstanding-transaction counters (both master sides) are cleared.
- Read reorder buffer is emptied.
- ID mapper LFSR is reloaded with its initial seed.
- Performance-counter internal state (per-ID FIFOs, running totals,
  peak trackers) is cleared.
- Error latches (`STATUS.rd_error`, `STATUS.wr_error`, and the
  corresponding `ISR` bits) are cleared.

### 11.2 AXI Output Signals During Reset

- All `*VALID` outputs (`ARVALID`, `AWVALID`, `WVALID`, and the
  AXI-Lite slave's `RVALID` / `BVALID`) are driven to 0.
- All `*READY` outputs the DUT drives (the read master's `RREADY`,
  the write master's `BREADY`, and the AXI-Lite slave's `AWREADY`,
  `WREADY`, `ARREADY`) are also driven to 0, preventing any
  accidental handshake during reset.
- Payload signals (`ARADDR`, `WDATA`, etc.) may take any value; the
  0-`*VALID` guarantees they are never sampled.

### 11.3 Outstanding Transactions and System Assumptions

Reset is treated as a **system-wide** event: the DUT, the AXI slaves it
communicates with, and any surrounding fabric are all assumed to be
reset together. The DUT drops all knowledge of in-flight transactions
on reset assertion; scenarios in which the DUT is reset while a slave
continues to run and later returns responses to a phantom request are
out of scope.

### 11.4 Post-Reset Behavior

- Once `rst_n` deasserts (synchronously to `clk`), the DUT is ready for
  AXI-Lite access on the next clock cycle. No additional wait period is
  required.
- The first activation may be programmed immediately; the DUT begins in
  `ap_idle = 1` with all configuration registers at their reset values.

### 11.5 Reset During an Active Run

If reset asserts while an activation is in progress, the DUT
immediately transitions to `IDLE` per Section 11.1. Any in-flight
transactions are abandoned from the DUT's point of view; per Section
11.3, the surrounding system is assumed to reset in the same event so
that no orphan responses arrive after reset deassertion.

---

## Change Log

- 2026-08-20: Document skeleton created and all sections filled in
  (Clock / Reset, interfaces, register map incl. performance counters
  and mapper control, memory layout, transaction behavior, control
  flow with auto-restart, operating modes, FFT details, error
  handling, reset behavior, top-level overview).
