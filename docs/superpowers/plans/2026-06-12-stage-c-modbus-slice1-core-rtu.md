# Stage C slice 1 — Modbus 코어 + RTU 구현 계획

> **요약**: 승인 스펙(`docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md`)의
> 구현 계획. 9개 Task: 브랜치 → 순수 코어(CRC16/FC 디코드, 호스트 TDD 3개 task) →
> mon 게이트 → `usart6_mb` RTU 전송층(DMA circular + 갭 프레이밍) → `app_reg` 명령
> 소스 확장(US_COMM + amp 추적) → `app_modbus` 통합(미러/쓰기 반영/점유 전환) →
> 최종 게이트(main+trace 0-warning, 호스트 전수 PASS). HW E2E(mbpoll)는 보드 후
> (§HW-gated 절차 수록). 스펙 정제/편차는 §Deviations에 명시.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** samd20 Modbus 슬레이브(코어 + RTU/RS-485)를 STM32F410 펌웨어에 흡수 — HW 없이 코드·호스트 테스트 완결.

**Architecture:** 순수 코어 `app_modbus_core`(HAL-free, 호스트 테스트) + 얇은 전송층 `fw/drivers/usart6_mb`(DMA circular RX + 샘플20 충실 갭 프레이밍, blocking TX) + 통합층 `app_modbus`(레지스터 의미·FRAM 커밋·US_COMM 명령·USART6 mon↔Modbus 점유 전환). 슈퍼루프에 `app_modbus_tick()` 한 줄 추가.

**Tech Stack:** STM32F410 HAL(UART/DMA), C11, 호스트 테스트 = `fw/test/` Makefile(cc, CHECK_EQ 패턴), 빌드 = `env -u STM32_TOOLCHAIN cmake` 0-warning 게이트.

---

## File Structure (책임 경계)

| 파일 | 책임 |
|---|---|
| Create: `fw/include/app_modbus_core.h` | H_REG 맵, mb_core_t, CRC16/디코드 API (HAL-free) |
| Create: `fw/src/app_modbus_core.c` | samd20 modbus.c 포팅: CRC16, FC 01/02/03/04/05/06, 응답 생성 |
| Create: `fw/test/test_app_modbus_core.c` | 코어 전수 호스트 테스트 |
| Modify: `fw/test/Makefile` | 두 번째 테스트 바이너리 추가 |
| Create: `fw/include/usart6_mb.h`, `fw/drivers/usart6_mb.c` | RTU 전송층: open/close/rx_frame/send |
| Modify: `fw/include/mon.h`, `fw/drivers/mon_usart6.c` | `mon_set_enabled()` 게이트 |
| Modify: `fw/include/periph.h`, `fw/src/periph.c` | `hdma_usart6_rx` 핸들 |
| Modify: `fw/include/app_lcd.h` | `lcd_measure_t`에 `max_amp`/`last_amp` 추가 |
| Modify: `fw/include/app_reg.h`, `fw/src/app_reg.c` | `app_reg_command(cmd, src)` + amp 추적 + COMM ceiling |
| Modify: `fw/src/app_lcd.c` | hook 호출처 src 인자 + comm hook 주석 |
| Create: `fw/include/app_modbus.h`, `fw/src/app_modbus.c` | 미러/쓰기 반영/명령 라우팅/점유 전환 |
| Modify: `fw/src/app.c`, `fw/src/main.c` | `app_modbus_tick()` / `app_modbus_init()` 배선 |

CMakeLists는 `file(GLOB src/*.c drivers/*.c)`라 **수정 불필요** (신규 파일 자동 포함). HAL 모듈 추가 없음(uart/dma 기존).

## Deviations (스펙 정제 — 리뷰 가시화용)

1. **프레임 경계 = IDLE-line 인터럽트 대신 폴링 갭 검출**: DMA 쓰기 위치가 per-baud break 시간(samd20 `max_break_cnt`×250µs를 1ms 그리드로 올림: 2400→14ms, 4800→7ms, 9600→4ms, ≥19200→2ms) 동안 정지하면 프레임 확정. 의미론 동일("유휴 갭 = 프레임 끝"), samd20 break-count 표 **더 충실**, `usart1.c`의 no-ISR 디시플린 유지(NVIC/ISR 0개 추가). USB-serial 마스터의 mid-frame 갭 내성도 IDLE(1-char)보다 큼.
2. **코어 범위 검사 추가(안전 수정)**: samd20 `read_reg`/`write_reg`/coil 함수는 주소·개수 무검사 → OOB read/**임의 메모리 write**. 범위 밖 = 무응답(bad-CRC와 동급).
3. **FC05 응답 수정**: samd20은 `response[1]=0x02` + 9바이트 반환(복붙 버그, 표준 마스터가 에러 처리). `0x05` 에코 + 8바이트로 수정(FRAM 주소 복붙버그 수정과 같은 승인 카테고리).
4. **FC01/02 8의 배수 버그 수정**: samd20은 `num%8==0`일 때 마지막 바이트를 채우지 않음(0 반환). 풀바이트도 채우도록 수정.
5. **`app_lcd_hook_comm_reconfigure`는 로그 전용 유지**: 점유/라인 파라미터 변화는 `app_modbus_tick()`이 매 iter cfg를 비교해 감지(edge-transition). samd20 메인루프 게이트 `(comm_mode==SERIAL)&&(addr!=0)` 매 루프 평가와 동일 의미 + comm_mode만 바뀌는 경로(serial hook 미발화)도 커버 + app_lcd→app_modbus→app_lcd 순환 의존 차단(M1 일관).
6. **미러 갱신 = 매 tick**: samd20은 메시지 처리 후에만 `update_holding_reg(0)` → 읽기가 1-트랜잭션 stale. 매 tick 미러로 ≤1 iter 신선도 + 클램프된 쓰기 즉시 정규화(samd20은 TIMEOVER 외 재발화 퀴크).
7. **TX 후 RX flush(의도적 안전 추가)**: V30 RS-485 auto-DE(U13/U16)가 자기 송신을 에코할 경우 자기 응답을 요청으로 재해석(FC06 에코 = 요청과 동일 바이트!)하는 루프 차단. 송신 직후 RX 링을 head까지 폐기.
8. **FRAM 커밋 = `app_config_save_all` 전체 맵 1회**(repo 패턴, `data_save_commit` 동일) — samd20 per-field save 대체, DELAY3→ADDR_TRIGGER2 / TRIGGER2→ADDR_DELAY2 복붙버그는 구조적으로 소멸(스펙 §3.2 승인).
9. **read_reg `>255` 분기 단순화**: samd20의 두 분기는 출력 바이트 동일(hi=0) → 무조건 hi/lo로 통합(byte-identical).
10. **EN_* LCD 에코 = `dgus_write_u16` 직접 호출**(samd20 `send_lcd_data_var` 등가; `app_lcd_input.c:771/777` 선례), work_cnt 리셋 에코 = `app_lcd_set_work_cnt(0)`.

알려진 한계(주석 문서화): ① 부팅 배너는 FRAM 로드 전 출력 → Modbus 소유 구성에서 부팅 시 수십 바이트가 한 번 버스에 실림(잘못된 baud의 노이즈 — 마스터 CRC가 거름). ② blocking TX 최악 정지 = 105B@2400 ≈ 482ms(스펙 승인; 기본 19200은 ~34ms). ③ 폴 간격 사이 DMA가 정확히 256B 돌아 head가 제자리면 프레임 미검출(CRC가 거름; 실사용 프레임 ≤ 수십 B라 비현실).

---

### Task 1: 브랜치 생성

**Files:** 없음 (git만)

- [ ] **Step 1: stacked tip 확인 + 브랜치 생성**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git status --short                      # working tree clean 확인 (.understand-anything/, ref/atmega16/M16_reverse/ untracked는 무시)
git checkout refactor/stage-d-m1-cfg-param-injection
git log --oneline -3                    # tip = f56f17d docs(stage-c) 확인
git checkout -b feat/stage-c-modbus-core-rtu
```

Expected: `Switched to a new branch 'feat/stage-c-modbus-core-rtu'`

- [ ] **Step 2: 기준선 빌드 + 호스트 테스트 PASS 확인**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -5
make -C test test
```

Expected: 0-warning, `all checks PASSED`

---

### Task 2: 코어 1 — 헤더 + `mb_core_init` + `mb_crc16` (TDD)

**Files:**
- Create: `fw/include/app_modbus_core.h`
- Create: `fw/src/app_modbus_core.c`
- Create: `fw/test/test_app_modbus_core.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: 헤더 작성**

```c
/* fw/include/app_modbus_core.h — pure Modbus slave core shared by RTU (slice 1)
 * and TCP (slice 2). samd20 modbus.c port: CRC16, FC 01/02/03/04/05/06 decode,
 * holding/coil tables. HAL-free — host-tested (fw/test). The transport layer
 * hands in ONE complete frame (RTU: addr..crc); the app layer owns what the
 * registers MEAN (app_modbus.c mirror/apply passes). */
#pragma once
#include <stdint.h>

#define MB_REG_COUNT    50u    /* samd20 holdingReg[50] */
#define MB_COIL_COUNT   50u    /* samd20 coils[50] */
#define MB_FRAME_MAX    125u   /* samd20 received[125] */
#define MB_RESP_MAX     125u   /* samd20 response[125]; FC03 all-50-regs = 105 B */

/* H_REG register map (samd20 modbus.h verbatim) */
#define MB_REG_WORK_CNTH    0x00u
#define MB_REG_WORK_CNTL    0x01u   /* write 0 = work counter reset (samd20 main.c:4539) */
#define MB_REG_DISP_POWER   0x02u
#define MB_REG_DISP_AMP     0x03u
#define MB_REG_DISP_FREQ    0x04u
#define MB_REG_DISP_ENERGY  0x05u
#define MB_REG_OUT_POWER    0x06u
#define MB_REG_ON_TIME      0x07u
#define MB_REG_ENERGY       0x08u
#define MB_REG_TIMEOVER     0x09u
#define MB_REG_DELAY1       0x0Au
#define MB_REG_DELAY2       0x0Bu
#define MB_REG_DELAY3       0x0Cu
#define MB_REG_TRIGGER2     0x0Du
#define MB_REG_TRIGGER3     0x0Eu
#define MB_REG_MULTI_T1     0x0Fu
#define MB_REG_MULTI_T2     0x10u
#define MB_REG_MULTI_O1     0x11u
#define MB_REG_MULTI_O2     0x12u
#define MB_REG_RUN_MODE     0x13u
#define MB_REG_EN_ENERGY    0x14u
#define MB_REG_EN_MULTI     0x15u
#define MB_REG_EN_SAFTY     0x16u
#define MB_REG_MODEL_FREQ   0x17u   /* read-only: mirror overwrites (samd20 faithful) */
#define MB_REG_MODEL_TYPE   0x18u   /* read-only: mirror overwrites (samd20 faithful) */
#define MB_REG_RESET        0x19u   /* command: consume-and-clear */
#define MB_REG_SEEK         0x1Au   /* command: consume-and-clear */
#define MB_REG_START        0x1Bu   /* command: consume-and-clear */
#define MB_REG_STOP         0x1Cu   /* command: consume-and-clear */
#define MB_REG_STATUS       0x1Du

/* STATUS bits (samd20 modbus.h). ESTOP/OVLD/OVTIME/OUTERR stay 0 this slice
 * (overload/weld machinery deferred — spec §3.1). */
#define MB_STATUS_US      0x01u
#define MB_STATUS_ESTOP   0x02u
#define MB_STATUS_OVLD    0x04u
#define MB_STATUS_OVTIME  0x08u
#define MB_STATUS_OUTERR  0x10u

/* decode mode (samd20 decode_comm(mode)): RTU checks slave addr + CRC,
 * TCP (slice 2, MBAP-stripped PDU) skips both. */
#define MB_MODE_RTU  0u
#define MB_MODE_TCP  1u

typedef struct {
    uint16_t holding[MB_REG_COUNT];
    uint8_t  coils[MB_COIL_COUNT];   /* FC 01/05 work; NO app mapping (samd20 faithful) */
    uint8_t  device_addr;
} mb_core_t;

/* Zero both tables + set the slave address (samd20 init_modbus tail). */
void mb_core_init(mb_core_t *mb, uint8_t device_addr);

/* Modbus CRC16 (poly 0xA001, init 0xFFFF), returned BYTE-SWAPPED like samd20
 * make_crc: high byte of the return = FIRST CRC byte on the wire (lo-first),
 * so emit resp[n] = crc>>8, resp[n+1] = crc&0xFF. */
uint16_t mb_crc16(const uint8_t *buf, uint8_t len);

/* Decode one complete frame and build the response into resp.
 * Returns the response length to transmit (0 = stay silent: other address,
 * bad CRC, unsupported FC, malformed/out-of-range — samd20 never sends Modbus
 * exception responses, faithful). *fc_out = function code actually processed
 * (0 if none); the app layer runs its write-apply pass when *fc_out == 0x06. */
uint8_t mb_core_decode(mb_core_t *mb, const uint8_t *frame, uint8_t len,
                       uint8_t mode, uint8_t resp[MB_RESP_MAX], uint8_t *fc_out);
```

- [ ] **Step 2: 실패하는 CRC 테스트 작성**

`fw/test/test_app_modbus_core.c` (전체 신규 — Task 3/4에서 테스트 함수 추가 예정):

```c
/* fw/test/test_app_modbus_core.c — host unit tests for the pure Modbus core
 * (CRC16, FC 01..06 decode, response build). No HAL, no hardware.
 * samd20 modbus.c port verification + port-fix coverage (bounds, FC05 echo,
 * full-byte coil reads). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "app_modbus_core.h"

static int failures = 0;

#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* Build an 8-byte RTU request: addr, fc, u16 a, u16 b, CRC. Returns 8. */
static uint8_t mk_req(uint8_t *f, uint8_t addr, uint8_t fc,
                      uint16_t a, uint16_t b)
{
    f[0] = addr; f[1] = fc;
    f[2] = (uint8_t)(a >> 8); f[3] = (uint8_t)a;
    f[4] = (uint8_t)(b >> 8); f[5] = (uint8_t)b;
    uint16_t crc = mb_crc16(f, 6);
    f[6] = (uint8_t)(crc >> 8); f[7] = (uint8_t)crc;
    return 8;
}

/* CRC16/MODBUS check value: "123456789" -> 0x4B37; wire order lo-first means
 * the samd20-swapped return is 0x374B. Classic frame vector: 11 03 00 6B 00 03
 * -> wire CRC bytes 76 87 -> swapped return 0x7687. */
static void test_crc16(void) {
    CHECK_EQ(mb_crc16((const uint8_t *)"123456789", 9), 0x374B);
    static const uint8_t classic[6] = { 0x11, 0x03, 0x00, 0x6B, 0x00, 0x03 };
    CHECK_EQ(mb_crc16(classic, 6), 0x7687);
    /* empty buffer = init value swapped */
    CHECK_EQ(mb_crc16(classic, 0), 0xFFFF);
}

static void test_core_init(void) {
    mb_core_t mb;
    memset(&mb, 0xAA, sizeof(mb));
    mb_core_init(&mb, 17);
    CHECK_EQ(mb.device_addr, 17);
    CHECK_EQ(mb.holding[0], 0);
    CHECK_EQ(mb.holding[MB_REG_COUNT - 1u], 0);
    CHECK_EQ(mb.coils[0], 0);
    CHECK_EQ(mb.coils[MB_COIL_COUNT - 1u], 0);
}

int main(void) {
    test_crc16();
    test_core_init();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
```

`fw/test/Makefile` 전체 교체:

```make
# fw/test/Makefile — host unit tests for the pure (HAL-free) layers:
# regulation compute (app_reg_calc) + Modbus core (app_modbus_core).
# Runs on the host toolchain. Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core

test: $(BIN_REG) $(BIN_MB)
	$(BIN_REG)
	$(BIN_MB)

$(BIN_REG): test_app_reg_calc.c ../src/app_reg_calc.c ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c ../src/app_reg_calc.c

$(BIN_MB): test_app_modbus_core.c ../src/app_modbus_core.c ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_core.c ../src/app_modbus_core.c

clean:
	rm -f $(BIN_REG) $(BIN_MB)

.PHONY: test clean
```

- [ ] **Step 3: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `app_modbus_core.c` 부재로 컴파일 에러 (RED)

- [ ] **Step 4: 최소 구현 — init + CRC**

`fw/src/app_modbus_core.c` 신규:

```c
/* fw/src/app_modbus_core.c — pure Modbus slave core (samd20 modbus.c port).
 * Stateless over an mb_core_t context (de-globalized: samd20 used file-scope
 * volatile arrays). HAL-free by design — host tests in fw/test/.
 * Port fixes vs samd20 (plan §Deviations): request bounds checks (samd20 did
 * OOB reads and an UNBOUNDED holdingReg write), FC05 echo (samd20 answered
 * fc=0x02/9 bytes — copy-paste bug), FC01/02 full-byte reads (samd20 left the
 * last byte empty when count%8==0). Everything else byte-faithful. */
#include <string.h>
#include "app_modbus_core.h"

void mb_core_init(mb_core_t *mb, uint8_t device_addr)
{
    memset(mb, 0, sizeof(*mb));
    mb->device_addr = device_addr;
}

uint16_t mb_crc16(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 8; j != 0; j--) {
            if ((crc & 0x0001u) != 0u) {
                crc >>= 1;
                crc ^= 0xA001u;
            } else {
                crc >>= 1;
            }
        }
    }
    /* samd20 make_crc swap: wire order is lo-byte first, so the first wire
     * byte rides in the HIGH byte of the return. */
    return (uint16_t)(((crc & 0x00FFu) << 8) | ((crc & 0xFF00u) >> 8));
}
```

- [ ] **Step 5: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: 두 바이너리 모두 `all checks PASSED`

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_modbus_core.h fw/src/app_modbus_core.c fw/test/test_app_modbus_core.c fw/test/Makefile
git commit -m "feat(stage-c): modbus core skeleton — H_REG map, mb_core_init, CRC16 (host-tested)"
```

---

### Task 3: 코어 2 — 디코드 디스패치 + FC 03/04 읽기 (TDD)

**Files:**
- Modify: `fw/src/app_modbus_core.c`
- Modify: `fw/test/test_app_modbus_core.c`

- [ ] **Step 1: 실패하는 테스트 추가**

`test_app_modbus_core.c`의 `main()` 위에 추가, `main()`에 호출 추가:

```c
/* FC 03 — single + multi + response CRC validity + FC 04 echo. */
static void test_read_regs(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;

    mb_core_init(&mb, 5);
    mb.holding[2] = 0x1234;
    mb.holding[3] = 0x00AB;

    /* single reg @0x0002 */
    mk_req(req, 5, 0x03, 0x0002, 0x0001);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);             /* addr fc cnt hi lo crc2 */
    CHECK_EQ(fc, 0x03);
    CHECK_EQ(resp[0], 5);
    CHECK_EQ(resp[1], 0x03);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x12);
    CHECK_EQ(resp[4], 0x34);
    /* response carries a valid CRC over its own first n-2 bytes */
    uint16_t crc = mb_crc16(resp, (uint8_t)(n - 2u));
    CHECK_EQ(resp[5], (uint8_t)(crc >> 8));
    CHECK_EQ(resp[6], (uint8_t)crc);

    /* two regs @0x0002 */
    mk_req(req, 5, 0x03, 0x0002, 0x0002);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 9);
    CHECK_EQ(resp[2], 4);
    CHECK_EQ(resp[5], 0x00);
    CHECK_EQ(resp[6], 0xAB);

    /* full-map read: 50 regs from 0 -> 3 + 100 + 2 = 105 */
    mk_req(req, 5, 0x03, 0x0000, 0x0032);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 105);

    /* FC 04 mirrors FC 03 with its own echo */
    mk_req(req, 5, 0x04, 0x0002, 0x0001);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(fc, 0x04);
    CHECK_EQ(resp[1], 0x04);
}

/* Port safety fix: out-of-range reads = silence (samd20 read past the table). */
static void test_read_regs_bounds(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x03, 0x0031, 0x0002);   /* 49 + 2 > 50 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0000);   /* zero count */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0033);   /* count 51 > 50 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}

/* Silence paths: other addr, bad CRC, unsupported FC, runt frame (samd20:
 * no exception responses). TCP mode skips addr + CRC filtering. */
static void test_filters(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);
    mb.holding[0] = 7;

    mk_req(req, 9, 0x03, 0x0000, 0x0001);   /* not our address */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);

    mk_req(req, 5, 0x03, 0x0000, 0x0001);
    req[6] ^= 0xFFu;                        /* corrupt CRC */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    mk_req(req, 5, 0x10, 0x0000, 0x0001);   /* FC 16 unsupported */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    mk_req(req, 5, 0x03, 0x0000, 0x0001);
    CHECK_EQ(mb_core_decode(&mb, req, 3, MB_MODE_RTU, resp, &fc), 0);  /* runt */
    CHECK_EQ(mb_core_decode(&mb, req, 7, MB_MODE_RTU, resp, &fc), 0);  /* short */

    /* TCP mode: wrong addr AND garbage CRC bytes still processed (slice 2
     * strips MBAP; samd20 decode_comm(mode!=0) identical). len 6 = PDU only. */
    req[0] = 0xEEu; req[6] = 0; req[7] = 0;
    uint8_t n = mb_core_decode(&mb, req, 6, MB_MODE_TCP, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(fc, 0x03);
    CHECK_EQ(resp[4], 7);                   /* holding[0] low byte */
}
```

`main()`에 추가:

```c
    test_read_regs();
    test_read_regs_bounds();
    test_filters();
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `mb_core_decode` 미정의 (RED)

- [ ] **Step 3: 구현 — 디스패치 + FC03/04**

`app_modbus_core.c`에 추가 (`mb_crc16` 아래):

```c
/* FC 03/04 — samd20 read_reg/read_input_reg folded into one (the two bodies
 * are identical except the echoed FC; samd20's >255 split produces the same
 * hi/lo bytes unconditionally — byte-identical simplification). */
static uint8_t mb_read_regs(const mb_core_t *mb, const uint8_t *frame,
                            uint8_t fc, uint8_t resp[MB_RESP_MAX])
{
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t num  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
    uint8_t  j    = 3;

    /* Port safety fix (NOT samd20): out-of-range request = silence. samd20
     * walked holdingReg past [50] and overflowed response past [125]. */
    if ((num == 0u) || (num > MB_REG_COUNT) ||
        ((uint32_t)addr + num > MB_REG_COUNT)) {
        return 0;
    }

    resp[0] = mb->device_addr;
    resp[1] = fc;
    resp[2] = (uint8_t)(num * 2u);
    for (uint16_t i = addr; i < (uint16_t)(addr + num); i++) {
        resp[j++] = (uint8_t)(mb->holding[i] >> 8);
        resp[j++] = (uint8_t)(mb->holding[i]);
    }
    uint16_t crc = mb_crc16(resp, j);
    resp[j++] = (uint8_t)(crc >> 8);
    resp[j++] = (uint8_t)(crc);
    return j;
}

uint8_t mb_core_decode(mb_core_t *mb, const uint8_t *frame, uint8_t len,
                       uint8_t mode, uint8_t resp[MB_RESP_MAX], uint8_t *fc_out)
{
    uint8_t n = 0;
    *fc_out = 0;

    /* samd20 decode_comm: RTU filters slave address + CRC; TCP skips both. */
    if (mode == MB_MODE_RTU) {
        if (len < 8u) {                 /* full RTU request floor: addr fc 4B crc2 */
            return 0;
        }
        if (frame[0] != mb->device_addr) {
            return 0;
        }
        uint16_t crc = mb_crc16(frame, (uint8_t)(len - 2u));
        if ((frame[len - 2u] != (uint8_t)(crc >> 8)) ||
            (frame[len - 1u] != (uint8_t)(crc))) {
            return 0;
        }
    } else {
        if (len < 6u) {                 /* PDU floor: addr fc 4B (MBAP stripped) */
            return 0;
        }
    }

    /* read_coil builds bits with ^= over a zeroed buffer (samd20
     * clear_response invariant) — zero up front. */
    memset(resp, 0, MB_RESP_MAX);

    switch (frame[1]) {
    case 0x03u:
    case 0x04u:
        n = mb_read_regs(mb, frame, frame[1], resp);
        break;
    default:
        return 0;   /* unsupported FC: silence (samd20 builds no exception) */
    }
    if (n != 0u) {
        *fc_out = frame[1];
    }
    return n;
}
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: `all checks PASSED` ×2

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_modbus_core.c fw/test/test_app_modbus_core.c
git commit -m "feat(stage-c): modbus core decode dispatch + FC03/04 reads (bounds-checked)"
```

---

### Task 4: 코어 3 — FC 06 쓰기 + FC 01/02/05 코일 (TDD)

**Files:**
- Modify: `fw/src/app_modbus_core.c`
- Modify: `fw/test/test_app_modbus_core.c`

- [ ] **Step 1: 실패하는 테스트 추가**

```c
/* FC 06 — store + echo (echo bytes == request bytes for a valid write). */
static void test_write_reg(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x06, MB_REG_OUT_POWER, 80);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(fc, 0x06);
    CHECK_EQ(mb.holding[MB_REG_OUT_POWER], 80);
    CHECK_EQ(memcmp(resp, req, 8), 0);      /* FC06 echo == request */

    /* port safety fix: out-of-range write = silence + no state change
     * (samd20 wrote holdingReg[addr] UNBOUNDED = arbitrary memory write). */
    mk_req(req, 5, 0x06, 50, 1);
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);
}

/* FC 05 — coil set/clear; port fix: proper 0x05 echo, 8 bytes (samd20 answered
 * fc=0x02 and 9 bytes — copy-paste bug). */
static void test_write_coil(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x05, 3, 0xFF00);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(fc, 0x05);
    CHECK_EQ(mb.coils[3], 0xFF);
    CHECK_EQ(resp[1], 0x05);
    CHECK_EQ(memcmp(resp, req, 8), 0);      /* with the fixed echo, == request */

    mk_req(req, 5, 0x05, 3, 0x0000);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(mb.coils[3], 0x00);

    mk_req(req, 5, 0x05, 50, 0xFF00);       /* out of range */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}

/* FC 01/02 — bit packing: partial byte, multi-byte, and the full-byte case
 * (count%8==0) that samd20 left empty (port fix). */
static void test_read_coils(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);
    mb.coils[0] = 1; mb.coils[2] = 1; mb.coils[9] = 1; mb.coils[15] = 1;

    /* 10 coils from 0: 2 bytes (8 + rem 2). byte0 = 0b00000101, byte1 = 0b10. */
    mk_req(req, 5, 0x01, 0, 10);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);                         /* addr fc cnt b0 b1 crc2 */
    CHECK_EQ(fc, 0x01);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x05);
    CHECK_EQ(resp[4], 0x02);

    /* 16 coils from 0 (count%8==0): samd20 bug left byte1 = 0. Fixed: bit15. */
    mk_req(req, 5, 0x01, 0, 16);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x05);
    CHECK_EQ(resp[4], 0x82);                /* coil9 -> bit1, coil15 -> bit7 */

    /* FC 02 echoes its own code */
    mk_req(req, 5, 0x02, 0, 8);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 6);                         /* addr fc cnt b0 crc2 */
    CHECK_EQ(fc, 0x02);
    CHECK_EQ(resp[1], 0x02);
    CHECK_EQ(resp[3], 0x05);

    mk_req(req, 5, 0x01, 45, 8);            /* 45 + 8 > 50: silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    mk_req(req, 5, 0x01, 0, 0);             /* zero count: silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}
```

`main()`에 추가:

```c
    test_write_reg();
    test_write_coil();
    test_read_coils();
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — FC 06/05/01/02가 default(silence)로 떨어져 CHECK 다수 실패 (RED)

- [ ] **Step 3: 구현 — write_reg / read_coils / write_coil + 디스패치 확장**

`app_modbus_core.c`의 `mb_read_regs` 아래에 추가:

```c
/* FC 06 — samd20 write_reg: store raw + echo the request. The app layer
 * interprets the value (clamp/persist/command) in its apply pass. */
static uint8_t mb_write_reg(mb_core_t *mb, const uint8_t *frame,
                            uint8_t resp[MB_RESP_MAX])
{
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t val  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    /* Port safety fix (NOT samd20): samd20 wrote holdingReg[addr] unbounded —
     * an out-of-range register write was an arbitrary memory write. */
    if (addr >= MB_REG_COUNT) {
        return 0;
    }
    mb->holding[addr] = val;

    resp[0] = mb->device_addr;
    resp[1] = 0x06u;
    resp[2] = frame[2];
    resp[3] = frame[3];
    resp[4] = frame[4];
    resp[5] = frame[5];
    uint16_t crc = mb_crc16(resp, 6u);
    resp[6] = (uint8_t)(crc >> 8);
    resp[7] = (uint8_t)(crc);
    return 8u;
}

/* FC 01/02 — samd20 read_coil/read_input_coil folded (identical bodies, own FC
 * echo). Port fix: samd20 wrote NO bits into the last byte when count%8==0
 * (its remainder loop ran zero times) — full bytes now always fill. */
static uint8_t mb_read_coils(const mb_core_t *mb, const uint8_t *frame,
                             uint8_t fc, uint8_t resp[MB_RESP_MAX])
{
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t num  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    /* Port safety fix: out-of-range = silence (samd20 read past coils[50]). */
    if ((num == 0u) || ((uint32_t)addr + num > MB_COIL_COUNT)) {
        return 0;
    }

    uint8_t bytes = (uint8_t)(num / 8u);
    uint8_t rem   = (uint8_t)(num % 8u);
    if (rem != 0u) {
        bytes++;
    }

    resp[0] = mb->device_addr;
    resp[1] = fc;
    resp[2] = bytes;

    uint8_t k = 3;
    uint8_t l = (uint8_t)addr;
    for (uint8_t i = bytes; i != 0u; i--) {
        uint8_t nbits = ((i > 1u) || (rem == 0u)) ? 8u : rem;
        for (uint8_t j = 0; j != nbits; j++) {
            resp[k] ^= (uint8_t)((mb->coils[l] ? 1u : 0u) << j);
            l++;
        }
        k++;
    }
    uint16_t crc = mb_crc16(resp, k);
    resp[k]      = (uint8_t)(crc >> 8);
    resp[k + 1u] = (uint8_t)(crc);
    return (uint8_t)(k + 2u);
}

/* FC 05 — samd20 write_coil semantics (any nonzero -> 0xFF). Port fix: echo
 * fc 0x05 / 8 bytes (samd20 answered 0x02 / 9 bytes — copy-paste bug). */
static uint8_t mb_write_coil(mb_core_t *mb, const uint8_t *frame,
                             uint8_t resp[MB_RESP_MAX])
{
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t val  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    if (addr >= MB_COIL_COUNT) {
        return 0;
    }
    mb->coils[addr] = (val != 0u) ? 0xFFu : 0x00u;

    resp[0] = mb->device_addr;
    resp[1] = 0x05u;
    resp[2] = frame[2];
    resp[3] = frame[3];
    resp[4] = frame[4];
    resp[5] = frame[5];
    uint16_t crc = mb_crc16(resp, 6u);
    resp[6] = (uint8_t)(crc >> 8);
    resp[7] = (uint8_t)(crc);
    return 8u;
}
```

`mb_core_decode`의 `switch`를 다음으로 교체:

```c
    switch (frame[1]) {
    case 0x01u:
    case 0x02u:
        n = mb_read_coils(mb, frame, frame[1], resp);
        break;
    case 0x03u:
    case 0x04u:
        n = mb_read_regs(mb, frame, frame[1], resp);
        break;
    case 0x05u:
        n = mb_write_coil(mb, frame, resp);
        break;
    case 0x06u:
        n = mb_write_reg(mb, frame, resp);
        break;
    default:
        return 0;   /* unsupported FC: silence (samd20 builds no exception) */
    }
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: `all checks PASSED` ×2

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_modbus_core.c fw/test/test_app_modbus_core.c
git commit -m "feat(stage-c): modbus core FC06 write + FC01/02/05 coils (samd20 echo/full-byte bugs fixed)"
```

---

### Task 5: mon 출력 게이트

**Files:**
- Modify: `fw/include/mon.h`
- Modify: `fw/drivers/mon_usart6.c`

- [ ] **Step 1: mon.h에 게이트 선언 추가**

`fw/include/mon.h` 전체:

```c
/* fw/include/mon.h */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mon_init(void);
void mon_write(const uint8_t *buf, size_t len);
void mon_writeln(const char *s);
int  mon_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Output gate (spec §5): while Modbus owns USART6 the mon output is suppressed
 * so diagnostics never pollute the RS-485 bus. Suppression only — init state
 * is untouched and all mon_* calls stay safe no-ops. Default = enabled. */
void mon_set_enabled(bool on);
```

- [ ] **Step 2: mon_usart6.c에 게이트 구현**

`fw/drivers/mon_usart6.c`에서 `mon_init` 아래에 추가하고 `mon_write` 수정:

```c
static bool s_mon_enabled = true;

void mon_set_enabled(bool on) { s_mon_enabled = on; }

void mon_write(const uint8_t *buf, size_t len) {
    if (!s_mon_enabled) {
        return;   /* Modbus owns USART6 — keep the RS-485 bus clean */
    }
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, MON_TX_TIMEOUT_MS);
}
```

(`mon_writeln`/`mon_printf`는 `mon_write` 경유라 추가 변경 없음.)

- [ ] **Step 3: 빌드 확인**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -5`
Expected: 0-warning

- [ ] **Step 4: 커밋**

```bash
git add fw/include/mon.h fw/drivers/mon_usart6.c
git commit -m "feat(stage-c): mon output enable gate for usart6 occupancy switching"
```

---

### Task 6: `usart6_mb` RTU 전송층 (DMA circular RX + 갭 프레이밍 + blocking TX)

**Files:**
- Create: `fw/include/usart6_mb.h`
- Create: `fw/drivers/usart6_mb.c`
- Modify: `fw/include/periph.h`, `fw/src/periph.c`

- [ ] **Step 1: periph에 DMA 핸들 추가**

`fw/include/periph.h`의 `hdma_usart1_rx` 줄 아래:

```c
extern DMA_HandleTypeDef hdma_usart6_rx;   /* USART6 RX — DMA2 S1 Ch5 circular (Modbus RTU) */
```

`fw/src/periph.c`의 `hdma_usart1_rx` 줄 아래:

```c
DMA_HandleTypeDef hdma_usart6_rx;   /* USART6 RX — DMA2 S1 Ch5 circular (Modbus RTU) */
```

- [ ] **Step 2: 헤더 작성**

```c
/* fw/include/usart6_mb.h — USART6 Modbus-RTU transport (PC6/PC7 AF8; RS-485
 * DE = HW-auto per V30 U13/U16, no FW control). Shares the USART6 peripheral
 * with mon_usart6: the app_modbus occupancy rule (comm_mode==SERIAL &&
 * addr!=0) guarantees exactly one owner at a time.
 * RX = DMA2 Stream1 Ch5 circular free-running (usart1.c precedent — no IRQ,
 * overrun-immune). Frame boundary = samd20 max_break_cnt soft gap (250 us
 * ticks), reproduced as a per-baud ms threshold polled from the superloop
 * (plan §Deviations 1). TX = blocking (spec §2).
 * Callers: fw/src/app_modbus.c only. */
#pragma once
#include <stdint.h>

/* Re-init USART6 at the Modbus line config (cfg indices, samd20 verbatim:
 * speed 0..5 = 2400/4800/9600/19200/38400/115200, default 19200; parity
 * 0=EVEN 1=ODD 2=NONE) and start the circular RX DMA. */
void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx);

/* Stop RX DMA + deinit the UART. The caller (app_modbus) re-runs usart6_init()
 * to restore the mon line config (115200 8N1). */
void usart6_mb_close(void);

/* Poll for one complete gap-delimited RTU frame. Copies it into buf and
 * returns its length, or 0 when no complete frame is pending. Frames longer
 * than maxlen are drained and dropped (line garbage; master retries). */
uint8_t usart6_mb_rx_frame(uint8_t *buf, uint8_t maxlen);

/* Blocking TX. Response <= 105 B; worst stall ~482 ms @2400 (len-scaled
 * timeout) — bounded and spec-approved. Flushes the RX ring afterwards so an
 * auto-DE transceiver echo of our own response is never re-parsed. */
void usart6_mb_send(const uint8_t *buf, uint8_t len);
```

- [ ] **Step 3: 드라이버 구현**

```c
/* fw/drivers/usart6_mb.c — Modbus-RTU transport on USART6.
 * Mechanism notes:
 *  - RX is the usart1.c free-running circular DMA pattern (no UART/DMA IRQ,
 *    no NVIC): DMA consumes DR on every RXNE, so the receiver cannot wedge.
 *  - Frame boundary: the DMA write position holding still for >= the per-baud
 *    break time while bytes are pending = end of frame. This is samd20's
 *    max_break_cnt x 250 us timer ported to the 1 ms sys_tick grid (ceiled),
 *    chosen over the USART IDLE flag for USB-master mid-frame-gap tolerance
 *    and to keep the zero-ISR discipline (plan §Deviations 1).
 *  - Undetectable corner: if exactly 256 bytes (a full wrap) land between two
 *    polls the head looks unmoved — the merged frame then fails CRC and the
 *    master retries. Real frames are <= ~10 B; not reachable in practice. */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"     /* Error_Handler */
#include "sys_tick.h"
#include "usart6_mb.h"

#define MB_RX_DMA_SIZE  256u   /* power of two — mask, no modulo (usart1.c) */

static const uint32_t mb_baud[6]  = { 2400u, 4800u, 9600u, 19200u, 38400u, 115200u };
/* samd20 init_modbus max_break_cnt x 250 us, ceiled to 1 ms:
 * 56->14ms, 28->7ms, 14->3.5->4ms, >=19200: 7->1.75->2ms. */
static const uint8_t  mb_gap_ms[6] = { 14u, 7u, 4u, 2u, 2u, 2u };

static volatile uint8_t s_rx_dma_buf[MB_RX_DMA_SIZE];
static uint16_t s_rx_tail;       /* superloop reader index (sole owner) */
static uint16_t s_prev_head;     /* DMA position at the previous poll */
static uint32_t s_last_rx_ms;    /* last time the DMA position moved */
static uint32_t s_baud  = 19200u;
static uint8_t  s_gap_ms = 2u;
static uint8_t  s_open;

static uint16_t rx_head(void)
{
    /* DMA write position = SIZE - NDTR; mask the NDTR reload edge (reads 0 ->
     * head 256) back to 0 since head is used for indexing here. */
    uint16_t head = (uint16_t)(MB_RX_DMA_SIZE -
                               __HAL_DMA_GET_COUNTER(&hdma_usart6_rx));
    return (uint16_t)(head & (MB_RX_DMA_SIZE - 1u));
}

void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* Re-init USART6 at the Modbus line config. GPIO PC6/PC7 AF8 was set by
     * usart6_init() at boot and is never unconfigured. */
    HAL_UART_DeInit(&huart6);
    huart6.Instance      = USART6;
    huart6.Init.BaudRate = (speed_idx < 6u) ? mb_baud[speed_idx]
                                            : 19200u;   /* samd20 default branch */
    s_baud   = huart6.Init.BaudRate;
    s_gap_ms = (speed_idx < 6u) ? mb_gap_ms[speed_idx] : 2u;
    /* STM32 parity rides in the 9th bit: EVEN/ODD need WORDLENGTH_9B for
     * 8 data bits + parity. 9B+NONE (the HAL's u16-buffer mode) never occurs. */
    if (parity_idx == 0u) {
        huart6.Init.Parity     = UART_PARITY_EVEN;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else if (parity_idx == 1u) {
        huart6.Init.Parity     = UART_PARITY_ODD;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else {
        huart6.Init.Parity     = UART_PARITY_NONE;
        huart6.Init.WordLength = UART_WORDLENGTH_8B;
    }
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }

    /* DMA2 Stream1 Ch5 = USART6_RX (RM0401 DMA2 request map; the Stream2 Ch5
     * alternate is unusable — Stream2 belongs to USART1 RX). Same circular
     * free-running config as usart1.c. */
    hdma_usart6_rx.Instance                 = DMA2_Stream1;
    hdma_usart6_rx.Init.Channel             = DMA_CHANNEL_5;
    hdma_usart6_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart6_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart6_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart6_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart6_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart6_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);

    s_rx_tail    = 0;
    s_prev_head  = 0;
    s_last_rx_ms = sys_tick_get_ms();

    if (HAL_DMA_Start(&hdma_usart6_rx, (uint32_t)&huart6.Instance->DR,
                      (uint32_t)s_rx_dma_buf, MB_RX_DMA_SIZE) != HAL_OK) {
        Error_Handler();
    }
    SET_BIT(USART6->CR3, USART_CR3_DMAR);
    s_open = 1u;
}

void usart6_mb_close(void)
{
    if (s_open == 0u) {
        return;
    }
    CLEAR_BIT(USART6->CR3, USART_CR3_DMAR);
    (void)HAL_DMA_Abort(&hdma_usart6_rx);
    (void)HAL_DMA_DeInit(&hdma_usart6_rx);
    (void)HAL_UART_DeInit(&huart6);
    s_open = 0u;
    /* caller re-runs usart6_init() to restore the mon line config */
}

uint8_t usart6_mb_rx_frame(uint8_t *buf, uint8_t maxlen)
{
    if (s_open == 0u) {
        return 0;
    }
    uint32_t now  = sys_tick_get_ms();
    uint16_t head = rx_head();

    if (head != s_prev_head) {       /* bytes still arriving — restart the gap */
        s_prev_head  = head;
        s_last_rx_ms = now;
        return 0;
    }
    if (head == s_rx_tail) {         /* nothing pending */
        return 0;
    }
    if ((uint32_t)(now - s_last_rx_ms) < s_gap_ms) {
        return 0;                    /* break gap not yet elapsed */
    }

    /* Gap elapsed: tail..head = one frame (samd20 endOfMessage equivalent). */
    uint16_t n = 0;
    while (s_rx_tail != head) {
        uint8_t b = s_rx_dma_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1u) & (MB_RX_DMA_SIZE - 1u));
        if (n < maxlen) {
            buf[n] = b;
        }
        n++;                         /* oversize: keep draining, then drop */
    }
    if (n > maxlen) {
        return 0;                    /* oversize frame dropped (line garbage) */
    }
    return (uint8_t)n;
}

void usart6_mb_send(const uint8_t *buf, uint8_t len)
{
    if (s_open == 0u) {
        return;
    }
    /* Blocking TX (spec). Timeout scaled to the frame time: 11 bits/char
     * (start + 8 + parity + stop) + 50 ms margin. Worst case 105 B @2400
     * ~= 482 ms — bounded superloop stall, spec-approved. */
    uint32_t timeout = ((uint32_t)len * 11000u) / s_baud + 50u;
    (void)HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, timeout);

    /* Echo guard (plan §Deviations 7): if the auto-DE transceiver loops our
     * own TX back into RX, discard everything received during the send so the
     * response (FC06 echo == request bytes!) is never re-parsed as a request. */
    s_rx_tail    = rx_head();
    s_prev_head  = s_rx_tail;
    s_last_rx_ms = sys_tick_get_ms();
}
```

- [ ] **Step 4: 빌드 확인**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -5`
Expected: 0-warning (GLOB이 신규 드라이버 자동 포함 — `cmake -B build`재구성이 필요하면 `env -u STM32_TOOLCHAIN cmake -B build -G Ninja` 후 빌드)

- [ ] **Step 5: 커밋**

```bash
git add fw/include/usart6_mb.h fw/drivers/usart6_mb.c fw/include/periph.h fw/src/periph.c
git commit -m "feat(stage-c): usart6_mb RTU transport — DMA circular RX + samd20 break-gap framing + blocking TX"
```

---

### Task 7: `app_reg` — 명령 소스(US_COMM) 확장 + amp 추적 + COMM ceiling

**Files:**
- Modify: `fw/include/app_lcd.h` (lcd_measure_t)
- Modify: `fw/include/app_reg.h`
- Modify: `fw/src/app_reg.c`
- Modify: `fw/src/app_lcd.c` (호출처)

- [ ] **Step 1: 호출처 전수 확인**

Run: `grep -rn "app_reg_command" fw/src fw/include fw/drivers`
Expected: 정의(`app_reg.c`) + 선언(`app_reg.h`) + 호출 1곳(`app_lcd.c` `app_lcd_hook_us_command`)만. 다른 호출처가 나오면 그곳도 src 인자 추가.

- [ ] **Step 2: `lcd_measure_t`에 amp 필드 추가**

`fw/include/app_lcd.h`의 lcd_measure_t에서:

```c
    uint16_t curr_amp;
```

를 다음으로 교체:

```c
    uint16_t curr_amp, max_amp, last_amp;   /* amp peak/latch — Modbus DISP_AMP
                                             * mirror (spec §3.1), same pattern
                                             * as max_power/last_power */
```

- [ ] **Step 3: `app_reg.h` 시그니처 확장**

`app_reg.h`의 `app_reg_command` 선언+주석을 다음으로 교체:

```c
/* Route an ultrasonic command into the run FSM. src = command source
 * (US_TOUCH from the panel hook, US_COMM from Modbus — samd20 us_run_status
 * taxonomy). START arms only from US_IDLE; RUN_RELEASE stops only the run its
 * own source started (samd20: comm STOP ==US_COMM, touch release ==US_TOUCH).
 * SEEK/RESET = no-op this slice (deferred, spec §9). Superloop single-thread —
 * mutates FSM state in place. us_cmd_t comes from the included app_lcd.h. */
void app_reg_command(us_cmd_t cmd, uint8_t src);
```

- [ ] **Step 4: `app_reg.c` 수정 — 상태 필드 + 명령 + ceiling + publish**

(a) `reg_state_t`의 `max_power`/`last_power` 줄 아래에 추가:

```c
    uint16_t max_amp;                /* running peak of curr_amp during the run */
    uint16_t last_amp;               /* amp latched on stop (samd20 last_amp) */
```

(b) `app_reg_command` 전체를 다음으로 교체:

```c
void app_reg_command(us_cmd_t cmd, uint8_t src)
{
    switch (cmd) {
    case US_CMD_START:
        /* M16-faithful: commands are ignored during the boot warm-up (Timer1
         * ISR skips the PINA dispatcher app_0x06d2 while g_main_state!=0,
         * disasm @0x041E); after warm-up RUN is an immediate level-follow
         * gate (no per-START ramp). == US_IDLE strict guard for BOTH sources
         * (intentional deviation from samd20's comm !=US_REMOTE takeover,
         * approved spec §4 — REMOTE arbitration is a later slice). */
        if ((g_reg.main_state == 0u) &&
            (g_reg.us_run_status == (uint8_t)US_IDLE)) {
            if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
                /* V30 RUN button sends data=0 on BOTH edges: after an on-time
                 * ceiling stop, the still-held button's release arrives mapped
                 * as START (input layer sees IDLE). Consume it once instead of
                 * restarting the run. Touch-only: a COMM START is a register
                 * write with no release back-mapping (spec §4). */
                g_reg.swallow_start = 0u;
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
            g_reg.max_power     = 0u;
            g_reg.max_amp       = 0u;    /* samd20 comm START zeroes max_amp too */
            g_reg.run_start_ms  = sys_tick_get_ms();
            /* samd20 zeroes us_on_time_200m at the run-start edge (main.c:4306);
             * the live compute would reach 0 on the first active publish anyway,
             * but zeroing here closes the <=2ms window where a disp read could
             * pair the old time value with the new run status. */
            g_measure.us_on_time_200m = 0u;
        }
        break;
    case US_CMD_RUN_RELEASE:
        /* Source-matched stop only (samd20 main.c:3699/4180 touch, 4405 comm):
         * a COMM STOP cannot kill a touch run and vice versa. Latch the
         * power/amp peaks for the stopped-display mirror. */
        if (g_reg.us_run_status == src) {
            g_reg.last_power    = g_reg.max_power;
            g_reg.last_amp      = g_reg.max_amp;
            g_reg.us_run_status = (uint8_t)US_IDLE;
        } else if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
            /* Any touch RUN_RELEASE arriving while IDLE after a ceiling stop
             * resyncs the press/release pairing so the next genuine press is
             * not eaten: legacy data=4 release, or SYS_PIC_NOW re-init (panel
             * reset means the physical release will never arrive). */
            g_reg.swallow_start = 0u;
        }
        break;
    case US_CMD_SEEK:
    case US_CMD_RESET:
    default:
        /* Regulation effect deferred (spec §9): the input layer already does
         * the RESET icon/page restore; SEEK/RESET drive is B-SEAM. NB samd20
         * comm RESET marks src US_TOUCH (main.c:4353 quirk) and clears the
         * error display — both inert until the error machine slice. */
        break;
    }
#ifdef REG_TRACE
    mon_printf("[reg] cmd=%u src=%u run=%u\r\n", (unsigned)cmd, (unsigned)src,
               (unsigned)g_reg.us_run_status);
#endif
}
```

(c) `app_reg_tick`의 on-time ceiling 블록(`if (g_reg.us_run_status == (uint8_t)US_TOUCH) { ... }` 전체)을 다음으로 교체:

```c
    /* Run on-time ceiling (limit_on_time x10 ms, 0 = off, panel-editable).
     * COMM runs: samd20-faithful (main.c:5296 applies it to COMM/REMOTE in
     * SYS_HAND; 2026-06-10 analysis doc authority). TOUCH runs: intentional
     * STM32 safety addition — the V30 RUN button's data=0 quirk can lose the
     * release edge (infinite run); the M16 itself also force-cleared the run
     * flag on an internal countdown (g_018F, Timer1 ISR @0x0572).
     * samd20's multi_ctrl/energy_ctrl run branches (main.c:5234..) belong to
     * the weld-cycle machine — deferred, spec §8. */
    {
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM)) {
            /* limit_on_time is injected by the caller from the live config
             * each call (M1: no call back into app_lcd), so a panel edit of
             * LV_MAX_ON_TIME still takes effect immediately, even mid-run. */
            if ((limit_on_time != 0u) &&
                ((uint32_t)(now - g_reg.run_start_ms) >=
                 (uint32_t)limit_on_time * ON_TIME_UNIT_MS)) {
                g_reg.last_power    = g_reg.max_power;   /* same latch as release */
                g_reg.last_amp      = g_reg.max_amp;
                g_reg.us_run_status = (uint8_t)US_IDLE;
                if (rs == (uint8_t)US_TOUCH) {
                    /* held button's release still pending — touch-only quirk */
                    g_reg.swallow_start = 1u;
                }
#ifdef REG_TRACE
                mon_printf("[reg] on-time ceiling (%u x10ms) -> stop\r\n",
                           (unsigned)limit_on_time);
#endif
            }
        }
    }
```

(d) `reg_publish_measure`에서 `g_measure.curr_amp = g_reg.ch0_avg;` 바로 아래에 추가:

```c
    if (active && (g_reg.ch0_avg > g_reg.max_amp)) {
        g_reg.max_amp = g_reg.ch0_avg;   /* amp peak — max_power와 동일 패턴 */
    }
```

그리고 `g_measure.max_power = g_reg.max_power;` 줄 아래에 추가:

```c
    g_measure.max_amp  = g_reg.max_amp;
    g_measure.last_amp = g_reg.last_amp;
```

(e) `reg_publish_measure`의 us_on_time_200m 주석에서 `Only TOUCH runs exist this slice, so run_start_ms is always the active run's stamp; REMOTE/COMM slices must stamp their own start.` 부분을 다음으로 교체:

```c
         * TOUCH and COMM runs both stamp run_start_ms at their START edge
         * (app_reg_command); a future REMOTE slice must stamp its own start.
```

- [ ] **Step 5: 호출처 수정**

`fw/src/app_lcd.c`의 `app_lcd_hook_us_command`:

```c
void app_lcd_hook_us_command(us_cmd_t cmd)
{
    mon_printf("[lcd-hook] us_command=%u\r\n", (unsigned)cmd);
    app_reg_command(cmd, (uint8_t)US_TOUCH);   /* panel keys = touch source */
}
```

- [ ] **Step 6: 빌드 + 호스트 테스트**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -5
make -C test test
```

Expected: 0-warning + `all checks PASSED` ×2. (run FSM은 호스트 미테스트 — slice 2b와 동일하게 HW 게이트 의도.)

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_lcd.h fw/include/app_reg.h fw/src/app_reg.c fw/src/app_lcd.c
git commit -m "feat(stage-c): run FSM command source (US_COMM) + amp peak tracking + COMM on-time ceiling"
```

---

### Task 8: `app_modbus` 통합층 + 슈퍼루프 배선

**Files:**
- Create: `fw/include/app_modbus.h`
- Create: `fw/src/app_modbus.c`
- Modify: `fw/src/app.c`, `fw/src/main.c`, `fw/src/app_lcd.c`(hook 주석)

- [ ] **Step 1: 헤더 작성**

```c
/* fw/include/app_modbus.h — Modbus slave integration layer: owns what the
 * mb_core registers MEAN (samd20 update_holding_reg port — live mirror +
 * clamped config writes + FRAM commit), routes commands into the run FSM as
 * US_COMM, and arbitrates USART6 between mon and Modbus-RTU per the occupancy
 * rule (comm_mode==SERIAL && comm_address!=0, spec §5). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Boot: evaluate the occupancy rule from the loaded config and acquire USART6
 * if Modbus owns it. Call after app_init() — needs app_config_load done. */
void app_modbus_init(void);

/* Superloop step: re-evaluate occupancy/line config (covers panel DATA_SAVE
 * changes — samd20 main-loop gate equivalent), drain one RTU frame, respond,
 * run the write-apply pass on FC06, refresh the live register mirror. */
void app_modbus_tick(void);

/* True while Modbus owns USART6 (mon output suppressed). */
bool app_modbus_owns_usart6(void);
```

- [ ] **Step 2: 통합층 구현**

`fw/src/app_modbus.c` 신규 (전체):

```c
/* fw/src/app_modbus.c — samd20 Modbus slave integration port (spec §3~§5).
 * Mirror pass = update_holding_reg(0) field-for-field; write-apply pass =
 * update_holding_reg(1) one-change-per-message else-if chain with the samd20
 * clamps. FRAM persistence = whole-map app_config_save_all (repo pattern;
 * structurally fixes the samd20 DELAY3->ADDR_TRIGGER2 / TRIGGER2->ADDR_DELAY2
 * copy-paste bugs — spec §3.2). Occupancy switching = per-tick cfg compare
 * (samd20 main-loop gate (comm_mode==SERIAL)&&(addr!=0), main.c:5043, plus
 * the DATA_SAVE close/init pair 3387/3429/3501 — tick-polled instead of
 * hook-driven so a comm_mode-only change also releases the port and no
 * app_lcd<->app_modbus include cycle forms; plan §Deviations 5). */
#include <string.h>
#include "app_modbus.h"
#include "app_modbus_core.h"
#include "usart6_mb.h"
#include "app_lcd.h"      /* app_lcd_cfg/app_lcd_measure/hooks/us enum */
#include "app_reg.h"
#include "app_config.h"
#include "dgus_lcd.h"     /* DISP_*_EN echo (samd20 send_lcd_data_var) */
#include "mon.h"

#define MB_COMM_MODE_SERIAL  0u   /* cfg->comm_mode: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP */

extern void usart6_init(void);    /* drivers/usart.c — mon line config restore */

static mb_core_t g_mb;
static struct {
    uint8_t owned;
    uint8_t speed_idx;
    uint8_t parity_idx;
    uint8_t addr;
} g_applied;

bool app_modbus_owns_usart6(void)
{
    return g_applied.owned != 0u;
}

/* samd20 update_holding_reg(0): live values -> holding mirror. Runs every
 * owned tick (plan §Deviations 6: fresher reads than samd20's post-message
 * refresh + immediately normalizes clamped writes). */
static void mirror_live(void)
{
    const app_config_t  *cfg = app_lcd_cfg();
    const lcd_measure_t *m   = app_lcd_measure();
    uint8_t running = (m->us_run_status != (uint8_t)US_IDLE) ? 1u : 0u;

    g_mb.holding[MB_REG_WORK_CNTH]   = (uint16_t)(cfg->work_cnt >> 16);
    g_mb.holding[MB_REG_WORK_CNTL]   = (uint16_t)(cfg->work_cnt);
    g_mb.holding[MB_REG_DELAY1]      = cfg->limit_delay_time1;
    g_mb.holding[MB_REG_DELAY2]      = cfg->limit_delay_time2;
    g_mb.holding[MB_REG_DELAY3]      = cfg->limit_delay_time3;
    g_mb.holding[MB_REG_TRIGGER2]    = cfg->limit_trigger_time2;
    g_mb.holding[MB_REG_TRIGGER3]    = cfg->limit_trigger_time3;
    g_mb.holding[MB_REG_OUT_POWER]   = cfg->output_power;
    g_mb.holding[MB_REG_ON_TIME]     = cfg->limit_on_time;
    g_mb.holding[MB_REG_ENERGY]      = (uint16_t)cfg->limit_energy;
    g_mb.holding[MB_REG_MULTI_T1]    = cfg->limit_mo_time1;
    g_mb.holding[MB_REG_MULTI_T2]    = cfg->limit_mo_time2;
    g_mb.holding[MB_REG_MULTI_O1]    = cfg->limit_mo_out1;
    g_mb.holding[MB_REG_MULTI_O2]    = cfg->limit_mo_out2;
    g_mb.holding[MB_REG_TIMEOVER]    = cfg->limit_out_time;
    /* DISP_*: running shows the live peak, stopped shows the latched last
     * (samd20 main.c:4564-4567 us_on_status mirror; freq/energy honest 0 —
     * machinery deferred, spec §3.1). */
    g_mb.holding[MB_REG_DISP_POWER]  = running ? m->max_power : m->last_power;
    g_mb.holding[MB_REG_DISP_AMP]    = running ? m->max_amp   : m->last_amp;
    g_mb.holding[MB_REG_DISP_FREQ]   = running ? m->curr_freq : m->last_freq;
    g_mb.holding[MB_REG_DISP_ENERGY] = running ? (uint16_t)m->curr_energy
                                               : (uint16_t)m->last_energy;
    g_mb.holding[MB_REG_MODEL_FREQ]  = cfg->model_freq;    /* read-only: mirror */
    g_mb.holding[MB_REG_MODEL_TYPE]  = cfg->model_type;    /* overwrites writes */
    g_mb.holding[MB_REG_RUN_MODE]    = cfg->run_mode;
    g_mb.holding[MB_REG_EN_ENERGY]   = cfg->energy_ctrl ? 1u : 0u;
    g_mb.holding[MB_REG_EN_MULTI]    = cfg->multi_ctrl  ? 1u : 0u;
    g_mb.holding[MB_REG_EN_SAFTY]    = cfg->f_safty;
    /* STATUS bit0 = run active (spec §3.1: us_run_status != US_IDLE);
     * ESTOP/OVLD/OVTIME/OUTERR stay 0 until the overload/weld slices. */
    g_mb.holding[MB_REG_STATUS]      = running ? MB_STATUS_US : 0u;
}

/* samd20 update_holding_reg(1): one else-if chain per message — commands
 * first (consume-and-clear), then the single config field that differs
 * (clamped, persisted). Chain order preserved verbatim. */
static void apply_writes(void)
{
    app_config_t *cfg = app_lcd_cfg();
    uint16_t v;
    bool save = false;

    if (g_mb.holding[MB_REG_RESET] == 1u) {
        /* Routed for the consume-and-clear shape; effect = no-op this slice
         * (RESET->SEEK chain + error machine deferred, spec §3.3). */
        app_reg_command(US_CMD_RESET, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_RESET] = 0u;
    } else if (g_mb.holding[MB_REG_SEEK] == 1u) {
        app_reg_command(US_CMD_SEEK, (uint8_t)US_COMM);   /* no-op, deferred */
        g_mb.holding[MB_REG_SEEK] = 0u;
    } else if (g_mb.holding[MB_REG_START] == 1u) {
        app_reg_command(US_CMD_START, (uint8_t)US_COMM);
        if (app_lcd_measure()->us_run_status == (uint8_t)US_COMM) {
            /* START accepted: samd20 comm START writes the amplitude pot in
             * the same breath (I2C_POT, main.c:4400) — stub hook logs until
             * B-SEAM/F2 resolves the pot identity. */
            app_lcd_hook_set_pot(cfg->output_power);
        }
        g_mb.holding[MB_REG_START] = 0u;
    } else if (g_mb.holding[MB_REG_STOP] == 1u) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_STOP] = 0u;
    } else if (g_mb.holding[MB_REG_DELAY1] != cfg->limit_delay_time1) {
        v = g_mb.holding[MB_REG_DELAY1];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY2] != cfg->limit_delay_time2) {
        v = g_mb.holding[MB_REG_DELAY2];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY3] != cfg->limit_delay_time3) {
        v = g_mb.holding[MB_REG_DELAY3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_delay_time3 = v;     /* samd20 saved this to ADDR_TRIGGER2 —
                                         * copy-paste bug, fixed by save_all */
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER2] != cfg->limit_trigger_time2) {
        v = g_mb.holding[MB_REG_TRIGGER2];
        if (v > 500u) { v = 500u; }
        cfg->limit_trigger_time2 = v;   /* samd20 saved to ADDR_DELAY2 — ditto */
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER3] != cfg->limit_trigger_time3) {
        v = g_mb.holding[MB_REG_TRIGGER3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_trigger_time3 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_OUT_POWER] != cfg->output_power) {
        v = g_mb.holding[MB_REG_OUT_POWER];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->output_power = (uint8_t)v;
        save = true;
    } else if (g_mb.holding[MB_REG_ON_TIME] != cfg->limit_on_time) {
        v = g_mb.holding[MB_REG_ON_TIME];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_on_time = v;
        save = true;
    } else if (g_mb.holding[MB_REG_ENERGY] != (uint16_t)cfg->limit_energy) {
        cfg->limit_energy = (uint32_t)g_mb.holding[MB_REG_ENERGY];
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T1] != cfg->limit_mo_time1) {
        v = g_mb.holding[MB_REG_MULTI_T1];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T2] != cfg->limit_mo_time2) {
        v = g_mb.holding[MB_REG_MULTI_T2];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O1] != cfg->limit_mo_out1) {
        v = g_mb.holding[MB_REG_MULTI_O1];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O2] != cfg->limit_mo_out2) {
        v = g_mb.holding[MB_REG_MULTI_O2];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_TIMEOVER] != cfg->limit_out_time) {
        v = g_mb.holding[MB_REG_TIMEOVER];
        if (v > 10u) { v = 10u; }
        cfg->limit_out_time = v;        /* samd20 wrote the clamp back into the
                                         * reg; our per-tick mirror does that */
        save = true;
    } else if (g_mb.holding[MB_REG_RUN_MODE] != cfg->run_mode) {
        cfg->run_mode = (uint8_t)g_mb.holding[MB_REG_RUN_MODE];   /* no clamp
                                         * (samd20 faithful — stored as-is) */
        save = true;
    } else if (((g_mb.holding[MB_REG_EN_ENERGY] == 0u) && cfg->energy_ctrl) ||
               ((g_mb.holding[MB_REG_EN_ENERGY] == 1u) && !cfg->energy_ctrl)) {
        /* samd20 acts only on exact 0/1 values (4512) — faithful. */
        cfg->energy_ctrl = (g_mb.holding[MB_REG_EN_ENERGY] == 1u);
        dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
        save = true;
    } else if (((g_mb.holding[MB_REG_EN_MULTI] == 0u) && cfg->multi_ctrl) ||
               ((g_mb.holding[MB_REG_EN_MULTI] == 1u) && !cfg->multi_ctrl)) {
        cfg->multi_ctrl = (g_mb.holding[MB_REG_EN_MULTI] == 1u);
        dgus_write_u16(DISP_MULTI_EN, cfg->multi_ctrl ? 1u : 0u);
        save = true;
    } else if (g_mb.holding[MB_REG_EN_SAFTY] != cfg->f_safty) {
        cfg->f_safty = (uint8_t)g_mb.holding[MB_REG_EN_SAFTY];
        dgus_write_u16(DISP_SAFTY, cfg->f_safty);
        save = true;
    } else if ((g_mb.holding[MB_REG_WORK_CNTL] == 0u) &&
               ((uint16_t)cfg->work_cnt != 0u)) {
        /* CNTL=0 write = work counter reset (samd20 main.c:4539: cfg + FRAM +
         * LCD refresh). Low-word compare faithful to the samd20 condition. */
        cfg->work_cnt = 0u;
        app_lcd_set_work_cnt(0u);
        save = true;
    }

    if (save) {
        /* Whole-map FRAM commit — codebase pattern (data_save_commit). */
        app_config_save_all(cfg);
    }
    /* mirror_live() runs right after in app_modbus_tick(): the next read
     * returns the clamped/applied value, and a clamped write can't re-fire
     * this chain on the next message. */
}

/* Occupancy + line-config edge detector. Cheap compares every tick;
 * transitions (close/open, mon gate) only on change. */
static void apply_config(void)
{
    const app_config_t *cfg = app_lcd_cfg();
    uint8_t want = ((cfg->comm_mode == MB_COMM_MODE_SERIAL) &&
                    (cfg->comm_address != 0u)) ? 1u : 0u;

    if ((want != 0u) && (g_applied.owned != 0u) &&
        (cfg->comm_speed_idx == g_applied.speed_idx) &&
        (cfg->comm_parity_idx == g_applied.parity_idx)) {
        if (cfg->comm_address != g_applied.addr) {
            /* address-only change: retarget the slave id, no line re-init */
            g_applied.addr   = cfg->comm_address;
            g_mb.device_addr = cfg->comm_address;
        }
        return;                                   /* steady state */
    }
    if ((want == 0u) && (g_applied.owned == 0u)) {
        return;                                   /* steady state */
    }

    if (g_applied.owned != 0u) {
        /* release (or reconfigure: close first, reacquire below) */
        usart6_mb_close();
        usart6_init();                            /* restore mon 115200 8N1 */
        mon_set_enabled(true);
        g_applied.owned = 0u;
        mon_printf("[mb] release usart6 (mode=%u addr=%u)\r\n",
                   (unsigned)cfg->comm_mode, (unsigned)cfg->comm_address);
    }
    if (want != 0u) {
        /* log BEFORE the gate closes so the transition is visible on mon */
        mon_printf("[mb] acquire usart6 speed=%u parity=%u addr=%u\r\n",
                   (unsigned)cfg->comm_speed_idx,
                   (unsigned)cfg->comm_parity_idx,
                   (unsigned)cfg->comm_address);
        mon_set_enabled(false);
        usart6_mb_open(cfg->comm_speed_idx, cfg->comm_parity_idx);
        mb_core_init(&g_mb, cfg->comm_address);   /* samd20 init_modbus zeroes tables */
        g_applied.owned      = 1u;
        g_applied.speed_idx  = cfg->comm_speed_idx;
        g_applied.parity_idx = cfg->comm_parity_idx;
        g_applied.addr       = cfg->comm_address;
        mirror_live();   /* baseline (samd20 boot update_holding_reg(0), main.c:5027) */
    }
}

void app_modbus_init(void)
{
    memset(&g_applied, 0, sizeof(g_applied));
    mb_core_init(&g_mb, 0u);
    apply_config();
}

void app_modbus_tick(void)
{
    apply_config();
    if (g_applied.owned == 0u) {
        return;
    }

    uint8_t frame[MB_FRAME_MAX];
    uint8_t len = usart6_mb_rx_frame(frame, sizeof frame);
    if (len != 0u) {
        uint8_t resp[MB_RESP_MAX];
        uint8_t fc = 0u;
        uint8_t n  = mb_core_decode(&g_mb, frame, len, MB_MODE_RTU, resp, &fc);
        if (n != 0u) {
            usart6_mb_send(resp, n);
        }
        if (fc == 0x06u) {
            apply_writes();          /* samd20: update_holding_reg(1) on FC06 */
        }
    }

    mirror_live();
}
```

- [ ] **Step 3: 슈퍼루프 + 부팅 배선**

`fw/src/app.c`: include 목록에 `#include "app_modbus.h"` 추가, `app_loop_iter()` 끝(`app_reg_tick(...)` 호출 아래)에 추가:

```c
    /* 4. Modbus slave — occupancy re-eval + one RTU frame per iter (spec §2).
     * After app_reg_tick so the mirror sees this iter's freshest measure. */
    app_modbus_tick();
```

`fw/src/main.c`: include 목록에 `#include "app_modbus.h"` 추가, `app_reg_init();` 아래에 추가:

```c
    app_modbus_init(); /* Stage C: USART6 occupancy decision (needs cfg loaded by app_init) */
```

- [ ] **Step 4: comm hook 주석 실체화**

`fw/src/app_lcd.c`의 `app_lcd_hook_comm_reconfigure`를 다음으로 교체 (로그 유지, 동작 주석화 — §Deviations 5):

```c
void app_lcd_hook_comm_reconfigure(uint8_t speed_idx, uint8_t parity_idx, uint8_t address)
{
    /* Intentionally log-only: app_modbus_tick() re-evaluates the occupancy
     * rule + line params against live cfg every superloop iter (samd20
     * main-loop gate equivalent), so the close/reinit happens within one iter
     * of DATA_SAVE — including the comm_mode-only change this hook never sees.
     * Keeping the body passive avoids an app_lcd <-> app_modbus include cycle
     * (M1 discipline). NB: suppressed while Modbus owns USART6 (mon gate). */
    mon_printf("[lcd-hook] comm speed=%u parity=%u addr=%u\r\n",
               (unsigned)speed_idx, (unsigned)parity_idx, (unsigned)address);
}
```

- [ ] **Step 5: 빌드 + 호스트 테스트**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -6
make -C test test
```

Expected: 0-warning, FLASH/RAM 사용률 출력 확인(메모: mb_core ≈151B + DMA buf 256B RAM 증가), `all checks PASSED` ×2

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_modbus.h fw/src/app_modbus.c fw/src/app.c fw/src/main.c fw/src/app_lcd.c
git commit -m "feat(stage-c): app_modbus integration — live mirror, clamped writes + FRAM, US_COMM commands, usart6 occupancy switching"
```

---

### Task 9: 최종 게이트 — main + trace 구성 0-warning, 문서

**Files:**
- Modify: `docs/changelog.md` (항목 추가)

- [ ] **Step 1: main 구성 클린 재빌드**

```bash
cd fw
rm -rf build && env -u STM32_TOOLCHAIN cmake -B build -G Ninja
env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -8
```

Expected: 0-warning, `-Wall -Wextra -Wundef -Wshadow` 통과, FLASH/RAM % 기록

- [ ] **Step 2: 트레이스 구성 syntax-check (게이트된 코드 포함 컴파일)**

```bash
cd fw
rm -rf build-trace
env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE -DLCD_TRACE_RX"
env -u STM32_TOOLCHAIN cmake --build build-trace 2>&1 | tail -8
```

(⚠ `-DCMAKE_C_FLAGS`는 툴체인 `CMAKE_C_FLAGS_INIT`을 덮어쓰므로 CPU 플래그를 반드시 함께 — 2026-06-02 정정, RESUME §First Step.)
Expected: 0-warning. **주의**: 이 빌드는 slice 2b 재검증용 보존 `fw/build-trace/`를 덮어씀 — Step 3에서 원복.

- [ ] **Step 3: slice 2b 보존 트레이스 바이너리 원복**

slice 2b HW 재검증은 브랜치 `feat/stage-d-slice2b-run-gate` tip 기준 바이너리가 필요. `fw/build-trace/`는 gitignore라 git이 보존하지 않으므로, **재검증 시점에 그 브랜치에서 다시 빌드**하면 됨(HANDOFF.md §Resume 절차에 이미 빌드 명령 포함 여부 확인). 여기서는 본 브랜치 trace 빌드 산출물을 지워 혼동 방지:

```bash
rm -rf fw/build-trace
```

- [ ] **Step 4: 호스트 테스트 최종 확인**

Run: `make -C fw/test test`
Expected: `all checks PASSED` ×2

- [ ] **Step 5: changelog 항목 추가**

`docs/changelog.md` 최신 항목 위에 (기존 포맷 따름):

```markdown
## 2026-06-XX — Stage C slice 1: Modbus 코어 + RTU (코드 완결, HW E2E 대기)

- 신규 `app_modbus_core.{c,h}`: samd20 modbus.c 충실 포팅(CRC16, FC 01~06, holdingReg/coils 50) — HAL-free, 호스트 테스트 전수(`fw/test/test_app_modbus_core.c`). 포팅 수정: 요청 범위검사(원본 OOB read/임의 write), FC05 에코(원본 0x02/9B 복붙버그), FC01/02 8의 배수 마지막 바이트.
- 신규 `usart6_mb.{c,h}`: RTU 전송층 — DMA2 S1 Ch5 circular RX(no-ISR, usart1 선례) + samd20 max_break_cnt 갭 프레이밍(ms 그리드) + blocking TX + TX-에코 가드.
- 신규 `app_modbus.{c,h}`: update_holding_reg 등가(매 tick 라이브 미러 + FC06 1-change 체인: samd20 클램프 + save_all FRAM 커밋[원본 FRAM 주소 복붙버그 2건 구조적 해소] + EN_* LCD 에코 + work_cnt 리셋), 명령 START/STOP/SEEK/RESET → run FSM US_COMM, USART6 mon↔Modbus 점유 전환(매 tick cfg 평가 = samd20 메인루프 게이트 등가) + mon 출력 게이트.
- `app_reg`: `app_reg_command(cmd, src)` 소스 확장(US_TOUCH/US_COMM, 소스일치 정지), max_amp/last_amp 추적(DISP_AMP 미러), on-time ceiling을 COMM 런에도 적용(원본충실; swallow_start는 TOUCH 전용 유지).
- HW E2E(mbpoll 매트릭스) = 보드 연결 후 (plan §HW-gated 절차).
```

(XX = 실제 작업일자로. RESUME/메모리 갱신은 세션 마감 루틴에서.)

- [ ] **Step 6: 커밋**

```bash
git add docs/changelog.md
git commit -m "docs(stage-c): changelog — modbus slice 1 core+RTU code-complete"
```

---

## HW-gated E2E 절차 (보드 연결 후 — 이 plan에서는 실행하지 않음)

전제: Mac ↔ V30 RS-485(USB-serial), `mbpoll` 설치(`brew install mbpoll`). 패널에서 comm: SERIAL + addr=1 + 19200 + NONE 저장 → mon 로그에 `[mb] acquire ...` 후 mon 침묵 확인.

1. **폴링(FC03 라이브)**: `mbpoll -a 1 -b 19200 -P none -t 4 -r 1 -c 30 /dev/cu.usbserial-XXXX` → 0x00~0x1D 덤프; OUT_POWER/ON_TIME 등 = 패널 설정값, STATUS=0.
2. **설정 쓰기 + 영속**: `mbpoll ... -r 7 -t 4 -- 80` (OUT_POWER=80) → 즉시 재읽기 80, 패널 SETUP 표시 일치, 전원사이클 후 80 유지(FRAM). 클램프: 30 쓰기 → 50 읽힘.
3. **원격 런**: `-r 28 -t 4 -- 1` (START 0x1B) → STATUS bit0=1, ICON_RUN, DISP_POWER 라이브; ceiling(기본 500ms) 자동 정지; `-r 29 -- 1` (STOP) 동작; 터치 RUN과 상호 비간섭(소스일치 정지).
4. **점유 전환 라이브**: 패널에서 addr=NONE 저장 → `[mb] release` + mon 복구; addr=1 재저장 → 재획득. comm_mode=ETH 저장 → 해제 확인.
5. **속도/패리티 매트릭스 샘플**: {9600,EVEN}, {115200,NONE} 재저장 → 각 설정으로 mbpoll 폴링 OK.
6. work_cnt 리셋: `-r 2 -t 4 -- 0` → LV_WORK_CNT=0 + FRAM 영속.

**트레이스 빌드 주의(HANDOFF에 옮길 것)**: REG_TRACE/LCD_TRACE_RX 출력은 mon 경유 — Modbus 점유 중엔 침묵이 정상. 트레이스 검증은 addr=NONE(미점유) 상태에서.

---

## Self-Review 결과 (작성 후 점검)

- **스펙 커버리지**: §2 모듈 3개 = Task 2-4/6/8, 슈퍼루프 = T8. §3.1 미러 전 필드 + work_cnt 리셋 + MODEL read-only + STATUS bit0 = T8. §3.2 클램프 전 행 + FRAM(save_all) + EN_* LCD 에코 = T8. §3.3 명령 4종 consume-and-clear = T8 + T7. §3.4 코일 무매핑 = T4. §3.5 무응답/TCP 스킵 = T3. §4 src 확장/엄격가드/소스일치 정지/COMM ceiling/swallow 면제/multi·energy 주석/워밍업 가드/set_pot = T7+T8. §5 점유규칙/부팅결정/재설정/mon 게이트/트레이스 주의 = T5+T8+T9. §6 호스트 테스트 목록 전 항목 = T2-T4, 게이트 = T9, HW 절차 = §HW-gated. §7 브랜치 = T1. 갭 없음.
- **플레이스홀더**: 코드 블록 전부 완결(…/TBD 없음). apply_writes/mirror_live 전 분기 명시.
- **타입 일관성**: `mb_core_decode(mb, frame, len, mode, resp, &fc)` 시그니처 = 헤더/구현/테스트/app_modbus 동일. `app_reg_command(us_cmd_t, uint8_t)` = 헤더/구현/app_lcd.c/app_modbus.c 동일. `usart6_mb_rx_frame(buf, maxlen)->uint8_t` 호출처 일치. lcd_measure_t 신규 필드명 `max_amp`/`last_amp` = app_reg.c/app_modbus.c 동일.
