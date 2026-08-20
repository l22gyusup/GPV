# GVP 검증 계획서 (VPLAN)

GVP DUT의 검증 계획서. 이 문서는 `GVP_RTL_SPEC.md` (DUT가 요구된 대로 무엇을
해야 하는지)와 앞으로 `tb/`에 구축될 UVM 테스트벤치 (각 요구사항을 어떻게
자극하고 사인오프할지)를 잇는 다리 역할을 한다.

이 문서는 영문 원본 `GVP_VPLAN.md`의 한국어 번역본이며, 불일치 시 영문본을
우선한다. 살아있는 문서(living document)이므로 UVM 컴포넌트가 하나씩
등장할 때마다 갱신된다.

---

## 1. 스코프 및 목표

**In scope**

- DUT 최상위 (FFT + DMA)의 세 가지 동작 모드 (spec Section 8) 각각에 대한
  기능 정확성.
- `MODE_FFT`에서 DUT의 Q2.14 출력이 C 참조 모델 (`c_model/fft_dma.c`)과
  bit-accurate 매칭.
- DUT 경계에서의 AXI4 / AXI4-Lite 프로토콜 준수.
- 설계로 고정된 신호들의 계약 준수 (`AxBURST`, `AxCACHE`, `AxPROT`,
  `AxQOS`, `AxLOCK`, `AxSIZE`, `WSTRB`, `{RID, BID} ∈ {0, 1, 2, 3}`).
- Performance counter 값이 독립적인 TB 측정치와 일치하는지.
- ID mapper wrapper 동작 (3개 policy 전부, read-side reorder).
- Spec Section 10에 따른 에러 처리 (R / B 채널의 SLVERR / DECERR).
- Auto-restart 및 configuration-latch 시맨틱 (Section 7).

**Out of scope (이 단계에서는)**

- 타이밍 / 물리 설계 클로저.
- Post-synthesis netlist 검증.
- 다중 clock CDC (spec Section 2에 따라 single-clock DUT).
- Cache-coherency 모델링 (`AxCACHE`는 opaque 상수 취급).
- HLS로 고정된 파라미터의 런타임 override (MO 상한, burst length 상한).
  이들은 런타임 자극이 아니라 *configuration sweeps* 로 커버.

## 2. 검증 전략

- **Methodology**: SystemVerilog + UVM 1.2, xsim 시뮬레이터, Xilinx AXI
  VIP를 slave BFM으로. 스택 선택 근거는 `README.md` 참조.
- **Style**: Coverage-Driven Constrained Random Verification (CDV /
  CRV). Random 자극이 합리적 시뮬레이션 예산 안에서 도달하기 어려운
  corner case는 directed sub-test로 보완.
- **Golden reference**: `c_model` 프로젝트. UVM 스코어보드는 C 테스트가
  쓰는 동일한 fixed-point 함수 (`fft_dma_fx`)를 호출 → C-model과
  스코어보드 간 divergence 종류의 버그 원천 제거.
- **Vectors**: `c_model/vectors/`의 CSV 재사용. C 모델 사인오프에 쓴 CSV를
  그대로 DUT에도 replay → 별도 golden 재생성 없이 교차 검증.
- **Configuration sweeps**: HLS로 고정된 파라미터 (MO 상한, burst length
  상한, data / addr / ID width)는 configuration axis로 취급. 여러 HLS
  build가 configuration coverage bin을 채운다.
- **런타임 randomization 축**: `NUM_FFTS`, `RD_ADDR`, `WR_ADDR`, `MODE`,
  `MAPPER_CTRL.policy`, sample 데이터, slave의 ready-delay /
  response-delay / response-code 프로파일.

## 3. 요구사항-검증 매트릭스

각 행은 하나의 요구사항 영역. Verified-via 컬럼은 주요 검증 메커니즘
(`FC` = functional covergroup, `AS` = assertion, `SB` = scoreboard,
`CFG` = configuration sweep, `PC` = performance counter check).
Primary Tests 컬럼은 요구사항을 자극하는 주요 test들 (간결성을 위해
`test_` prefix 생략). Priority: `P0` = 기능 완료 선언에 필수, `P1` =
중요, `P2` = 있으면 좋음.

| # | Spec §     | Feature                                    | Verified via        | Primary Tests                                                          | Priority |
|---|------------|--------------------------------------------|---------------------|------------------------------------------------------------------------|----------|
| F01 | 2.2      | 비동기 assert / 동기 deassert reset        | AS + directed       | smoke_reset_and_regs, reset_mid_run, reset_post_access                 | P0       |
| F02 | 3.4, 4.2 | `GIE`/`IER`에 따른 인터럽트 출력           | FC + directed       | ctrl_irq_ack                                                           | P0       |
| F03 | 4.1      | `ap_ctrl_hs` 핸드셰이크                    | AS + SB             | smoke_single_activation, sanity_multi_activation, ctrl_ap_start_ignored | P0       |
| F04 | 4.2, 4.3 | GIE / IER / ISR 인터럽트 로직              | AS + directed       | ctrl_irq_ack                                                           | P0       |
| F05 | 4.4, 10  | STATUS sticky, `ap_start` 자동 clear       | AS + directed       | err_slverr_read, err_slverr_write, err_continue_on_error               | P0       |
| F06 | 4.5      | LO/HI로 조립된 64-bit 주소                 | SB + FC             | smoke_reset_and_regs, addr_alignment_sweep                             | P0       |
| F07 | 4.6      | `NUM_FFTS` boundary 값                     | FC + directed       | num_ffts_boundary                                                      | P0       |
| F08 | 4.7      | `MODE` enum 값                             | FC                  | sanity_mode_fft / _ro / _wo, mode_fft_directed                         | P0       |
| F09 | 4.9      | Performance counter 7종 전부               | PC + SB             | perf_counters, perf_mo_saturation, stress_random_latency               | P0       |
| F10 | 4.10     | `MAPPER_CTRL` policy: SEQ / RR / RANDOM    | FC + AS             | mapper_policy_rr, mapper_policy_seq, multi_id_reorder                  | P0       |
| F11 | 5.1-5.3  | Sample / beat 패킹 (little-endian, LSB)    | SB (데이터 비교)    | sanity_mode_fft, mode_fft_directed, mode_wo_directed                   | P0       |
| F12 | 5.4      | 주소 정렬 (32 B, 정렬된 것만)              | Constraint + AS     | addr_alignment_sweep                                                   | P0       |
| F13 | 6.1      | 고정 AXI 신호 계약 준수                    | AS + FC             | axi_burst_lengths, stress_backpressure, stress_random_latency          | P0       |
| F14 | 6.2      | 4 KB 경계 auto-split                       | FC + directed       | 4kb_boundary                                                           | P1       |
| F15 | 6.3      | ID mapper: reorder buffer, per-ID 순서     | AS + SB             | multi_id_reorder, stress_max_mo, stress_random_latency                 | P0       |
| F16 | 7.1      | ap_done / ap_ready / ap_idle 타이밍        | AS + SB             | smoke_single_activation, sanity_multi_activation                       | P0       |
| F17 | 7.2      | `ap_ready`에서 config latch                | Directed            | ctrl_config_latch                                                      | P0       |
| F18 | 7.3      | Mode 별 done 조건                          | AS + SB             | sanity_mode_fft / _ro / _wo, mode_ro_directed, mode_wo_directed        | P0       |
| F19 | 7.4      | `auto_restart` 흐름                        | Directed            | ctrl_auto_restart                                                      | P1       |
| F20 | 8.1      | MODE_FFT full pipeline                     | SB (데이터 비교)    | sanity_mode_fft, mode_fft_directed, mode_fft_random                    | P0       |
| F21 | 8.2      | MODE_READ_ONLY (write 트래픽 없음)         | AS + FC             | sanity_mode_ro, mode_ro_directed                                       | P0       |
| F22 | 8.3      | MODE_WRITE_ONLY (카운터 패턴)              | SB + FC             | sanity_mode_wo, mode_wo_directed                                       | P0       |
| F23 | 9        | FFT 수치 정확성 (Q2.14 bit-exact)          | SB vs C reference   | sanity_mode_fft, mode_fft_directed, mode_fft_random                    | P0       |
| F24 | 10.1     | SLVERR / DECERR 감지 (R / B)               | Directed error inj. | err_slverr_read, err_decerr_read, err_slverr_write, err_decerr_write   | P0       |
| F25 | 10.2     | Continue-on-error (조기 abort 없음)        | Directed            | err_continue_on_error                                                  | P0       |
| F26 | 10.3     | `ap_start`로 ISR clear 안 됨               | Directed            | ctrl_irq_ack, err_slverr_read (부수 확인)                              | P0       |
| F27 | 11       | Activation 도중 reset                      | Directed            | reset_mid_run                                                          | P1       |
| F28 | Cross    | HLS config sweep 커버리지                  | CFG                 | 각 HLS build variant에서 α/β suite 실행                                | P1       |

## 4. 커버리지 모델

명명 규약: covergroup은 `cg_<domain>`, coverpoint는 `cp_<name>`, 개별
bin은 `bin_<name>`. Cross 이름은 `cx_<a>_x_<b>`.

### 4.1 Functional Coverage — Covergroups

#### 4.1.1 `cg_config` — 매 `ap_ready`마다 sample

| Coverpoint      | Source            | Bin Name       | Values / Range          | 목적                                              |
|-----------------|-------------------|----------------|-------------------------|--------------------------------------------------|
| `cp_mode`       | `MODE[1:0]`       | `bin_fft`      | 0                       | MODE_FFT 자극됨                                   |
|                 |                   | `bin_ro`       | 1                       | MODE_READ_ONLY 자극됨                             |
|                 |                   | `bin_wo`       | 2                       | MODE_WRITE_ONLY 자극됨                            |
| `cp_num_ffts`   | `NUM_FFTS`        | `bin_1`        | 1                       | 최소 activation 크기                              |
|                 |                   | `bin_2`        | 2                       | Single-pair 경계                                  |
|                 |                   | `bin_small`    | `[3:7]`                 | 작은 배치, 단일 burst                             |
|                 |                   | `bin_medium`   | `[8:31]`                | Two-burst 영역                                    |
|                 |                   | `bin_large`    | `[32:127]`              | Multi-burst 영역                                  |
|                 |                   | `bin_huge`     | `[128:1023]`            | 다수 burst, 4 KB 경계 넘을 확률 높음             |
|                 |                   | `bin_massive`  | `[1024:$]`              | 극단 배치                                         |
| `cp_src_align`  | `RD_ADDR mod 4KB` | `bin_32`       | `[0x00:0x1F]`           | 32-byte만 정렬                                    |
|                 |                   | `bin_64`       | 64의 배수               | 64-byte 정렬                                      |
|                 |                   | `bin_128`      | 128의 배수              | Cache-line 정렬                                   |
|                 |                   | `bin_256`      | 256의 배수              | 넓은 정렬                                         |
|                 |                   | `bin_page`     | 0                       | 4 KB page 시작                                    |
| `cp_dst_align`  | `WR_ADDR mod 4KB` | `cp_src_align`과 동일                     | Write 쪽 동일 의도                       |
| `cp_mapper_pol` | `MAPPER_CTRL[1:0]`| `bin_seq`      | 0                       | SEQUENTIAL policy                                 |
|                 |                   | `bin_rr`       | 1                       | ROUND_ROBIN                                       |
|                 |                   | `bin_rand`     | 2                       | RANDOM                                            |
| Cross           | —                 | `cx_mode_x_num_ffts`  | 전체 3 × 7 grid  | Mode × 배치 크기 상호작용                        |
| Cross           | —                 | `cx_mode_x_pol`       | 전체 3 × 3 grid  | Mode × mapper policy                             |
| Cross           | —                 | `cx_num_ffts_x_pol`   | 전체 7 × 3 grid  | 배치 크기 × mapper policy                        |

#### 4.1.2 `cg_axi_rd` — DUT 경계에서 매 AR 핸드셰이크마다 sample

| Coverpoint       | Source                 | Bin Name          | Values / Range | 목적                                                |
|------------------|------------------------|-------------------|----------------|-----------------------------------------------------|
| `cp_arlen`       | `ARLEN`                | `bin_1_beat`      | 0              | Single-beat read                                    |
|                  |                        | `bin_2_4_beats`   | `[1:3]`        | Short burst                                         |
|                  |                        | `bin_5_8_beats`   | `[4:7]`        | Medium burst                                        |
|                  |                        | `bin_9_16_beats`  | `[8:15]`       | Max burst (HLS pragma 제한)                         |
| `cp_arid`        | `ARID`                 | `bin_id0`         | 0              | SEQ 또는 RR / RANDOM이 0 발행                       |
|                  |                        | `bin_id1`         | 1              | RR / RANDOM이 1 발행                                |
|                  |                        | `bin_id2`         | 2              | RR / RANDOM이 2 발행                                |
|                  |                        | `bin_id3`         | 3              | RR / RANDOM이 3 발행                                |
| `cp_cross_4kb`   | derived                | `bin_no`          | intent 안 넘음  | 정상 burst                                          |
|                  |                        | `bin_yes`         | intent 넘음     | Auto-split된 burst (spec Section 6.2)              |
| `cp_burst_gap`   | 직전 AR로부터 cycle 수 | `bin_b2b`         | 0              | Back-to-back                                        |
|                  |                        | `bin_1`           | 1              | 1-cycle gap                                         |
|                  |                        | `bin_2_4`         | `[2:4]`        | 작은 gap                                            |
|                  |                        | `bin_5_15`        | `[5:15]`       | 중간 gap                                            |
|                  |                        | `bin_16_plus`     | `[16:$]`       | 긴 gap                                              |
| `cp_outstanding` | 미완료 AR 개수         | `bin_1`           | 1              | Pipelining 없음                                     |
|                  |                        | `bin_2`           | 2              | 최소 pipeline                                       |
|                  |                        | `bin_3_7`         | `[3:7]`        | 중간                                                |
|                  |                        | `bin_8_15`        | `[8:15]`       | HLS 상한 근처                                       |
|                  |                        | `bin_max`         | `[16:$]`       | 상한 도달/초과 (build 의존)                         |
| Cross            | —                      | `cx_arlen_x_arid` | 4 × 4 grid     | Burst length × ID                                   |
| Cross            | —                      | `cx_arlen_x_outstanding` | 4 × 5 grid | Burst length × outstanding                       |

#### 4.1.3 `cg_axi_wr` — 매 AW 핸드셰이크마다 sample

`cg_axi_rd`와 동일한 모양에 AR을 AW로, `ARLEN` / `ARID`를 `AWLEN` /
`AWID`로 대체. 추가:

| Coverpoint            | Source            | Bin Name       | Values / Range | 목적                                    |
|-----------------------|-------------------|----------------|----------------|-----------------------------------------|
| `cp_wstrb_all_one`    | 매 beat의 `WSTRB` | `bin_full`     | `'1`           | 모든 W beat가 full-strobe (계약)        |

#### 4.1.4 `cg_axi_resp` — 매 B 핸드셰이크 및 매 RLAST beat마다 sample

| Coverpoint       | Source        | Bin Name       | Values / Range     | 목적                                      |
|------------------|---------------|----------------|--------------------|-------------------------------------------|
| `cp_bresp`       | `BRESP`       | `bin_okay`     | `2'b00`            | 정상 write 완료                           |
|                  |               | `bin_slverr`   | `2'b10`            | Slave error 주입                          |
|                  |               | `bin_decerr`   | `2'b11`            | Decode error 주입                         |
| `cp_rresp_last`  | RLAST 시 `RRESP` | `bin_okay`  | `2'b00`            | 정상 read 완료                            |
|                  |               | `bin_slverr`   | `2'b10`            | 주입된 read error                         |
|                  |               | `bin_decerr`   | `2'b11`            | 주입된 decode error                       |
| `cp_lat_rd`      | 트랜잭션당 cycle (AR→RLAST) | `bin_fast`     | `[1:15]`   | 빠른 slave                                |
|                  |               | `bin_medium`   | `[16:63]`          | 보통                                      |
|                  |               | `bin_slow`     | `[64:255]`         | 느림                                      |
|                  |               | `bin_very_slow`| `[256:1023]`       | 매우 느림                                 |
|                  |               | `bin_extreme`  | `[1024:$]`         | Stress                                    |
| `cp_lat_wr`      | 트랜잭션당 cycle (AW→B) | `cp_lat_rd`와 동일 bin           | Write 쪽 동일 의도                        |

#### 4.1.5 `cg_mapper` — DUT 경계에서 매 AR / AW마다 sample

| Coverpoint       | Source        | Bin Name          | Values / Range          | 목적                                          |
|------------------|---------------|-------------------|-------------------------|-----------------------------------------------|
| `cp_rr_sequence` | ID 전이       | `bin_0_to_1`      | prev=0, curr=1          | RR가 0→1 발행 확인                            |
|                  |               | `bin_1_to_2`      | prev=1, curr=2          | RR가 1→2 발행 확인                            |
|                  |               | `bin_2_to_3`      | prev=2, curr=3          | RR가 2→3 발행 확인                            |
|                  |               | `bin_3_to_0`      | prev=3, curr=0          | RR wrap 확인                                  |
| `cp_random_hist` | RANDOM 하 ID 값 | `bin_id0`,`bin_id1`,`bin_id2`,`bin_id3` | 0, 1, 2, 3 | LFSR 기반 ID 균형 sanity           |

#### 4.1.6 `cg_control` — 매 `ap_done`마다 sample

| Coverpoint         | Source              | Bin Name          | Values / Range    | 목적                                          |
|--------------------|---------------------|-------------------|-------------------|-----------------------------------------------|
| `cp_status`        | `STATUS[1:0]`       | `bin_clean`       | 2'b00             | 에러 없이 activation 완료                     |
|                    |                     | `bin_rd_err`      | 2'b01             | Read-side error만                              |
|                    |                     | `bin_wr_err`      | 2'b10             | Write-side error만                             |
|                    |                     | `bin_both`        | 2'b11             | 양쪽 다 error                                  |
| `cp_perf_mo_rd`    | `MO_MAX[15:0]`      | `bin_1`,`bin_2`,`bin_3_7`,`bin_8_15`,`bin_max` | 1, 2, [3:7], [8:15], [16:$] | 관측된 read MO 최대치 |
| `cp_perf_mo_wr`    | `MO_MAX[31:16]`     | `cp_perf_mo_rd`와 동일 bin           | 관측된 write MO 최대치                        |
| `cp_auto_restart`  | `CTRL.auto_restart` 카운터 | `bin_off` | 0 restart         | 표준 흐름                                     |
|                    |                     | `bin_on_1`        | 1 restart         | 1회 재실행                                    |
|                    |                     | `bin_on_2`        | 2 restart         | 2회 재실행                                    |
|                    |                     | `bin_on_3plus`    | 3+ restart        | 지속적 auto-restart                           |

### 4.2 Assertion Coverage

Assertion은 DUT bind 파일에 있음. 모든 assertion은 대응되는
`cover property`를 함께 가져서 "assertion이 N번 성공적으로 발화됐다"가
측정 가능.

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
| `a_ap_start_ignored` | ap_ctrl_hs| `!ap_idle && write(ap_start,1) |-> 상태 변화 없음`            | P0       |
| `a_ap_ready_pulse` | ap_ctrl_hs  | `$rose(ap_ready) |-> ##1 !ap_ready`                           | P0       |
| `a_ap_done_sticky` | ap_ctrl_hs  | `ap_done |-> ap_done throughout(!CTRL_read)`                  | P0       |
| `a_status_rd_err`  | STATUS      | `RVALID && RRESP != OKAY |-> ##1 STATUS.rd_error`             | P0       |
| `a_status_wr_err`  | STATUS      | `BVALID && BRESP != OKAY |-> ##1 STATUS.wr_error`             | P0       |
| `a_status_clear`   | STATUS      | `$rose(ap_start) |-> ##1 STATUS == '0`                        | P0       |
| `a_isr_not_cleared`| ISR         | `$rose(ap_start) |-> ISR 변화 없음`                           | P0       |
| `a_isr_rd_rise`    | ISR         | `$rose(STATUS.rd_error) |-> $rose(ISR.rd_err_int)`            | P0       |
| `a_isr_wr_rise`    | ISR         | `$rose(STATUS.wr_error) |-> $rose(ISR.wr_err_int)`            | P0       |
| `a_align_rd`       | Alignment   | `$rose(ap_start) |-> RD_ADDR[4:0] == 0`                       | P0       |
| `a_align_wr`       | Alignment   | `$rose(ap_start) |-> WR_ADDR[4:0] == 0`                       | P0       |
| `a_no_4kb_cross`   | Protocol    | 발행된 burst의 byte 범위가 4 KB를 안 넘음                     | P0       |
| `a_wlast_matches_awlen` | Protocol | 각 트랜잭션마다 `WLAST` 개수 == `AWLEN`                      | P0       |
| `a_rlast_matches_arlen` | Protocol | 각 트랜잭션마다 `RLAST` 개수 == `ARLEN`                      | P0       |

*(Xilinx AXI VIP가 이 DUT-specific 리스트 외에 추가 protocol assertion을
제공한다.)*

### 4.3 Configuration Coverage

여러 HLS build를 통해 채워진다. 각 build는 자신의 configuration을
`build_info_pkg`에 embed해 TB로 함께 컴파일된다.

| Coverpoint         | Values          | Source (HLS pragma)                | 목적                                |
|--------------------|-----------------|-------------------------------------|-------------------------------------|
| `cp_hls_mo_rd`     | 4, 8, 16, 32    | `num_read_outstanding`              | Build별 Read MO 상한                |
| `cp_hls_mo_wr`     | 4, 8, 16, 32    | `num_write_outstanding`             | Build별 Write MO 상한               |
| `cp_hls_burst_max` | 16, 32, 64, 128 | `max_read/write_burst_length`       | Burst length 상한                   |
| `cp_hls_pipeline`  | II=1, II=2      | `#pragma HLS PIPELINE II=...`       | Compute pipeline 깊이 (sweep 시)    |
| Cross              | 4 × 4 grid      | `cx_mo_rd_x_burst_max`              | MO / burst 상호작용                 |
| Cross              | 4 × 4 grid      | `cx_mo_wr_x_burst_max`              | MO / burst 상호작용                 |

### 4.4 Code / Toggle Coverage

- 모든 시뮬레이션 실행에서 xsim이 자동으로 수집.
- 목표:
  - `rtl/` (손수 작성된 wrapper)의 line coverage: 100% (모듈별 waiver).
  - `rtl/`의 branch coverage: 100% (모듈별 waiver).
  - Toggle coverage: report하지만 사인오프 gate로 삼지 않음.
  - HLS 생성 RTL: report하지만 gate 삼지 않음 (opaque; 기능 매칭으로
    검증).

## 5. 테스트 계획

### 5.1 Test Categories

| Cat | Name         | 목적                                                          |
|-----|--------------|---------------------------------------------------------------|
| C0  | Smoke        | Reset + 레지스터 접근만; boot smoke                            |
| C1  | Sanity       | Mode 별 단일 happy-path activation                             |
| C2  | Functional   | 데이터 경로 정확성, mode별, alignment / boundary sweep         |
| C3  | Protocol     | AXI protocol, ID mapping, 4 KB 경계                            |
| C4  | Perf         | Performance counter 정확성                                     |
| C5  | Stress       | Backpressure, latency, max MO, long batch                      |
| C6  | Error        | SLVERR / DECERR 주입, error propagation                        |
| C7  | Control      | Config latching, auto-restart, IRQ 처리                        |
| C8  | Reset        | Activation 중 reset, reset 후 접근                             |
| C9  | Full Random  | 대규모 CRV                                                    |

### 5.2 Test List

| Test                              | Cat | Priority | Notes                                                             |
|-----------------------------------|-----|----------|-------------------------------------------------------------------|
| `test_smoke_reset_and_regs`       | C0  | P0       | Reset 후 모든 register read; reset value 확인                     |
| `test_smoke_single_activation`    | C0  | P0       | 최소 MODE_FFT `NUM_FFTS=1` activation, done 관측                  |
| `test_sanity_mode_fft`            | C1  | P0       | 단일 FFT (impulse), bit-exact 데이터 확인                         |
| `test_sanity_mode_ro`             | C1  | P0       | 단일 READ_ONLY, write 트래픽 없음, dst untouched                  |
| `test_sanity_mode_wo`             | C1  | P0       | 단일 WRITE_ONLY, dst에 counter 패턴                               |
| `test_sanity_multi_activation`    | C1  | P0       | 서로 다른 config로 2회 연속 activation                            |
| `test_mode_fft_directed`          | C2  | P0       | `fft_dma_vectors.csv`의 MODE_FFT 시나리오                         |
| `test_mode_fft_random`            | C2  | P0       | 200+ 랜덤 케이스, `num_ffts ∈ [1, 1024]`                          |
| `test_mode_ro_directed`           | C2  | P0       | READ_ONLY, `ARVALID` 관측, `AWVALID` == 0 assertion              |
| `test_mode_wo_directed`           | C2  | P0       | WRITE_ONLY, dst 메모리에서 counter 패턴 확인                      |
| `test_addr_alignment_sweep`       | C2  | P0       | src / dst 주소를 정렬 bin 전반에 걸쳐 sweep                       |
| `test_num_ffts_boundary`          | C2  | P0       | `NUM_FFTS` = 1, 2, 63, 64, 65 (burst-fill 경계)                   |
| `test_axi_burst_lengths`          | C3  | P0       | Constrained-random `NUM_FFTS`로 `AxLEN` sweep                     |
| `test_4kb_boundary`               | C3  | P1       | Burst가 4 KB를 넘게 되는 src / dst 배치                           |
| `test_multi_id_reorder`           | C3  | P0       | `MAPPER_CTRL = RANDOM`, slave가 out-of-order 응답                 |
| `test_mapper_policy_rr`           | C3  | P0       | ROUND_ROBIN 시퀀스 확인                                           |
| `test_mapper_policy_seq`          | C3  | P1       | SEQUENTIAL에서 모든 ID == 0 확인                                  |
| `test_perf_counters`              | C4  | P0       | DUT counter vs TB scoreboard 교차 확인                            |
| `test_perf_mo_saturation`         | C4  | P1       | Slave stall로 MO를 HLS 상한까지 밀어올림                          |
| `test_stress_backpressure`        | C5  | P0       | 랜덤 slave READY stall, 데드락 없음 확인                          |
| `test_stress_random_latency`      | C5  | P0       | 트랜잭션별 랜덤 R/B 응답 latency; 기능, perf, 데드락 없음 확인    |
| `test_stress_max_mo`              | C5  | P1       | 긴 window 동안 max outstanding 유지                               |
| `test_stress_long_batch`          | C5  | P1       | `NUM_FFTS` 수천 단위                                              |
| `test_err_slverr_read`            | C6  | P0       | 하나의 R 응답에 SLVERR 주입, sticky flag / IRQ 확인               |
| `test_err_decerr_read`            | C6  | P0       | 위와 같지만 DECERR                                                |
| `test_err_slverr_write`           | C6  | P0       | B 응답에 SLVERR 주입                                              |
| `test_err_decerr_write`           | C6  | P0       | 위와 같지만 DECERR                                                |
| `test_err_continue_on_error`      | C6  | P0       | 에러 후 남은 트랜잭션 모두 완료 확인                              |
| `test_ctrl_config_latch`          | C7  | P0       | Activation 중 `RD_ADDR` 변경, 이번 run엔 무영향 확인              |
| `test_ctrl_auto_restart`          | C7  | P1       | auto_restart 활성화, restart 횟수 카운트                          |
| `test_ctrl_irq_ack`               | C7  | P0       | IRQ assert, ISR W1TC로 ack, drop 확인                             |
| `test_ctrl_ap_start_ignored`      | C7  | P1       | 실행 중 `ap_start=1` write, 무시 확인                             |
| `test_reset_mid_run`              | C8  | P1       | Activation 중 reset, clean state 확인                             |
| `test_reset_post_access`          | C8  | P0       | Reset 직후 모든 register 접근                                     |
| `test_full_random_1k`             | C9  | P1       | 1000회 랜덤 activation, 모든 mode                                 |
| `test_full_random_soak`           | C9  | P2       | 장시간 랜덤 실행, 모든 mode                                       |

### 5.3 단계 (α / β)

Test list는 두 릴리스 단계로 묶는다. 별도 release-candidate 단계는 없다.
α에 필요하지 않은 것은 전부 β.

**α (Alpha) — "동작하나? 옳게 실패하나?"** (18 tests)

기본 기능 커버리지 + 전체 에러 처리 커버리지. 여기 나열된 모든 test가
통과하면 α 종료.

- 모든 C0 Smoke (2)
- 모든 C1 Sanity (4)
- 모든 C2 Functional (6)
- C3 `test_axi_burst_lengths`, `test_mapper_policy_rr`, `test_multi_id_reorder`
- 모든 C6 Error (5)

**β (Beta) — "견고하고 완성되었나?"** (18 tests)

α에 없는 모든 것. Perf, stress, control edge case, reset 시나리오,
full-random 회귀 포함. 모든 β test가 통과하고 Section 7의 커버리지 목표를
만족하면 β 종료.

- C3 `test_4kb_boundary`, `test_mapper_policy_seq`
- 모든 C4 Perf (2)
- 모든 C5 Stress (4)
- 모든 C7 Control (4)
- 모든 C8 Reset (2)
- 모든 C9 Full Random (2)

### 5.4 상세 테스트 시나리오

각 시나리오는 커버하는 요구사항 ID, sequence가 정확히 어떤 자극을
만들어야 하는지, scoreboard / assertion이 무엇을 검증하는지, 채워질
covergroup (Section 4.1)이 무엇인지를 명시한다. 서브섹션 헤더 안에서
`test_` prefix는 간결성을 위해 생략.

#### C0 — Smoke

**`smoke_reset_and_regs`** — Covers F01, F06
- **Stimulus**: `rst_n = 0`을 ≥ 16 clock 유지 → 동기 deassert. `rst_n`이
  1로 돌아온 직후 사이클부터 `[0x00, 0x50]` 범위의 모든 정의된 offset에
  AXI4-Lite read를 발행.
- **Checks**: 각 register가 RTL spec Section 4에 정의된 reset value 반환.
  `a_align_rd / a_align_wr` assertion 침묵 (`ap_start` 아직 없음). `rst_n
  = 0` 동안 DUT-driven 모든 `*VALID`가 0.
- **Coverage**: 직접적 커버 없음 (이후 activation에서 `cg_control`만 채움).

**`smoke_single_activation`** — Covers F03, F16, F18
- **Stimulus**: `RD_ADDR = 0x100`, `WR_ADDR = 0x1000`, `NUM_FFTS = 1`,
  `MODE = FFT` 설정. Backing memory는 zero sample로 미리 채움.
  `CTRL.ap_start = 1` write.
- **Checks**: `ap_ready`가 single-cycle pulse로 관측됨. `ap_done`이
  10 000-cycle watchdog 내 관측됨. `a_ap_ready_pulse`,
  `a_ap_done_sticky` assertion 실패 없음. dst 데이터 값은 여기서 비교
  안 함.
- **Coverage**: `cg_config bin_fft × bin_1`; `cg_control bin_clean ×
  bin_off`.

#### C1 — Sanity

**`sanity_mode_fft`** — Covers F20, F23
- **Stimulus**: `NUM_FFTS = 1`, `MODE = FFT`, src = impulse (Q2.14에서
  `[1.0+0j, 0, 0, ...]`).
- **Checks**: dst 영역이 C-reference 출력 (`fft_dma_fx` 동일 입력)과
  bit-exact 일치. Scoreboard가 diff 없다고 보고.
- **Coverage**: `cg_config bin_fft × bin_1`, `cg_axi_rd bin_1_beat (or
  bin_2_4_beats)`, `cg_control bin_clean`.

**`sanity_mode_ro`** — Covers F21
- **Stimulus**: `NUM_FFTS = 1`, `MODE = READ_ONLY`, src에 랜덤 데이터.
- **Checks**: dst 영역이 pre-activation 스냅샷과 byte-compare 동일.
  `AWVALID == 0` assertion 유지. Read master가 정확히 `NUM_FFTS × 2`
  beat 발행.
- **Coverage**: `cg_config bin_ro`, `cg_axi_rd` beat 관측,
  `cg_axi_wr` 미 sample.

**`sanity_mode_wo`** — Covers F22
- **Stimulus**: `NUM_FFTS = 1`, `MODE = WRITE_ONLY`, dst 영역을 zero로
  미리 clear.
- **Checks**: 각 sample index `n ∈ [0, 16)`에 대해 `WR_ADDR + 4·n`의
  32-bit little-endian 값이 `n`과 일치. `ARVALID == 0` assertion 유지.
- **Coverage**: `cg_config bin_wo`, `cg_axi_wr` sample, `cg_axi_rd`
  미 sample.

**`sanity_multi_activation`** — Covers F03, F16
- **Stimulus**: 두 activation 연속. Activation 1: `NUM_FFTS = 1`,
  `RD_ADDR = 0x100`, `WR_ADDR = 0x1000`, `MODE=FFT`. 사이에 `ap_done`
  대기, `CTRL` read (clear), `RD_ADDR = 0x800`, `WR_ADDR = 0x2400`으로
  재프로그램.
- **Checks**: 두 activation 각각 독립적으로 완료. 각 결과가 해당 C-reference
  출력과 일치.
- **Coverage**: `cg_config` 두 번 sample; `cg_control` 두 번 sample
  (`bin_off` restart).

#### C2 — Functional

**`mode_fft_directed`** — Covers F08, F11, F20, F23
- **Stimulus**: `c_model/vectors/fft_dma_vectors.csv`의 모든 MODE_FFT
  시나리오 (10개) replay.
- **Checks**: 시나리오별 dst 영역 vs CSV expected 비교.
- **Coverage**: `cg_config bin_fft` + CSV가 자극하는 `num_ffts` 분포.

**`mode_fft_random`** — Covers F08, F20, F23
- **Stimulus**: 200회 랜덤 activation, `NUM_FFTS ∈ [1, 1024]`
  (log-uniform), 랜덤 32 B-정렬 주소, 랜덤 Q2.14 sample 데이터,
  `MODE = FFT`.
- **Checks**: 예상 메모리 내용에 대해 `fft_dma_fx_dpi`로 DPI, dst 영역
  byte 비교.
- **Coverage**: `cg_config`의 모든 `bin_fft × cp_num_ffts` bin이 결국
  hit; `cg_axi_rd`, `cg_axi_wr` bin 채움.

**`mode_ro_directed`** — Covers F18, F21
- **Stimulus**: `MODE = READ_ONLY`, `NUM_FFTS ∈ {1, 5, 50}`, 3개
  서로 다른 src 주소.
- **Checks**: 각각에서 write master idle assertion 유지, dst 스냅샷
  보존. `ap_done` 시 `RD_BEAT_CNT`가 `NUM_FFTS × 2`.
- **Coverage**: `cg_config bin_ro` × 세 개의 `num_ffts` bin.

**`mode_wo_directed`** — Covers F18, F22
- **Stimulus**: `MODE = WRITE_ONLY`, `NUM_FFTS ∈ {1, 5, 50, 500}`,
  서로 다른 dst 주소.
- **Checks**: 각각 counter 패턴 확인. Read master idle assertion.
  `WR_BEAT_CNT` = `NUM_FFTS × 2`.
- **Coverage**: `cg_config bin_wo` × 네 개의 `num_ffts` bin.

**`addr_alignment_sweep`** — Covers F06, F11, F12
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 4`. `cp_src_align` /
  `cp_dst_align`에 정의된 모든 alignment bin (32 B, 64 B, 128 B, 256 B,
  page start)에 걸쳐 `RD_ADDR` 및 `WR_ADDR` sweep.
- **Checks**: 각 alignment에서 데이터 correct.
- **Coverage**: `cg_config cp_src_align`, `cp_dst_align` 모든 bin 커버.

**`num_ffts_boundary`** — Covers F07
- **Stimulus**: `NUM_FFTS` 값 1, 2, 63, 64, 65, 127, 128, 129을 랜덤
  데이터와 함께 directed sweep.
- **Checks**: 각 `NUM_FFTS`에서 데이터 correct. Beat count assertion.
- **Coverage**: `cg_config cp_num_ffts`의 `bin_1`, `bin_2`, `bin_medium`,
  `bin_large`.

#### C3 — Protocol

**`axi_burst_lengths`** — Covers F13
- **Stimulus**: `cp_arlen` / `cp_awlen` 전체 bin set을 exercise하도록
  bias된 constrained-random `NUM_FFTS`.
- **Checks**: 고정 AXI-신호 assertion 실패 없음; 관측된 `AxBURST` 전부
  INCR, `AxSIZE` 전부 32 B 등.
- **Coverage**: `cg_axi_rd cp_arlen`, `cg_axi_wr cp_awlen` 모든 bin 채움;
  `a_ar_burst_incr` / `a_aw_burst_incr` assertion coverage.

**`4kb_boundary`** — Covers F14
- **Stimulus**: `NUM_FFTS = 4`. `RD_ADDR = 0x0FE0`, `WR_ADDR = 0x1FE0`
  으로 배치해서 첫 burst intent (64 B)가 4 KB를 넘게 됨. `NUM_FFTS = 8`
  및 더 큰 intent burst로도 반복.
- **Checks**: `a_no_4kb_cross` assertion 실패 없음. 관측된 AR / AW가
  더 짧은 두 burst로 split됨. `cp_cross_4kb bin_yes` hit.
- **Coverage**: `cg_axi_rd cp_cross_4kb bin_yes`, `cg_axi_wr` 동일.

**`multi_id_reorder`** — Covers F10, F15
- **Stimulus**: `MAPPER_CTRL.policy = RANDOM`. Slave BFM이 R 응답을
  요청 순서와 무관하게 ID간 interleave.
- **Checks**: dst 데이터가 C reference와 일치 (즉, mapper reorder
  buffer가 올바르게 재조립). 주어진 ID에 대해 RLAST out-of-order 없음
  (AXI 규칙).
- **Coverage**: `cg_axi_rd cp_arid` 네 bin 모두; `cg_axi_resp`.

**`mapper_policy_rr`** — Covers F10
- **Stimulus**: `MAPPER_CTRL.policy = ROUND_ROBIN`. 8개 이상 AR / AW를
  발행할 정도의 `NUM_FFTS`로 여러 activation.
- **Checks**: 관측된 `AxID` sequence가 0, 1, 2, 3, 0, 1, ...
  `cg_mapper cp_rr_sequence`의 네 transition 모두 hit.
- **Coverage**: `cg_mapper cp_rr_sequence`.

**`mapper_policy_seq`** — Covers F10
- **Stimulus**: `MAPPER_CTRL.policy = SEQUENTIAL`. 여러 activation.
- **Checks**: 모든 관측 `AxID == 0`. Assertion으로 강제 가능.
- **Coverage**: `cg_axi_rd cp_arid bin_id0` (그 bin만).

#### C4 — Perf

**`perf_counters`** — Covers F09
- **Stimulus**: 다양한 `NUM_FFTS`로 여러 activation. Slave BFM이
  재현 가능한 latency 프로파일 주입 (고정 seed).
- **Checks**: `ap_done` 후 `CYCLE_CNT`, `RD_BEAT_CNT`, `WR_BEAT_CNT`,
  `RD_LAT_ACC`, `WR_LAT_ACC`, `RD_TXN_CNT`, `WR_TXN_CNT`, `MO_MAX` read.
  TB scoreboard가 같은 이벤트를 DUT 경계에서 독립적으로 카운트했음;
  값 일치 필수.
- **Coverage**: `cg_control cp_perf_mo_rd/wr` 다양한 bin.

**`perf_mo_saturation`** — Covers F09
- **Stimulus**: Slave가 R 응답을 hold해서 outstanding read가 HLS build의
  `num_read_outstanding` 상한까지 상승.
- **Checks**: `MO_MAX[15:0]`이 build 선언 상한과 일치.
- **Coverage**: `cg_control cp_perf_mo_rd bin_max`.

#### C5 — Stress

**`stress_backpressure`** — Covers F13
- **Stimulus**: Slave BFM에서 `RREADY` / `AWREADY` / `WREADY` /
  `BREADY`의 랜덤 per-cycle stall. Mixed mode로 여러 activation.
- **Checks**: 모든 activation이 watchdog 내 `ap_done` 도달. 데이터 correct.
  AXI assertion 실패 없음.
- **Coverage**: `cg_axi_rd cp_burst_gap`, `cg_axi_wr cp_burst_gap` bin
  채워짐 (다양한 gap 관측).

**`stress_random_latency`** — Covers F09, F13, F15
- **Stimulus**: Slave BFM이 wide distribution (예: uniform 1–200 cycle)
  에서 뽑은 트랜잭션별 응답 latency를 R, B 채널 양쪽에 주입. Mapper
  policy = RANDOM. 다수 activation.
- **Checks**: (a) 데이터 C reference와 correct. (b) TB 계산 평균 latency
  ≈ `RD_LAT_ACC / RD_TXN_CNT` (허용 오차 내). (c) `MO_MAX` 자연 변동.
  (d) 데드락 없음.
- **Coverage**: `cg_axi_resp cp_lat_rd`, `cp_lat_wr` 모든 bin;
  `cg_control cp_perf_mo_*` 다양성.

**`stress_max_mo`** — Covers F15
- **Stimulus**: Slave가 긴 window (≥ 5 000 cycle) 동안 max-MO 상태
  (outstanding pool 만원)을 유지 후 release.
- **Checks**: Reorder buffer overflow 없음. 트랜잭션 손실 없음
  (BEAT / TXN counter 일관). 데이터 correct.
- **Coverage**: `cg_control cp_perf_mo_rd bin_max`, `cp_perf_mo_wr
  bin_max` 지속.

**`stress_long_batch`** — Covers F07
- **Stimulus**: `NUM_FFTS ≥ 5000`, 단일 activation.
- **Checks**: Activation 완료. Perf counter 예상대로 saturate (예:
  극단값에서 `CYCLE_CNT` saturation). dst 데이터 spot-check (10개 랜덤
  sample 위치).
- **Coverage**: `cg_config cp_num_ffts bin_massive`.

#### C6 — Error

**`err_slverr_read`** — Covers F05, F24, F25
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 10`. Slave BFM이 5번째 R
  응답에 `RRESP = SLVERR` 주입.
- **Checks**: Activation 후 `STATUS.rd_error = 1` (sticky). Rising
  edge에서 `ISR.rd_err_int` set. `GIE = 1 && IER.rd_err_int = 1`이면
  `irq_o` assert. 남은 모든 트랜잭션 발행/완료. `ap_done` 정상.
- **Coverage**: `cg_axi_resp cp_rresp_last bin_slverr`;
  `cg_control cp_status bin_rd_err`.

**`err_decerr_read`** — Covers F24
- **Stimulus**: 위와 같지만 `DECERR`.
- **Checks**: 동일. `STATUS.rd_error = 1`.
- **Coverage**: `cg_axi_resp cp_rresp_last bin_decerr`.

**`err_slverr_write`** — Covers F05, F24, F25
- **Stimulus**: `MODE = FFT`, `NUM_FFTS = 10`. Slave가 5번째 B 응답에
  `BRESP = SLVERR` 주입.
- **Checks**: `STATUS.wr_error = 1`. `ISR.wr_err_int` set. Continue-on-error
  확인.
- **Coverage**: `cg_axi_resp cp_bresp bin_slverr`;
  `cg_control cp_status bin_wr_err`.

**`err_decerr_write`** — Covers F24
- **Stimulus**: 위와 같지만 `DECERR`.
- **Checks**: `STATUS.wr_error = 1`.
- **Coverage**: `cg_axi_resp cp_bresp bin_decerr`.

**`err_continue_on_error`** — Covers F25
- **Stimulus**: 배치 중간에 에러 주입, 관측된 모든 AR / AW / B / R
  이벤트 log.
- **Checks**: 총 관측 beat count 및 TXN count가 pre-activation 예상과
  일치 (조기 abort 없음). `RD/WR_BEAT_CNT` 및 `RD/WR_TXN_CNT` register
  값이 TB 관측 count와 일치 → spec의 "에러 트랜잭션도 카운트" 규칙 확인.
- **Coverage**: `cg_control cp_status` 채워짐 (어느 error bin이든).

#### C7 — Control

**`ctrl_config_latch`** — Covers F17
- **Stimulus**: Activation A를 `RD_ADDR = A`로 시작. 실행 중에
  `RD_ADDR = A'` write. `ap_done` 대기. 추가 재프로그램 없이 즉시
  `ap_start` 재발행.
- **Checks**: Activation A는 처음부터 끝까지 주소 `A` 사용. Activation
  B는 주소 `A'` 사용. 두 activation의 데이터가 서로 영역 침범 없음.
- **Coverage**: sanity, 신규 bin 없음.

**`ctrl_auto_restart`** — Covers F19
- **Stimulus**: `CTRL.auto_restart = 1`, 작은 activation 프로그램,
  `ap_start = 1`. 3번째 restart 후 다음 `ap_done` 전에 `auto_restart`
  clear.
- **Checks**: 정확히 4회 activation 발생 (initial + 3 restart). 4번째
  후 DUT가 idle로 복귀.
- **Coverage**: `cg_control cp_auto_restart bin_on_3plus`.

**`ctrl_irq_ack`** — Covers F02, F04, F26
- **Stimulus**: `GIE = 1`, `IER.ap_done_int = 1` 프로그램. Activation
  시작. `irq_o = 1`일 때 `CTRL` read (`ap_done` clear)하고
  `ISR.ap_done_int`에 1 write (interrupt clear).
- **Checks**: `ISR` clear 후 `irq_o` deassert. `CTRL`만 read해서는
  `irq_o` deassert 안 됨.
- **Coverage**: `a_isr_not_cleared` assertion 발화.

**`ctrl_ap_start_ignored`** — Covers F03
- **Stimulus**: Activation 중 (`ap_idle = 0`), `ap_start = 1` write.
- **Checks**: 효과 없음. 원본 activation에 대해 `ap_ready` pulse가 한
  번만 관측됨. `a_ap_start_ignored` assertion 발화.
- **Coverage**: `a_ap_start_ignored`의 assertion coverage.

#### C8 — Reset

**`reset_mid_run`** — Covers F27
- **Stimulus**: `NUM_FFTS = 20`으로 activation 시작. 절반쯤 (예:
  `NUM_FFTS/2` 완료 후) reset assert.
- **Checks**: DUT 즉시 `ap_idle = 1`로 복귀. Deassert 후 모든 register
  reset value. 다음 activation 정상 완료.
- **Coverage**: reset 동작 assertion coverage.

**`reset_post_access`** — Covers F01, F06
- **Stimulus**: Reset ≥ 16 cycle assert → deassert. Deassert 직후
  사이클에 모든 RW register에 AXI-Lite write, 그 후 read로 확인.
- **Checks**: 모든 write 수용, 모든 read가 write된 값 반환. AXI-Lite
  protocol 위반 없음.
- **Coverage**: sanity.

#### C9 — Full Random

**`full_random_1k`** — Covers F20, F21, F22, F23
- **Stimulus**: 모든 mode, 주소 범위, mapper policy, slave-timing
  프로파일에 걸친 1 000회 랜덤 activation.
- **Checks**: 매 activation에 대해 DPI 기반 golden. Assertion clean.
  Scoreboard가 mismatch 없다고 보고.
- **Coverage**: 모든 covergroup에 걸쳐 광범위 채움.

**`full_random_soak`** — Covers all
- **Stimulus**: 100 000+ activation, 확장된 random space. 장시간
  discovery 목적.
- **Checks**: `full_random_1k`과 동일, 지평선 확장. 실패 seed 아카이빙
  (Section 9 open questions 참조).
- **Coverage**: `full_random_1k`가 못 채운 남은 gap 폐쇄.

## 6. 골든 레퍼런스 및 스코어보드

### 6.1 Reference 선택

- C 참조 모델 (`c_model/fft_dma.c`)이 유일한 golden.
- DUT 출력과 reference 간 divergence는 크기와 무관하게 P0 버그.
  **Reference가 tolerance 자체이다**; DUT / reference 비교 경계에는
  fuzz margin 없음.

### 6.2 DPI-C Linkage (계획)

UVM 스코어보드는 C 테스트가 쓰는 동일한 fixed-point 구현을 DPI-C로
호출한다. 계획된 prototype:

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

C 쪽 노출:

```c
// c_model/fft_dma_dpi.c (fft_dma_fx의 얇은 wrapper)
void fft_dma_fx_dpi(svOpenArrayHandle mem, uint64_t src, uint64_t dst,
                    uint32_t num_ffts, int mode);
```

`MODE_FFT`, `MODE_WRITE_ONLY`에서 destination 영역의 byte-exact 비교.
`MODE_READ_ONLY`에서는 destination 영역이 pre-activation 내용 대비
untouched 확인.

### 6.3 CSV Replay 경로

C 모델 사인오프에 쓴 동일 CSV vector 파일을 RTL 사인오프에도 replay
가능 → RTL 사인오프용 golden 별도 생성 불필요.

- Utility: `tb/utils/csv_replay.sv` (계획) — `c_model/vectors/*.csv`를
  파싱, `in_re / in_im`으로 slave BFM의 backing memory 초기화, 케이스마다
  AXI-Lite agent로 1회 activation 구동, expected output을 scoreboard로
  push하는 UVM sequence.
- Curated (`fft_dma_vectors.csv`)와 random regression 파일
  (`fft_dma_random_*.csv`) 모두 replay 가능.
- Random 파일 생성에 쓴 seed는 파일 header comment에 embed. 실패는 CSV
  경로 참조로 exact case 재현.

### 6.4 실패 시맨틱

- Byte mismatch → scoreboard가 (activation id, sample index, got,
  expected) 보고 후 test abort.
- CSV replay가 문제 row 기록.
- DPI 실패 (C 측 exception, allocation 실패)는 `uvm_fatal` 경로.

## 7. 사인오프 기준

Section 5.3에 따라 두 단계.

**α 사인오프**

- 모든 α test 통과 (18개).
- 관련 feature 영역 (functional + error)의 assertion이 예기치 않은
  실패 없음.
- Scoreboard가 모든 α test에서 zero mismatch 보고.
- α는 "동작하나? 옳게 실패하나?" 게이트. 커버리지 목표는 α 종료 시점에
  강제하지 않음.

**β 사인오프 (프로젝트 사인오프)**

- 전체 HLS config sweep에 걸쳐 모든 α + β test 통과.
- Weighted functional coverage ≥ 95% (waiver는 이 파일에 inline 문서화).
- `rtl/`의 line + branch coverage ≥ 100% (모듈별 waiver 문서화).

## 8. Traceability

요구사항은 Section 3의 ID 컬럼 (`F01`, `F02`, ...)으로 추적. Section
5.2의 모든 test는 file header comment에 자신이 커버하는 requirement ID를
나열. Coverage report는 covergroup 이름 옆에 requirement ID를 emit
하여 gap이 requirement별로 보이도록 함.

Test 파일 안의 traceability format:

```systemverilog
// Covers: F03, F16, F18 (ap_ctrl_hs handshake and done semantics)
```

## 9. 미결정 사항

- C-model DPI-C linkage를 bit-accurate scoreboard로 쓸지 vs SV에서
  golden을 재계산할지. Bit-accurate가 선호되지만 DPI 배관 필요; α
  이후로 지연 가능.
- HLS config sweep을 config별 testbench build로 몰지, 컴파일 타임 /
  파라미터 기반 config switch가 있는 단일 testbench로 몰지. 첫 HLS
  build 사용 가능해질 때까지 지연.
- `NUM_FFTS = 0`: spec은 동작을 undefined로 표시. DUT의 실제 동작을
  못박는 directed test (예: 모든 counter 0인 조기 `ap_done`)를 추가할지
  아니면 커버 표면 밖에 둘지 결정 필요.
- `FFT_CFG` (0x28)는 reserved / no-op. Register write 후 unchanged
  read-back하는 짧은 test 추가할지 아니면 reset-and-regs smoke의 암묵적
  커버에 맡길지 결정 필요.
- 실패 seed 아카이빙 정책 (버그 재현 CSV 저장 위치 — repository issue,
  별도 디렉토리, ignored 파일?). 첫 실제 회귀 실패가 나올 때까지 지연.
- Regression 자동화 메커니즘 (git hook / CI / cron)은 이 문서에서
  다루지 않는다. RTL / TB 코드베이스가 수동 호출이 병목이 될 만큼
  커질 때까지 지연.

## Change Log

- 2026-08-20: 초기 버전 생성 및 전 섹션 채움. VPLAN 뼈대 → 세 번의
  대규모 revision을 거쳐 최종 형태 도달:
  1) Smoke / Sanity 분리, `test_stress_random_latency` 추가, regression
     test 이름을 `test_full_random_*`로 rename, 4-milestone ordering을
     2-stage α / β 구조로 대체 (errors를 α에 편입).
  2) Section 7 (Regression Strategy)을 자동화 미조기로 삭제. Feature
     Matrix에 Primary Tests 컬럼 추가, Golden Reference 섹션을 DPI-C
     prototype과 CSV replay 세부로 확장, `NUM_FFTS=0`, `FFT_CFG` verify,
     seed archiving, 자동화 관련 open question 추가.
  3) Section 4를 표 형식으로 재작성 (covergroup → coverpoint → named
     bin → 목적). 36개 test 전체에 대한 상세 시나리오 (stimulus,
     checks, coverage points)를 Section 5.4에 추가.
- 2026-08-20: 한국어 번역본 (`GVP_VPLAN_KOR.md`) 생성.
