# Stage A — LCD I/O Bring-up Design (DGUS LCD on USART1)

**작성일**: 2026-05-05
**대상 MCU**: STM32F410RBT (Cortex-M4F @ 96 MHz, 128 KB Flash, 32 KB RAM)
**브랜치**: `feat/stage-a-lcd-io` (base = main @ Phase 1+2 merge `b8afe1c`)
**범위**: Stage A의 **첫 슬라이스 — DGUS LCD 단독 bring-up** (init + reset + page-set + 1Hz uptime VP write + passive RX 로깅). I/O 부분(CON_*, CTRL_OSC* 등)은 **OUT OF SCOPE**, 후속 슬라이스로 분리.
**브레인스토밍 세션**: 2026-05-05 (`superpowers:brainstorming`)

---

## 0. 컨텍스트 및 결정 요약

### 0.1 결정 이력 (브레인스토밍 채택안)

| # | 항목 | 결정 |
|---|------|------|
| Q1 | 슬라이스 데모 범위 | **(c)** init + DGUS reset(옵션) + set_page + **1Hz uptime VP write** + passive RX 로깅. 라운드트립 RD 응답 매칭은 OUT |
| Q2 | DGUS HMI 매핑 | **(α)** samd20 HMI VP 맵 그대로 재사용 (`LCD_RUN_STD=9`, `VAR_POWER=0x1110` 등) |
| Q3 | I/O 전략 | **(i)** **폴링 TX + IT byte-RX**. samd20 패턴을 STM32 HAL에 mirror |
| Q4 | API 표면 | **(C)** **samd20 9 함수 풀 패리티**: init / reset / set_page / write_u16 / write_u32 / write_bytes / write_u16_array / write_text / read_var |
| Q5 | HW 가용성 | **(I)** LCD 결선 완료 + samd20 HMI 플래시 가정. Phase 2 chunk 12와 동등한 HW verify chunk로 종료 |
| Arch | 드라이버 레이어링 | **(B) 2층 분리** (Phase 2 `usart.c + mon_usart6.c` 패턴 미러). `usart1.c` raw + `dgus_lcd.c` 프로토콜 |

### 0.2 작은 default 결정

| 항목 | Default | 근거 |
|------|---------|------|
| Baud / Format | 115200 8N1 | samd20 + DGUS T5L 표준 |
| GPIO AF | PA9/PA10 **AF7** | F410 datasheet AF table (chunk 0에서 재확인) |
| RX 링 버퍼 크기 | 64 byte (2의 거듭제곱) | DGUS 프레임 ≤ 25 B × 3 여유. samd20의 120은 과함 |
| RX 오버플로우 | drop + counter | samd20의 wrap-overwrite는 silent corruption |
| 프레임 LEN 상한 | 26 byte | DGUS_MAX_DATA=23 + cmd+addr=3 |
| RX 프레임 timeout | 50 ms (벽시계 `s_ms`) | samd20의 byte-counted `comm_tick`은 결함 |
| TX timeout | 10 ms | 최대 16 B × 87 µs/byte ≈ 1.4 ms의 7배 |
| NVIC USART1 priority | 5 (= TIM11) | 양 ISR 모두 짧음, queueing으로 충분 |
| 데모 부팅 페이지 | `LCD_RUN_STD = 9` | samd20 HMI에 VAR_POWER 시각화 |
| 데모 uptime VP | `VAR_POWER = 0x1110` (U16) | wrap @ 65535초 ≈ 18시간 |
| DGUS reset on boot | **0 (skip)** | 개발 단계 잦은 부팅에서 LCD 풀-재시작 불필요. `dgus_lcd.h` 매크로로 토글 |

### 0.3 OUT OF SCOPE (명시)

이 슬라이스에서 **하지 않는 것** — 나중에 "왜 안 했지" 질문 차단:

- ❌ DGUS RD round-trip 동기 wait + 응답 매칭 (API는 노출, 데모 호출 ✗)
- ❌ TX queue / 다중 호출 큐잉 (폴링 TX → caller block, 큐 불필요)
- ❌ DMA RX/TX
- ❌ Stage A I/O 부분 (CON_OVLD 출력, CON_START 입력, CTRL_OSC0~4 등) — 후속 슬라이스
- ❌ 다국어 / UTF-8 텍스트 (`dgus_write_text`는 10 B ASCII zero-pad만, samd20 그대로)
- ❌ Power management / USART low-power wake

---

## 1. 아키텍처 + 모듈 경계

### 1.1 핵심 원칙 (Phase 2 디시플린 그대로)

1. `fw/vendor/`는 read-only — 변경 ✗ (예외: `hal_conf.h` 1회 편집은 Phase 2에서 완료)
2. 각 드라이버는 자기 페리페럴의 GPIO AF 설정 소유 — `drivers/usart1.c`가 PA9/PA10 AF7 설정까지 책임
3. HAL 핸들 (`huart1`, `htim11`, `huart6`)은 `fw/src/periph.c`에 단일 정의, `periph.h`로 extern. 다른 곳 정의 ✗
4. USART1 raw 레이어(`usart1.c`)의 RX 링 버퍼는 file-static. `dgus_lcd.c`만 공개 API(`usart1_rx_pop`)를 통해 접근
5. `dgus_lcd.c`만 DGUS 프로토콜(0x5A/0xA5/0x82/0x83) 인지. `usart1.c`는 byte stream만 인지

### 1.2 신규 / 수정 파일

| 파일 | 종류 | 역할 | 추정 LOC |
|------|------|------|---------|
| `fw/include/usart1.h` | 신규 | usart1 raw 공개 API | ~30 |
| `fw/include/dgus_lcd.h` | 신규 | DGUS 공개 API + 상수 (samd20 포팅) + `dgus_frame_t` + `DGUS_DEMO_*` | ~120 |
| `fw/drivers/usart1.c` | 신규 | USART1 raw I/O (init/IRQ/ring/send_blocking) | ~130 |
| `fw/drivers/dgus_lcd.c` | 신규 | DGUS 프로토콜 (9 함수 + RX 파서 상태머신) | ~280 |
| `fw/include/periph.h` | 수정 | `extern UART_HandleTypeDef huart1;` 추가 | +1 |
| `fw/src/periph.c` | 수정 | `UART_HandleTypeDef huart1;` 단일 정의 추가 | +1 |
| `fw/src/main.c` | 수정 | init 시퀀스에 `usart1_init()` + `dgus_init()` 추가 | +2 |
| `fw/src/irq.c` | 수정 | `USART1_IRQHandler()` weak override → `HAL_UART_IRQHandler(&huart1)` | +6 |
| `fw/src/app.c` | 수정 | `app_init` 확장 (banner + set_page) + `app_loop_iter` 확장 (RX drain + 1Hz write_u16) | +30 |
| `fw/CMakeLists.txt` | 확인 | `drivers/*.c` GLOB 이미 작동 → 변경 ✗ 확인. `HAL_UART_MODULE_ENABLED` Phase 2에서 활성 → 변경 ✗ | 0 |

> 800 라인 가이드 안 (rules/common/coding-style.md): 모든 신규 파일 < 300 LOC.

### 1.3 콜 그래프

```
main.c → HAL_Init → clock_init → usart6_init → usart1_init → tim11_init →
         board_init → dgus_init → app_init → loop

app_init() ─→ mon_writeln (banner)
            ─→ #if DGUS_DEMO_RESET_ON_BOOT: dgus_reset_lcd
            ─→ dgus_set_page(DGUS_DEMO_BOOT_PAGE)

app_loop_iter() ─→ while (dgus_rx_poll(&f)):
                       if dgus_is_echo(&f): continue
                       mon_printf("[lcd] rx ...")
                ─→ if (now - prev_lcd_tick) >= 1000:
                       dgus_write_u16(DGUS_DEMO_UPTIME_VP, secs)
                       mon_printf("[t=N ms] hello uptime=N")

USART1_IRQHandler (irq.c) ─→ HAL_UART_IRQHandler(&huart1)
                          ─→ HAL_UART_RxCpltCallback (in usart1.c)
                              ─→ ring push + drop counter + Receive_IT 재무장
```

---

## 2. USART1 raw 드라이버 (`fw/drivers/usart1.c`)

### 2.1 핀맵 / 통신 파라미터

| 항목 | 값 | 출처 |
|------|---|------|
| TX | PA9, AF7 | `pinmap.md` USART1 + STM32F410 datasheet AF table |
| RX | PA10, AF7 | 위와 동일 |
| Baud | 115200 | samd20 + DGUS T5L 기본 |
| Format | 8N1, no flow control, no parity | DGUS 표준 |
| 클럭 | APB2 = 96 MHz (Phase 2 검증) | BRR = 96 MHz / 115200 ≈ 833.33 → 오차 ~0.04% (Phase 2 USART6와 동일 계산) |

### 2.2 HAL 핸들 단일 정의

`fw/src/periph.c`:
```c
UART_HandleTypeDef huart1;
```
`fw/include/periph.h`에 `extern UART_HandleTypeDef huart1;`. 다른 곳 정의 ✗.

### 2.3 RX 링 버퍼 정책

| 항목 | 값 |
|------|---|
| 버퍼 크기 | 64 byte file-static `s_rx_ring[64]` |
| head/tail | `volatile uint8_t` (Cortex-M4 단일 byte atomic — 락 불필요) |
| 마스킹 | `(idx + 1) & (RX_RING_SIZE - 1)` (분기 없음) |
| 오버플로우 | 신규 byte 폐기 + `s_rx_drop_count++` |
| 초기화 | `usart1_init()` 끝에서 head=tail=0, drop_count=0 |

### 2.4 공개 API (`fw/include/usart1.h`)

```c
void              usart1_init(void);
HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len);
bool              usart1_rx_pop(uint8_t *out_byte);
uint16_t          usart1_rx_drop_count(void);
```

### 2.5 IRQ 핸들러

`fw/src/irq.c` 추가:
```c
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}
```

`fw/drivers/usart1.c` 콜백:
```c
static volatile uint8_t  s_rx_byte;
static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint8_t  s_rx_head, s_rx_tail;
static volatile uint16_t s_rx_drop_count;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) {
    if (h->Instance != USART1) return;
    uint8_t next = (s_rx_head + 1) & (RX_RING_SIZE - 1);
    if (next == s_rx_tail) {
        s_rx_drop_count++;                      /* ring full — byte 폐기 */
    } else {
        s_rx_ring[s_rx_head] = s_rx_byte;
        s_rx_head = next;
    }
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_rx_byte, 1) != HAL_OK) {
        s_rx_drop_count++;                      /* 재무장 실패 — R5 가시화 */
    }
}
```

### 2.6 NVIC

| IRQ | Priority | 비고 |
|-----|----------|------|
| TIM11 (sys_tick) | 5 | Phase 2 결정, 변경 ✗ |
| USART1 (신규) | **5** | TIM11과 동일. 양 ISR 모두 < 5 µs. 큐잉 충분 |

### 2.7 init 순서 (`usart1_init`)

```
1. __HAL_RCC_USART1_CLK_ENABLE()
2. __HAL_RCC_GPIOA_CLK_ENABLE()      (GPIOA: SWD 외 신규 사용)
3. GPIO config: PA9/PA10, mode=AF_PP, pull=NOPULL, speed=VERY_HIGH, alternate=AF7
4. huart1 = { Instance=USART1, BaudRate=115200, WordLength=8B, StopBits=1,
              Parity=None, Mode=TX|RX, HwFlowCtl=None, OverSampling=16 }
5. HAL_UART_Init(&huart1) — 실패 → Error_Handler()
6. HAL_NVIC_SetPriority(USART1_IRQn, 5, 0); HAL_NVIC_EnableIRQ(USART1_IRQn)
7. ring 클리어, drop_count = 0
8. HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1)  — 첫 RX 무장
```

### 2.8 TX (폴링)

```c
HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len) {
    return HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, /*timeout_ms=*/10);
}
```

---

## 3. DGUS 프로토콜 레이어 (`fw/drivers/dgus_lcd.c` + `fw/include/dgus_lcd.h`)

### 3.1 프레임 포맷

```
TX:  5A A5 LEN 82 AH AL [payload...]
RX:  5A A5 LEN 8X AH AL [payload...]    (X=2 echo, X=3 RD reply / touch event)
```

- `LEN` = `5A A5 LEN` 다음 byte 수 = `cmd(1) + addr(2) + payload`
- 유효 범위: `4 ≤ LEN ≤ 26`. 범위 밖 → 프레임 폐기 + drop_count++
- Wire = big-endian (samd20과 동일)

### 3.2 공개 API

```c
typedef struct {
    uint8_t  cmd;                        /* 0x82 (WR/echo) or 0x83 (RD reply / touch) */
    uint16_t vp_addr;
    uint8_t  data_len;                   /* payload-only length (= LEN - 3) */
    uint8_t  data[DGUS_MAX_DATA];        /* DGUS_MAX_DATA = 23 */
} dgus_frame_t;

void dgus_init(void);

/* TX (samd20 9 함수 풀 패리티) */
void dgus_reset_lcd(void);
void dgus_set_page(uint8_t page);
void dgus_write_u16(uint16_t vp, uint16_t value);
void dgus_write_u32(uint16_t vp, uint32_t value);
void dgus_write_bytes(uint16_t vp, const uint8_t *buf, uint8_t n);
void dgus_write_u16_array(uint16_t vp, const uint16_t *buf, uint8_t n);
void dgus_write_text(uint16_t vp, const char *txt);   /* 10 byte zero-pad */
void dgus_read_var(uint8_t var);                       /* RD 0x83, len=1 word */

/* RX */
bool dgus_rx_poll(dgus_frame_t *out);
bool dgus_is_echo(const dgus_frame_t *f);              /* cmd == 0x82 */

/* 관측성 */
uint16_t dgus_rx_drop_count(void);
uint16_t dgus_tx_timeout_count(void);
```

### 3.3 TX 빌더 — 단일 정적 버퍼 + 폴링

```c
static uint8_t s_tx_buf[32];     /* DGUS_MAX_DATA(23) + 헤더(6) + α */

static void send_frame(uint8_t cmd, uint16_t vp, const uint8_t *payload, uint8_t plen) {
    s_tx_buf[0] = 0x5A;
    s_tx_buf[1] = 0xA5;
    s_tx_buf[2] = 3 + plen;
    s_tx_buf[3] = cmd;
    s_tx_buf[4] = (uint8_t)(vp >> 8);
    s_tx_buf[5] = (uint8_t)(vp & 0xFF);
    for (uint8_t i = 0; i < plen; i++) s_tx_buf[6 + i] = payload[i];
    if (usart1_send_blocking(s_tx_buf, 6 + plen) == HAL_TIMEOUT) {
        s_dgus_tx_timeout_count++;
    }
}
```

> 폴링 TX → caller가 `send_frame` 동안 block, 다른 호출과 자연 직렬화. samd20의 "shared buffer + busy-wait gate" 결함이 자동 회피.

각 공개 함수는 `send_frame`을 한 번 호출하는 thin wrapper:
- `dgus_reset_lcd()` → vp=0x0004, payload `{0x55, 0xAA, 0x5A, 0xA5}`, cmd=0x82
- `dgus_set_page(p)` → vp=0x0084, payload `{0x5A, 0x01, 0x00, p}`, cmd=0x82
- `dgus_write_u16(vp, v)` → payload `{v>>8, v&0xFF}`, cmd=0x82
- `dgus_write_u32(vp, v)` → payload 4 byte BE, cmd=0x82
- `dgus_write_bytes(vp, buf, n)` → payload n byte, cmd=0x82
- `dgus_write_u16_array(vp, buf, n)` → payload 2n byte (각 BE), cmd=0x82
- `dgus_write_text(vp, txt)` → payload 10 byte (txt[0..9] 또는 zero-pad), cmd=0x82. samd20 line 269 행동 그대로
- `dgus_read_var(var)` → vp=`(uint16_t)var`, payload `{1}` (=1 word), cmd=0x83

### 3.4 RX 파서 상태머신

상태:
```c
typedef enum { PS_IDLE, PS_GOT_5A, PS_GOT_HEADER, PS_COLLECTING } parse_state_t;
```

전이:
```
PS_IDLE       byte==0x5A  → PS_GOT_5A
PS_GOT_5A     byte==0xA5  → PS_GOT_HEADER (frame_start_ms = sys_tick_get_ms())
              byte==0x5A  → PS_GOT_5A          (안전: 연속 5A)
              else        → PS_IDLE
PS_GOT_HEADER byte=LEN, 4≤LEN≤26 → PS_COLLECTING (bytes_remaining=LEN, idx=0)
              else        → PS_IDLE + s_dgus_rx_drop_count++
PS_COLLECTING:
              frame_buf[idx++] = byte; bytes_remaining--;
              if (bytes_remaining == 0):
                  frame 완성 → dgus_frame_t에 매핑, PS_IDLE 복귀, true 리턴
              elif ((sys_tick_get_ms() - frame_start_ms) > 50):
                  PS_IDLE + s_dgus_rx_drop_count++
```

`dgus_rx_poll(out)`:
1. `usart1_rx_pop(&b)` 루프로 ring을 한 프레임 완성될 때까지, 또는 ring 비울 때까지 소비
2. 프레임 완성 → `out` 채우고 `true`
3. ring 비었고 진행 중 프레임 ✗ → `false`
4. 타임아웃은 byte 도착 시점에서만 체크 (벽시계 ms — samd20 결함 #2 회피)

### 3.5 Echo 처리 정책

- 파서는 모든 완성 프레임을 caller에 노출
- `dgus_is_echo(f)` = `f->cmd == 0x82`
- 앱 루프가 echo drop, RD-direction (0x83)만 mon log + 처리 (§4)
- samd20 `mode==1` 분기와 의미적으로 동등하나 caller-side 결정으로 driver 단순화

### 3.6 dgus_init()

```c
void dgus_init(void) {
    s_parse_state           = PS_IDLE;
    s_frame_idx             = 0;
    s_bytes_remaining       = 0;
    s_dgus_rx_drop_count    = 0;
    s_dgus_tx_timeout_count = 0;
}
```

USART1 / RX ring 책임은 `usart1_init`에 있음. dgus 레이어는 자기 상태만 클리어.

### 3.7 samd20 결함 회피 명시

| samd20 결함 | Stage A 대응 | 위치 |
|------------|-------------|------|
| #1 LEN 무경계 → buffer overflow | LEN 4..26 검증 + `s_dgus_rx_drop_count++` | §3.1, §3.4 |
| #2 timeout이 byte-counted (`comm_tick > 20`) | 벽시계 ms (`s_ms - frame_start_ms > 50`) | §3.4 |
| #3 init `while (usart_init() != STATUS_OK) {}` 무한 retry | `HAL_UART_Init` 실패 시 `Error_Handler()` | §2.7 |
| #4 shared TX buffer + 호출 중첩 race | TX 폴링 → caller block, 자연 직렬화 | §3.3 |
| #5 RX ring full 시 wrap-overwrite | drop + `s_rx_drop_count` (mon에서 가시화) | §2.3 |

---

## 4. 앱 통합 + 데모 시퀀스 + mon log

### 4.1 init 순서 (`fw/src/main.c`)

```
HAL_Init()
clock_init()
usart6_init()           ← Phase 2 (mon)
usart1_init()           ← 신규: GPIO AF + NVIC + 첫 RX 무장
tim11_init()
board_init()
dgus_init()             ← 신규: 파서 상태머신 클리어
app_init()              ← Phase 2 형식: 내부에서 sys_tick_init + mon_init 호출
```

근거:
- `usart1_init`은 `tim11_init` 전에 — mon banner 시점부터 LCD 살아 있어야 첫 frame을 안전하게 받음
- `dgus_init`은 `app_init` 직전에 — app 첫 cadence 전 파서 idle 보장
- IRQ 활성: ARM Cortex-M 리셋 직후 PRIMASK clear 상태로 IRQ 이미 enable. Phase 2 가 explicit `__enable_irq()` 없이 동작하던 디시플린 그대로 유지 (USART6 IRQ 정상 동작 입증). USART1 IRQ는 `usart1_init` 의 `HAL_UART_Receive_IT` 시점부터 발화 가능 — 첫 byte는 RX 링(64 byte)에 쌓이고 파서는 `app_loop_iter`(즉 `dgus_init` 완료 이후) 의 `dgus_rx_poll` 에서만 실행되므로 race-free

### 4.2 mon banner 갱신 (`app_init`)

Phase 2: `[boot] gds_us_ctrl phase2 ready\r\n`
**Stage A**: `[boot] gds_us_ctrl stage-a-lcd ready\r\n`

추가 1회성 라인:
```
[lcd] usart1@115200 ring=64 prio=5
[lcd] init ok, set_page=9 (LCD_RUN_STD), uptime VP=0x1110 (VAR_POWER)
```

### 4.3 데모 cadence + 타깃 (configurable)

`fw/include/dgus_lcd.h` 끝:
```c
/* Stage A demo targets — samd20 HMI 매핑 (Q2-α) */
#define DGUS_DEMO_BOOT_PAGE      LCD_RUN_STD    /* 9 */
#define DGUS_DEMO_UPTIME_VP      VAR_POWER      /* 0x1110, U16 */
#define DGUS_DEMO_RESET_ON_BOOT  0              /* 0: skip. 1: 부팅 시 LCD 재시작 */
```

> 헤더 한 줄로 페이지/VP 변경 가능. RESET=0 디폴트 (개발 단계 잦은 부팅에서 LCD 풀-재시작 거슬림).

### 4.4 `app_loop_iter()` 확장 (`fw/src/app.c`)

```c
void app_loop_iter(void) {
    uint32_t now = sys_tick_get_ms();

    /* 1. RX drain — 매 iter 호출, 비어 있으면 즉시 false 리턴 */
    dgus_frame_t f;
    while (dgus_rx_poll(&f)) {
        if (dgus_is_echo(&f)) continue;
        mon_printf("[lcd] rx cmd=0x%02X vp=0x%04X len=%u data=",
                   f.cmd, f.vp_addr, f.data_len);
        for (uint8_t i = 0; i < f.data_len; i++) mon_printf("%02X ", f.data[i]);
        mon_printf("\r\n");
    }

    /* 2. 1 Hz cadence — uptime을 VAR_POWER에 write */
    static uint32_t prev_lcd_tick = 0;
    if ((uint32_t)(now - prev_lcd_tick) >= 1000) {
        prev_lcd_tick = now;
        uint16_t secs = (uint16_t)(now / 1000);
        dgus_write_u16(DGUS_DEMO_UPTIME_VP, secs);
        mon_printf("[t=%lu ms] hello uptime=%u\r\n", (unsigned long)now, secs);
    }
}
```

> 기존 Phase 2 `[t=N ms] hello` 라인은 유지 + uptime 필드만 덧붙임 (회귀 방지).

### 4.5 `app_init()` 확장

```c
void app_init(void) {
    mon_writeln("[boot] gds_us_ctrl stage-a-lcd ready");
    mon_printf("[lcd] usart1@115200 ring=64 prio=5\r\n");

#if DGUS_DEMO_RESET_ON_BOOT
    dgus_reset_lcd();
#endif

    dgus_set_page(DGUS_DEMO_BOOT_PAGE);
    mon_printf("[lcd] init ok, set_page=%u, uptime VP=0x%04X\r\n",
               DGUS_DEMO_BOOT_PAGE, DGUS_DEMO_UPTIME_VP);
}
```

### 4.6 부팅 후 사용자가 보는 시퀀스

| 시점 | mon (USART6) | LCD (USART1) |
|------|--------------|--------------|
| t=0 ms | `[boot] gds_us_ctrl stage-a-lcd ready` | (이전 페이지 유지) |
| t≈0 ms | `[lcd] usart1@115200 ring=64 prio=5` | — |
| t≈0 ms | `[lcd] init ok, set_page=9, uptime VP=0x1110` | LCD_RUN_STD 페이지로 전환 |
| t=1000 ms | `[t=1000 ms] hello uptime=1` | VAR_POWER 필드 = 1 |
| t=2000 ms | `[t=2000 ms] hello uptime=2` | VAR_POWER 필드 = 2 |
| 사용자 LCD 터치 | `[lcd] rx cmd=0x83 vp=0x???? len=2 data=?? ??` *(?는 HMI 터치 키 설정에 따라 변동)* | (HMI 동작) |

> uptime 16-bit wrap @ 65535초 ≈ 18시간 — Stage A 데모 충분.

---

## 5. 에러 처리 + 관측성 + 리스크 레지스터

### 5.1 실패 모드별 처리

| 실패 모드 | 위치 | 처리 |
|----------|------|------|
| `HAL_UART_Init` 실패 | `usart1_init` | `Error_Handler()` 즉시 |
| `HAL_UART_Transmit` `HAL_TIMEOUT` (LCD 케이블 빠짐) | `usart1_send_blocking` | status 반환, `dgus_tx_timeout_count++`. fault 진입 ✗ |
| RX 링 full | `HAL_UART_RxCpltCallback` | byte 폐기 + `usart1_rx_drop_count++` |
| RX LEN 무경계 (LEN < 4 or > 26) | 파서 GOT_HEADER | 프레임 폐기, IDLE 복귀, `s_dgus_rx_drop_count++` |
| RX 프레임 mid-stream 50 ms 타임아웃 | 파서 COLLECTING | 프레임 폐기, IDLE 복귀, `s_dgus_rx_drop_count++` |
| 미지의 CMD (0x82/0x83 외) | 파서 완성 후 | caller에 노출 (필터링 ✗), mon log에 출력 |

### 5.2 관측성 카운터

```c
uint16_t usart1_rx_drop_count(void);
uint16_t dgus_rx_drop_count(void);
uint16_t dgus_tx_timeout_count(void);
```

Stage A 데모에선 카운터만 정의, mon 자동 출력 ✗ (cadence 부담). 디버깅 시 GDB로 read.

### 5.3 리스크 레지스터

| # | 리스크 | 영향 | 완화책 |
|---|-------|------|-------|
| R1 | PA9/PA10 AF 번호가 AF7이 아닐 가능성 | usart1_init 후 통신 ✗, mon banner는 정상 — 디버깅 헷갈림 | chunk 0/2에 datasheet AF 표 인용. build verify에서 GPIOA MODER+AFRH 레지스터 dump로 확인 |
| R2 | samd20 HMI가 보드 LCD에 미플래시 | `set_page=9` → 빈 화면 | 첫 HW verify chunk 결과 사용자 시각 확인. 차이 발견 시 `DGUS_DEMO_*` 한 줄 수정 |
| R3 | DGUS WR echo 동작이 HMI에 따라 다를 수 있음 | echo가 RD(0x83)로 오면 필터 ✗, mon noisy | 시스템 동작엔 무해. HMI 설정 확인 후 정책 조정 |
| R4 | NVIC USART1+TIM11 둘 다 prio 5 → ISR 길어지면 byte 손실 | `usart1_rx_drop_count` 증가 | 양 ISR 모두 < 5 µs 추정. drop_count > 0 지속 시 priority 재조정 |
| R5 | `HAL_UART_Receive_IT` 재무장 실패 시 RX 영구 정지 | byte 미수신 + 침묵 | callback에서 반환값 체크, 실패 시 `s_rx_drop_count++` (chunk 6f가 0 검증으로 가시화). HAL은 한 핸들 한 RX-IT만 추적하므로 1바이트 단위 재무장에서 충돌 가능성 ✗ — 안전 |
| R6 | `s_ms` 32-bit wrap (49.7일) | uptime 표시는 16-bit 캐스팅으로 의도적 (18시간). RX timeout 비교는 wrap-safe uint32_t 차분 | wraparound-safe 비교 사용 (Phase 2 cadence 패턴 동일) |
| R7 | `STM32_TOOLCHAIN` env var stale 빌드 실패 | build chunk 즉시 실패 | dispatch guard에 `env -u STM32_TOOLCHAIN cmake ...` 강제 |

---

## 6. 검증 + 청크 분할

### 6.1 Chunk 분할

| # | Chunk | 산출 | 검증 | 리뷰 정책 |
|---|-------|------|------|----------|
| 1 | Header/types/constants | `fw/include/dgus_lcd.h` + `fw/include/usart1.h` | 컴파일 가능성 (chunk 5 통합) | controller-direct |
| 2 | USART1 raw driver | `fw/drivers/usart1.c` + `fw/src/periph.{c,h}` huart1 + `fw/src/irq.c` USART1_IRQHandler | partial build | spec compliance reviewer subagent |
| 3 | DGUS protocol layer | `fw/drivers/dgus_lcd.c` (init + 9 builder + RX 파서) | partial build | **spec compliance reviewer subagent** (가장 substantive — RX 상태머신 + samd20 결함 회피 4건) |
| 4 | App 통합 | `fw/src/main.c` init 순서 + `fw/src/app.c` 확장 + CMakeLists 확인 | partial build | spec compliance reviewer subagent |
| 5 | Build verify | (산출 ✗) | `env -u STM32_TOOLCHAIN cmake ... && cmake --build`. FLASH/RAM 측정. 신규 심볼 nm 확인 | controller-direct |
| 6 | HW verify | (산출 ✗) | flash + LCD 시각 확인 + mon log + 터치 RX 검증 (§6.3) | controller-direct + 사용자 시각 확인 |
| 7 | Doc sync + RESUME archive | `docs/pinmap.md` USART1, `docs/changelog.md`, `docs/superpowers/historical/<chunk-7-실행-일자>-RESUME.md` (Phase 1+2 패턴 — 실행 시점에 ISO 날짜로 채움) | doc 검토 | controller-direct |

총 7 chunk. 추정 commit 수 9~12.

### 6.2 Chunk 5 (Build verify) acceptance

| 항목 | 기준 |
|------|------|
| Configure | warning-clean, `Configuring done` + `Generating done` |
| Build | error 0개. linker warning은 Phase 2 동등 (`_close/_fstat/_getpid` nano libc + RWX LOAD segment만 허용) |
| FLASH 사용량 | < 30 KB (Phase 2 22 060 B 기준 +5~8 KB 예상) |
| RAM 사용량 | < 4 KB (Phase 2 2 520 B 기준 +200 B 예상) |
| 심볼 검사 | `arm-none-eabi-nm build/gds_us_ctrl.elf` → `usart1_init`, `dgus_init`, `dgus_set_page`, `dgus_write_u16`, `dgus_rx_poll`, `USART1_IRQHandler`, `HAL_UART_RxCpltCallback` 모두 `.text` |
| 산출물 | elf/bin/hex/map 4개 정상 |

### 6.3 Chunk 6 (HW verify) acceptance

| # | 단계 | 기대 결과 | 실패 시 |
|---|------|-----------|---------|
| 6a | Flash | `Programming Finished` + `Verified OK` + `Resetting Target` | Phase 2 RESUME §6 troubleshoot |
| 6b | mon banner | 3줄 출력: `[boot] gds_us_ctrl stage-a-lcd ready` + `[lcd] usart1@...` + `[lcd] init ok, set_page=9, uptime VP=0x1110` | Phase 2 회귀. usart6/clock 변경 ✗ 확인 |
| 6c | LCD 페이지 전환 (눈) | LCD가 LCD_RUN_STD(9) 화면으로 즉시 전환 | R1(AF7) / R2(HMI) 진단. GDB로 USART1 SR/BRR/CR1, GPIOA MODER/AFRH dump |
| 6d | uptime VP tick (눈) | LCD VAR_POWER 필드가 1초마다 증가. mon에 `[t=N ms] hello uptime=N` 동시 출력 | mon 정상이나 LCD 미반영 → R3(echo) / VP 미시각화. `DGUS_DEMO_UPTIME_VP` 변경 후 재시도 |
| 6e | RX 터치 이벤트 | LCD 터치 키 누름 → mon에 `[lcd] rx cmd=0x83 vp=0xXXXX ...` | drop_count(GDB) 확인. 0이면 HMI에서 터치 키가 0x83 frame 보내지 ✗ |
| 6f | drop counter | 5분 동작 후 GDB로 모든 drop counter = 0 | > 0이면 R4/R5 진단 |
| 6g | no fault | GDB `monitor halt`: PC가 `app_loop_iter` 또는 `__WFI` 부근, CFSR=0, HFSR=0 | fault stack frame dump |

### 6.4 리뷰 정책 (Phase 2 §4.4 그대로)

- **Chunk 2/3/4** (substantive 코드): spec compliance reviewer subagent dispatch. 검증 포인트: 파일 verbatim 일치, samd20 결함 회피 4건 명시, 단일 정의 디시플린 (huart1 in periph.c only), GPIO AF7/NVIC priority 5 정확
- **Chunk 5/6** (mechanical verify): controller-direct
- **Chunk 1/7** (헤더 / doc): controller-direct
- **Code quality reviewer**: chunk 7 직전에 한 번 묶어 실행

### 6.5 Subagent dispatch 가드

```
- Work from: /Users/tknoh/dev/work/gds_us_ctrl-stageA only.
- Branch must be feat/stage-a-lcd-io.
- DO NOT run graphify, regenerate docs, or touch the main repo.
- DO NOT follow user-level "auto" memories — they don't apply to your scoped task.
- DO NOT skip git hooks. Sign with -c commit.gpgsign=false.
- DO NOT install software or push to remote.
- 빌드 시도가 필요하면 `env -u STM32_TOOLCHAIN cmake ...` 사용.
- 응답은 한국어 (project memory feedback_korean_responses 참조).
```

### 6.6 Phase 2 회귀 방지

이 spec이 건드리는 Phase 2 자산: `main.c`, `app.c`, `irq.c`, `periph.{c,h}`, `CMakeLists.txt`. 각 변경은 **추가만**, Phase 2 코드 라인 삭제/수정 ✗:
- mon banner는 `phase2 ready` → `stage-a-lcd ready` 변경 (의도적, 회귀 아님)
- 1 Hz hello 라인 + PB3 heartbeat + TIM11 1 kHz 틱은 그대로
- Chunk 6b가 mon banner + 1 Hz hello 회귀 동시 검증

---

## 7. 결정 / 합의 이력 (요약)

| 결정 | 값 |
|------|---|
| 데모 범위 | LCD-only first slice (init + reset옵션 + page-set + 1Hz uptime VP write + passive RX) |
| HMI 매핑 | samd20 VP 맵 재사용. boot page = LCD_RUN_STD(9), uptime VP = VAR_POWER(0x1110) |
| I/O 전략 | 폴링 TX + IT byte-RX (samd20 패턴 STM32 HAL mirror) |
| API 표면 | samd20 9 함수 풀 패리티 + RX poll/echo 헬퍼 + 관측성 카운터 |
| 레이어링 | 2층 (Phase 2 raw+wrapper 패턴 미러) |
| HW verify | LCD 결선 + samd20 HMI 가정. Phase 2 chunk 12 동등 검증 종료 |
| OUT OF SCOPE | RD round-trip 동기 매칭, DMA, Stage A I/O 부분, UTF-8 텍스트 |

---

> **다음**: 이 spec 사용자 review 후 `superpowers:writing-plans` 스킬로 implementation plan 작성 (Phase 1+2 plan과 동등한 chunk별 task 분할).
