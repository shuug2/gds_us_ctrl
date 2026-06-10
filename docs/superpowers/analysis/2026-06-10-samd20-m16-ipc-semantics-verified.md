# SAMD20↔M16 IPC 의미론 검증 + Stage D 충실도 정정 (2026-06-10)

> **요약**: samd20 소스와 M16 디스어셈블리를 교차 검증하여 두-MCU IPC(START/SEEK/RESET/OVLD 4선)의 정확한 의미론을 확정했다. 핵심 발견 4건: ① **M16 램프는 per-START 소프트스타트가 아니라 부팅 1회 워밍업**(재진입 불가, 램프 중 OSC off + 명령 무시) — slice 2a의 per-START 램프 설계는 오독이었고 **원본 충실로 회귀**(사용자 결정), ② samd20의 `limit_on_time` 타임아웃은 REMOTE/COMM+SYS_HAND 전용(터치 런 무제한) — STM32는 V30 data=0 quirk 안전 대비로 **터치 런 ceiling을 의도적 추가**, ③ **RESET은 500ms 후 자동으로 SEEK으로 체인**(SEEK도 500ms 자동 해제) — SEEK/RESET 구현 시 필수 의미론으로 명문화, ④ **B-SEAM 가설 축소**: M16의 OSC 구동은 명령 3선의 active-LOW 레벨 미러일 가능성이 높음(×6/룩업 코어는 미연결 7-seg 표시 전용이었을 공산). 동일 날짜에 `app_reg.c` 수정 적용(워밍업 회귀 + on-time ceiling + ADC 1ms 페이스). 후속으로 **LCD 포팅 전수 감사(§6b) = 합격** — 단 LCD→제어 말단 스텁 현황과 미문서 갭 3건(RESET/SEEK 아이콘 엣지, us_on_time_200m, work_cnt 증가)을 deferred로 기록.

---

## 1. 검증 방법 / 출처

- samd20: `ref/samd20/main.c`, `ASF/common2/boards/user_board/user_board.h`, `init.c` 직독.
- M16: `ref/atmega16/M16_reverse/out/01_disassembly.asm` 전수 검색(0x0195 쓰기, PINA.4/5/6 읽기, OSC 플래그 쓰기 추적).
- 기존 fact base(`2026-05-31-m16-regulation-core-verified.md`)와 모순 없음 — 본 문서는 **런타임 의미론**(누가 언제 무엇을 구동하나)을 추가.

## 2. IPC 라인 확정 (4선이 전부)

| 라인 | samd20 핀 | M16 핀 | 방향 | 극성/형태 |
|---|---|---|---|---|
| M_START | PB13 out | PA4 in | S→M | **active-LOW 레벨**(런 동안 LOW 유지) |
| M_SEEK | PB11 out | PA5 in | S→M | active-LOW 레벨, **500ms 자동 해제** |
| M_RESET | PB12 out | PA6 in | S→M | active-LOW 레벨, 500ms 후 자동 해제 + **SEEK 자동 발행** |
| M_OVLD | PB10 in | PC0 out | M→S | 레벨 샘플링(엣지/디바운스 없음), 이중 부정 주의(`main.c:1201` `re_ovld=!level` 후 `re_ovld==0→issued=1`) — **과부하 포팅 시 보드 실측으로 극성 확정 필수** |

- 근거: `user_board.h:114-117`(`CTRL_INT_ON=0/OFF=1`), `init.c:79-81`(부팅 시 3선 모두 HIGH=해제), `main.c:4256/4262/4282/4287/4303/4313`(do_control 구동부).
- **M16 PA7/PC1/PC4는 samd20 측 대응 핀 없음** — IPC 아님(미해결 질문에서 제거 가능). M_RESET은 MCU 하드웨어 리셋이 아니라 논리 명령.

## 3. 발견 ① — M16 램프 = 부팅 1회 워밍업 (per-START 소프트스타트 아님)

디스어셈블리 전수 검색 결과:

- `g_main_state(0x0195)`에 nonzero 쓰기는 **부팅 경로 @0x1B8A 단 1곳**. @0x137C(rc≥401)에서 0으로 내려가면 HW 리셋 전까지 재진입 불가. 램프 카운터(0x0173) 0-리셋도 `app_0x1226` 진입부(부팅 1회) 뿐.
- **램프 중 OSC 플래그(g_0189/018A/0196)는 부팅 초기값 0 유지** — 램프는 미연결 7-seg 패턴 변수(g_019F/A0/A1)만 구동. 출력 없음.
- **램프 중 명령 무시**: Timer1 ISR이 `g_0195!=0`이면 PINA 디스패처 `app_0x06d2`를 스킵(@0x041E). 즉 부팅 후 ~4초간 START/SEEK/RESET 무반응.
- samd20도 RUN 전에 M_RESET을 펄스하지 않음(RUN과 RESET/SEEK은 직교). **원본에서 RUN은 즉시 풀 구동**이고 진폭은 samd20이 `I2C_POT`으로 직접 설정.
- M_START는 **레벨-팔로우 hold-to-run**: PINA.4 LOW 동안 `g_0189=1`(`app_0x0c70` @0x0CA4-0CCC), HIGH면 즉시 0. slice 2b의 momentary 게이트와 일치(이 부분은 충실했음).

**조치(사용자 결정 = 원본 충실 회귀)**: `app_reg.c` — 워밍업은 `app_reg_init`에서 1회 arm, 무조건 진행(run 상태 무관), START는 `main_state==0 && US_IDLE`에서만 수락, 수락 시 램프 재-arm 없이 즉시 `reg_scale(ch0_avg)` 적용. `reg_ramp_level()`은 출력 경로에서 제거(검증된 테이블 레퍼런스로 calc에 보존, 호스트 테스트 유지). 기존 `app_reg.c:67` 주석("M16's ramp is edge-driven on M_START")은 오류였으므로 폐기.

## 4. 발견 ② — limit_on_time의 원본 적용 범위와 STM32 의도적 편차

- samd20 강제 지점은 `main.c:5296-5302` **단 1곳**: `(us_run_status==US_COMM||US_REMOTE) && sys_mode==SYS_HAND`일 때 `us_on_time >= limit_on_time/10`(us_on_time은 100ms 단위, `main.c:5380` 부근 증가) → `us_off()`. **터치 런에는 원본 타임아웃 없음.**
- 단위: `limit_on_time` = ×10ms (`main.c:769` `=50; // x 10mS` → 기본 500ms), 패널 `LV_MAX_ON_TIME` 편집 가능.
- M16 자체에도 내부 카운트다운 백스톱 존재(g_018F 만료 시 Timer1 ISR이 g_0189=0, @0x0572-057E).

**조치(STM32 의도적 안전 추가, 원본 비충실임을 명기)**: V30 RUN 버튼 data=0 quirk(release 엣지 유실 시 무한 구동)에 대비해 **US_TOUCH 런에 `limit_on_time×10ms` ceiling** 적용(`app_reg_tick`). 0이면 비활성. ceiling 정지 후 버튼이 아직 눌린 상태의 release(data=0→IDLE 매핑→START 도착)는 `swallow_start` 플래그로 1회 소비(재시동 방지); legacy data=4 release는 RUN_RELEASE-while-IDLE 경로에서 플래그 해소. 기본 500ms가 터치 UX에 맞는지는 HW 세션에서 확인(필요 시 패널에서 조정).

## 5. 발견 ③ — RESET→SEEK 자동 체인 (SEEK/RESET 구현 시 필수 의미론)

`main.c:5388-5408` `sys_tick_func()` 100ms 틱:

1. RESET 발행(`us_reset_mode=RESET_ISSUE`) → M_RESET LOW 유지.
2. `us_reset_cnt > MAX_US_RESET_CNT(=5)` 즉 **500ms 후**: `sig_reset_status=OFF` + **`sig_seek_status=ON` 자동 발행**(TOUCH/COMM 소스일 때 무조건 체인) → M_RESET HIGH, M_SEEK LOW.
3. SEEK도 동일 타이머로 **500ms 후 자동 해제**. (B_SEEK 물리 버튼 경로는 누르는 동안 레벨-팔로우 + 해제 후 타임아웃.)
4. 기타: SEEK 중 RESET 누르면 SEEK 취소(`main.c:3641-3644`), REMOTE B_RESET release 시 RESET 해제+SEEK 발행(`main.c:1312-1327`).

**deferred 스펙 명문화**: 추후 `app_reg_command`의 SEEK/RESET 구현(OSC 미러 포함) 시 위 ①~④(500ms 레벨 유지, RESET→SEEK 무조건 체인)를 그대로 재현할 것. 과부하 복구 시퀀스도 동일 경로(OVLD→START 해제→사용자 RESET→자동 SEEK→idle).

## 6. 발견 ④ — B-SEAM 가설 축소

증거 종합: ⓐ 룩업/램프 산출물(g_019F/A0/A1)은 물리 미연결 PORTD(7-seg) 전용, ⓑ 진폭 제어는 samd20의 I2C_POT 직접 구동, ⓒ OSC 3선(g_0189→PC7, g_018A→PB1, g_0196→PB0)은 START/SEEK/RESET의 active-LOW 레벨 미러.

→ **M16의 OSC 인터페이스 = "명령 3선 릴레이 + ADC 과부하 감시"가 유력**. slice 1의 ×6/룩업 코어는 보드 상 죽은 표시 코드였을 가능성이 높음(단, lcd_measure 표시 도메인 값으로는 계속 사용). **B-SEAM 벤치 측정 항목을 "3선 레벨 미러 가설 확인"으로 축소**: RUN hold 중 OSC 커넥터(STM32 대응 PB14/PB2/PB10)에 LOW 레벨이 나타나는지 + 룩업 밴드 변화가 OSC 신호에 영향 없는지만 보면 됨.

## 6b. LCD 포팅 전수 감사 결과 (2026-06-10 후속, 코드 무변경)

samd20 LCD 기능(parse_lcd_comm 34 VP 핸들러·표시 TX·init/SYS_PIC_NOW 복구) × STM32 `app_lcd_*` 전수 대조 — **합격**. 모든 VP가 1:1 대응(STD_SETUP_PARAM은 양쪽 모두 0x1020 확인), V30 적응 4건(RUN data=0 `b78cbbf` / long-press data=0 페어링 / `temp_comm_mode=0xFF` 부팅 arm / STD 통신 페이지 DISP_COMM_MODE 재전송) + 의도적 편차(HAND 저장→MULTI 페이지, STD도 comm/ether 커밋, VAR_AMP max/last 미분리, 표시주기 ~20→~40ms 코스메틱)는 stage-lcd spec에 기록됨.

**LCD→제어 말단 스텁 현황**: RUN 게이트 ✅(OSC 물리 구동만 B-SEAM 대기) / `limit_on_time` ✅(본 세션부터 ceiling 실효) / SEEK·RESET ⬜ no-op(§5 체인 의미론 대기) / **`set_pot` ⬜ I2C 미구동(F2: U4 칩 정체 미확정)** / Modbus 적용 ⬜(Stage C) / horndown·energy·time 한계 ⬜(용접 사이클 deferred). §3 발견(진폭 제어 실체 = I2C_POT)과 결합하면 **set_pot 구현은 UI 부속이 아니라 실출력 제어의 본체** — 실출력 = ① B-SEAM 3선 미러 + ② I2C_POT 구동(F2 해소)이 전부.

**미문서 갭 (deferred 목록 추가, SEEK/RESET·사이클 구현 시 함께)**:
1. **ICON_RESET/ICON_SEEK/ICON_ERROR_RESET 점등 엣지 미포팅** — samd20은 `sig_reset/seek_status` 엣지에서 점등(`do_control`); STM32는 ICON_RUN만 엣지 렌더, RESET/SEEK 아이콘은 부팅 클리어뿐 → 현재 버튼을 눌러도 점등 안 됨. **§5 RESET→SEEK 체인 구현 시 아이콘 엣지 렌더 포함할 것.**
2. **`us_on_time_200m` 미공급** — LV_TIME 바가 실제 터치 런 중에도 항상 0. slice 2b로 런이 실재하게 됐으므로 `app_reg`가 런 중 200ms 카운터(max 200)를 채우는 저비용 후속 후보(용접 사이클 FSM 불필요; samd20 main.c:5223 등가).
3. **`LV_WORK_CNT` 증가 미포팅** — samd20은 용접 사이클 완료(RUN_CYL2)에 증가; 용접 사이클 FSM deferred 소속. `DISP_STD_DATA1` 센서(SENSE_DN) 엣지 갱신도 물리 입력 deferred에 동반.

## 7. 적용된 펌웨어 변경 (2026-06-10, `app_reg.c`/`app_reg.h`/`app_reg_calc.c` 주석)

1. **부팅 워밍업 회귀**(발견 ①): boot 1회 ~4s, 명령 무시, sel=0; START 즉시 구동; per-START 램프 폐기.
2. **TOUCH on-time ceiling**(발견 ②): `limit_on_time×10ms`, swallow_start 페어링 보정 포함.
3. **ADC 페이스 복원**: 기존 2ms 틱당 1채널 교대(ch0_avg 40ms/ch1_avg 400ms — 원본 8.3/42ms 대비 5~10×低)를 **1ms 페이스 양채널**로 변경 → ch0_avg 10ms/ch1_avg 50ms(ms-grid 최선).
4. 빌드 0-warning(main + REG_TRACE), 호스트 테스트 PASS(순수함수 무변경).
