# SETUP1 진입 시 horn shadow 시드 정정 (F1) — 설계 spec (2026-09-11)

> **문서 요약**: LCD SETUP1 페이지에 들어갈 때 horn-down 체크박스 VP 는 **현재 horn 모드**로 그리면서 저장용 shadow `temp_horndown` 은 **0 으로 리셋**하는 두 줄이 어긋나 있어, horn 이 이미 ON 인 상태에서 다른 항목(예: Safe mode)만 바꾸고 SAVE 하면 화면은 ✓ 인데 SAVE 가 `temp_horndown==0` 을 적용해 **horn 모드가 실제로 꺼진다**(표시만이 아니라 상태·솔레노이드·START 게이트까지). 조사 `research/2026-09-11-safty-horn-save-investigation.md` §D-3·§E. 수정 = 진입 시드 1줄을 **shadow 도 현재 모드로 시드**(F1)하고 오인용 legacy 주석을 정정한다. 거동 변화 = "SETUP1 에서 안 건드린 horn 체크박스는 SAVE 뒤에도 그대로 유지" 하나. `f_safty` 는 원래 꺼지지 않으므로 무관. 원격기가 `0x30` 으로 켠 horn 도 LCD SAVE 에 살아남게 되어 2026-09-06 벤치 함정("SETUP 저장이 horn down 재전송")이 함께 사라진다. 브랜치 `feat/modbus-write-lcd-echo` 위에 얹는다(사용자 결정).

## 0. 확정 결정 (2026-09-11 사용자)

| # | 결정 | 귀결 |
|---|---|---|
| 1 | **F1 채택** — `handle_setup_param_enter()` 의 `state->temp_horndown = 0u;` → 현재 모드 시드. F2(진입 시드 삭제)·F3(shadow 3상태)·F4(RUN 배지 재기록) 기각 | 1줄 변경 + 주석 정정. shadow 규칙 자체(SAVE 가 shadow 무조건 적용)는 그대로 |
| 2 | **지금 브랜치(`feat/modbus-write-lcd-echo`)에 얹는다** | 같은 벤치 세션에서 LCD 에코와 함께 검증. 벤치 항목 E-11b(HORN shadow 불일치 기록)는 이 수정으로 **기대값이 바뀐다** — §4 |

## 1. 배경 (조사 인용)

- SETUP1 진입 `fw/src/app_lcd_input.c:455-460` `handle_setup_param_enter()`: `dgus_write_u16(DISP_HORNDOWN, app_horn_mode_active())` 로 체크박스를 현재 모드로 그리고, 바로 다음 줄에서 `state->temp_horndown = 0u;`. 주석은 "legacy main.c:3617-3622 verbatim" 이라 하지만 **그 라인은 samd20 의 CANCEL 분기**(`ref/samd20/main.c:3511-3630`)다. samd20 의 SETUP_PARAM 진입(`:3754-3771`)은 `change_lcd_page` 만 하고 temp 를 리셋하지 않는다 → legacy 에서는 SAVE 뒤 temp=1 이 남아 다음 SAVE 에도 horn 유지. **포트가 CANCEL 의 리셋을 진입에 잘못 옮긴 것**(2026-07-18 `519d908` 부터).
- SAVE `fw/src/app_lcd_comm.c:388` `app_lcd_hook_horn(state->temp_horndown == 1u)` 무조건 적용 → shadow 0 이면 horn OFF(`app_horn_set_mode(0)`: SOL OFF, `app_reg.c:154` START 게이트 해제, STATUS bit6 0).
- 체크박스는 DGUS 자산(`13TouchFile.bin` @0x930/@0x950, 증분조절 0..1 루프 + 업로드)이 자체 토글하고 값을 올리므로 사용자가 ✓ 를 보고 **안 건드리면** shadow 는 0 그대로.
- `f_safty` 는 터치 즉시 cfg(`input.c:518`), SAVE 로 FRAM — 꺼지는 경로 없음(조사 §C).

## 2. 범위

**In**: `fw/src/app_lcd_input.c` `handle_setup_param_enter()` 1줄 + 주석 정정 · `docs/changelog.md` 항목 · plan Task 5 벤치 표의 E-11/E-11b 기대값 갱신.
**Out**: CANCEL 의 `temp_horndown = 0u`(`comm.c:442`, legacy 동일 — 유지) · SAVE 의 무조건 적용 규칙(`comm.c:388`) · shadow 3상태(F3) · RUN 페이지 HORN 배지 재기록(F4, 표시 전용 — 별건) · `app_horn.c` · 원격기.

## 3. 변경

```c
/* before — fw/src/app_lcd_input.c:455-460 */
    /* horn-down 체크박스 = 현재 SYS_HORN 모드 미러 + shadow 리셋 (legacy
     * main.c:3617-3622 verbatim — 저장 시 체크 안 건드리면 temp==0이라
     * 모드 이탈되는 legacy 거동 포함). */
    dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
    state->temp_horndown = 0u;

/* after */
    /* horn-down 체크박스 = 현재 SYS_HORN 모드 미러. shadow 도 **같은 값**으로 시드해
     * "화면 ✓ = SAVE 가 적용할 값" 을 지킨다 — 안 건드린 체크박스는 SAVE 뒤에도 유지.
     * (구 코드는 여기서 temp=0 리셋 — legacy main.c:3617-3622 를 인용했지만 그 라인은
     * samd20 의 CANCEL 분기다. samd20 SETUP_PARAM 진입(:3754-3771)은 temp 를 안 건드려
     * 다음 SAVE 에도 horn 이 유지됐다. 2026-09-11 F1, 조사 research/2026-09-11-safty-horn-save-investigation.md) */
    dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
    state->temp_horndown = (uint8_t)app_horn_mode_active();
```

- `app_horn_mode_active()` 반환형·0/1 정규화는 `app_horn.h` 확인 후 캐스트 결정(구현자).
- 터치 핸들러 `input.c:601` (`temp_horndown = (data16==1)`) 과 CANCEL `comm.c:442` 는 무변경.

## 4. 거동 변화와 검증

| 시나리오 | 전 | 후 |
|---|---|---|
| horn OFF → SETUP1 → Safe ✓·Horn ✓ → SAVE | 둘 다 ON | 둘 다 ON (무변경) |
| **horn ON(직전 SAVE 또는 원격 `0x30=1`) → SETUP1 → Safe ✓ → SAVE** | **horn OFF**(화면 ✓ 인데) | **horn ON 유지** |
| horn ON → SETUP1 → Horn 탭 1회(☐) → SAVE | horn ON(temp 0→… 패널 토글 뒤 값 0 → OFF) — 실기 확인 | horn OFF (☐ 대로) |
| horn ON → SETUP1 → CANCEL | horn ON, temp 0 | 동일 (CANCEL 무변경) |
| 원격 `0x30=1` → 조작자 SETUP 저장(horn 안 만짐) | horn OFF (09-06 벤치 함정) | **horn ON 유지** |

- host 테스트: 이 글루는 미링크 — 게이트 = 빌드 경고 0 + host 17 무회귀 + **HW 벤치**.
- 벤치(plan Task 5 표에 반영): **E-11** 은 그대로(원격 1→0 토글, 체크박스 따라옴). **E-11b 기대값 변경**: 진입 시 horn ON(`m.write(0x30,1)` 후 SETUP1 진입) → Safe 탭 → SAVE → `m.r1(0x1D) & 0x40 == 0x40` **유지**, SETUP1 재진입 ✓. 추가 **E-11c**: 위 상태에서 Horn 탭 1회(☐) → SAVE → bit6 0. **X 복원** 뒤 horn 0 확인.
- 09-06 벤치 노트의 "벤치 중 LCD SAVE 금지" 는 이 수정으로 **horn 재전송 원인이 사라지지만**, 다른 shadow(comm) 부작용은 남으므로 규칙은 유지.

## 5. 리스크

- 진입 시드가 모드를 따라가므로 원격이 horn 을 켠 채 조작자가 SETUP 을 지나가도 SAVE 가 horn 을 끄지 않는다 — 이는 의도한 변화이며 원격기 계약 문서의 "LCD SAVE 가 horn 을 끈다" 서술(원격기 CLAUDE.md:181 부근, 조사 인용)은 벤치 PASS 후 통보 문구에 한 줄 추가.
- legacy 이탈이 아니라 **legacy 복원**(samd20 진입은 temp 무접촉)에 가깝다 — changelog 에 그렇게 적는다.

## 6. 실행

- 브랜치 `feat/modbus-write-lcd-echo` 위 커밋 1개 `fix(lcd): SETUP1 진입 시 horn shadow 를 현재 모드로 시드 — 안 건드린 horn 이 SAVE 에 꺼지던 결함 (F1)` + changelog 항목 + plan 벤치 표 갱신(문서는 같은 커밋 또는 별도 docs 커밋). 트레일러 2줄. 빌드 날짜 `_260911` 그대로(같은 벤치 빌드).
- 벤치 PASS → 이 브랜치 전체가 태그 `hw-revA_fw-stage-lcd-echo` 로 함께 간다.
