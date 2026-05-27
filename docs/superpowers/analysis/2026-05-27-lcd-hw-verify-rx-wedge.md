# HW 검증 finding — USART1 DGUS RX 영구 wedge (2026-05-27)

> 브랜치 `feat/stage-lcd-full-behavior`, T10 HW bench 검증 중 발견.
> 상태: **검증 차단(blocker)** — 정상 사용자 조작(energy 슬라이더 드래그)이 DGUS RX를 영구 정지시킴.
> systematic-debugging Phase 1~2 완료. Fix 미착수(사용자 의사결정 대기).

## 증상

- HAND(RUN) 페이지 정상 부팅, `run_page_confirmed=1`(체크리스트 ① PASS).
- SETUP에서 **energy 슬라이더 드래그 중 표시 멈춤**.
- 이후 **SAVE/CANCEL(및 모든 터치) 미동작**. 일부 버튼은 "반응"하나 이는 DGUS 패널 자체 페이지 점프(펌웨어 무관).

## 증거 (openocd, live)

PC 샘플 6회 — 전부 IPSR=0 (Thread 모드, **fault/exception 아님**), tight loop 아님(분산). 4/6이 `UART_WaitOnFlagUntilTimeout`(블로킹 UART TX).

RX 상태 스냅샷 (1초 간격 2회 동일 = 영구 상태):
- `s_rx_head` = `s_rx_tail` = 0x09 → 링 비어있음(드레인 완료, 백로그 아님)
- `s_rx_error_count` (ERRCNT) = **0** → `HAL_UART_ErrorCallback` 한 번도 미진입
- `usart1 s_rx_drop_count` = 0x1C = **28** (링-풀 드롭, `usart1.c:105`)
- `dgus s_dgus_rx_drop_count` = 0x3CA4 = **15524** (파서 desync 드롭, `dgus_lcd.c:222/234`)
- `s_parse_state` = 3 (PS_COLLECTING — 마지막 프레임 중간에서 멈춤)
- **USART1 SR = 0xF8** → bit3 ORE=1(오버런), bit5 RXNE=1(읽히지 않은 바이트가 DR에 대기), TC/TXE/IDLE=1
- **USART1 CR1 = 0x200C** → RE=1, TE=1, UE=1, 그러나 **bit5 RXNEIE = 0 (RX 인터럽트 비활성)**

심볼 주소(이 빌드): `s_dgus_rx_drop_count`=0x2000033c, `s_parse_state`=0x20000368, `s_rx_error_count`=0x2000038e, `s_rx_drop_count`=0x20000390, `s_rx_tail`=0x20000392, `s_rx_head`=0x20000393. USART1 base=0x40011000 (SR=+0x00, CR1=+0x0C).

## Root cause (2계층)

**Empirical 사실 (load-bearing)**: RXNEIE=0 + RXNE=1 + ORE=1 = RX 인터럽트가 더 이상 안 뜨고, 읽히지 않은 바이트가 DR에 갇혀 있으며, 자가복구 불가. → 펌웨어가 DGUS 프레임을 전혀 수신 못 함.

**1. 트리거/증폭기**: per-rx 디버그 트레이스가 루프를 포화시킴.
- `app_lcd_input.c:619` — 모든 RD 프레임마다 무조건 `mon_printf("[lcd] rx vp=...")`.
- `mon_usart6.c:12` — `mon_write` → `HAL_UART_Transmit(huart6, …, HAL_MAX_DELAY)` = **무한 타임아웃 블로킹** (~2.2ms/줄 @115200).
- energy 슬라이더(`LV_ENERGY_EDIT` 0x1202)는 드래그 중 VP를 연속 플러드 → 이벤트당 ~2.2ms 블로킹 → 슈퍼루프가 `usart1_rx_pop` 드레이닝을 못 함 → 64B 링(≈5.5ms분) 오버플로(28 드롭).
- (energy 편집 핸들러 `app_lcd_input.c:690-692`는 1줄 대입, 무관.)

**2. 실제 wedge (아키텍처 버그, must-fix)**: 1바이트 `HAL_UART_Receive_IT` RX가 오버런/재무장 실패 시 영구 정지.
- HAL 1바이트 IT 모드는 매 바이트 후 `UART_EndRxTransfer`가 RXNEIE를 clear → 콜백(`RxCpltCallback`)이 재무장해야 함. 재무장 실패(`usart1.c:111`, 28 드롭 중 일부) 시 RXNEIE=0으로 영구히 남음.
- ORE-복구용 `HAL_UART_ErrorCallback`(`usart1.c:120`, 재무장 `:132`)은 **미진입**(ERRCNT=0) — F4 HAL IT 모드는 EIE 미설정이라 ORE가 에러 콜백으로 라우팅되지 않음(추론, non-load-bearing).
- 즉 루프를 ~5.5ms 막는 **어떤 이벤트든** DGUS RX를 영구 wedge할 수 있음. mon 플러드는 트리거일 뿐.

## 신규 vs 기존

- USART1 1바이트 IT RX 드라이버 = **Phase 2/Stage A 산출물**(286d908, d8d413c). LCD 브랜치가 도입한 게 아님.
- Stage A 검증은 wire-level/cadence였고 **sustained 플러드 RX 미테스트**. LCD 브랜치가 **처음으로 패널→MCU 대량 RD 경로(parse_lcd_comm 입력)를 추가**해 기존 RX 버그를 노출.
- 결론: **LCD 포팅 로직 자체는 정상**. RX wedge는 driver-layer 버그(노출은 LCD 브랜치).

## Fix 방향 (advisor 권고, 사용자 의사결정 대기)

같은 브랜치 focused 후속 작업 (spec-first 권장):
1. **DMA circular RX** — USART1_RX 고정 매핑(DMA2, Ch4), 256B 순환버퍼 + NDTR 기반 head/firmware tail로 `usart1_rx_pop` 시그니처 유지. 오버런 면역, per-byte 재무장 제거 → 드라이버 단순화. (self-heal 폴링/EIE-trust는 구조적 취약성 미해결 → 기각.)
2. **per-rx 트레이스 게이트** — `app_lcd_input.c:619`를 디버그 플래그(default off)로. 플러드 증폭 제거.
3. **mon_write 타임아웃 한정** — `HAL_MAX_DELAY` → 유한(예: 50ms). 진단 채널이 앱을 영구 블록 못 하게(독립 robustness).

재검증: reflash → 드롭카운터 0 리셋 → energy 슬라이더 드래그 반복해도 RX 생존 + drop_count 0 + SAVE/CANCEL 동작 확인.

## 재현

1. 부팅 → SETUP 진입 → energy 슬라이더를 좌우로 빠르게 드래그.
2. 표시 멈춤(~수백ms 플러드) 후, openocd로 USART1 CR1 읽으면 RXNEIE=0, SR ORE=1.
3. 이후 모든 터치 미수신(SAVE/CANCEL 무동작). reset로만 복구.

---

# Finding 2 — LCD 입력 값 추출 off-by-one (SAVE 및 전 입력 오독) (2026-05-27, RX fix 후 발견)

> RX 하드닝(DMA circular) HW 검증 통과 후, SAVE 미동작을 추적하다 발견. RX wedge가 원래 모든 입력 테스트를 막아 이번에 처음 노출. **RX fix와 별개의 LCD-port 버그.**

## 증상
- DMA RX 검증 통과 후 SAVE를 눌러도 영속 안 됨(리셋 후 `[cfg] energy=100000` 공장기본 유지).

## Root cause (samd20 대조로 확정)
DGUS **0x83(read) 응답 프레임** = `5A A5 LEN 83 ADDR_H ADDR_L **READ_LEN** DATA_H DATA_L` — VP 주소 뒤에 **읽기 워드수(READ_LEN=0x01) 바이트**가 있음.

포트 파서(`dgus_lcd.c`) 매핑: `frame_buf[3+i]→data[i]` → `data[0]=READ_LEN`, `data[1]=DATA_H`, `data[2]=DATA_L`.

`app_lcd_input.c:617` 현재: `data16 = (data[0]<<8)|data[1]` = `(READ_LEN<<8)|DATA_H` = `(0x01<<8)|DATA_H`. **READ_LEN을 값 MSB로 오독.**

- SAVE 값 1 → 프레임 `…83 10 50 01 00 01` → data16 = (0x01<<8)|0x00 = **256** → `256 != SAVE_COMMIT(1)` → **`data_save_cancel()` 실행**(롤백) → 영속 안 됨.
- `(uint8_t)data16` 필드(model_freq/type 등, 값<256): data16=256 → (uint8_t)=0 → 항상 0 오독.
- 슬라이더(LV_ENERGY_EDIT): 1차 디버깅 때 본 값 256~285 = `(0x01<<8)|DATA_H`(DATA_H=0..0x1D) = garbage(실제값 아님).
- **전 numeric 입력 VP가 오독** (SAVE/CANCEL/슬라이더/한도/comm/ether…).

## samd20 권위 대조
`ref/samd20/dgus_lcd.h`: `LCD_COMM_MODE=1, ADDR_H=2, ADDR_L=3, DATA_H=5, DATA_L=6` — **offset 4(READ_LEN) 건너뜀**. `ref/samd20/main.c:3264-3265`: `lcd_data = lcd_buf[5]:lcd_buf[6]`(DATA_H:DATA_L). 즉 samd20은 READ_LEN을 건너뛰고 DATA를 읽음. 포트는 안 건너뜀 = 버그.

## Fix (1줄 + 주석)
`app_lcd_input.c:617`: `data16 = (uint16_t)(((uint16_t)f->data[1] << 8) | f->data[2]);` (data[0]=READ_LEN 건너뜀). `DGUS_MAX_DATA=23 ≥ 3`이라 `data[2]` 안전. 613-615 F5 주석 정정. spec의 F5 가정(틀림)도 정정 필요.

## 영향
LCD 입력이 전혀 정상 동작 안 했음(SAVE=cancel, 편집=garbage 저장). 이 1줄이 LCD 입력 전체를 정상화. RX fix와 함께 merge blocker.

## 잔여 관측
- 패널이 idle에 `5A A5 03 82 4F 4B`(LEN=3, cmd=0x82 WR) 프레임을 연속 스트리밍 → 파서가 `LEN<DGUS_LEN_MIN(4)`로 거부(정상, 무해). DMA RX가 wedge 없이 처리. dgus_rx_drop_count가 이 때문에 항상 비0(체크리스트 ⑨ 재해석됨). VP 0x4F4B 정체/스트리밍 이유는 향후 조사 후보(코딩 차단 아님).
