# RS-485 첫-write 간헐 무효 — 코드 조사 보고 (read-only)

> **요약**: 벤치에서 관측된 "RS-485 어댑터 접속 후 mbpoll 첫 FC06 write가 간헐적으로 무효"
> 현상을 4개 가설(+5번째 후보)에 대해 정적 코드 분석으로 판정했다. **결론: H1(stale
> DMA 인덱스)·H2(첫 프레임 gap 판정 실패)는 기각**(`usart6_mb_open`이 `HAL_DMA_Start`로
> NDTR을 리셋하고 인덱스 3종을 명시적으로 0/현재시각으로 세팅 → stale 상태 없음, `head !=
> s_prev_head` 가드가 조기 절단을 막음). **H4(응답 OK인데 미적용)는 "간헐적 첫-write"의
> 원인으로는 기각**(에코는 apply와 무관하게 항상 송신되며, 미적용은 동일값·클램프처럼
> *결정적*으로만 발생 → 첫-write에 국한되지 않음). **최유력 원인 = H3 파생 "선두 글리치
> 바이트 병합"**: USB-RS485 어댑터 접속/포트 오픈 시 라인 글리치 바이트가 free-running
> DMA 버퍼에 실려, 첫 실프레임과 **`s_gap_ms` 미만 간격으로 인접하면 하나의 프레임으로
> 병합** → address/CRC 불일치 → 침묵(무응답) → 마스터 재시도 시 성공. 글리치-프레임
> 간격이 gap 이상이면 글리치가 먼저 flush 되어 첫-write 성공 → **간헐성의 직접적 설명**.
> 이는 samd20 idle-gap 프레이밍에 내재된 RTU-on-glitchy-line 거동으로, 펌웨어 결함이라기보다
> 마스터 재시도로 해소되는 성질. 저위험 하드닝 후보(오픈 창 ORE/DR flush)는 §7에 제시(미적용).
> **관찰 변종 미기록**(무응답 vs 응답-후-무효)이 핵심 불확실성 → 재현 절차(§6)에서
> FC06→FC03 read-back으로 두 변종을 분리하도록 설계.

---

## 1. 분석 대상 파일 (읽은 실제 코드)

- `fw/drivers/usart6_mb.c` (전체 180줄) — USART6 Modbus-RTU 트랜스포트, free-running circular DMA RX, idle-gap 프레이밍.
- `fw/src/app_modbus.c` (361줄) — 점유 판정(`apply_config`), `mirror_live`/`app_modbus_apply_writes`, `app_modbus_tick` RTU 분기.
- `fw/src/app_modbus_core.c` (212줄) — 순수 Modbus 코어(`mb_core_decode`, `mb_write_reg`, CRC).
- `fw/include/app_modbus_core.h` — 레지스터 맵.
- `fw/src/app.c:138`, `fw/src/main.c:56` — 슈퍼루프 폴 케이던스(비-게이팅 free-run).

각 주장에 `파일:라인` 인용. **FACT** = 읽은 코드 사실, **INFERENCE** = 코드로부터 연역한 런타임 거동.

---

## 2. 관찰 정의 — 두 시나리오

벤치 노트(2026-07-04 g)는 "첫 FC06 write가 간헐적으로 무효, 그래서 매 write를 검증하도록 학습"만
기록. **실패 양태가 아래 둘 중 무엇인지 기록되지 않음** → 두 변종 모두에 가설을 매핑한다.

| 변종 | 마스터(mbpoll)가 보는 것 | 슬레이브 상태 |
|------|--------------------------|----------------|
| **V-A "무응답"** | FC06 요청 후 타임아웃(에코 없음). | 프레임이 address/CRC/floor에서 침묵 드롭 → `mb_core_decode` return 0 → 송신 없음. |
| **V-B "응답 OK인데 미적용"** | FC06 에코(정상 8B) 수신, 그러나 이후 FC03 read-back이 옛값. | `mb_write_reg`가 `holding[addr]` 기록+에코했으나 `app_modbus_apply_writes`가 `cfg` 반영을 건너뜀. |

두 변종은 재현 절차(§6)에서 **FC06→즉시 FC03 read-back**으로 분리한다.

---

## 3. RX 경로 사실 정리 (파일:라인)

**프레이밍 메커니즘 (FACT)** — `usart6_mb.c:1-12` 주석 + 구현:
- RX는 usart1.c의 무-IRQ/무-NVIC circular DMA. DMA가 매 RXNE마다 DR를 소비 → 수신부 wedge 불가 *(단, DMA가 이미 구동 중일 때만; §4-H5 참조)*.
- 프레임 경계 = "DMA write 위치가 `s_gap_ms` 이상 정지 + 미소비 바이트 존재" (samd20 `max_break_cnt×250us`를 1ms sys_tick 격자로 올림). `mb_gap_ms[6] = {14,7,4,2,2,2}` (`usart6_mb.c:25`).

**오픈 시 인덱스 초기화 (FACT)** — `usart6_mb_open`:
- 이중 오픈 가드: `s_open`이면 no-op (`usart6_mb.c:49-51`).
- `HAL_DMA_Start(...)`가 NDTR을 `MB_RX_DMA_SIZE`(256)로 리셋 (`usart6_mb.c:105-108`) → `rx_head() = 256 - NDTR = 0` (`usart6_mb.c:35-42`).
- `s_rx_tail = 0; s_prev_head = 0; s_last_rx_ms = sys_tick_get_ms();` (`usart6_mb.c:101-103`).
- **오픈 순서(중요)**: `HAL_UART_Init`(`:79`, USART UE/RE=1)이 먼저, 그 뒤 `HAL_DMA_Start`(`:105`) + `SET_BIT(CR3, DMAR)`(`:109`). 즉 `:79`~`:109` 사이 **USART는 수신 가능하나 DMA는 미구동**인 짧은 창이 존재 (FACT).

**프레임 완성 로직 (FACT)** — `usart6_mb_rx_frame:126-160`:
- `head != s_prev_head` → 바이트 도착 중, gap 재시작(`s_prev_head=head; s_last_rx_ms=now; return 0`) (`:134-138`).
- `head == s_rx_tail` → 미처리 없음, return 0 (`:139-141`).
- `(now - s_last_rx_ms) < s_gap_ms` → break gap 미경과, return 0 (`:142-144`).
- gap 경과 → `s_rx_tail..head`를 한 프레임으로 drain (`:148-159`). oversize(>maxlen)면 drain 후 return 0 (`:156-158`).

**송신/에코 가드 (FACT)** — `usart6_mb_send:162-180`:
- blocking `HAL_UART_Transmit` (`:172`), 타임아웃 = `len*11000/baud + 50ms`.
- 송신 후 `s_rx_tail = rx_head(); s_prev_head = s_rx_tail; s_last_rx_ms = now` (`:177-179`) — auto-DE 트랜시버가 자기 TX를 RX로 되먹여도 그 구간 수신을 폐기(FC06 에코 == 요청 바이트 재파싱 방지).

**디코드/어드레스/CRC 필터 (FACT)** — `mb_core_decode:161-212`:
- RTU: `len < 8` → return 0 (`:169`); `frame[0] != device_addr` → return 0 (`:172`); CRC 불일치 → return 0 (`:176-178`). 셋 다 **침묵 드롭**(예외 응답 없음, samd20 충실).
- FC06 → `mb_write_reg`: `addr >= 50` 아니면 `holding[addr] = val` 기록 후 에코 8B (`app_modbus_core.c:77-91`). **에코는 apply와 독립**.

**폴 케이던스 (FACT/INFERENCE)** — `main.c:56` `while(1) { app_loop_iter(); }` 무-지연, `app.c:138` `app_modbus_tick()` 매 iter. → `usart6_mb_rx_frame`는 서브-ms 주기로 폴(INFERENCE: 대부분 저비용 연산; 단 `app_config_save_all` FRAM 커밋 시 ~2ms, I2C 타임아웃 최악 50ms로 늘어남 — `app_modbus.c:237-242`).

**tick 순서 (FACT)** — `app_modbus_tick:329-340`: `rx_frame` → `mb_core_decode` → (n!=0) `usart6_mb_send` → (fc==0x06) `app_modbus_apply_writes` → `mirror_live`. 매 tick 말미 `mirror_live`가 `cfg`→`holding[]` 재미러 (`:340`).

---

## 4. 가설별 판정

### H1. open 직후 stale DMA 인덱스 → 선두 노이즈 병합 → CRC 실패 → 무응답
**판정: 기각 (REJECTED).**
- (FACT) `HAL_DMA_Start`(`usart6_mb.c:105`)가 NDTR을 256으로 리셋하므로 오픈 직후 `rx_head()=0`이고, `s_rx_tail`/`s_prev_head`도 명시적으로 0으로 세팅됨(`:101-102`). "현재 DMA 위치와 다른 stale 0" 이라는 가설 전제가 성립하지 않는다 — 세 값이 서로 정합.
- (FACT) close→open 재오픈 시에도 `HAL_DMA_Abort`+`HAL_DMA_DeInit`(`:119-120`) 후 재-Init/Start로 NDTR 재리셋 → cross-open stale 없음.
- (FACT) `s_open` 이중오픈 가드(`:49-51`)로 live DMA를 중간에 죽이는 경로도 차단.
- (INFERENCE) "open **전** 라인 노이즈"는 DMAR 미인에이블 상태라 버퍼에 실리지 않음. 오픈 **후** 노이즈는 H3의 소관이지 stale-index 문제가 아니다.
- **부기**: 단 `:79`~`:109` 오픈 창(USART RX on, DMA off) 동안 도착한 바이트는 DR에 latch되어 DMAR 인에이블 시 `buf[0]`로 들어갈 수 있음 → 이는 H1의 "stale index"가 아니라 **H5(오픈-창 latch/ORE)** 로 분리해 다룬다. 부팅(어댑터 미연결) 시나리오에서는 이 창이 비어 있어 무관.

### H2. 첫 프레임 idle-gap 판정 실패/조기 절단 (`s_last_rx_ms` 초기값 vs `mb_gap_ms`)
**판정: 기각 (REJECTED).**
- (FACT) `usart6_mb_rx_frame`는 `head != s_prev_head`를 **가장 먼저** 검사(`:134`)하여, 새 바이트를 처음 관측한 폴에서 항상 `s_last_rx_ms=now`로 gap을 재시작하고 return 0. 오픈 시 `s_last_rx_ms`가 아무리 과거(부팅~어댑터 연결까지 수 초)여도, 그 값으로 첫 프레임을 조기 절단할 수 없다 — gap 검사(`:142`)는 `head==s_prev_head`가 성립한 *다음* 폴에서만 도달.
- (FACT/INFERENCE) 오픈 시 `head=0`이므로 오픈 시점에 버퍼에 "이미 앉아 있는" 프레임이 없다 → `s_last_rx_ms` 초기값이 첫 프레임 판정에 개입할 여지 자체가 없음.
- (INFERENCE) 서브-ms 폴 케이던스(§3) ≪ `s_gap_ms`(2~14ms)라 gap 경과는 항상 다수 폴에 걸쳐 안전 관측. mbpoll 기본 타임아웃 1s ≫ gap이라 지연도 무해.
- **부기(비-원인)**: FRAM 저장(`:237-242`)이 폴을 50ms까지 지연시키면 그 프레임 소비가 늦어질 수 있으나, 이는 첫-write 특이 현상이 아니고 무응답도 아님(마스터 타임아웃 1s 내).

### H3. 어댑터 접속/DE 턴어라운드 글리치 바이트가 프레임 선두 오염
**판정: 성립 — 단 "병합" 하위형에서만 (CONDITIONAL / 최유력).**
- (INFERENCE, 강함) USB-RS485 어댑터는 접속·enumeration·포트 open 시 DTR/RTS 토글 또는 트랜시버 인에이블 과도로 라인에 글리치(1~수 바이트)를 흘리는 것이 흔함(하드웨어 성질). 이 바이트들은 free-running DMA에 실려 `rx_head`를 전진시킴.
- **두 갈래로 분기 (INFERENCE, 코드 기반):**
  1. **글리치와 첫 실프레임 사이 간격 ≥ `s_gap_ms`** → 글리치가 먼저 독립 "프레임"으로 drain됨. `len < 8`(`app_modbus_core.c:169`) 또는 `frame[0] != device_addr`(`:172`)로 침묵 드롭, `s_rx_tail` 전진. 이어지는 실프레임은 **깨끗하게** 소비 → **첫-write 성공**.
  2. **간격 < `s_gap_ms`** → `usart6_mb_rx_frame`가 글리치+실프레임을 **하나의 프레임으로 병합**(`:148` drain은 `s_rx_tail..head` 전체). 병합 프레임은 `frame[0]`=글리치(≠addr → 드롭) 또는 CRC가 프레임 실경계와 불일치(→드롭) → **침묵(V-A 무응답)** → 마스터 재시도 시 버퍼가 이미 비어 성공.
- **간헐성의 직접적 설명**: 실패 여부가 "글리치-프레임 시간 간격"이라는 물리 타이밍에 좌우 → 매번 다름 → 벤치의 "간헐적" 관찰과 정확히 일치.
- (FACT) idle-gap 프레이밍은 프레임 **내부 재동기**를 못 함(바이트 단위 경계 없음, `:1-12` 설계 주석) → 병합을 코드 레벨에서 분리 불가. 이는 samd20 충실 이식이며 RTU 마스터의 재시도로 해소되는 성질(펌웨어 결함 아님으로 분류 가능).
- 매핑: **V-A(무응답)**.

### H4. decode→apply 체인에서 응답은 정상인데 적용 누락
**판정: "간헐적 첫-write"의 원인으로는 기각. 단 V-B(응답 OK·값 상이)의 *결정적* 설명으로는 성립.**
- (FACT) 에코는 `mb_write_reg`가 `holding[addr]` 기록 직후 무조건 생성(`app_modbus_core.c:80-91`)하고 `app_modbus_tick`이 `usart6_mb_send`로 송신(`app_modbus.c:334`). **apply 성패와 무관하게 에코는 나간다** → "응답 OK"는 apply를 보증하지 않음.
- (FACT) `apply_writes`는 else-if 1-change-per-message 체인(`app_modbus.c:114-235`). `mirror_live`가 매 tick `cfg`→`holding` 재미러(`:340`)하므로, 단일 FC06는 쓴 그 레지스터 하나만 `cfg`와 달라짐 → 그 필드가 정확히 매칭·적용됨. 오픈 직후 baseline `mirror_live`도 보장됨(`:295`) → 첫-write에 특이한 skip 조건 없음.
- **미적용이 발생하는 결정적(비-간헐) 경로 (FACT):**
  - (a) **동일값 write**: `holding[X] == cfg->field`면 `holding[X] != cfg->field` 거짓 → skip. 정의상 no-op(값 이미 동일 → 관측차 없음). FRAM 부팅값이 이미 목표값이면(메모리: OUT_POWER=55 잔재) 발생.
  - (b) **클램프**: 예) OUT_POWER<50→50, >100→100 (`:165-170`). **에코는 원시 프레임 바이트 그대로**(`app_modbus_core.c:84-87`) 반환하므로 마스터는 자기가 쓴 값(예 30)을 에코로 보지만 `cfg`는 50, 다음 read-back도 50. → "응답 OK인데 쓴 값과 다름". 그러나 클램프는 **범위밖 write마다 항상** 발생 → 첫-write 국한 아님.
  - (c) **command 레지스터 선점**: RESET/SEEK/START/STOP가 config 필드보다 먼저 검사(`:114-138`)되어 비영일 때 config apply를 그 tick에 선점. 그러나 이들은 오픈 시 `mb_core_init` memset 0(`app_modbus_core.c:13`, `app_modbus.c:290`)이라 부팅 첫-write엔 0 → 선점 없음.
- 결론: H4는 V-B를 *설명은 하나* "첫 write에서만 간헐"이라는 관찰 특성과 부합하지 않음(모두 결정적·반복적). → 첫-write 간헐 원인으로 기각.

### H5 (추가 발굴). 오픈-창 latch 바이트 / ORE 미해제로 인한 첫 프레임 오염·wedge
**판정: 추가정보 필요 (부팅 시나리오엔 무관, live-line 오픈 시나리오엔 잠재 위험).**
- (FACT) 오픈 순서 `HAL_UART_Init:79` → `HAL_DMA_Start:105` → `DMAR:109` 사이 창 동안 USART는 RX 가능하나 DMA는 미구동. 이 창에 1바이트 도착 시 RXNE latch → DMAR 인에이블 시 `buf[0]`로 유입(선두 오염, H3 병합과 동형). 2바이트 이상이면 **ORE 세트**.
- (FACT) 이 트랜스포트에는 **에러 ISR도, ORE 클리어 코드도 없음**(파일 전체에 `__HAL_UART_CLEAR_OREFLAG`/SR·DR 클리어 부재). 설계 주석(`:6-7`)의 "wedge 불가"는 *DMA가 이미 구동 중일 때만* 성립. 오픈 창에서 ORE가 서면 DMAR 인에이블 후에도 RXNE→DMA 이벤트가 막혀 **수신 wedge** 가능(INFERENCE, F4 USART/DMA 동작 기준).
- **부팅+어댑터 미연결 재현 절차(§6)에서는 이 창이 비어 있어 무관.** 단 **어댑터 연결 상태에서 LCD로 comm 설정을 바꿔 재오픈**하거나, 연결 순간과 오픈이 겹치면 발동 가능 → live-line 재오픈 별도 관찰 필요(§6 확장 케이스).

---

## 5. 최유력 원인 순위

| 순위 | 원인 | 관찰변종 | 근거 요지 | 결함성 |
|------|------|----------|-----------|--------|
| **#1** | **H3-병합**: 접속 글리치 바이트 + 첫 프레임이 `<s_gap_ms` 인접 → 병합 → addr/CRC 드롭 → 무응답, 재시도 성공 | V-A | 간헐성=글리치-프레임 타이밍(§4-H3), idle-gap 재동기 불가(`usart6_mb.c:1-12`,`:148`) | 와이어/어댑터 성질 + RTU 재시도로 해소. FW 결함 낮음 |
| #2 | **H3-분리형**(참고, 실패 아님): 글리치가 gap≥로 분리되면 flush 후 첫-write 성공 | — | #1의 반대 분기; 왜 *가끔* 성공하는지를 설명 | — |
| #3 | **H5**: 오픈-창 latch/ORE (live-line 재오픈 한정) | V-A(wedge)/오염 | 오픈 순서 창(`:79-109`) + ORE 미해제(에러 핸들 부재) | 잠재 FW 하드닝 여지, 부팅 시나리오 무관 |
| #4 | **H4-b 클램프 / H4-a 동일값**: 에코 OK인데 값 상이/무변화 | V-B | 결정적(간헐 아님); 범위밖·동일값 write에서만 | 사양대로(samd20 충실), 결함 아님 |
| — | H1 stale index / H2 gap 조기절단 | — | 기각(§4) | — |

**논거 핵심**: 관찰이 "**간헐적**"이라는 점이 H1/H2/H4를 배제한다(이들은 결정적이거나 발생 조건 부재).
간헐성을 물리 타이밍으로 자연히 설명하는 것은 **H3-병합(#1)** 뿐이다. 다만 벤치가 V-A/V-B를
구분 기록하지 않았으므로, §6은 두 변종을 분리 측정하도록 설계했다 — 만약 실제가 V-B로 판명되면
원인 순위는 H4로 재편해야 한다(→ 클램프/동일값 확인).

---

## 6. 벤치 재현 절차

### 6.1 사전 조건 / 함정 회피 (프로젝트 기지식)
- **SWD halt 금지 (런타임)**: gdb `halt`는 `sys_tick`을 멈춰 프레이밍·run 상태를 오진(전량 아티팩트). 런타임 관측은 **mbpoll + LCD 육안**만. SWD 정적 read는 부팅직후/포스트모템 단발만 (`[[feedback-swd-halt-breaks-board-validation]]`).
- **tty 잔재 함정**: 이전 mon 세션의 `stty 115200`이 남으면 RTU CRC가 깨짐. mbpoll 실행 전 tty를 **설정 baud/parity로 리셋**. 보드 기본(메모리)=SERIAL/addr=1/9600/EVEN. 예: `stty -f /dev/tty.usbserial-XXXX 9600 cs8 parenb -parodd -cstopb`. (mbpoll이 자체 설정하나 잔재 확인 권장.)
- **mon 병행 불가**: RTU 점유 시 `mon_set_enabled(false)`(`app_modbus.c:288`), USART6는 RS-485와 물리 공유·DE 미제어. mon 라인 캡처는 RTU와 동시 사용 불가.
- **전원 사이클은 물리적으로**: openocd `reset`은 boot mon을 안 주고 오픈-창 조건도 재현 못 함. 콘센트/전원 물리 재인가.

### 6.2 핵심 반복 시험 (×10, 첫-write 성패 기록)
대상 레지스터 예: `MB_REG_OUT_POWER = 0x06`(0-based holding[6]). mbpoll 기본 1-based →
`-r 7` 또는 0-based `-0 -r 6`. 클램프 회피 위해 유효범위 값(50~100) 사용.

각 iteration:
1. USB-RS485 어댑터 **분리**.
2. 보드 **물리 전원 사이클** → LCD 부팅 완료 대기.
3. 어댑터를 RS-485 버스에 **연결**(이 순간이 글리치 소스).
4. **딱 한 번** FC06 write + 즉시 read-back으로 **두 변종 분리**:
   ```bash
   # 첫 FC06 (holding[6]=OUT_POWER=80) — 에코/타임아웃 기록
   mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -0 -r 6 -1 /dev/tty.usbserial-XXXX 80
   # 즉시 read-back (FC03) — 값 반영 확인
   mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -0 -r 6 -c 1 -1 /dev/tty.usbserial-XXXX
   ```
   - `-1` = 1회 트랜잭션 후 종료(반복 폴 금지, "첫 write" 격리).
   - **판정 표기**: FC06이 **타임아웃**(에코 없음) → **V-A(무응답)**. FC06 **에코 OK**인데 FC03이 옛값 → **V-B(미적용)**. 둘 다 OK → **성공**.
5. 결과를 `iter, 결과(성공/V-A/V-B), 재시도성공여부` 로 로그. 실패 시 **즉시 동일 FC06 재시도** 1회 → 재시도 성공이면 #1(전이성 첫-프레임 드롭) 강한 방증.

**해석 규칙**:
- V-A 다수 + 재시도 항상 성공 → **H3-병합(#1) 확정 방향**.
- V-B 발생 → 쓴 값이 클램프 대상(범위밖)/동일값이었는지 로그 대조 → **H4-b/a**. (이 경우 §5 순위 재편.)
- 성공률이 어댑터 브랜드/케이블 길이/접속 순서에 민감 → 와이어측(H3) 방증.

### 6.3 실패 시 관측 포인트 (비침습 우선)
1. **버스 스니퍼(1순위, 진짜 비침습)**: 로직 애널라이저 또는 **두 번째 USB-RS485를 listen-only**로 A/B에 병렬. 첫 write 실패 순간의 원시 바이트열을 직접 캡처 → **선두 글리치 바이트 존재/글리치-프레임 간격**을 눈으로 확인(#1의 결정적 증거). 스니퍼는 보드를 멈추지 않음.
2. **openocd 정적 read (포스트모템, halt 수용)**: 실패 write **직후 1회** attach+`halt` 후 `read_memory`로
   `s_rx_tail`/`s_prev_head`/`s_last_rx_ms`(`usart6_mb.c:28-30`), `g_mb.holding[...]`(`app_modbus.c:29`) 관측.
   halt가 슈퍼루프를 멈추므로 **latch된 드롭 상태의 스냅샷 용도로만**(연속 관측 불가). 예:
   ```bash
   openocd -f fw/openocd/stm32f410.cfg -c "init; halt; \
     mdw <&s_rx_tail_addr> 4; resume; exit"
   # 심볼 주소는 build/*.elf 에서: arm-none-eabi-nm build/gds_us_ctrl.elf | grep -E 's_rx_tail|s_prev_head|s_last_rx_ms'
   ```
   가능하면 스니퍼(1)로 갈음 — halt는 최후수단.
3. **live-line 재오픈 케이스(H5) 확장**: 어댑터 연결·mbpoll 폴링 상태에서 LCD로 comm 설정 토글(예 addr 1→2→1) → 재오픈 후 첫 write 성패. 여기서 실패가 특이하게 잦으면 §4-H5(오픈-창 latch/ORE) 방증.

---

## 7. 수정 후보 (미적용 — 컨트롤러 결정)

> 모든 후보는 **미적용**. §6로 원인을 실측 확정하기 전에는 코드 변경 권장하지 않음.
> 특히 최유력 #1은 와이어/어댑터 성질 + RTU 재시도로 해소되는 성질이라 **FW 변경 불요**가 1순위 답.

### 후보 A — 운영/마스터측 (위험도 없음, 권장)
- Modbus RTU 마스터는 재시도가 표준. mbpoll은 기본 무재시도이므로 **재시도 래퍼**(성공까지 N회) 또는 접속 후 첫 폴 전 **짧은 정적 대기(글리치 소멸)**. 펌웨어 무변경. 사양(samd20 충실)에 부합.

### 후보 B — 오픈-창 하드닝 (H5 한정, 위험도 LOW, 부분적)
`usart6_mb_open`의 `HAL_DMA_Start`(`usart6_mb.c:105`) **직전**에 DR flush + ORE 클리어 추가.
live-line 재오픈 시 선두 latch 바이트/ORE-wedge만 방어(부팅 글리치 #1은 방어 못 함).

```c
    /* (제안, 미적용) 오픈 창(:79~:109)에서 DR에 latch된 stray 바이트를 버리고
     * ORE를 클리어 — 그렇지 않으면 buf[0] 오염 또는 DMAR wedge. 라인 idle 부팅 시 no-op. */
    __HAL_UART_CLEAR_OREFLAG(&huart6);
    (void)huart6.Instance->DR;
    /* --- 기존 HAL_DMA_Start(...) 이어짐 --- */
```
- **위험도 LOW**: 빈 DR read·ORE 클리어는 무해. 단 **#1(부팅 글리치 병합)은 해결하지 못함** — 그 글리치는 DMA 구동 *후* 도착하므로. H5(live-line 재오픈)만 개선.
- 적용 전 §6.3-(3)로 H5 실재 확인 필요.

### 후보 C — 프레임 내부 재동기 (위험도 HIGH, 비권장)
선두 non-addr 바이트를 프레임 경계로 스킵하는 재동기는 idle-gap 프레이밍 의미를 바꾸고 samd20
충실성을 깨며 회귀 위험 큼. **비권장** — #1은 재시도로 해소되는 것이 정석.

---

## 8. 잔여 불확실성

- **관찰 변종 미기록(V-A vs V-B)** 이 최대 불확실성 → §6.2 read-back으로 분리 필수. V-B로 판명 시 §5 순위를 H4로 재편.
- #1(H3-병합)의 **글리치 바이트 실재**는 코드로 증명 불가(와이어 현상) → §6.3-(1) 버스 스니퍼가 유일한 결정적 증거.
- H5의 ORE-wedge는 F4 USART/DMA 동작에 대한 INFERENCE — 실측(§6.3-3) 또는 RM0401 정밀 대조로만 확정.
