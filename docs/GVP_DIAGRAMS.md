# GVP Diagrams

Visual references for the GVP DUT and its verification environment.
Diagrams are rendered by GitHub automatically via Mermaid. If a diagram
here disagrees with `GVP_RTL_SPEC.md`, the spec wins.

---

## 1. Top-Level System

The DUT sits between a control host (the testbench, in verification;
a CPU / MicroBlaze in a real system) and an external memory model.

```mermaid
flowchart LR
    subgraph HOST[Host / Testbench]
        SW[Config + IRQ handler]
    end

    subgraph DUT[GVP DUT]
        CTRL[AXI4-Lite Slave]
        CORE[HLS Core + ID Mapper + Perf Monitor]
    end

    subgraph MEM[External Memory Model]
        BFM[AXI Slave BFM<br/>Xilinx AXI VIP<br/>Backing memory]
    end

    SW -->|AXI4-Lite| CTRL
    CTRL --> CORE
    CORE -->|AXI4 Read Master| BFM
    CORE -->|AXI4 Write Master| BFM
    CORE -.->|irq_o| SW
```

Legend:
- Solid arrows are AXI traffic.
- Dashed arrow is the level-sensitive interrupt.

---

## 2. DUT Internal Hierarchy

The DUT top wrapper hosts an HLS-generated core plus two thin
hand-written layers: ID mappers (per master) and performance monitors
(at the DUT boundary, outside the mappers).

```mermaid
flowchart TB
    LITE[s_axi_lite_ Ctrl] --> HLS

    subgraph TOP[DUT Top Wrapper]
        HLS[HLS Core<br/>fft_dma<br/>emits ARID=0 / AWID=0]

        subgraph MAP[ID Mapper Layer]
            RDMAP[id_mapper_rd<br/>ID assign + reorder buffer]
            WRMAP[id_mapper_wr<br/>ID assign + completion track]
        end

        subgraph MON[Performance Monitor Layer]
            RDMON[perf_monitor_rd<br/>per-ID FIFO, LAT_ACC, MO_MAX]
            WRMON[perf_monitor_wr]
        end
    end

    HLS -->|AR / R<br/>ID=0| RDMAP
    HLS -->|AW / W / B<br/>ID=0| WRMAP
    RDMAP -->|AR / R<br/>ID=0..3| RDMON
    WRMAP -->|AW / W / B<br/>ID=0..3| WRMON
    RDMON -->|AR / R| EXT[DUT Boundary → Slave]
    WRMON -->|AW / W / B| EXT
```

The performance monitors are pass-through observers: signals cross
them unchanged. They only compute counters that feed the `PERF_CNT`
register block through `CTRL` (not shown for clarity).

---

## 3. Activation Timeline (`ap_ctrl_hs`)

One end-to-end activation from the host's point of view.

```mermaid
sequenceDiagram
    autonumber
    participant SW as Host / TB
    participant CTRL as CTRL Register
    participant DUT as DUT Core
    participant IRQ as irq_o

    Note over SW,DUT: Idle. ap_idle = 1, ap_done = 0.

    SW->>CTRL: Program RD_ADDR / WR_ADDR / NUM_FFTS / MODE
    SW->>CTRL: Write ap_start = 1 (W1S)

    CTRL->>DUT: sample config on ap_ready
    DUT-->>CTRL: ap_ready = 1 (single-cycle pulse)
    CTRL-->>SW: ap_start auto-cleared

    Note over DUT: Running. ap_idle = 0.<br/>Read / compute / write in flight.

    DUT-->>CTRL: ap_done = 1 (sticky, COR)
    CTRL-->>IRQ: assert irq_o<br/>(if GIE & IER.ap_done_int)

    SW->>CTRL: Read CTRL
    CTRL-->>SW: ap_done bit returned, then auto-cleared
    SW->>CTRL: Write ISR.ap_done_int = 1 (W1TC)
    CTRL-->>IRQ: deassert irq_o

    Note over SW,DUT: Back to idle. Ready for next activation.
```

- Reading `CTRL` clears `ap_done` but does not clear `ISR` bits.
- `ISR` requires explicit W1TC so an ISR can acknowledge without
  racing a `CTRL` read.

---

## 4. AXI Read Transaction (Single Burst)

A minimal read transaction from the DUT's read master.

```mermaid
sequenceDiagram
    autonumber
    participant M as DUT Read Master
    participant S as AXI Slave BFM

    M->>S: ARVALID = 1, ARADDR, ARLEN, ARID
    S-->>M: ARREADY = 1
    Note over M,S: AR handshake<br/>Performance monitor starts latency timer

    S->>M: RVALID = 1, RDATA[0]
    M-->>S: RREADY = 1
    S->>M: RVALID = 1, RDATA[1]
    M-->>S: RREADY = 1
    Note over M,S: ...RDATA[k]...
    S->>M: RVALID = 1, RDATA[LAST], RLAST = 1
    M-->>S: RREADY = 1
    Note over M,S: RLAST handshake<br/>Latency counter accumulates
```

For MO > 1 the master can issue several AR handshakes before the
first R response arrives. The slave may interleave R responses of
different IDs (out-of-order); the DUT's ID mapper wrapper
reassembles them.

---

## 5. AXI Write Transaction (Single Burst)

```mermaid
sequenceDiagram
    autonumber
    participant M as DUT Write Master
    participant S as AXI Slave BFM

    M->>S: AWVALID = 1, AWADDR, AWLEN, AWID
    S-->>M: AWREADY = 1
    Note over M,S: AW handshake<br/>Performance monitor starts latency timer

    M->>S: WVALID = 1, WDATA[0], WSTRB = '1
    S-->>M: WREADY = 1
    M->>S: WVALID = 1, WDATA[1]
    S-->>M: WREADY = 1
    Note over M,S: ...
    M->>S: WVALID = 1, WDATA[LAST], WLAST = 1
    S-->>M: WREADY = 1

    S->>M: BVALID = 1, BRESP
    M-->>S: BREADY = 1
    Note over M,S: B handshake<br/>Latency counter accumulates
```

W beats can start immediately after AW (or even before, per AXI4).
The B response cannot precede the corresponding WLAST.

---

## 6. ID Mapper — Read-Side Reorder

Illustrates the RANDOM policy: three consecutive AR requests from
the HLS core get different IDs; the slave replies out of order; the
mapper reorders them before returning to the HLS core.

```mermaid
sequenceDiagram
    autonumber
    participant HLS as HLS Core
    participant MAP as id_mapper_rd
    participant S as Slave

    HLS->>MAP: AR (id=0)   [txn A]
    MAP->>S:   AR (id=1)   [assigned]
    HLS->>MAP: AR (id=0)   [txn B]
    MAP->>S:   AR (id=3)   [assigned]
    HLS->>MAP: AR (id=0)   [txn C]
    MAP->>S:   AR (id=2)   [assigned]

    Note over MAP,S: Slave may respond out of order across IDs

    S->>MAP:   R (id=3, data, LAST)   [txn B done first]
    S->>MAP:   R (id=1, data, LAST)   [txn A done second]
    S->>MAP:   R (id=2, data, LAST)   [txn C done last]

    Note over MAP: Reorder buffer holds B until A returns, then C

    MAP->>HLS: R (id=0, data of A, LAST)
    MAP->>HLS: R (id=0, data of B, LAST)
    MAP->>HLS: R (id=0, data of C, LAST)
```

The HLS core sees only ID 0 responses in exactly the order it made
the requests. The AXI channel between the mapper and the slave
exercises the full ID range and out-of-order behavior.

---

## 7. Sample / Beat Layout

How Q2.14 complex samples pack into a 256-bit AXI beat and how
one 16-point FFT lays out across two beats.

```text
                                    32-bit sample slot
                                    ┌─────────┬─────────┐
                                    │ imag    │ real    │
                                    │ Q2.14   │ Q2.14   │
                                    │ [31:16] │ [15:0]  │
                                    └─────────┴─────────┘

Beat 0 (32 bytes = 256 bits), samples 0..7 packed LSB→MSB:
[255:224] [223:192] [191:160] [159:128] [127:96] [95:64] [63:32] [31:0]
 sample 7  sample 6  sample 5  sample 4  sample 3 sample 2 sample 1 sample 0

Beat 1 (32 bytes), samples 8..15 packed LSB→MSB:
[255:224] [223:192] [191:160] [159:128] [127:96] [95:64] [63:32] [31:0]
 sample15  sample14  sample13  sample12  sample11 sample10 sample 9 sample 8

One 16-point FFT = 2 beats = 64 bytes.
FFT k occupies bytes [base + k*64, base + (k+1)*64).
```

Little-endian throughout. Rules are enforced by
`c_model/fft_dma.c` (`read_sample` / `write_sample`) and mirrored in
the HLS `INTERFACE` pragmas.

---

## 8. Verification Data Flow

How the same CSV vectors flow through C sign-off, DPI-based
scoreboard, and CSV replay for the future UVM environment.

```mermaid
flowchart LR
    PY[Python numpy<br/>gen_vectors.py<br/>gen_dma_vectors.py] --> CSV[(CSV vectors<br/>vectors/*.csv)]

    CSV --> CTEST[C test binaries<br/>test_fft16<br/>test_fft_dma]
    CTEST --> CPASS([C sign-off])

    CSV --> REPLAY[UVM CSV replay<br/>tb/utils/csv_replay.sv<br/>planned]
    REPLAY --> DUT[DUT under UVM env]
    REPLAY --> SB[Scoreboard]

    CMODEL[c_model/fft_dma.c<br/>fft_dma_fx] --> DPI[DPI-C wrapper<br/>fft_dma_fx_dpi<br/>planned]
    DPI --> SB

    DUT --> SB
    SB --> UVMPASS([RTL sign-off])
```

Key property: the same Python golden used to sign off the C model
is reused (via the same CSV files and via DPI to the same C
function) to sign off the DUT. No parallel golden path exists — a
divergence would show up as a scoreboard mismatch on identical
vectors.

---

## Related Documents

- `docs/GVP_RTL_SPEC.md` — Authoritative behavior spec.
- `docs/GVP_VPLAN.md` — Test list and coverage plan.
- `README.md` — Project overview with the top-level ASCII block
  diagram.
