# COMM 페이지 모드 아이콘 지연 재기록 (B) — 설계 spec (2026-09-13)

> **문서 요약**: 전원 재투입 후 LCD 통신 페이지에 처음 들어가면 **화면은 이더넷 페이지(STDE 27)로 맞게 진입하는데 모드 선택 아이콘만 "시리얼"** 로 보인다. 실기 3단계 판별과 조사(`research/2026-09-12-comm-mode-display-investigation.md`) + 원격기 세션 회신으로 **cfg·FRAM·부팅 로드·TCP 개통·원격기 쓰기는 전부 정상**이고, 결함은 **DGUS 패널이 페이지 전환 직후의 `DISP_COMM_MODE`(0x140c) 쓰기를 반영하지 않는 표시 특성** 하나로 좁혀졌다(2026-05-31 분석 `analysis/2026-05-31-std-comm-page27-display-port-faithful.md` 의 page 27 특이점과 동일 증상 — 그때 넣은 set_page 직후 재기록 Fix C 가 이 보드에서는 먹지 않는다). 사용자 결정 **B**: 페이지 전환 뒤 **일정 지연(100 ms) 후 1회** 아이콘을 다시 쓴다. 이번 에코 브랜치와 무관한 기존 결함(legacy 도 동일)이며, 브랜치 `feat/modbus-write-lcd-echo` 에 얹어 같은 벤치에서 검증한다.

## 0. 확정 결정 (2026-09-13 사용자)

| # | 결정 | 귀결 |
|---|---|---|
| 1 | **B 채택** — 지연 1-shot 재기록. A(패널 자산 재플래시) · C(SYS_PIC_NOW read 후 재기록) · D(주기 재기록) 기각 | 펌웨어 ~15줄, 자산 무변경. 패널 자산이 나중에 고쳐져도 무해(중복 쓰기) |
| 2 | 지금 브랜치에 얹는다 | 같은 벤치(`_260911`)에서 E-17 로 검증 |

## 1. 근거 (조사·실기)

- 실기: 재부팅 후 TCP 접속 OK(`mb_tcp.py`) · 이더넷 페이지로 진입 · 아이콘만 시리얼. SAVE 직후 접속도 OK.
- 원격기(`gds_us_remote` 2026-09-13 회신): 부팅·접속 직후 FC06 0건, `0x21` 쓰기 경로 없음 — 원격기 원인 배제.
- 펌웨어 값 흐름(조사 §1): 부팅 `temp_comm_mode=0xFF`(`app_lcd.c:216`) → COMM 진입 시드 `= cfg->comm_mode`(`app_lcd_render.c:135-136`) → `DISP_COMM_MODE=1` 을 **set_page 전**(`:215`) 과 **set_page 후 Fix C**(`render_comm_tail` `:160`, 호출 `:233-236`) 두 번 쓴다. 0 을 쓰는 경로는 시리얼 버튼 터치(`app_lcd_comm.c:92`)뿐.
- 패널 자산(`14ShowFile.bin`, `22_Config.bin`): 0x140c var-icon 은 4 페이지 레코드 동일, VP 초기값 0 → 미반영 시 시리얼로 보임. 물리 패널의 자산이 저장소본과 같은지는 미확인(추측: 06-08 재플래시).
- 이 브랜치(`3f82c06..6ec5b78`)의 fw 변경은 0x140c·`temp_comm_mode` 무접촉. legacy samd20(`main.c:3159-3172`)은 set_page 전에만 써서 같거나 더 나쁨 → **기존 결함**.
- 05-31 실증: page 27 은 **활성 중 라이브 쓰기**(터치 핸들러 경로)만 반영 → set_page 직후 쓰기는 패널이 페이지를 아직 그리기 전이라 유실되는 것으로 추정. 지연 후 쓰기 = 그 라이브 쓰기와 같은 조건.

## 2. 범위

**In**: `fw/src/app_lcd.c` `app_lcd_tick()` 에 지연 재기록 1블록 · `fw/include/app_lcd.h` state 필드 1개 · `fw/src/app_lcd_render.c` 에서 재기록 무장(arm) 1줄 + `render_comm_tail` 을 공개 함수로 승격(tick 에서 호출) · 상수 1개 · changelog · plan 벤치 표 E-17.
**Out**: 패널 자산 · SYS_PIC_NOW 게이트(200 ms) 변경 · 주기 재기록 · `temp_comm_mode` 시드 규칙 · Fix C 삭제(유지 — MULTI 페이지 25 는 그것으로 이미 정상, 중복 무해).

## 3. 설계

### 3.1 상태·상수

```c
/* fw/include/app_lcd.h — lcd_app_state_t 에 추가 (last_set_page_ms 옆) */
    uint8_t  comm_icon_reassert_pending;  /* 1 = comm 페이지 set_page 뒤 지연 재기록 대기 (B, 2026-09-13) */

/* fw/include/app_lcd.h 또는 app_lcd_render.c — 상수 */
#define LCD_COMM_ICON_REASSERT_MS  100u   /* set_page 뒤 아이콘 재기록 지연. 05-31: 활성 페이지 라이브 쓰기만 반영 → 페이지가 그려진 뒤 써야 한다 */
```

지연값 100 ms 근거: SYS_PIC_NOW 루프 가드 200 ms 보다 작아 재초기화 체인과 겹치지 않고, DGUS 페이지 전환 렌더(수십 ms)보다 크다. **실측으로 조정 가능**(벤치 E-17 에서 안 되면 200 ms 로 올려 1회 재시험 — spec 안에서 허용되는 유일한 변형).

### 3.2 무장 — `app_lcd_change_page()` 의 comm 분기 (기존 Fix C 호출 옆)

```c
    if (page == LCD_SETUP_MHC || page == LCD_SETUP_STDC ||
        page == LCD_SETUP_MHE || page == LCD_SETUP_STDE) {
        render_comm_tail(page, state);                 /* 기존 Fix C — 유지 */
        state->comm_icon_reassert_pending = 1u;        /* B: 지연 재기록 무장 */
    }
```

다른 페이지로 `change_page` 하면 무장은 그대로 남을 수 있으므로, tick 의 발화 조건에 **현재 페이지가 여전히 comm 페이지**인지 포함한다(§3.3). 페이지를 떠났으면 무장 해제만.

### 3.3 발화 — `app_lcd_tick()` (fault 미러와 disp_step 사이)

```c
    /* B (2026-09-13): comm 페이지 set_page 뒤 LCD_COMM_ICON_REASSERT_MS 지나면 모드 아이콘 1회 재기록.
     * page 27(STDE) 은 표시 시 DISP_COMM_MODE 를 auto-load 하지 않고 활성 중 라이브 쓰기만 반영
     * (analysis/2026-05-31-std-comm-page27-display-port-faithful.md). set_page 직후 쓰기(render_comm_tail
     * 호출 1)는 이 보드에서 유실됐다 — 페이지가 그려진 뒤 한 번 더 쓴다. 값은 render_comm_tail 과 동일 소스
     * (temp_comm_mode) 라 표시 = shadow = cfg. 재초기화 루프 가드(200 ms) 안쪽이라 SYS_PIC_NOW 와 겹치지 않는다. */
    lcd_app_state_t *st = app_lcd_state();
    if (st->comm_icon_reassert_pending != 0u &&
        (uint32_t)(now - st->last_set_page_ms) >= LCD_COMM_ICON_REASSERT_MS) {
        st->comm_icon_reassert_pending = 0u;
        if (st->lcd_status == LCD_SETUP_MHC || st->lcd_status == LCD_SETUP_STDC ||
            st->lcd_status == LCD_SETUP_MHE || st->lcd_status == LCD_SETUP_STDE) {
            app_lcd_comm_icon_refresh(st->lcd_status);
        }
    }
```

`now` 는 이미 `app_lcd_tick` 에 있다(`uint32_t now = sys_tick_get_ms();`) — 선언 위치를 이 블록 앞으로 올린다(코드 순서 변경 1건, 거동 동일).

### 3.4 `render_comm_tail` 승격

`static inline __attribute__((always_inline)) void render_comm_tail(uint8_t page, const lcd_app_state_t *state)` → 공개 `void app_lcd_comm_icon_refresh(uint8_t page)` (state 는 `app_lcd_state()` 로 취득). 본체 verbatim. `change_page` 의 기존 호출은 새 이름으로 교체. 선언은 `app_lcd.h` `app_lcd_run_std_refresh` 옆. (2026-09-11 Task 2 와 같은 패턴 — always_inline 해제로 코드 1벌.)

### 3.5 거동

| 상황 | 전 | 후 |
|---|---|---|
| 재부팅 → COMM(27) 첫 진입 | 아이콘 시리얼(유실) | 진입 ~100 ms 뒤 이더넷 아이콘 |
| MULTI COMM(25) 진입 | 정상 | 정상 + 100 ms 뒤 동일값 재기록(무해) |
| COMM 진입 직후 100 ms 안에 다른 페이지로 이동 | — | 무장 해제만, 쓰기 없음 |
| COMM 체류 중 원격 staged 쓰기 | 표시 무변화(shadow 기반) | 동일(범위 밖) |
| 터치로 모드 변경 | 라이브 쓰기 정상 | 동일 |

추가 UART: 8~16 B 1회/진입(≈1.4 ms), hold 예산 무관(LCD 진입 이벤트).

## 4. 검증

- 빌드 STD·REMOTE 경고 0, host 17. FLASH 추정 +40~60 B.
- **HW 벤치 E-17**(plan Task 5 표에 추가): 전원 재투입 → COMM 진입(STD 모델 = page 27) → **아이콘이 이더넷**인지 육안(≤0.5 s 안에). 실패 시 `LCD_COMM_ICON_REASSERT_MS` 200 으로 1회 재시험(§3.1). DHCP 체크(`DISP_EN_DHCP`)도 같이 확인. MULTI 모델이면 page 25 무회귀.
- E-17b: COMM 진입 직후 즉시 다른 페이지로 이탈 → 아이콘 쓰기 없음(다른 페이지에서 잔상 없음) — 육안.
- 회귀: SYS_PIC_NOW 재초기화 루프 없음(부팅 시 페이지 전환 정상), COMM SAVE/CANCEL 동작 무변경.

## 5. 리스크

- 100 ms 가 부족하면 그대로 시리얼 — spec 이 200 으로의 1회 조정을 허용. 그래도 실패면 원인이 타이밍이 아니라는 뜻(자산 A 로).
- legacy 이탈이지만 "표시가 cfg 를 따른다" 방향의 보정이고, LCD 전용이라 원격 계약 무관.

## 6. 실행

- 커밋 1개 `fix(lcd): COMM 페이지 모드 아이콘 set_page 뒤 100 ms 지연 재기록 — page 27 auto-load 미반영 (B)` + changelog 불릿 + plan E-17/E-17b 행. 트레일러 2줄. 빌드 날짜 `_260911` 유지(같은 벤치 빌드).
