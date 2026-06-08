# Stage D slice 2b — RUN 명령 게이트 (design spec)

> **상태**: 설계 spec, 리뷰 대기. 브랜치 `feat/stage-d-slice2b-run-gate`(예정).
> **범위**: 터치 **RUN start/stop 게이트** + ICON_RUN + `us_run_status` 소스 taxonomy + 부팅 IDLE 전환. **SEEK/RESET regulation 효과·overload·weld-cycle·Modbus·OSC 물리구동 = 범위 밖**(후속 / B-SEAM / Stage A·C).
> **전제**: slice 2a(상태머신 + soft-start 램프) = main 머지됨(`43fda87`, tag `hw-revA_fw-stage-d2`). 본 슬라이스는 그 위에 명령 게이트를 얹어 부팅 auto-run을 명령 구동으로 전환.
> **사실 출처**: `ref/samd20/main.c`(명령 FSM 원본 — run 경로 `3676-3704`, sig 엣지 핸들러 `4298-4340`, `us_off` `4180-4202`, US_* defines `110-113`, init `880`). 디컴파일 함수명 무시, 제어흐름/레지스터 사실만.

---

## 1. 개요 / 목표

slice 1/2a는 M16 regulation core(compute)를 흡수했고, 검증 편의를 위해 **부팅 즉시 auto-run**(`main_state=1` → 램프 → lookup, `us_run_status` 상시 RUNNING)했다. 통합 제품에서 초음파 출력은 **명령으로 게이트**되어야 한다(원 2-MCU 구조: SAMD20 명령 FSM이 `M_START`를 assert해야 M16이 구동). 본 슬라이스는 SAMD20 명령 FSM의 **RUN 게이트 절반**을 흡수하고, slice 1/2a가 흡수한 regulation core 절반과 **내부 함수 호출로 연결**한다(원 inter-MCU GPIO `M_START`를 대체).

핵심 변경: **부팅 = IDLE**(출력 없음). 터치 패널 RUN을 누르는 동안만 regulation 구동(soft-start 램프 → lookup), 떼면 정지. = **momentary hold-to-run** + **START re-arm**.

slice 1/2a와 동일한 measure-first 규율: **OSC 물리 출력은 여전히 deferred**(B-SEAM). 본 슬라이스는 명령→상태 전환 + LCD 표면(ICON_RUN/us_run_status/power 숫자)으로 검증한다.

---

## 2. 검증된 사실 (bucket A — samd20 명령 FSM)

### 2.1 터치 RUN 실행 모델 = momentary hold-to-run (`main.c:3674-3704`)
- **press**(DGUS key data=3): `if(us_run_status != US_REMOTE)` → `us_run_status = US_TOUCH; sig_run_status = ON;` + `us_on_time=0` + `curr_energy=last_energy=max_amp=max_power=acc_energy=0` + DAC write(`I2C_POT`).
- **release**(data=4): `if(us_run_status == US_TOUCH)` → `us_run_status = US_IDLE; sig_run_status = OFF;`.
- **model_type 무관**(STD/HAND/MULTI 동일 경로). STM32 입력층 `handle_key_multi`가 이미 이 계약을 인코딩(data 3→`US_CMD_START`, 4→`US_CMD_RUN_RELEASE`, DAC는 `app_lcd_hook_set_pot`).

### 2.2 소스 taxonomy + arbitration (`main.c:110-113`)
- `US_IDLE=0, US_REMOTE=1, US_TOUCH=2, US_COMM=3`. `us_run_status`가 **"누가 구동 중인가"** 소스를 보유.
- arbitration: 터치 start는 `!= US_REMOTE`일 때만 수락; 터치 release는 `== US_TOUCH`일 때만 정지. (REMOTE=물리 스위치/IPC, COMM=Modbus는 본 슬라이스 범위 밖이나 **enum/게이트 구조는 보존**해 후속 소스가 끼어들 자리를 만든다.)

### 2.3 sig_run_status 엣지 핸들러 (`main.c:4298-4340`)
- OFF→ON: `ICON_RUN=1`, `M_START` on, `B_USOUT` on, accumulator 리셋(`curr_energy=last_energy=max_amp=max_power=acc=0`), `us_on_status=ON`.
- ON→OFF: `us_off()`.
- 통합 STM32에선 `M_START`/`B_USOUT` GPIO 대신 **regulation core 재기동/정지**(soft-start 램프 re-arm)로 대체.

### 2.4 `us_off()` 정지 경로 (`main.c:4180-4202`)
- `ICON_RUN=0`, 출력 라인 off, **`last_amp = max_amp; last_power = max_power;`**(curr_가 아니라 peak를 latch — 놓치기 쉬움), `curr_energy=acc_energy=0`, `us_run_status = US_IDLE`, `sig_run_status=OFF`.

### 2.5 부팅 init (`main.c:880, 946`)
- `us_run_status = us_reset_status = us_seek_status = US_IDLE;` `sig_run_status = bak_run_status = 0;` → **부팅 IDLE**(명령 전엔 미구동).

---

## 3. 아키텍처 (계층 분리 — slice 1 구조 유지)

```
DGUS RX ─ app_lcd_tick() ─ 입력층 handle_key_multi ─ app_lcd_hook_us_command(cmd)   [app_lcd.c]
                                                            └─▶ app_reg_command(cmd)  ← NEW (라우팅)
app_reg_tick() (슈퍼루프)                                       [app_reg.c]
 ├─ run FSM: us_run_status / main_state(ramp|lookup) / ramp_counter / max_*
 ├─ 출력 MUX: IDLE→sel=0 / ramp→reg_ramp_level / lookup→reg_scale(ch0_avg)
 └─ publish lcd_measure_t (us_run_status, curr_power=sel, max/last_power, curr_amp)
app_lcd_disp_step() (4ms step machine)                          [app_lcd_disp.c]
 ├─ disp_send_val: VAR_POWER ← on?max_power:last_power  (숫자 = 램프 가시면)
 └─ ICON_RUN 엣지 렌더: us_run_status running-ness 변화 시 1회 write   ← NEW
```

- **app_reg.c** = compute + run FSM + `lcd_measure_t` 단일 소유(DGUS-free 유지).
- **app_lcd_disp.c** = measure 렌더 + ICON_RUN 엣지(주석 `:39-41`이 예고한 "Stage D가 us_run_status 채우면 아이콘 담당").
- 신규 파일 없음. `main.c`/`board.c`/`adc1.*`/순수계산 `app_reg_calc.*` **무변경**(SEEK/RESET regulation 효과가 없으므로 순수함수 추가 불필요).

### 3.1 출력 MUX (slice-1 무회귀 보존)
`sel` = active output setpoint(scaled 0..1024):
- `us_run_status == US_IDLE` → `sel = 0`(정지).
- 활성 + `main_state==1` → `sel = reg_ramp_level(ramp_counter)`(slice 2a 램프).
- 활성 + `main_state==0` → `sel = reg_scale(ch0_avg)` = **slice-1 verbatim** → 활성 lookup 경로 무회귀.

`band = reg_output_level(sel)`(deferred 출력용, 보류). slice-1/2a의 scale/lookup/ramp **계산은 무변경** — 본 슬라이스는 그 둘레에 run 게이트만 두른다.

---

## 4. Run FSM 구체 정의 (`app_reg.c`)

상태 필드(신규/변경):
- `us_run_status` (uint8_t): 소스 taxonomy. **`g_measure.us_run_status` 발행값을 FSM이 직접 소유**(slice 2a의 상시 `US_RUNNING` 하드코딩 대체).
- `main_state` (기존): 활성 run 내부의 ramp(1)/lookup(0) **하위상태**. IDLE일 땐 의미 없음.
- `ramp_counter`(기존), `max_power`(신규 running-max), `last_power`(신규 latch).

전환:

| 트리거 | 가드 | 동작 |
|--------|------|------|
| `app_reg_init` (부팅) | — | `us_run_status=US_IDLE`, `main_state=0`, `ramp_counter=0`, `max_power=last_power=0`. **auto-run 폐기.** |
| `US_CMD_START` | `us_run_status == US_IDLE` | `us_run_status=US_TOUCH`; `main_state=1`; `ramp_counter=0`; `max_power=0`(re-arm 리셋). = soft-start 램프 재발화. **idle→run 엣지만**(§4.2). |
| `US_CMD_RUN_RELEASE` | `us_run_status == US_TOUCH` | `last_power = max_power`(latch, §2.4); `us_run_status=US_IDLE`. = us_off 등가. |
| `US_CMD_SEEK` / `US_CMD_RESET` | — | **본 슬라이스 무동작**(mon trace만). 입력층이 이미 RESET icon/page 복원 처리. regulation 효과 = deferred(§9). |

### 4.2 START 가드 = `== US_IDLE` (samd20 `!= US_REMOTE`에서 의도적 분기)
samd20은 START를 `!= US_REMOTE`로 게이트했다(§2.1) — 그 2-MCU 구조에선 press가 `sig_run_status=ON`만 세우고 **soft-start 램프는 M16의 `M_START` OFF→ON 엣지로 구동**됐으므로, 이미 ON인 상태의 재press는 엣지를 안 만들어 램프가 중단 없이 진행했다. 통합 코드는 램프를 직접 소유하므로, `!= US_REMOTE`를 그대로 쓰면 **RUN 유지 중 패널 auto-repeat(`data=3` 반복)** 가 매 폴마다 `ramp_counter=0`으로 램프를 재시작 → 401 미도달 → lookup 핸드오프 실패(HW §8.2 item 2 실패 모드). 따라서 **`== US_IDLE`**(real idle→run 엣지)로 가드 — TOUCH-only 슬라이스에선 strictly 안전(활성 run 재시작 불가)하며 M16 엣지구동 램프에 충실. REMOTE/COMM 소스 + 재press energy 리셋 정제는 후속 슬라이스(§9).

- `app_reg_tick`의 ramp cadence/MUX는 slice 2a 그대로 — 단 10ms 램프 증가 게이트에 **활성 run 조건 추가**: `(us_run_status != US_IDLE) && main_state==1`일 때만 `ramp_counter++`. IDLE 중엔 동결(release를 ramp 도중에 해도 카운터 stale 증가/wrap 없음; 다음 START가 0으로 리셋). 10ms 게이트의 `≥401 → main_state=0` 핸드오프 유지.

### 4.1 max/last 추적 (faithful, mirror collapse 폐기)
- slice 2a는 `curr_power=max_power=last_power=sel`로 **collapse**(테스트 편의). 본 슬라이스는 이를 폐기하고 §2.4를 충실 재현:
  - 활성 중 매 publish: `if(sel > max_power) max_power = sel;` (running peak).
  - 정지 시: `last_power = max_power`(latch).
- `curr_power = sel`는 **라벨된 setpoint 표면으로 유지**(활성 중 현재 출력 레벨; IDLE→0). 실측 전력(B3 전달함수)은 6b/B-SEAM에서 대체. → VAR_POWER 숫자가 `max_power`(peak-hold)를 따라 램프 가시(전압주입 불필요, slice 2a가 실제 검증한 표면과 동일).

---

## 5. 명령 라우팅 (`app_reg_command`)

```c
/* app_reg.h: 터치/통신 명령을 regulation run FSM에 전달. 슈퍼루프 단일스레드 —
 * 호출 즉시 FSM 상태 변이, 다음 app_reg_tick이 소비. SEEK/RESET = 본 슬라이스 무동작. */
void app_reg_command(us_cmd_t cmd);
```

- `app_lcd.c`의 `app_lcd_hook_us_command()` 스텁 → `app_reg_command(cmd)` 호출(기존 mon trace 한 줄 유지).
- `app.c` 호출 순서(`app_lcd_tick` → `app_reg_tick`)상 같은 사이클 처리 — 추가 지연 없음.
- 다른 훅(`set_pot`/`comm_reconfigure`)은 **스텁 그대로**(범위 밖).

---

## 6. LCD 노출

- **`us_run_status` enum 확장**: `app_lcd.h`의 `enum { US_IDLE=0, US_RUNNING=1 }` → **`enum { US_IDLE=0, US_REMOTE=1, US_TOUCH=2, US_COMM=3 }`**(samd20 §2.2). `US_RUNNING`(=1)은 `US_REMOTE`(=1)와 값 충돌하므로 **제거** — 잔여 참조는 2곳뿐(`app_lcd.h` 정의 + `app_reg.c:96` 발행), 둘 다 본 슬라이스가 교체(app_reg FSM이 `US_TOUCH`/`US_IDLE` 발행). disp 게이트 `!= US_IDLE`(`app_lcd_disp.c:156`)는 그대로 유효.
- **ICON_RUN**(0x1152) 엣지 렌더: `app_lcd_disp.c`가 이전 running-ness(`prev_on`) 추적, `(us_run_status != US_IDLE)` 변화 시 `dgus_write_u16(ICON_RUN, on?1:0)` **1회**(매 4ms 스팸 방지). app_reg는 DGUS 미접근(계층 분리). init 클리어(`app_lcd.c:125`)는 유지.
- **VAR_POWER 숫자**(`disp_send_val`): `on?max_power:last_power` — 활성 중 peak-hold 상승, 정지 후 latch peak. = 램프 가시면(slice 2a §8.2가 확정한 "power 숫자" 표면).
- **출력 바 `LV_OUTPUT`**: `disp_compute_output(curr_amp,…)` = **amplitude 구동, 무변경**. 전압주입 없으면 idle(slice 2a §8.2 by-design). 본 슬라이스가 바를 sel로 거짓 구동하지 않음.
- energy/cycle/freq/amp-split = **0 유지**(weld-cycle deferred). `curr_amp=ch0_avg`(실측).

---

## 7. 통합 / 변경 파일

| 파일 | 변경 |
|------|------|
| `fw/include/app_reg.h` | `app_reg_command(us_cmd_t)` 선언 (`us_cmd_t`는 이미 include한 `app_lcd.h`에서 가시) |
| `fw/src/app_reg.c` | run FSM(us_run_status 소유, START/RELEASE 전환, max/last latch), 출력 MUX에 IDLE→sel=0, `app_reg_init` 부팅 IDLE, `app_reg_command` 구현; REG_TRACE에 명령/us_run_status 추가 |
| `fw/include/app_lcd.h` | `us_run_status` enum → 4-값 taxonomy(§6) |
| `fw/src/app_lcd.c` | `app_lcd_hook_us_command` 스텁 → `app_reg_command(cmd)` 라우팅(mon trace 유지) |
| `fw/src/app_lcd_disp.c` | ICON_RUN 엣지 렌더(`prev_on` 추적) 추가; disp_send_val max/last 동작은 measure 값 따라감(코드 무변경 가능) |

`main.c`/`app.c`/`board.c`/`adc1.*`/`app_reg_calc.*`/`fw/test/*` **무변경**(순수함수 추가 없음 — 게이트는 상태 전환만).

---

## 8. 테스트 / 검증

### 8.1 호스트 단위테스트
- 순수함수 추가 없음 → 기존 `reg_scale`/`reg_output_level`/`reg_ramp_level` 테스트 **무회귀 확인**만.
- (선택) run FSM 전환을 호스트에서 테스트하려면 `app_reg_command`+상태 조회가 필요하나, FSM이 `app_reg.c` static 상태에 묶여 있어 본 슬라이스는 **HW 검증 1차**(YAGNI — 순수함수 분리는 효과가 compute일 때만 가치). plan에서 재검토.

### 8.2 HW 검증 (REG_TRACE + LCD, 전압 주입 불필요)
- **부팅**: `us_run_status=IDLE`, `sel=0`, 출력 미구동(배너/LCD 네비 정상). slice 2a의 부팅 auto-ramp **없음**(= 의도된 변경, slice 2a 부팅램프 합격기준 대체).
- **RUN press**(패널): mon `[reg] cmd=START` → `st=1` `rc` 0→401 ~4s 단조 + `sel` 128→1024 + **ICON_RUN 점등** + VAR_POWER 숫자 peak 추종 상승 → ~4s 후 `st=0`(lookup, sel=reg_scale(ch0_avg)).
- **RUN release**: mon `[reg] cmd=RELEASE` → `us_run_status=IDLE` + **ICON_RUN 소등** + `sel=0` + VAR_POWER = last_power(latch peak).
- **re-arm**: 다시 RUN press → `max_power` 0 리셋 → VAR_POWER 0에서 재상승 + 램프 재발화(가시적).
- **무회귀**: 활성 lookup 경로 = slice-1 verbatim; OSC idle-HIGH(slice 1 6a) 유지; LCD 배너/네비.
- REG_TRACE에 `cmd`/`us_run_status` 추가.

---

## 9. 범위 밖 / deferred

- **SEEK/RESET regulation 효과**: SEEK=주파수 헌팅=OSC 구동(B-SEAM, HW-gated); RESET=error/overload 클리어인데 그 시스템이 deferred → 클리어 대상 없음(입력층이 이미 icon/page 복원). 본 슬라이스는 명령 수신 mon trace만.
- **overload 검출/플래그/ICON_OL + error_status FSM**: 후속(임계 calibration = 6b/B-SEAM).
- **weld-cycle FSM**(RUN_READY/CYL1/WELD/HOLD): 핸드 용접 시퀀스 + 물리 START 스위치 = Stage A I/O.
- **Modbus 명령소스**(US_COMM, H_REG_START/STOP): Stage C.
- **blink**: V30 7-seg 부재 → 대부분 dead.
- **OSC 물리 구동** + 정확한 패턴 바이트: B-SEAM.
- **6b 신호 calibration**: HW 준비 후.

---

## 10. flagged (코딩 차단 ✗, 주석)

- **부팅 IDLE 전환**이 통합 제품의 의도된 동작임(원 SAMD20이 M_START를 명령으로 게이트). slice 2a auto-run은 명시적 테스트 단순화였고 그 spec §10이 본 재검토를 예고함.
- `curr_power = sel`는 라벨된 setpoint 표면(실측 전력 = B3 전달함수, 6b 대체). advisor가 mirror 완전 폐기 옵션(검증을 ICON_RUN+us_run_status로만)을 제시했으나, 전압주입 없는 벤치에서 램프 가시성을 위해 setpoint 표면 유지 + max/last는 faithful로 — 사용자 승인(2026-06-08 brainstorming).
- 소스 arbitration(START `== US_IDLE` / RELEASE `== US_TOUCH`)은 단일 소스(TOUCH)뿐인 본 슬라이스에선 자명하나, 후속 REMOTE/COMM 소스가 끼어들 구조로 보존(§4.2 가드 분기 근거 포함).
- **ICON_RUN 엣지 재assert**: disp는 `us_run_status` *전환* 시에만 ICON_RUN을 write. 만약 run 중 페이지 rebuild가 ICON_RUN을 재클리어하면 엣지 트래커가 재assert 못 함. momentary hold-to-run에선 RUN 버튼 유지 중 페이지 네비 불가 → **본 슬라이스 unreachable**(코드 추가 ✗). 후속 latched/COMM run 도입 시 재검토.
