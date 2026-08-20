# GVP RTL 명세서

Gyusup Verification Platform의 DUT(FFT + DMA 블록)에 대한 명세서.
이 문서는 영문 원본 `GVP_RTL_SPEC.md`의 한국어 번역본이며, 두 문서가
일치하지 않을 경우 영문본을 우선으로 한다.

---

## 1. 최상위 개요

DUT는 Xilinx Vitis HLS로 대부분 생성되고, HLS 단독으로는 만들 수 없는
기능(멀티 ID AXI 트래픽, 성능 모니터링)을 추가하기 위해 소량의 손수
작성된 SystemVerilog RTL로 감싸진 FFT + DMA 블록이다. 호스트는 AXI4-Lite
슬레이브 인터페이스를 통해 DUT를 제어하고, 데이터는 두 개의 AXI4
마스터(하나는 read, 하나는 write) 인터페이스로 이동한다.

**기능.** 한 activation마다 DUT는 다음을 수행한다:

1. Read 마스터로 `RD_ADDR`부터 `NUM_FFTS * 16`개의 복소 sample을 외부
   메모리에서 읽는다.
2. 16-sample 청크마다 1개씩 16-point radix-2 forward FFT를 수행한다.
   연산은 Q2.14 fixed-point이며 각 stage마다 `>> 1` 블록 스케일링이
   적용된다.
3. 결과를 Write 마스터로 `WR_ADDR`부터 외부 메모리에 쓴다.

단일 마스터만 검증하기 위한 read-only / write-only 모드도 제공된다
(Section 8 참조).

**외부 인터페이스.** 단일 clock / active-low reset 페어(Section 2),
설정과 상태를 위한 AXI4-Lite slave(Section 3.1), 데이터용 두 개의
AXI4 master(Section 3.2 / 3.3), level-sensitive 인터럽트 출력
(Section 3.4).

**계층 구조.** DUT top wrapper는 다음을 포함한다:

- HLS 코어(`fft_dma`). AXI 트랜잭션을 항상 `ARID = 0` / `AWID = 0`
  으로 발행한다.
- 손수 작성된 ID mapper wrapper 두 개(마스터별 1개). `MAPPER_CTRL`
  정책에 따라 AXI ID를 재할당하고, read 쪽에서는 out-of-order 응답을
  요청 순서로 재정렬해 HLS 코어에 돌려준다.
- 손수 작성된 performance monitor 두 개. Mapper와 DUT 경계 사이에
  위치해 `PERF_CNT` 레지스터 블록에 값을 공급한다.

**설정 모델.** 모든 설정은 AXI4-Lite 레지스터로 이루어진다. 값은
`ap_ready` 사이클에 latch되므로, activation 도중에 쓴 값은 다음
activation부터 유효하다(Section 7.2).

**검증 관점.** 특정 AXI 신호(`AxBURST`, `AxCACHE`, `AxPROT`, `AxQOS`,
`AxLOCK`, `AxSIZE`, `WSTRB`)는 설계로 고정된다. 이들은 assertion +
coverage 형태의 "계약 준수 검증"으로 다루며, 실제 자극 다양성의 축은
`NUM_FFTS`, `RD_ADDR` / `WR_ADDR`, `MODE`, `MAPPER_CTRL.policy`,
sample 데이터, 그리고 slave 타이밍이다.

---

## 2. Clock과 Reset

### 2.1 Clock 도메인

- DUT 전체가 단일 clock 도메인.
- 모든 인터페이스(AXI4-Lite slave, AXI4 read master, AXI4 write
  master)와 내부 FFT 연산은 같은 clock을 공유한다.
- 신호명: `clk`.
- 근거: HLS로 만들어진 블록은 대개 단일 clock으로 합성된다. Top
  레벨을 단일 도메인으로 유지하면 CDC 설계·검증 부담이 사라진다.
  Async FIFO wrapping이 필요하면 통합하는 상위 SoC의 몫이며 DUT
  자체의 책임이 아니다.

### 2.2 Reset

- 신호명: `rst_n`.
- Polarity: active-low.
- 방식: 비동기 어썰트, 동기 디어썰트 (Xilinx AXI 관례).
- 최소 어썰트 유지 시간: 16 clock cycle (AXI 권장).
- 모든 레지스터, outstanding 트랜잭션 상태, FSM은 reset 동안 clear
  되어야 한다. Reset 해제 이후 어떤 출력도 X로 구동되어서는 안 된다.

---

## 3. 인터페이스 요약

DUT는 3개의 AXI 인터페이스와 1개의 인터럽트 출력을 노출한다. 모든
인터페이스는 Section 2에서 정의한 `clk` / `rst_n` 페어를 공유한다.

| 인터페이스 | 종류            | 신호 prefix           | 비고                     |
|-----------|-----------------|-----------------------|--------------------------|
| Control   | AXI4-Lite slave | `s_axi_lite_`         | 레지스터 접근            |
| Data read | AXI4 master     | `m_axi_gmem_rd_`      | AR / R 채널만            |
| Data write| AXI4 master     | `m_axi_gmem_wr_`      | AW / W / B 채널만        |
| Interrupt | 단일 wire       | `irq_o`               | Level-sensitive, active-high |

### 3.1 AXI4-Lite Slave (Control)

- Data width: 32 bit.
- Address width: 12 bit (4 KB 레지스터 공간, 확장 여유).
- 표준 AXI4-Lite 채널 AW / W / B / AR / R. Sideband 신호 없음.
- 신호 prefix: `s_axi_lite_`.
- 근거: 32-bit data는 AXI4-Lite 관례이자 HLS 기본값. 12-bit address는
  Xilinx IP 관례에 맞고 확장 여지도 남긴다.

### 3.2 AXI4 Master #1 (Read)

- Data width: 256 bit.
- Address width: 64 bit.
- ID width: 2 bit (ID 0 ~ 3).
- 채널: AR, R (read-only master. AW / W / B 없음).
- Sideband 신호(`ARUSER`, `RUSER` 등): 사용하지 않음.
- 신호 prefix: `m_axi_gmem_rd_`.

### 3.3 AXI4 Master #2 (Write)

- Data width: 256 bit.
- Address width: 64 bit.
- ID width: 2 bit (ID 0 ~ 3).
- 채널: AW, W, B (write-only master. AR / R 없음).
- Sideband 신호(`AWUSER`, `WUSER`, `BUSER`): 사용하지 않음.
- 신호 prefix: `m_axi_gmem_wr_`.

### 3.4 인터럽트 / 상태 출력

- 단일 인터럽트 라인 `irq_o`.
- Polarity: active-high, level-sensitive (Xilinx AXI IP 관례).
- 현재 activation이 `done` 상태에 도달하면 어썰트되고, AXI-Lite
  레지스터 인터페이스로 `done` 플래그를 clear할 때까지 유지된다
  (Section 7 참조).
- `STATUS` / `CTRL` 레지스터를 통한 polling도 지원되며, 인터럽트
  사용 여부는 통합자·테스트벤치의 선택이다.
- 다른 상태 출력은 없다. 모든 상태는 레지스터로 노출한다.

---

## 4. 레지스터 맵

모든 레지스터는 32-bit 폭, word-aligned(4-byte offset)이다. 배치는
Xilinx HLS `ap_ctrl_hs` 관례를 따르므로, HLS가 생성한 slave 로직을
그대로 재사용할 수 있다.

| Offset | 레지스터     | Access | Reset | 설명                                          |
|--------|-------------|--------|-------|-----------------------------------------------|
| 0x00   | `CTRL`       | RW     | 0     | 제어 / 핸드셰이크 (4.1 참조)                  |
| 0x04   | `GIE`        | RW     | 0     | Global interrupt enable                       |
| 0x08   | `IER`        | RW     | 0     | IP interrupt enable                           |
| 0x0C   | `ISR`        | W1TC   | 0     | IP interrupt status (write-1-to-clear)        |
| 0x10   | `RD_ADDR_LO` | RW     | 0     | Read base address, `[31:0]`                   |
| 0x14   | `RD_ADDR_HI` | RW     | 0     | Read base address, `[63:32]`                  |
| 0x18   | `WR_ADDR_LO` | RW     | 0     | Write base address, `[31:0]`                  |
| 0x1C   | `WR_ADDR_HI` | RW     | 0     | Write base address, `[63:32]`                 |
| 0x20   | `NUM_FFTS`   | RW     | 0     | 이번 activation에서 처리할 16-point FFT 개수  |
| 0x24   | `MODE`       | RW     | 0     | 0=`FFT`, 1=`READ_ONLY`, 2=`WRITE_ONLY`        |
| 0x28   | `FFT_CFG`    | RW     | 0     | 미래 FFT 설정용, 현재 예약                    |
| 0x2C   | `STATUS`     | RO     | 0     | 에러 / 상태 플래그 (4.4 참조)                 |
| 0x30   | `CYCLE_CNT`  | RO     | 0     | `ap_start` ~ `ap_done` 사이 clock cycle 수    |
| 0x34   | `RD_BEAT_CNT`| RO     | 0     | 이번 activation의 R beat 수                   |
| 0x38   | `WR_BEAT_CNT`| RO     | 0     | 이번 activation의 W beat 수                   |
| 0x3C   | `RD_LAT_ACC` | RO     | 0     | Read latency 누적 (4.9 참조)                  |
| 0x40   | `WR_LAT_ACC` | RO     | 0     | Write latency 누적 (4.9 참조)                 |
| 0x44   | `MO_MAX`     | RO     | 0     | Outstanding 최대치: `[15:0]`=read, `[31:16]`=write |
| 0x48   | `RD_TXN_CNT` | RO     | 0     | 완료된 read 트랜잭션 수 (RLAST 개수)          |
| 0x4C   | `WR_TXN_CNT` | RO     | 0     | 완료된 write 트랜잭션 수 (B 핸드셰이크 개수)  |
| 0x50   | `MAPPER_CTRL`| RW     | 0     | ID mapper 정책 (4.10 참조)                    |

`[0x00, 0xFFF]` 범위 내의 위 목록에 없는 offset은 모두 reserved이다.
Write는 무시되고 read는 0을 반환한다.

### 4.1 `CTRL` (0x00) — 제어 / 핸드셰이크

Xilinx HLS `ap_ctrl_hs` 프로토콜을 따른다.

| Bit  | 이름            | Access | Reset | 설명                                                       |
|------|-----------------|--------|-------|------------------------------------------------------------|
| 0    | `ap_start`      | W1S    | 0     | 1을 쓰면 activation 시작. `ap_ready` 시 HW가 자동 clear.   |
| 1    | `ap_done`       | COR    | 0     | Activation 완료 시 set. Read하면 clear.                     |
| 2    | `ap_idle`       | RO     | 1     | Idle 상태에서 1.                                            |
| 3    | `ap_ready`      | RO     | 0     | `ap_start`를 수락한 사이클에 1 cycle 동안 1.                |
| 6:4  | Reserved        | RO     | 0     |                                                            |
| 7    | `auto_restart`  | RW     | 0     | 1이면 `ap_done` 시 HW가 자동으로 `ap_start`를 재어썰트.     |
| 31:8 | Reserved        | RO     | 0     |                                                            |

Access 표기: `W1S` = write 1 to set; `COR` = clear on read; `RO` = read-only.

### 4.2 `GIE` (0x04) — Global Interrupt Enable

| Bit  | 이름    | Access | Reset | 설명                                                 |
|------|---------|--------|-------|------------------------------------------------------|
| 0    | `gie`   | RW     | 0     | 1 = 인터럽트 출력 enable; 0 = `irq_o` 강제 0.        |
| 31:1 | Reserved| RO     | 0     |                                                      |

### 4.3 `IER` / `ISR` (0x08 / 0x0C) — IP Interrupt Enable / Status

| Bit  | 이름            | IER Access | ISR Access | 설명                                                            |
|------|-----------------|------------|------------|-----------------------------------------------------------------|
| 0    | `ap_done_int`   | RW         | W1TC       | Activation 완료 시 인터럽트.                                    |
| 1    | `ap_ready_int`  | RW         | W1TC       | `ap_ready` 시 인터럽트.                                         |
| 2    | `rd_err_int`    | RW         | W1TC       | Read-side 응답 에러(`STATUS.rd_error` 상승) 시 인터럽트.        |
| 3    | `wr_err_int`    | RW         | W1TC       | Write-side 응답 에러(`STATUS.wr_error` 상승) 시 인터럽트.       |
| 31:4 | Reserved        | RO         | RO         |                                                                 |

`irq_o = GIE.gie & |(IER & ISR)`.

`rd_err_int` / `wr_err_int` ISR 비트는 `STATUS.rd_error` /
`STATUS.wr_error`가 0에서 1로 전이하는 사이클(즉 activation 중 최초
에러 응답 시점)에 set된다. `ISR`에 W1TC로 쓰지 않는 한 clear되지
않는다. `STATUS` 플래그가 `ap_start`에서 자동 clear되어도 ISR 비트는
**clear되지 않는다**.

### 4.4 `STATUS` (0x2C) — 에러 / 상태 플래그

| Bit  | 이름       | Access | Reset | 설명                                                        |
|------|------------|--------|-------|-------------------------------------------------------------|
| 0    | `rd_error` | RO     | 0     | Read master가 `SLVERR` 또는 `DECERR` 수신. Activation 내 sticky. |
| 1    | `wr_error` | RO     | 0     | Write master가 `SLVERR` 또는 `DECERR` 수신. Activation 내 sticky. |
| 2    | `overflow` | RO     | 0     | 연산 오버플로 플래그용 예약 (Section 9.6).                  |
| 31:3 | Reserved   | RO     | 0     |                                                             |

Sticky 플래그는 `ap_start`에서 자동 clear된다. 성능 카운터 정책과
일치한다. 자세한 에러 처리 규칙은 Section 10 참조.

### 4.5 주소 레지스터 시맨틱

- `RD_ADDR_LO`와 `RD_ADDR_HI`가 합쳐서 64-bit read base address를
  구성한다. 이어붙임은 `{RD_ADDR_HI, RD_ADDR_LO}`.
- `WR_ADDR_LO` / `WR_ADDR_HI`도 동일한 방식.
- `ap_start`를 쓰기 전에 두 반쪽 모두 프로그램되어 있어야 한다.
- 정렬 요구 조건은 Section 5.4에서 정의한다.

### 4.6 `NUM_FFTS` (0x20)

- 이번 activation에서 처리할 16-point FFT 개수. 부호 없는 32-bit.
- `ap_start` 시 non-zero여야 한다. `NUM_FFTS = 0` 시의 동작은 미정의
  (추후에 조기 `ap_done` 어썰트로 규정할 수 있음).

### 4.7 `MODE` (0x24)

- `[1:0]`이 동작 모드를 선택하며, 상위 비트는 reserved.
- `0` = `MODE_FFT`, `1` = `MODE_READ_ONLY`, `2` = `MODE_WRITE_ONLY`.
- 각 모드의 상세 동작은 Section 8 참조.

### 4.8 `FFT_CFG` (0x28)

- 예약. 하드웨어는 모든 비트를 무시. Read는 마지막에 쓴 값을 반환.

### 4.9 Performance Counters (0x30 – 0x4C)

모든 performance counter는 32-bit read-only. 매 `ap_start`마다 자동으로
clear되고 `ap_done` 어썰트 시 갱신이 멈추므로, `ap_done` 이후에 읽은
값은 가장 최근에 완료된 activation의 값을 반영한다.

**측정 위치.** Performance monitor는 ID mapper wrapper(Section 6.3)와
DUT의 AXI 경계 사이에 위치한다. 이 지점이 실제 AXI 프로토콜이
드러나는 곳이다. ID 0 ~ 3 전부가 나타나고, out-of-order 응답도
관측된다. 여기서 측정해야 slave 관점의 실제 동작(latency,
backpressure, MO 활용)을 정확히 잡을 수 있다. HLS 코어 내부의
in-order / 단일 ID 관점이 아니다.

**ID tracking.** 마스터별(read / write) per-ID FIFO 4개가 outstanding
요청의 핸드셰이크 cycle을 저장한다. 대응되는 응답 핸드셰이크에서
FIFO를 pop하고, 경과 cycle 수를 `RD_LAT_ACC` / `WR_LAT_ACC`에
누적한다. AXI 순서 규칙상 같은 ID의 응답은 순서를 지키므로 FIFO
엔트리에 ID 필드가 없어도 요청-응답 매칭이 성립한다. FIFO 깊이는
각 마스터의 `num_*_outstanding` 이상이어야 한다.

카운터 정의:

- **`CYCLE_CNT`**: `ap_start`(포함) ~ `ap_done`(포함) 사이 `clk` cycle
  수. 2^32 − 1 cycle 이상 걸리면 `0xFFFF_FFFF`에서 saturate.
- **`RD_BEAT_CNT`**: DUT 경계에서 관측된 R 채널 data-beat 핸드셰이크
  수 (`RVALID & RREADY`).
- **`WR_BEAT_CNT`**: DUT 경계에서 관측된 W 채널 data-beat 핸드셰이크
  수 (`WVALID & WREADY`).
- **`RD_LAT_ACC`**: 트랜잭션별 latency의 합. 한 트랜잭션의 latency는
  AR 핸드셰이크부터 같은 ID의 매칭되는 RLAST 사이 cycle 수. 모든 ID에
  대한 aggregate이며 per-ID latency는 노출하지 않는다. `0xFFFF_FFFF`
  에서 saturate.
- **`WR_LAT_ACC`**: 마찬가지로 AW 핸드셰이크와 매칭되는 B 핸드셰이크
  사이 cycle 수의 합. 모든 ID aggregate. `0xFFFF_FFFF` saturate.
- **`MO_MAX`**: 이번 activation의 outstanding 최대 관측값. `[15:0]`은
  read 마스터의 최대치(AR 발행 후 아직 RLAST 못 받은 요청 수 총합),
  `[31:16]`은 write 마스터의 최대치. 각 half는 `0xFFFF`에서 saturate.
- **`RD_TXN_CNT`**: 이번 activation에서 완료된 read 트랜잭션 수
  (RLAST 개수와 동일).
- **`WR_TXN_CNT`**: 이번 activation에서 완료된 write 트랜잭션 수
  (B 핸드셰이크 개수와 동일).

테스트벤치가 off-chip에서 계산할 수 있는 파생값:

- 평균 read latency: `RD_LAT_ACC / RD_TXN_CNT`.
- 평균 write latency: `WR_LAT_ACC / WR_TXN_CNT`.
- Read throughput (byte / cycle): `RD_BEAT_CNT * 32 / CYCLE_CNT`.
- Write throughput (byte / cycle): `WR_BEAT_CNT * 32 / CYCLE_CNT`.

주의:
- Wrap 대신 saturate로 두어 post-run 분석의 모호함을 방지.
- `ap_start`에서 clear되므로 소프트웨어는 다음 activation 전에 반드시
  값을 읽어야 한다.

### 4.10 `MAPPER_CTRL` (0x50) — ID Mapper 정책

HLS 코어(항상 ID 0)가 발행한 트랜잭션에 대해 ID mapper wrapper가 AXI
ID를 어떻게 부여할지 제어한다. Read / write 마스터 양쪽에 동일한
정책이 적용된다.

| Bit  | 이름       | Access | Reset | 설명                                                |
|------|------------|--------|-------|-----------------------------------------------------|
| 1:0  | `policy`   | RW     | 0     | 0=`SEQUENTIAL`, 1=`ROUND_ROBIN`, 2=`RANDOM`          |
| 31:2 | Reserved   | RO     | 0     |                                                     |

정책:

- `SEQUENTIAL`: 항상 ID 0 (pass-through와 동일). Mapper 앞뒤 동작
  상관 관계를 파악할 때 유용.
- `ROUND_ROBIN`: 트랜잭션 요청 순서대로 ID 0 → 1 → 2 → 3 → 0 → ...
  Read / write 여부와 무관하게 순환.
- `RANDOM`: 매 트랜잭션마다 {0, 1, 2, 3} 중 균등 무작위 ID. Reset에
  seeding된 내부 LFSR 사용.

정책은 트랜잭션 요청마다 sample된다. 언제든 재프로그램 가능하며 이후
트랜잭션부터 유효하다. 소프트웨어는 activation 도중 정책이 뒤바뀌는
상황을 피하기 위해 보통 DUT가 idle일 때 정책을 설정한다.

---

## 5. 메모리 데이터 레이아웃

### 5.1 Sample 포맷

- 각 sample은 복소값 `(real, imag)`이며 real / imag 각각 Q2.14 부호
  있는 fixed-point, 16-bit이다. Sample 총폭은 32-bit.
- 워드 내 배치: `sample[15:0] = real`, `sample[31:16] = imag`.
- 메모리 상 byte order: AXI4 규약을 따르는 little-endian. Real이
  `+0, +1` 바이트, Imag이 `+2, +3` 바이트.

### 5.2 Beat 패킹

- AXI data width는 256-bit = 32 byte = beat당 8 sample.
- Sample index 오름차순이 bit position 오름차순으로 packing된다:

  | Bit range  | Sample index |
  |------------|--------------|
  | `[ 31:  0]`| 0            |
  | `[ 63: 32]`| 1            |
  | `[ 95: 64]`| 2            |
  | `[127: 96]`| 3            |
  | `[159:128]`| 4            |
  | `[191:160]`| 5            |
  | `[223:192]`| 6            |
  | `[255:224]`| 7            |

- 따라서 16-point FFT 1개는 정확히 2 beat를 차지한다. 첫 beat에 sample
  0 ~ 7, 두 번째 beat에 sample 8 ~ 15.

### 5.3 배치 레이아웃

배치 내 FFT들은 padding 없이 연속으로 packing된다. FFT `k`는 byte
범위 `[base + k * 64, base + (k + 1) * 64)`를 차지한다. `base`는 입력
쪽이면 `RD_ADDR`, 출력 쪽이면 `WR_ADDR`. 각 FFT가 정확히 64 byte
(2 beat)이므로 `base`가 5.4의 정렬 조건만 만족하면 각 FFT는 32-byte
beat 크기에 자연스럽게 정렬된다.

Activation당 전송량:

- Read side: `NUM_FFTS * 2` beat = `NUM_FFTS * 64` byte.
- Write side: `NUM_FFTS * 2` beat = `NUM_FFTS * 64` byte.

### 5.4 주소 정렬 요구 사항

- `RD_ADDR`, `WR_ADDR`은 32-byte 정렬이어야 한다(비트 `[4:0]` = 0).
  DUT는 partial-beat 처리를 하지 않는다. 정렬 준수는 소프트웨어의
  계약이다.
- 정렬을 위반한 경우 동작은 undefined이다. 검증은 directed stimulus로
  misaligned base를 자극하지 말 것.

### 5.5 AXI 4 KB 경계

AXI4는 단일 burst가 4 KB 경계를 넘는 것을 금지한다. Master가 발행하려는
burst가 이 경계를 넘게 되면 HLS가 생성한 master가 자동으로 두 burst로
분할한다. 5.4를 넘어서는 추가 padding이나 정렬은 이 규칙 준수를 위해
필요하지 않다.

---

## 6. 트랜잭션 동작

### 6.1 AXI Burst 파라미터

아래 값은 별도 언급이 없는 한 read master(AR 채널)와 write master
(AW / W 채널) 양쪽에 적용된다. 고정값은 설계로 강제되며, 테스트벤치의
assertion과 coverage로 확인한다(계약 준수 검증).

| 필드                  | 값                | 비고                                                        |
|-----------------------|-------------------|-------------------------------------------------------------|
| `AWSIZE` / `ARSIZE`   | `0b101` (32 B)    | 256-bit data width와 매칭. Narrow transfer 없음.            |
| `AWBURST` / `ARBURST` | `0b01` (INCR)     | INCR만 지원. FIXED / WRAP 사용 안 함.                       |
| `AWLEN` / `ARLEN`     | 0 ~ 15 (1 ~ 16 beat) | Burst당 최대 16 beat. 실제 값은 HLS master가 결정. 4 KB 규칙으로 더 짧게 나뉠 수 있음. |
| `AWCACHE` / `ARCACHE` | `0b0011`          | Modifiable + Bufferable, non-cacheable.                     |
| `AWPROT` / `ARPROT`   | `0b000`           | Data, Secure, Unprivileged.                                 |
| `AWQOS` / `ARQOS`     | `0b0000`          | QoS 사용 안 함.                                             |
| `AWLOCK` / `ARLOCK`   | `0b0`             | Normal access. Exclusive access 없음.                       |
| `AWID` / `ARID`       | 0 ~ 3             | ID mapper wrapper가 할당(Section 6.3).                      |
| `WSTRB`               | 전부 1 (`0xFF..F`)| Full-beat write만. Partial write 없음.                      |
| `WLAST`               | 각 write burst의 마지막 beat에서 어썰트 | AXI 규격.                              |
| `RLAST`               | 각 read burst의 마지막 beat에서 소비    | AXI 규격.                              |

고정값 근거: DUT는 연속 메모리에 접근하는 FFT + DMA 블록이다. INCR가
유일하게 의미 있는 burst 타입이며, CACHE / PROT / QOS / LOCK도 모두
표준적인 non-cacheable data access 값(Xilinx HLS 기본값과 일치)이다.

### 6.2 4 KB 경계 처리

- AXI4는 단일 burst가 4 KB 경계를 넘는 것을 금지한다. HLS가 생성한
  master는 경계를 넘게 될 burst를 자동으로 연속된 두 burst로 분할한다.
- 분할은 master 내부에서 처리되므로 상위 로직(FFT 연산 경로)은
  이를 알지 못한다.
- 테스트벤치 monitor는 (coverage로) 경계를 넘게 될 요청이 실제로
  분할되었는지, 분할된 sub-burst가 올바르게 정렬되었는지를 관측할
  수 있다.

### 6.3 AXI ID 사용

**HLS 코어 내부.** HLS m_axi bundle은 모든 요청을 `ARID = 0`(read
master) 또는 `AWID = 0`(write master)으로 발행한다. Xilinx HLS의
고정 동작이다.

**ID mapper wrapper.** HLS 코어와 DUT 경계 사이에 손수 작성된 얇은
wrapper(`rtl/`)가 위치한다. 현재 정책(`MAPPER_CTRL.policy`,
Section 4.10)에 따라 각 요청의 ID를 재라벨링한다:

- `SEQUENTIAL`: ID 0 pass-through.
- `ROUND_ROBIN`: ID 0 → 1 → 2 → 3 → 0 → ...
- `RANDOM`: {0, 1, 2, 3}에서 균등 무작위, LFSR 기반.

정책은 runtime에 재프로그램 가능하며 이후 트랜잭션부터 유효하다.

**Read reorder buffer.** Wrapper의 read 쪽은 최소
`num_read_outstanding` 크기의 reorder buffer를 갖는다. Slave에서 서로
다른 ID들의 R 응답이 out-of-order로 올 수 있는데, 이를 요청 순서로
재조립해 HLS 코어에는 오직 in-order, ID 0 응답만 전달한다.

**Write 완료 처리.** Write 쪽은 데이터 자체는 재정렬할 필요가 없지만
(W 채널은 master가 발행한 순서 그대로 나감), outstanding AW → B 쌍을
추적해서 HLS 코어에 돌려주는 완료 알림이 master의 요청 순서를 지키게
해야 한다.

**성능 monitor와의 상호작용.** 모든 성능 카운터(Section 4.9)는 mapper
**바깥**(즉, DUT 경계)에서 AXI 신호를 관측한다. 따라서 ID 0 ~ 3 전체와
slave가 유도하는 out-of-order 동작이 모두 카운터에 반영된다.

---

## 7. 제어 흐름 (Activation Protocol)

### 7.1 Start / Done / Idle 시맨틱

DUT는 Xilinx HLS `ap_ctrl_hs` 프로토콜을 따른다. 한 activation은 다음
단계를 거친다:

1. **Idle** — `ap_idle = 1`, `ap_start = 0`, `ap_done = 0`. 새 요청
   대기 상태.
2. **Configuration** — 소프트웨어가 `RD_ADDR_LO/HI`, `WR_ADDR_LO/HI`,
   `NUM_FFTS`, `MODE`, (선택) `MAPPER_CTRL`을 프로그램한다.
3. **Start** — 소프트웨어가 `CTRL.ap_start = 1`을 쓴다(W1S).
4. **Accept** — DUT가 start 조건을 만족한 사이클에 `ap_ready`가 정확히
   1 clock cycle 동안 어썰트되고, 하드웨어가 `ap_start`를 자동
   clear하며, `ap_idle`이 0으로 내려간다. 설정 레지스터는 이 사이클에
   latch된다(7.2 참조).
5. **Running** — DUT가 read / compute / write 트래픽을 발생시킨다.
   내내 `ap_idle = 0`, `ap_done = 0`.
6. **Completion** — 이번 activation의 마지막 outstanding 응답이 수신
   되면(7.3 참조) `ap_done`이 어썰트되고 `ap_idle`이 1로 복귀한다.
7. **Done acknowledgement** — 소프트웨어가 `CTRL`을 read하면 자동으로
   `ap_done`이 clear된다(COR). 이 read는 `ISR.ap_done_int`를 clear
   하지 **않는다**. 인터럽트 상태 플래그는 반드시 `ISR`에 W1TC 쓰기로
   명시 clear해야 한다.

신호 시맨틱:

- **`ap_ready`**: accept 사이클에 어썰트되는 1-cycle pulse.
- **`ap_done`**: read될 때까지 sticky(COR). Set 되어 있는 동안 `GIE` /
  `IER` 설정을 만족하면 `irq_o`도 어썰트될 수 있다.
- **`ap_done`과 `ISR.ap_done_int`는 독립**. `CTRL`을 read하면
  `ap_done`만 clear된다. `ISR.ap_done_int`는 `ISR`에 W1TC 쓰기로만
  clear된다. 이 분리 덕분에 ISR과 status read가 race 없이 처리된다.
- **`ap_idle = 0` 상태에서 `ap_start`에 write**: 무시된다. 진행 중인
  activation에 영향 없음.

### 7.2 Configuration Latching

Configuration 레지스터(`RD_ADDR_*`, `WR_ADDR_*`, `NUM_FFTS`, `MODE`,
`FFT_CFG`, `MAPPER_CTRL`)는 언제든 write할 수 있지만, **하드웨어는
`ap_ready` 사이클에만 값을 sample**한다. 따라서 activation 도중의
write는 현재 activation을 방해하지 않고 다음 `ap_start`에서 유효해진다.

Coverage / debugging의 모호함을 피하기 위해 소프트웨어는 가능한 한
`ap_idle = 1`일 때 이 레지스터를 프로그램하도록 권장한다.

### 7.3 Done 조건

`ap_done`은 activation의 데이터 트래픽이 완전히 정리된 시점에
어썰트된다:

- `MODE_FFT`: 마지막 write burst의 마지막 B 응답이 수신되고 **그리고**
  마지막 read burst의 마지막 RLAST beat가 소비된 시점.
- `MODE_READ_ONLY`: 마지막 read burst의 마지막 RLAST beat가 소비된
  시점. (이 모드에서는 write가 발행되지 않는다.)
- `MODE_WRITE_ONLY`: 마지막 write burst의 마지막 B 응답이 수신된
  시점. (이 모드에서는 read가 발행되지 않는다.)

성능 카운터(Section 4.9)는 `ap_done`이 어썰트되는 동일 사이클에
갱신을 멈춘다.

### 7.4 Restart Handshake

두 가지 재시작 흐름을 지원한다:

**Software-driven restart.** 기본 흐름.

1. `ap_done = 1` (또는 인터럽트)을 대기한다.
2. `CTRL`을 read해 `ap_done`을 clear한다. 인터럽트를 사용했다면
   `ISR.ap_done_int`도 W1TC로 clear한다.
3. 필요 시 설정 레지스터를 재프로그램한다.
4. `CTRL.ap_start = 1`을 써서 다음 activation을 launch한다.

**Auto-restart.** `CTRL.auto_restart = 1`을 설정하면, 하드웨어가
`ap_done`을 어썰트하는 바로 그 사이클에 이미 latch된 설정값으로
`ap_start`를 재어썰트한다. 소프트웨어 개입 없이 back-to-back
activation이 발생한다.

- Auto-restart는 최근 `ap_ready`에서 latch된 설정을 사용한다.
  Activation 사이에 쓴 레지스터 값은 다음 `ap_ready` 사이클 이전에
  써졌을 때만 반영된다.
- Auto-restart를 진행 중 activation의 `ap_done` 이전에 clear하면
  해당 activation이 끝난 뒤 DUT가 Idle로 복귀하고 일반적인
  software-driven flow로 돌아온다.

---

## 8. 동작 모드

`MODE` 레지스터(0x24)가 세 가지 동작 모드 중 하나를 선택한다. 다른
설정 레지스터와 마찬가지로 `MODE`도 `ap_ready`에 latch된다(7.2).
Activation 도중 `MODE`에 write해도 진행 중인 activation에는 영향이
없다.

### 8.1 MODE_FFT (`MODE = 0`)

Full pipeline: read → compute → write.

- Read master가 `RD_ADDR`부터 `NUM_FFTS * 2` beat를 발행한다
  (Section 5.3).
- 각 16-sample 청크는 16-point FFT를 통과한다(Section 9).
- Write master가 `WR_ADDR`부터 `NUM_FFTS * 2` beat를 발행한다.
- Read와 write는 각 master의 outstanding 한도 안에서 자유롭게 겹칠
  수 있다.
- `ap_done`은 마지막 B 응답 **과** 마지막 RLAST beat가 모두 관측된
  이후에 어썰트된다.
- 성능 카운터: read / write 카운터 모두 누적된다.

### 8.2 MODE_READ_ONLY (`MODE = 1`)

Read master만 동작. FFT 연산과 write master는 disable 유지.

- Read master가 `RD_ADDR`부터 `NUM_FFTS * 2` beat를 발행한다.
- 수신된 데이터는 FFT 연산 입력에서 폐기된다. FFT 파이프라인은 clock
  받거나 활성화되지 않으며, 데이터에 근거한 내부 상태 변화도 없다.
- Write master는 어떤 트래픽도 발생시키지 않는다(`AWVALID`, `WVALID`가
  0 유지).
- `ap_done`은 마지막 RLAST beat 이후 어썰트된다.
- 성능 카운터: read 카운터만 누적된다. Write 카운터는 `ap_done` 시
  모두 0이다.
- 목적: write-side backpressure나 FFT 파이프라인의 영향 없이 read side
  (AR / R 채널)만을 격리해 stress한다.

### 8.3 MODE_WRITE_ONLY (`MODE = 2`)

Write master만 동작. Payload는 결정적 인덱스 패턴 사용. Read master와
FFT 연산은 disable 유지.

- Write master가 `WR_ADDR`부터 `NUM_FFTS * 2` beat를 발행한다.
- **데이터 패턴**: write 영역 시작부터의 zero-based sample index `n`에
  대해 각 32-bit sample slot의 값은 `n`(부호 없는 32-bit)이다. 구체
  적으로, write 영역에 `NUM_FFTS * 16`개의 sample이 있으면 byte offset
  `WR_ADDR + 4 * n`의 sample은 little-endian 32-bit 값 `n`을 갖는다.
- 32-bit sample의 `(real, imag)` 해석(Section 5.1)은 wire 상 유지되지만
  (real이 `[15:0]`, imag이 `[31:16]`), 이 패턴은 산술적 의미가 아니라
  checker 단순성을 위해 선택되었다.
- Read master는 어떤 트래픽도 발생시키지 않는다(`ARVALID`가 0 유지).
- `ap_done`은 마지막 B 응답 이후 어썰트된다.
- 성능 카운터: write 카운터만 누적된다. Read 카운터는 `ap_done` 시
  모두 0이다.
- 목적: read-side latency나 FFT 파이프라인 커플링 없이 write side
  (AW / W / B 채널)만을 격리해 stress한다.

### 8.4 Mode 전환 타이밍

`MODE`는 표준 configuration-latch 규칙(Section 7.2)을 따른다. Mode를
깨끗하게 전환하려면 DUT가 idle일 때(혹은 activation 사이) 새 `MODE`를
프로그램한 뒤 `ap_start`를 어썰트한다.

---

## 9. FFT 세부 사항

### 9.1 알고리즘과 Point Size

- Point size: 16 (고정).
- Radix: 2.
- 구조: Decimation-in-Time(DIT) 4 stage.
- 방향: forward only. Inverse FFT는 이 spec의 범위 밖이다. `FFT_CFG`
  (0x28)는 향후 방향 비트를 위해 예약되어 있다.
- 구현: HLS C++로 손수 작성. 외부 FFT 라이브러리 사용하지 않음.
- Bit-reversal은 **입력 쪽**에서 적용된다. 즉 sample `i`가 butterfly
  stage 전에 `bit_reverse_4(i)` 위치로 배치된다. 출력은 natural order
  (bin 0 → bin 15).

### 9.2 Fixed-Point 포맷

- Sample 표현: Q2.14 signed, real / imag 각각 16-bit.
  - 1 sign bit, 1 integer bit, 14 fractional bit.
  - 수 범위: `[-2, 2 - 2^-14]`.
  - 실제 값 = `raw / 16384`.
- 복소 sample 폭: 32-bit (`real`은 `[15:0]`, `imag`는 `[31:16]`).
  Section 5.1 정의.

### 9.3 Scaling 정책

- 스테이지마다 arithmetic right shift by 1(블록 스케일링). Butterfly
  이후 real / imag 각각에 독립 적용.
- 4 stage × `>> 1` = 총 스케일 팩터 `1 / 16` (unscaled DFT 대비). 즉
  DUT 출력은 `DFT(x) / N`.
- Shift의 반올림: **truncation** (arithmetic right shift; signed 값
  기준 floor toward -∞). HW 단순성과 C model과의 정확한 일치를 위해
  선택. Convergent / half-up 등 rounding-mode 변경은 계획 없음.
- 블록 스케일링은 입력이 Q2.14 범위 안에 있으면 sample slot 오버플로를
  수학적으로 방지한다. 오버플로 플래그 정책은 Section 9.6 참조.

### 9.4 Twiddle Precision

- Twiddle 표현: Q1.15 signed, real / imag 각각 16-bit.
  - 수 범위: `[-1, 1 - 2^-15]`.
  - `+1.0`은 LUT 대칭성 유지를 위해 `0x7FFF ≈ 0.99997`로 근사. `-1.0`
    도 마찬가지로 `-0x7FFF`로 표현하여(그렇지 않으면 `INT16_MIN =
    -0x8000`) 부호 반전이 오버플로되지 않도록 한다.
- 저장: 8-entry lookup table `W16[0..7]`을 상수로 하드코딩(대칭성으로
  전반부만 저장).
- Runtime twiddle 계산 없음(CORDIC 없음, 8-entry LUT 외에 ROM
  addressing 없음).

### 9.5 출력 순서

- Natural order. 출력 bin `k`는 `k = 0 .. 15`에 대해 DFT 주파수
  인덱스 `k`와 대응한다. 입력에 bit-reversal이 적용되므로(9.1) 출력에
  별도 permutation이 필요 없다.

### 9.6 Butterfly Datapath 정밀도

- 복소 곱셈 `W · b`:
  - 포맷: Q1.15 × Q2.14 → Q3.29 곱, signed 32-bit 중간값에 저장.
  - Q3.14로 정규화: arithmetic right shift by 15 (**truncation**,
    round 없음).
- Butterfly 합 / 차:
  - `a + Wb`, `a − Wb`를 signed 32-bit로 계산해 mid-stage 오버플로
    방지.
  - Q2.14 slot에 저장 전 right-shift by 1 (Section 9.3).
- 오버플로 플래그(`STATUS.overflow`, `STATUS`의 bit 2): 이 revision
  에서는 예약, 0으로 하드와이어. Q2.14 입력과 고정된 블록 스케일링
  정책 하에서는 sample slot에서의 산술 오버플로가 수학적으로 방지되
  므로 런타임 검출 로직을 인스턴스화하지 않는다. 향후 스케일링
  정책이 완화되면 플래그를 활성화한다.

---

## 10. 에러 처리

### 10.1 감지

- Read master: 매 R 채널 beat 수용 시 `RRESP[1:0]`을 sample한다.
  `OKAY`(`0b00`) 외의 값 — 즉 `SLVERR`(`0b10`), `DECERR`(`0b11`) —
  은 에러로 취급된다.
- Write master: 매 B 채널 핸드셰이크 수용 시 `BRESP[1:0]`을 sample
  한다. `OKAY` 외의 값은 에러로 취급된다.
- `EXOKAY`(`0b01`)는 DUT가 exclusive access를 어썰트하지 않으므로
  (`AWLOCK` / `ARLOCK` = 0) 발생이 예상되지 않는다. 상태 판정에서는
  `OKAY`와 동일하게 취급되며, assertion으로 추가적인 이상 신호로 잡을
  수 있다.
- 에러 감지는 DUT 경계(**mapper 바깥**)에서 신호를 관측한다. 성능
  모니터 위치와 일치한다.

### 10.2 응답 동작

- DUT는 에러 발생 시 **abort하지 않는다.** 현재 activation에서
  계획된 모든 read / write는 그대로 발행된다.
- `STATUS.rd_error` / `STATUS.wr_error`는 해당 유형의 첫 에러 응답
  사이클에 1로 latch되고, activation 동안 sticky로 유지된다.
- `STATUS.rd_error`의 0 → 1 전이 사이클에 `ISR.rd_err_int`가 set된다.
  Write 쪽도 마찬가지로 `STATUS.wr_error`와 `ISR.wr_err_int`. 인터럽트
  는 `GIE` / `IER` 설정을 만족하면 어썰트된다.
- `ap_done`은 Section 7.3에서 정의한 대로 모든 outstanding
  트랜잭션이 정리되면 정상적으로 어썰트된다. 소프트웨어는 `ap_done`
  이후에 `STATUS`를 확인해 activation이 깨끗하게 완료되었는지 판정
  해야 한다.

### 10.3 상태 clear

- `STATUS.rd_error`와 `STATUS.wr_error`는 매 `ap_start`마다 자동으로
  clear된다. 성능 카운터 clear 정책과 일치한다. 이로써 STATUS는 가장
  최근에 완료된 activation만 반영한다.
- `ISR.rd_err_int` / `ISR.wr_err_int`는 `ap_start`로 clear되지 **않는다.**
  표준 인터럽트-ack 흐름(Section 4.3)을 따라 `ISR`에 W1TC 쓰기로만
  clear된다.

### 10.4 성능 카운터와의 상호작용

에러가 발생한 트랜잭션도 표준 성능 카운터(Section 4.9)에 그대로
집계된다. `RD_BEAT_CNT`, `WR_BEAT_CNT`, `RD_LAT_ACC`, `WR_LAT_ACC`,
`RD_TXN_CNT`, `WR_TXN_CNT`는 응답이 `SLVERR` 또는 `DECERR`였던
트랜잭션의 기여분도 포함한다. 에러 발생은 `STATUS`와 `ISR`을 통해서
별도로 신호된다.

### 10.5 Auto-Restart와의 상호작용

`CTRL.auto_restart = 1`인 상태에서 activation 중 에러가 발생해도 DUT
는 `ap_done`을 어썰트하고 같은 사이클에 다음 activation을 자동
launch한다. Sticky한 `STATUS` 비트들은 이 auto-generated `ap_start`
에 의해 clear되므로, `auto_restart`를 쓰는 소프트웨어는 activation
사이에서 에러를 감지하기 위해 (자동 clear되지 않는) `ISR`을 사용해야
한다.

---

## 11. Reset 동작

Reset 신호 자체(polarity, style, 최소 어썰트 시간)는 Section 2.2에서
정의한다. 이 절은 reset의 내부 효과를 규정한다.

### 11.1 Reset 어썰트 시 clear되는 상태

`rst_n = 0` 동안 DUT는 완전한 idle 상태로 강제된다:

- 모든 memory-mapped 레지스터(0x00 ~ 0x50)가 Section 4의 reset 값으로
  복귀.
- 모든 내부 FSM이 `IDLE`로 복귀.
- Outstanding-transaction 카운터(양쪽 master)가 clear.
- Read reorder buffer가 empty.
- ID mapper LFSR가 초기 seed로 재로드.
- 성능 카운터 내부 상태(per-ID FIFO, running total, peak tracker)가
  clear.
- 에러 latch(`STATUS.rd_error`, `STATUS.wr_error` 및 대응되는 `ISR`
  비트)가 clear.

### 11.2 Reset 중 AXI 출력 신호

- 모든 `*VALID` 출력(`ARVALID`, `AWVALID`, `WVALID`, 그리고 AXI-Lite
  slave의 `RVALID` / `BVALID`)이 0으로 구동된다.
- DUT가 구동하는 모든 `*READY` 출력(read master의 `RREADY`, write
  master의 `BREADY`, AXI-Lite slave의 `AWREADY`, `WREADY`, `ARREADY`)
  도 0으로 구동되어 reset 도중 우발적 handshake가 발생하지 않게 한다.
- Payload 신호(`ARADDR`, `WDATA` 등)는 어떤 값이든 될 수 있다.
  `*VALID`가 0이므로 sample되지 않기 때문이다.

### 11.3 Outstanding 트랜잭션과 시스템 가정

Reset은 **시스템 전체** 이벤트로 취급된다. 즉 DUT, DUT가 통신하는
AXI slave, 주변 fabric 모두가 함께 reset된다고 가정한다. DUT는
reset 어썰트 시 모든 in-flight 트랜잭션 정보를 폐기한다. DUT가
reset되는 동안 slave가 계속 동작하다가 나중에 유령 요청에 대한
응답을 돌려주는 시나리오는 spec 범위 밖이다.

### 11.4 Reset 이후 동작

- `rst_n`이 `clk`에 동기로 디어썰트되면, 다음 clock 사이클부터 DUT는
  AXI-Lite 접근을 받을 준비가 된다. 추가 대기 시간은 필요 없다.
- 첫 activation을 즉시 프로그램해도 된다. DUT는 `ap_idle = 1`, 모든
  설정 레지스터는 reset 값으로 시작한다.

### 11.5 활성 실행 도중 Reset

Activation 진행 중 reset이 어썰트되면 DUT는 Section 11.1대로 즉시
`IDLE`로 전이한다. In-flight 트랜잭션들은 DUT 관점에서 폐기된다.
Section 11.3에 따라 주변 시스템도 같은 이벤트에서 reset된다고 가정
하므로 reset 해제 이후 orphan 응답이 도착하지 않는다.

---

## Change Log

- 2026-08-20: 문서 뼈대 생성 및 전 섹션 채움 (Clock / Reset, 인터페이스,
  성능 카운터·mapper 제어 포함 레지스터 맵, 메모리 레이아웃, 트랜잭션
  동작, auto-restart 포함 제어 흐름, 동작 모드, FFT 세부, 에러 처리,
  reset 동작, 최상위 개요).
