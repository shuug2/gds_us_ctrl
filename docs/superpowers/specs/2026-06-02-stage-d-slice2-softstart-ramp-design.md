# Stage D slice 2a — 상태머신 + soft-start 램프 (design spec)

> **상태**: 설계 spec, 리뷰 대기. 브랜치 `feat/stage-d-slice2-softstart`.
> **범위**: `g_main_state` 상태머신 + soft-start 램프 **compute만**. 명령 FSM(re-arm)·overload·blink·OSC 물리 구동·정확한 패턴 바이트 = **범위 밖**(slice 2b+ / B-SEAM).
> **전제**: slice 1(레귤레이션 코어 compute) = main 머지됨(`5aea06f`, tag `hw-revA_fw-stage-d`). 본 슬라이스는 그 위에 부팅 soft-start를 얹음.
> **사실 출처**: `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` §2(SLICE 2 boundary) + `ref/atmega16/M16_reverse/out/04_reconstruction.c:241-259`(램프 rung) / `03_behavior_spec.md`. 디컴파일 함수명은 무시, 레지스터/제어흐름 사실만.

---

## 1. 개요 / 목표

M16은 부팅 시 출력을 즉시 full로 켜지 않고 ~4s에 걸쳐 누진 상승(soft-start)시킨 뒤 정상 레귤레이션(slice 1 lookup)으로 핸드오프한다. 본 슬라이스는 그 **상태머신 + 램프 누진 로직**을 STM32 슈퍼루프에 흡수한다.

slice 1과 동일한 measure-first 규율: **OSC 물리 출력은 deferred**. 램프가 구동하는 것은 "출력 setpoint"이고, 그 setpoint를 (a) deferred 출력용 band로, (b) 검증용 LCD bar로 surface한다.

---

## 2. 검증된 사실 (bucket A)

### 2.1 상태머신 `g_main_state` (verified §2, cadence-C6)
- 부팅 init = **1** (M16 `@0x1B8A`).
- 램프 카운터 ≥ **401**(0x191) 시 `→0` (M16 `@0x137C` / recon `:259`). **단방향 1→0**.
- `state==1`: 램프가 출력 패턴 vars 구동. `state==0`: slice-1 lookup이 구동.

### 2.2 soft-start 램프 `app_0x1226` (verified §2, cadence-C8)
- 16-bit tick 카운터 `@0x0173`, **Timer1 OVF ~10.1ms**(TCNT1=0xFFB1) 페이스로 증가. `state!=0`일 때만 증가.
- 10-rung 임계 사다리. 카운터가 각 임계를 넘을 때 출력 패턴 상향. 총 ≈ 401 × ~10ms ≈ **4s**.
- 같은 패턴 vars `0x019F/0x01A0/0x01A1`에 씀(= slice-1 lookup 출력과 동일 도메인).

### 2.3 per-rung 패턴 (recon `:249-259`, ground-truth 교차)
| rung | counter < | `g_019F` (주 패턴) | popcount(=fill bits) |
|------|-----------|-------------------|----------------------|
| 0 | 41  | 0x01 | 1 |
| 1 | 81  | 0x03 | 2 |
| 2 | 121 | 0x07 | 3 |
| 3 | 161 | 0x0F | 4 |
| 4 | 201 | 0x1F | 5 |
| 5 | 241 | 0x3F | 6 |
| 6 | 281 | 0x7F | 7 |
| 7 | 321 | 0xFF | 8 (full) |
| 8 | 361 | 0xFF | 8 (+2단 `g_01A0=0xFD`) |
| 9 | 401 | 0xFF | 8 (+2단 `g_01A0`) |
| — | ≥401 | (종료, `g_0195=0`) | — |

`g_019F` = 전형적 thermometer fill `(1<<(rung+1))-1`, rung 7에서 full(0xFF) 포화. rung 8-9는 2단 digit(`g_01A0`)을 추가 누진 — **2단 패턴은 deferred 출력단 몫**(B-SEAM). PORTC bit6 토글(rung 0 set / rung 2 clear, recon `:249/251`)도 정체 미상 출력 → deferred.

---

## 3. 아키텍처 (Approach A — slice 1 구조 미러)

```
app_reg_tick() (슈퍼루프, 매 패스)
 ├─ [10ms gate] state==1 이면 ramp_counter++; ≥401 → state=0   ← NEW (cadence)
 ├─ [2ms gate]  reg_acquire_step()  (ADC 교대 1변환, 평균)       ← slice 1
 │              sel = (state==1) ? reg_ramp_level(ramp_counter)  ← NEW (MUX)
 │                               : reg_scale(ch0_avg)            ← slice 1 (= adc_scaled_value)
 │              band = reg_output_level(sel)                     ← slice 1 (deferred 출력용, 보류)
 │              publish: curr_power = sel; curr_amp = ch0_avg;   ← curr_power가 sel로 통일
 │                       us_run_status = running while active    ← NEW (LCD 상태)
```

- **순수 계산**(`app_reg_calc.{c,h}`, HAL-free, 호스트 테스트): `reg_ramp_level()` 추가.
- **상태·cadence·MUX·발행**(`app_reg.c`): `g_main_state`/`ramp_counter` 상태 + 10ms 게이트 + MUX + us_run_status.
- 신규 파일 없음. `main.c`/`app.c`/`app_lcd.c`(오케스트레이터)/`board.c`/`adc1.*` 변경 없음. (LCD 상태 enum은 `app_lcd.h`+`app_lcd_disp.c` 최소 변경 — §7.)

### 3.1 sel 통일 (핵심 — 최소 변경)
`sel` = active output setpoint(scaled 도메인 0..~1024). state로 source만 MUX:
- `state==1`: `sel = reg_ramp_level(ramp_counter)` (램프 누진)
- `state==0`: `sel = reg_scale(ch0_avg)` = **기존 slice-1 adc_scaled_value와 완전 동일** → **state==0 무회귀 보장**.

`band = reg_output_level(sel)`, `curr_power = sel`는 두 경로 공통.

---

## 4. `reg_ramp_level()` 구체 정의 (순수함수)

```c
/* 카운터(0..401) → 10-rung soft-start 출력 setpoint(scaled 도메인).
 * §2.3 thermometer fill (g_019F popcount = rung+1, rung7에서 8bit full 포화)을
 * scaled 도메인으로 표현. 정확한 패턴 바이트(g_019F/2단 g_01A0)→OSC 매핑은 deferred
 * (B-SEAM, slice-1 C-LADDER와 동일 처리). 단조 증가, top에서 full(band 0). */
uint16_t reg_ramp_level(uint16_t counter);
```
- 임계 `{41,81,121,161,201,241,281,321,361,401}`로 rung 0..9 결정.
- rung→setpoint (full-scale 1024 / 8 thermometer bits = **step 128**, rung7+ 포화):
  `ramp_sel[10] = {128,256,384,512,640,768,896,1024,1024,1024}`.
- `counter < 41` → rung 0(128); `counter ≥ 401` → 1024(단, 호출부가 state→0 하므로 다음 틱부터 lookup).
- 결과: band가 부팅 시 off(21)→18→…→0(rung7)로 단조 하강, LCD bar는 128→1024 누진.

**의도적 표현 refinement (slice-1 패턴 미러)**: rung 8-9의 2단 patten(`g_01A0`)·PORTC bit6 토글·정확한 패턴 바이트는 출력단(B-SEAM)과 함께 deferred. 본 슬라이스는 타이밍(10 임계·10ms cadence)·단조 상승·핸드오프를 충실 재현.

---

## 5. cadence (Timer1 ~10ms 등가)
- `REG_RAMP_MS = 10`. `app_reg.c`에 `prev_ramp_ms` 게이트(slice-1 2ms 게이트와 별개).
- 10ms마다 `state==1`이면 `ramp_counter++`; `>= 401`이면 `state=0`(단방향). state==0이면 카운터 동결.
- 96MHz STM32가 ms 단위로 직접 재현(B-CADENCE 결정). slice-1 `sys_tick_get_ms()` 재사용.

---

## 6. LCD 노출 (라벨된 테스트 브리지)
- `curr_power = sel`: `state==1`엔 램프 setpoint(128→1024 누진), `state==0`엔 slice-1 scaled 피드백으로 **복귀**. M16은 LCD에 측정값을 표시했으므로, state==1 동안 setpoint를 bar에 싣는 것은 **명확히 주석된 테스트 브리지**(전압 주입 없이 패널서 램프 육안 검증). `max_power/last_power`도 sel 따라감(slice-1과 동일 패턴).
- `us_run_status`: active(부팅 후) 동안 `US_RUNNING`(=1, non-IDLE) → 패널 run 아이콘(`app_lcd_disp.c:158` 게이트 `!= US_IDLE`). **단일 "running" 값만** — 명령 source 구분(US_REMOTE/TOUCH/COMM)은 slice 2b 명령 FSM 몫. ramp/steady 모두 active이므로 부팅 후 계속 running(전환은 bar로 관측). 현재 `US_IDLE=0u`는 `app_lcd_disp.c` 로컬 정의 — contract home인 `app_lcd.h`로 끌어올려 `enum { US_IDLE=0, US_RUNNING=1 }` 정의, disp는 헤더 값 참조(consolidation).
- `curr_amp = ch0_avg` 유지(slice 1).

---

## 7. 통합 / 변경 파일
| 파일 | 변경 |
|------|------|
| `fw/include/app_reg_calc.h` | `reg_ramp_level` 선언 + rung 임계/step 상수 주석 |
| `fw/src/app_reg_calc.c` | `reg_ramp_level` 구현 (§4) |
| `fw/src/app_reg.c` | `g_main_state`/`ramp_counter` 상태, 10ms 게이트, sel MUX, us_run_status 발행; `app_reg_init`서 state=1 |
| `fw/test/test_app_reg_calc.c` | `reg_ramp_level` 호스트 테스트 추가 |
| `fw/include/app_lcd.h` | `us_run_status` 값 enum `{ US_IDLE=0, US_RUNNING=1 }` 추가 (contract home; §6 consolidation) |
| `fw/src/app_lcd_disp.c` | 로컬 `#define US_IDLE 0u` 제거 → 헤더 값 사용 (동작 무변경) |

`main.c`/`app.c`/`board.c`/`adc1.*`/`fw/test/Makefile` **무변경**.

---

## 8. 테스트 / 검증

### 8.1 호스트 단위테스트 (`reg_ramp_level`)
- 각 임계 경계: counter ∈ {0,40,41,80,81,…,400,401} 전후 계단값 검증.
- 단조 비감소(`reg_ramp_level(c) <= reg_ramp_level(c+1)`).
- counter=0 → 128(rung0); counter=320 → 1024(rung7, 첫 full); counter≥401 → 1024.
- 기존 `reg_scale`/`reg_output_level` 테스트 무회귀.

### 8.2 HW 검증 (REG_TRACE + LCD, 전압 주입 불필요)
- 부팅 시 trace: `state=1` → `sel`/`band`이 ~4s 동안 128→1024 / band 18→0 단조 + **LCD 출력 bar 상승 육안** + us_run_status running 아이콘.
- ~4s 후 `state→0` 전환, 이후 `sel = reg_scale(ch0_avg)`(slice-1 lookup 거동) + bar가 실측 scaled로 복귀.
- 무회귀: 배너·LCD 네비·OSC idle-HIGH(slice 1 6a) 유지.
- trace에 `state`/`ramp_counter` 추가(REG_TRACE 게이트).

> **검증 결과 (2026-06-08) — compute PASS, 육안 합격기준 2건 정정.** 시리얼 trace로 램프 전 구간 검증(sel 128→1024 단조 / band 18→0 / rc=401 ~4s 핸드오프 / st=0 = slice-1 verbatim 무회귀) + LCD **power 숫자(VAR_POWER)** 가 램프 추종 상승 = 전압주입 없이 램프가 패널까지 도달함을 입증(§8.2 목표 = 달성). 단, 위 두 육안 항목은 디스플레이 계층 오독에 따른 **잘못된 예측**이었음:
> - **"LCD 출력 bar 상승"** ✗예측: 출력 바(`LV_OUTPUT`)는 `disp_compute_output(curr_amp,…)`로 구동되는 **amplitude 바**이지 `curr_power`가 아님. 램프 미러는 power **숫자**(`disp_send_val`: `VAR_POWER ← max_power ← sel`). 전압주입 없으면 `curr_amp` idle(≈3 ≤ 10) → 바 정지 = **by-design**(samd20 충실 포팅). 바를 `sel`로 구동하면 충실 위젯을 벤치 편의로 거짓 구동 → ✗.
> - **"running 아이콘"** ✗예측: `ICON_RUN`(0x1152)은 samd20에서 run **명령 FSM** 소속(`ref/samd20/main.c:4302` = `sig_run_status` 엣지 + `M_START` + accumulator 리셋) → **slice 2b**(§9). 2a는 `app_lcd.c:125`에서 init 클리어만, 점등 안 함이 정상. `us_run_status != US_IDLE` 게이트(`app_lcd_disp.c:156`)는 숫자 소스(max/last_power) 선택일 뿐 아이콘 미구동.
> 결론: **펌웨어 수정 불필요** — compute 합격, 바/아이콘은 예상된 거동. 본 §8.2 두 육안 항목은 **power 숫자 상승**으로 대체.

---

## 9. 범위 밖 / deferred
- **명령 FSM**(us_start/seek/reset, START re-arm): slice 2b. 본 슬라이스 램프는 **부팅 1회**만.
- **overload 검출/플래그**, **blink**: slice 2b+.
- **OSC 물리 구동** + 정확한 패턴 바이트(g_019F 2단·PORTC bit6): B-SEAM (벤치 측정 후).
- **6b 신호 calibration**(`>>2` 도메인·물리단위): HW 준비 후.

---

## 10. flagged (코딩 차단 ✗, 주석)
- `ramp_sel` step=128은 thermometer fill의 scaled-도메인 표현 — 정확한 패턴→OSC는 B-SEAM에서 확정.
- 부팅 auto-run(state init=1)이 통합 제품에서 의도된 동작인지(또는 START 명령 게이트가 필요한지)는 slice 2b 명령 FSM에서 재검토.
