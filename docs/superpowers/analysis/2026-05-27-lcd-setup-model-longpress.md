# HW 검증 finding — SETUP_MODEL 롱프레스 진입 불가 (2026-05-27)

> 브랜치 `feat/stage-lcd-full-behavior`, T10 HW bench 검증 중(항목 ⑥ 진행 도중) 발견.
> 상태: **systematic-debugging Phase 1~4 완료, P2 fix HW 검증 PASS.**
> 사용자 결정: 펌웨어측 수정(P2) 채택, 패널 .bin 재설정(P1)은 보류.

## 증상

- SETUP 진입 후 **모델 변경용 `2_setup_model` 페이지 진입 키가 안 먹힘**.
- 처음엔 "재부팅"으로 의심했으나 **사용자 정정**: 패널 화면 리셋 없음, **다른 키는 정상**(다른 키로 빠져나올 수 있음). MCU는 계속 살아있음 → 크래시/리셋 아님.
- (mon 로그의 부팅 배너는 직전 ③ 교차확인 때 돌린 openocd 리셋의 **잔여 시리얼 버퍼**가 NULL 플러드와 함께 순서 뒤섞여 흘러든 것 — fresh 부팅 아님. 오진의 원인이었음.)

## 증거 (LCD_TRACE_RX 진단 빌드, live)

별도 `build-trace`를 `-DLCD_TRACE_RX`로 빌드(머지 `build/`·CMakeLists는 원복)·플래시 후, `app_lcd_input.c:624`의 per-rx 트레이스로 패널이 실제 보내는 VP/data 캡처:

```
[lcd] rx vp=0x1100 data=1     ← SETUP_PARAM(잘 되는 키): 단일 터치 data=1 → 즉시 동작
[lcd] rx vp=0x1202 data=567   ← 값 필드
[lcd] rx vp=0x1084 data=0     ← SETUP_MODEL: PRESS
[lcd] rx vp=0x1084 data=0     ← PRESS (반복)
...
```

3회 독립 캡처(탭+홀드 / 깨끗한 4초 홀드 ×2)에서 일관:
- **단일 4초 홀드 = `vp=0x1084 data=0` 정확히 2이벤트**(터치-다운 + 터치-업), **둘 다 value 0**.
- **`data=2`(릴리스)는 전 로그에서 0건.**
- **auto-repeat 없음** (4초 홀드인데 2이벤트뿐).
- 다른 키(0x1100)는 값 무관 단일 터치로 즉시 동작 → 패널↔MCU 통신 정상, RX 드롭 아님.

프레임 매핑 확인(`dgus_lcd.c:241-250`): 터치 이벤트는 0x83 auto-upload 형식, `data16=(data[1]<<8)|data[2]`(d6c681f fix) 올바름. 즉 release가 value 2였다면 `data=2`로 디코딩됐을 것 → **패널이 정말 2를 안 보냄**.

## Root cause

DGUS 모델 키(VP **0x1084**)는 **터치-다운과 터치-업(릴리스) 양쪽 모두 value 0**을 올린다(이벤트 각 1회, repeat 없음). 그러나 포팅된 `long_press_released()`(samd20 `main.c` verbatim)는 **`data==2`를 릴리스로 가정**하여 hold 시간을 계산·발화한다. 이 버튼은 2를 영영 안 보내므로:

1. 다운(data=0) → `key_press_ms` 기록, false 반환.
2. 업(data=0) → FSM이 **"또 다른 press"로 처리**(`key_press_ms` 재기록), false 반환.
3. → **롱프레스가 절대 완성되지 않음** → `enter_model_setup()` 미호출 → 페이지 미전환.

포팅 로직은 samd20과 **verbatim 일치**(업=2 기대)이므로, **이 패널 설정에서는 samd20 원본도 동일 구조로 실패**했을 것. = 포팅 회귀가 아니라 패널 DGUS 터치 설정과 펌웨어 프로토콜 가정의 불일치.

## 수정 갈래 (제품 의도: 모델 변경은 캘리브레이션을 날리므로 롱프레스 가드는 의도된 안전장치)

- **P1 (패널측)**: DGUS `.bin` 터치 설정을 0x1084 업에서 value 2를 올리도록 변경 → 펌웨어 무수정, verbatim 유지. 단 DGUS 에디터+패널 재플래시 필요. **→ 보류(사용자).**
- **P2 (펌웨어측, 채택)**: 같은 VP의 **연속 두 data=0을 press→release로 페어링**, 간격 ≥ KEY_HOLD_MS(2000ms)면 발화. 2초 가드 유지(탭은 미진입). `data==2`도 하위호환 유지. VP로 키잉해 0x1084/0x1094 간섭 없음.

## P2 구현 + 검증

- `fw/include/app_lcd.h`: `key_press_vp` 필드 추가(현재 눌린 long-press VP, 0=none).
- `fw/src/app_lcd_input.c`: `long_press_released(uint16_t vp, uint16_t data16)`로 시그니처 변경 + 다운/업 페어링 로직. 호출부 2곳(SETUP_MODEL/SETUP_PARAM_MOOHAN) `vp` 전달.
- 빌드 **0-warning**, FLASH 26.80% / RAM 10.16%.
- **HW 검증 PASS** (머지 `build/`, trace off): ① 모델 키 2초 홀드 → 모델 셋업 페이지 진입 ✓, ② 짧은 탭 → 미진입(가드 유지) ✓.

## 알려진 한계 / flag (코딩 차단 아님)

- **토글 desync 위험**: 0x1084 다운 프레임이 RX 드롭되면 업(data=0)을 다운으로 오인 → 다음 다운이 업으로 페어링되어 hold가 길면 spurious 진입 가능. 확률 낮음(DMA circular RX로 드롭 완화, 패널 idle junk는 LEN=3라 파서가 dispatch 전 거부, echo는 0x82라 drop). samd20도 유사 fragility 보유.
- **0x1094(SETUP_PARAM_MOOHAN)** 동일 거동 여부 미확인(검증 시 해당 키 트리거 불가). 같은 페어링 로직을 공유하므로 0x1094도 down=0/up=0이면 함께 수정됨. 추후 확인 권장.
- 본 패널의 0x1084 down/up이 항상 정확히 2이벤트라는 가정 — 다른 패널/펌웨어 리비전에서 재확인 필요.
