# 직접 energy 런 종료 (에너지-도달 정상정지 + OVTIME fault) — 설계

> **요약**: samd20의 energy 모드 직접-초음파 런 종료 쌍을 F410에 포팅한다 — (a) **에너지-도달 정상정지**(`curr_energy >= limit_energy`, samd20 main.c:5272)와 (b) **과대시간 fault ERR_OVTIME**(`us_on_time >= limit_out_time*10`, main.c:5288). 결정 로직은 순수 함수 `reg_energy_termination()`(`app_reg_calc.c`)로 분리해 **host-test**(6b 독립 — `curr_energy`는 주입값). 글루는 `app_reg_tick`에서 **energy 모드일 때 on-time ceiling을 대체**(legacy 충실)하고, OVTIME fault는 **이미 main에 완비됐으나 안 먹여지던 인프라**(`lcd_measure_t.error_status` → `app_lcd_show_error`/LCD_WARNING + `MB_STATUS_OVTIME`)에 먹인다. 복구는 기존 키-클리어 + app_reg의 START/RESET 자동 클리어. 실제 에너지 절대값·HW 표면 검증은 6b/보드 게이트.

## 1. 배경 / legacy 근거 (`ref/samd20/main.c`)

energy 모드 런 종료(5270–5294, `us_run_status`=COMM/REMOTE/WELD & `energy_ctrl`):
```c
if (curr_energy >= limit_energy) { ...정상 종료(WELD→HOLD, else us_off())... }   // 5272
else if (us_on_time >= (limit_out_time * 10)) {                                   // 5288
    us_off(); sys_status = SYS_ERROR; error_status |= ERR_OVTIME;                  // 5292 (유일한 OVTIME set)
}
```
- 모드 우선순위(5234 multi → 5270 energy → 5296 hand on-time): **energy 모드면 on-time ceiling(limit_on_time) 미적용** — energy/OVTIME가 지배.
- `us_on_time`: 100ms마다 +1(5380, max 200=20s). `limit_out_time`: 기본 10, **Modbus max 10 클램프**(4503), LCD 편집(3802), `ADDR_TIMEOVER` = 초 단위. → OVTIME 한계 = `limit_out_time*10`(100ms 단위) = **limit_out_time초**(최대 10s).
- 표시: ERR_OVTIME → LCD_WARNING 모달(ovtime_msg) + Modbus STATUS_OVTIME 비트. 복구: RESET이 `error_status=0`(3717).

## 2. F410 현 상태 (main, 본 작업 base)

- `app_reg_tick(limit_on_time)`의 직접 런 종료 = **on-time ceiling만**(TOUCH/COMM, `app_reg.c:264-284`). **에너지-도달 정상정지는 직접런에 미포팅**(weld 슬라이스2가 "energy exit weld-only"로 의도 이연 → energy_ctrl=ON 직접런도 ceiling 종료).
- `curr_energy`/`acc_energy` 누산 존재(`curr_power` 적분, `app_reg.c:209-210`; 벤치 무신호→0).
- **fault 인프라 완비·미공급**: `lcd_app_state_t.error_status`(ERR_* 비트마스크, app_lcd.h:83) + `app_lcd_show_error()`→LCD_WARNING(app_lcd_disp.c:277, "provider가 0이라 unreachable") + 키 복구(app_lcd_input.c:152 ERR_OVTIME 클리어) + `MB_STATUS_OVTIME=0x08`(app_modbus_core.h:51, 정의됨·미공급). → **본 작업이 이 인프라의 첫 공급자**.
- ⚠ **slice-D(미머지)가 "on-time ceiling 이중화"(절대30s+legacy hand)로 이 종료 블록을 개조** → 두 브랜치 머지 시 종료 블록 충돌 조정 필요(§8).

## 3. 목표 / 범위 (확정)

- **범위 = energy 종료 쌍 전체**(사용자 확정): 에너지-도달 정상정지 + OVTIME fault. OVTIME만 떼면 정상정지 없어 의미 약함.
- **§2 정책 = energy 모드가 on-time ceiling 대체**(사용자 확정, legacy 충실).
- **§4 fault 노출 = `lcd_measure_t.error_status`**(사용자 확정, app_reg가 fault source).
- **6b 독립**: 결정 로직은 시간/임계 비교뿐. 실 `curr_energy` 절대값과 LCD/Modbus 표면 시각 검증은 HW/6b 게이트.

## 4. 순수 decision (`app_reg_calc.{c,h}`, host-test 대상)

```c
typedef enum {
    REG_RUN_CONTINUE     = 0,
    REG_RUN_STOP_ENERGY  = 1,   /* 정상: 목표 에너지 도달 (5272) */
    REG_RUN_FAULT_OVTIME = 2    /* fault: 시간 내 미도달 (5288) */
} reg_energy_outcome_t;

#define OVTIME_SEC_MS  1000u    /* limit_out_time 단위 = 초 (legacy us_on_time*100ms * limit_out_time*10) */

reg_energy_outcome_t reg_energy_termination(
    uint8_t energy_ctrl, uint32_t curr_energy, uint32_t limit_energy,
    uint32_t elapsed_ms, uint16_t limit_out_time)
{
    if (!energy_ctrl)                 return REG_RUN_CONTINUE;       /* 비-energy → 호출측 on-time ceiling */
    if (curr_energy >= limit_energy)  return REG_RUN_STOP_ENERGY;    /* 정상 (5272) */
    /* legacy us_on_time>=limit_out_time*10 — 0=off 가드 없음(있으면 energy 모드가
     * ceiling 대체라 limit_out_time=0이 never-stop, advisor). 0→즉시 OVTIME. */
    if (elapsed_ms >= (uint32_t)limit_out_time * OVTIME_SEC_MS)
        return REG_RUN_FAULT_OVTIME;                                /* fault (5288) */
    return REG_RUN_CONTINUE;
}
```
- HAL 비의존(순수). `curr_energy`/`elapsed_ms` 주입 → host-test 완결.
- **단위 환산**: legacy `limit_out_time*10`(100ms 단위) = `limit_out_time` 초. F410 `elapsed_ms = now - run_start_ms`와 직접 비교: `elapsed_ms >= limit_out_time*1000`.

## 5. 글루 (`app_reg_tick`) — energy 모드가 on-time ceiling 대체

기존 ceiling 블록(264-284)을 energy_ctrl로 분기:
```
if (energy_ctrl) {
    outcome = reg_energy_termination(energy_ctrl, g_measure.curr_energy, limit_energy,
                                     now - run_start_ms, limit_out_time);
    REG_RUN_STOP_ENERGY  → 정상 stop: last_* 래치 + us_run_status=US_IDLE  (ceiling stop과 동일 경로)
    REG_RUN_FAULT_OVTIME → stop(동일) + g_reg.error_status |= ERR_OVTIME
} else {
    기존 on-time ceiling (TOUCH/COMM, limit_on_time) — 변경 없음
}
```
- **TOUCH/COMM 한정**(US_CYCLE=weld는 제외 — 기존과 동일). 정상정지·fault 모두 출력 차단은 현 ceiling stop 경로(US_IDLE → publish 시 setpoint 0 + hook off)와 같음.
- energy 모드에선 on-time ceiling 미평가(legacy 충실).

## 6. 파라미터 주입 (`reg_run_limits_t`)

`app_reg_tick`이 현재 `limit_on_time`만 받음. energy 경로에 `energy_ctrl/limit_energy/limit_out_time` 추가 필요 → **작은 구조 주입**(M1 패턴: app_lcd 콜백 없이 매 tick cfg에서 주입, 라이브 편집 반영):
```c
typedef struct { uint16_t limit_on_time; uint8_t energy_ctrl; uint32_t limit_energy; uint16_t limit_out_time; } reg_run_limits_t;
void app_reg_tick(const reg_run_limits_t *lim);
```
호출측(app.c:94) `app_lcd_cfg()`에서 채워 전달.

## 7. Fault surfacing + 복구 (기존 인프라 공급, 신규 모듈 0)

- **노출**: `lcd_measure_t`에 `uint8_t error_status` 필드 추가 — app_reg가 publish(`reg_publish_measure`). OVTIME 시 ERR_OVTIME 세팅.
- **LCD**: app_lcd가 measure.error_status를 state->error_status로 받아 `app_lcd_show_error()`→LCD_WARNING(기존 구현 부활).
- **Modbus**: app_modbus STATUS 빌드(app_modbus.c:84-86)에 `(measure.error_status & ERR_OVTIME) → | MB_STATUS_OVTIME` 추가.
- **복구** (legacy 충실):
  - **source(app_reg) 자동 클리어**: START(US_IDLE→런, app_reg_command) + RESET 명령(US_CMD_RESET)에서 `g_reg.error_status &= ~ERR_OVTIME`.
  - ⚠ **핵심 배선점**: 기존 LCD 키-클리어(app_lcd_input.c:152 `error_status=0`)는 표시 카피만 지움 → app_reg가 매 publish 재공급하면 재점등. **키-클리어가 app_reg source도 클리어하도록 라우팅**(RESET 명령 경유) 필요. 미배선 시 RESET이 안 먹힘.

## 8. 범위 밖 / 이연

- **에너지-도달 정상정지의 실 `curr_energy` 절대값** = 6b(벤치 무신호로 항상 0 → energy 모드 직접런은 벤치선 항상 OVTIME으로 끝남, 정상). 로직은 host.
- **slice-D ceiling 이중화와 머지 조정** — 같은 종료 블록 개조. 머지 시 energy 분기 + 절대30s + legacy hand ceiling 통합.
- **#7 ERR_OUTERR = 6b calibration 스테이지 하위 항목으로 명시**(NEXT_STEPS §1.2 갱신; legacy에서 트리거 주석처리=비활성, `curr_amp`(ADC) 의존 → 6b 종속).
- multi 모드 종료(이미 weld 슬라이스3), WELD energy exit(weld 슬라이스2)은 본 작업 무관.

## 9. 테스트

- **host (신규 스위트 `test_reg_energy_term` 또는 reg_calc 편입)**: `reg_energy_termination` — `energy_ctrl=0`→CONTINUE / `curr_energy>=limit_energy`→STOP_ENERGY / 미달+`elapsed<thr`→CONTINUE / 미달+`elapsed>=thr`→FAULT_OVTIME / `limit_out_time=0`→즉시 FAULT_OVTIME(never-stop 방지) / 단 0이어도 STOP_ENERGY 우선 / 경계(`elapsed==limit_out_time*1000`).
- **HW-gated**: energy 모드 직접런 OVTIME → LCD_WARNING + MB_STATUS_OVTIME(mbpoll) + RESET 복구; 에너지-도달 정상정지(실 curr_energy=6b 후).

## 10. 구현 단위 요약 (plan 입력)

| 파일 | 변경 | host-test |
|---|---|---|
| `fw/include/app_reg_calc.h` | `reg_energy_outcome_t` enum + `reg_energy_termination` proto + `OVTIME_SEC_MS` | ✓ |
| `fw/src/app_reg_calc.c` | `reg_energy_termination` 순수 함수 | ✓ |
| `fw/include/app_reg.h` | `app_reg_tick` 시그니처 → `reg_run_limits_t*`; `lcd_measure_t`에 `error_status` 필드; app_reg fault 클리어 경로 | — |
| `fw/src/app_reg.c` | tick에 energy 분기(ceiling 대체) + OVTIME→error_status set + START/RESET 클리어 + publish error_status | ✗ |
| `fw/src/app.c` | `app_reg_tick` 호출을 `reg_run_limits_t` 주입으로 | ✗ |
| `fw/src/app_lcd*.c` | measure.error_status → state->error_status → show_error; 키-클리어 → app_reg RESET 라우팅 | ✗ |
| `fw/src/app_modbus.c` | STATUS에 MB_STATUS_OVTIME OR | ✗ |
| `fw/test/test_reg_energy_term.*` (또는 reg_calc) | `reg_energy_termination` 경계 | ✓ |
| `docs/NEXT_STEPS.md` | §1.2 6b에 #7 ERR_OUTERR 하위항목 명시 | — |
