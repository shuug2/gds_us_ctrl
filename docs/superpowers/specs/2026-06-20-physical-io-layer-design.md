# 물리 IO 레이어 설계 (OSC 발진단 제외) — 2026-06-20

> **요약**: 부록 D(SAMD20→STM32 레거시 IO 대응)의 커넥터/패널 IO를 legacy SAMD20 동작을 기준으로 STM32 펌웨어에 구현한다. **OSC 발진 3채널(PB2/PB10/PB14)과 진폭 pot(I2C_POT)만 stub 유지**(B-SEAM), 그 외 물리 입력(START/RESET/SEEK/KEY/센서/E-stop/overload)·출력(USOUT/SOL_DN/OVLD 릴레이/부저)·FREQ_IN 주파수 측정을 전부 구현한다. 단일 spec, **5개 슬라이스**로 단계 구현(슬라이스마다 빌드+호스트테스트, HW 검증은 묶어서). 명령 흐름 = 물리 버튼은 `US_REMOTE` 소스로 **통일 strict** 중재(US_IDLE에서만 시작, source-matched 정지).

---

## 1. 목표 / 범위

### 1.1 In scope
- 모든 커넥터 GPIO 설정(입력 pull-up / 출력 안전 idle) + raw read/write 드라이버
- 물리 입력 → 명령/사이클 통합: B_START·B_RESET·B_SEEK(US_REMOTE), SW_START1+2(양손 weld 트리거), SENSE_UP/DN(센서), SW_EMSW(E-stop)
- 출력 실구동: USOUT(PB4), SOL_DN(PB5), CON_OVLD 릴레이(PB3), 부저(PA2)
- Overload: PB13 입력 → 정지/에러/LCD + PB3 릴레이 + seek-reset 복구 재사용
- FREQ_IN(PA0): TIM5_CH1 입력 캡처 → 주파수 측정 → DISP_FREQ(LCD + Modbus)

### 1.2 Out of scope (stub 유지 — B-SEAM/6b 이연)
- OSC 발진 3채널 구동: PB2(SEEK)/PB10(RESET)/PB14(RUN) — `app_seek_reset_hook_signal` stub 유지
- 진폭 pot 실구동: I2C_POT(U4) — `app_lcd_hook_set_pot`/`app_weld_hook_set_amp` stub 유지
- 부재 상태 의존 legacy UX: HORN 모드(`sys_status==SYS_HORN`), RESET 시 model_type 페이지 전환, "SENSOR ON/OFF" LCD 텍스트 — 명시적 이연

### 1.3 출처
- legacy 거동: `ref/samd20/main.c`(입력 처리 1187-1434, 부저 243-251, FREQ 173-179/4150, SOL 1415+, 출력 1663/4186/4304)
- 핀/극성: `docs/pinmap.md` 부록 D + 상단 2026-06-20 정정 + overload(PB13 active-H, M16 disasm ×5 디바운스)

---

## 2. 활성 레벨 (legacy + pinmap 근거; HW sanity는 후속)

| STM32 | Net | 방향 | 활성 | legacy 근거 |
|---|---|---|---|---|
| PA15 | CON_START | IN | LOW(눌림=0) | `B_START` re_start==0 |
| PC10 | CON_RESET | IN | LOW | `B_RESET` re_reset==0 |
| PC11 | CON_ESTOP / SEEK | IN | **이중역할** | EMSW active-HIGH(re_emsw==1→estop) / model_type=hand·multi이면 `B_SEEK` active-LOW |
| PC12 | CON_KEY1 | IN | LOW | `SW_START1` re_start1==0 |
| PB11 | CON_KEY2 | IN | LOW | `SW_START2` re_start2==0 |
| PA11 | CON_SENS_DN | IN | LOW | `SENSE_DN` re_dn==0 |
| PA12 | CON_SENS_UP | IN | LOW | `SENSE_UP` re_up==0 |
| PB13 | overload | IN | HIGH | M16 PA7 HIGH ×5 디바운스 |
| PB4 | CON_USOUT | OUT | HIGH | `B_USOUT` CTRL_ON=1 |
| PB5 | CON_SOL_DN | OUT | LOW | `SOL_DN` SOL_ON=0 |
| PB3 | CON_OVLD 릴레이 | OUT | HIGH | `B_OVLD` CTRL_ON=1 |
| PA2 | BUZZER | OUT | HIGH | `BUZZER` BUZZER_ON=1 |

> **PC11 이중역할**: legacy `main.c:1189-1198`이 `sys_mode`로 분기. STM32는 `cfg->model_type`(0=hand/1=multi → SEEK active-LOW, 그 외 → EMSW active-HIGH)로 재현. 슬라이스 D에서 처리.

---

## 3. 아키텍처 / 모듈

```
drivers/io.{c,h}        ← 모든 커넥터 GPIO config + raw read/write 헬퍼 (HW 레이어)
src/app_input.c         ← 10ms 물리 입력 스캔: edge-detect/debounce → 명령 디스패치 (글루)
src/app_input_fsm.c     ← 순수 edge/debounce 로직 (HAL-free, host-test)
src/app_buzzer.c        ← 비블로킹 timed-beep 상태기 (io_buzzer 구동)
(기존) app_reg.c        ← USOUT run-output hook 추가; US_REMOTE는 통일 strict로 자연 수용
(기존) app_weld.c       ← app_weld_hook_sol_dn → io_sol_dn 배선
(기존) app_seek_reset   ← overload 복구가 app_seek_reset_request 재사용
src/app_overload.c      ← PB13 디바운스 → 정지/에러/LCD/릴레이/비프 (+_fsm host-test)
board.c                 ← heartbeat(PB3) 제거(dormant, 호출처 없음); PB3 → CON_OVLD로 해방. OSC idle 유지
```

원칙: HW 접근은 `drivers/io`로 격리, 순수 로직(edge/debounce/overload 디바운스)은 `*_fsm.c`로 분리해 host-test. 기존 hook stub(SOL_DN/USOUT)을 실 GPIO로 배선하되 제어 로직(FSM/레귤레이션)은 무변경.

### 3.1 명령 흐름 통합 (US_REMOTE 통일 strict)
물리 버튼 → `app_reg_command(cmd, US_REMOTE)`. 기존 `app_reg_command`의 START 가드 `==US_IDLE`가 REMOTE에도 그대로 적용되므로 **코드 변경 거의 없음**(US_REMOTE는 이미 enum에 존재, src로 저장됨). RUN_RELEASE는 기존 source-matched 로직이 US_REMOTE도 처리. on-time ceiling은 현재 TOUCH/COMM만 대상 → **REMOTE도 ceiling 대상에 포함**(슬라이스 D, 안전).

---

## 4. 슬라이스 (구현·검증 단위)

### 슬라이스 A — GPIO 인프라 + 출력 미러 (최저 리스크)
- `drivers/io.{c,h}` 신설: 전 커넥터 핀 config(§2 극성/방향), raw `io_read_*`/`io_write_*` 헬퍼.
- **heartbeat 정리**: board.c의 PB3 heartbeat 제거(dormant, 호출처 0) — PB3을 CON_OVLD용으로 해방. (필요 시 PB8로 이전하나 호출처 없으므로 retire 권장.)
- **USOUT(PB4)**: `us_run_status` idle↔active 전이에 run-output hook(app_reg) → `io_usout(on)` active-HIGH. legacy `main.c:4186/4304`.
- **SOL_DN(PB5)**: `app_weld_hook_sol_dn(on)` → `io_sol_dn(on)` active-LOW (현재 mon_printf만).
- **부저 드라이버**: `app_buzzer` 비블로킹 timed-beep(`io_buzzer`). 트리거는 슬라이스 C/D에서 배선(여기선 API+드라이버만).
- host-test: io 헬퍼는 HAL 의존이라 host 제외; USOUT/SOL_DN 전이 로직은 기존 host 스위트에서 hook 호출 관찰. 부저 timed-beep FSM은 host-test.
- HW: USOUT가 run 연동 토글, SOL_DN이 weld 사이클에 토글, 부저 비프 — 스코프/LED 또는 멀티미터.

### 슬라이스 B — FREQ_IN 주파수 측정 (독립 추가)
- `drivers/io` 또는 `drivers/tim_ic.c`: TIM5_CH1 입력 캡처 init(PA0, 라이징 엣지, 32-bit). TIM5 클럭 = APB1 타이머 96 MHz.
- 주기 캡처 Δ → `freq = 96e6 / Δcount`. legacy 충실: 10샘플 링버퍼 평균(`freq_buf[10]`, `main.c:173-179/4150`).
- 출력: `curr_freq` → LCD DISP_FREQ VP + Modbus `MB_REG_DISP_FREQ`(0x04) 미러.
- ⚠ 표시 단위 스케일은 legacy `curr_freq` 의미에 맞춰 HW에서 sanity(예상 15~40 kHz). 무신호 시 0/타임아웃 처리.
- host-test: 평균/변환 순수 함수(캡처 카운트 입력 → 표시값) 검증.
- HW: 실 초음파 신호 필요 → **부분 HW-gated**(로직은 host, 절대값은 rig).

### 슬라이스 C — Overload (PB13 → 응답 + 릴레이)
- `app_overload`(+`_fsm`): PB13 active-HIGH **×5 연속 디바운스**(M16 disasm) → overload assert.
- 응답(legacy SAMD20): US 정지(전 소스 RUN_RELEASE) + 에러 상태 ERR_OVLD + LCD ICON_OL + 부저 + **복구 = `app_seek_reset_request(US_CMD_RESET)`**(RESET→SEEK 자동 체인 재사용).
- 출력: `io_ovld_relay(on)` PB3 active-HIGH.
- 해제: overload 입력 clear + (RESET 명령/복구 체인 완료) → 에러/릴레이 클리어. Modbus STATUS OVLD bit(0x04) 반영.
- host-test: 디바운스 카운터 + assert/clear 전이 FSM.
- HW: PB13에 신호 인가 → 정지+ICON_OL+릴레이+복구 체인 관찰.

### 슬라이스 D — 물리 명령 입력 + E-stop
- `app_input`(+`_fsm`): 10ms 스캔, 단일 edge-detect(`_bak` 패턴).
- **B_START(PA15)** → `app_reg_command(US_CMD_START/RUN_RELEASE, US_REMOTE)` (눌림=START, 뗌=RELEASE; 통일 strict).
- **B_RESET(PC10)** → `app_reg_command(US_CMD_RESET, US_REMOTE)`.
- **PC11**: `model_type` hand/multi → **B_SEEK** active-LOW → `US_CMD_SEEK`; 그 외 → **EMSW** active-HIGH → E-stop.
- **E-stop**: 전 소스 런 강제 정지 + `io_sol_dn(off)` + estop 플래그(해제까지 신규 START 차단) + Modbus STATUS ESTOP bit(0x02). 풀 sys_status/HORN UX 이연.
- on-time ceiling 대상에 US_REMOTE 추가(app_reg).
- host-test: edge-detect + estop 게이팅 FSM.
- HW: 각 버튼 → 명령 효과(STATUS/ICON), E-stop → 즉시 정지+차단.

### 슬라이스 E — weld 물리 트리거 (기존 weld 슬라이스4 본체)
- **양손 SW_START1(PC12)+SW_START2(PB11)** 동시 눌림 + READY → `app_weld_request_start()`(현재 호출처 0).
- **센서 SENSE_UP/DN(PA12/PA11)**: 사이클 위치/TRIGGER 모드 연동(legacy `main.c:1219-1268`). "SENSOR ON/OFF" 텍스트는 이연.
- **must-fix(기존 메모 LOW-1)**: LCD `app_lcd_input.c:752` LV_OUT_POWER `[50,100]` 클램프(`output_power<50` 진폭 언더플로 방지; Modbus는 이미 클램프).
- SOL_DN 실구동은 슬라이스 A 완료분 재사용.
- host-test: 양손 동시성 + 사이클 트리거 게이팅.
- HW: 양손 START → 사이클(CYL1→WELD→HOLD→CYL2→work_cnt++) + SOL_DN 물리 동작.

---

## 5. 핵심 결정 (확정)

| 항목 | 결정 |
|---|---|
| REMOTE 중재 | **통일 strict** — US_IDLE에서만 시작, source-matched 정지 (사용자 확정) |
| 부저 | legacy 충실 = **GPIO on/off**(능동 부저) + 비블로킹 timed-beep. ⚠ 수동 부저면 TIM9 PWM 톤 전환 — **HW 확인 항목** |
| E-stop | 전 소스 정지 + SOL_DN OFF + estop 플래그 + STATUS ESTOP bit; 풀 HORN/sys_status UX 이연 |
| 디바운스 | 10ms 스캔 + 단일 edge; overload만 ×5 연속(disasm) |
| 충실도 | 코어 거동만 이식; 부재 상태 의존 UX(HORN/페이지전환/센서텍스트) 이연 |
| PC11 이중역할 | `model_type`로 SEEK(hand/multi) vs EMSW 분기 |

---

## 6. 미해결 HW 확인 항목 (코드는 legacy 기준 선구현, sanity는 후속)
1. 부저 타입(능동 on/off vs 수동 PWM 톤)
2. 전 핀 활성 극성 sanity(특히 PB13 overload, PC11 EMSW NC)
3. FREQ_IN 표시 단위 스케일(실 초음파 rig)
4. PC11 EMSW/SEEK 이중역할 물리 배선 확인

---

## 7. 검증 전략
- **슬라이스마다**: 빌드 0-warning + host-test 스위트(순수 로직) + cpp-reviewer.
- **HW 묶음 검증**: 슬라이스 A/C/D/E는 보드에서 입출력 관찰(STATUS/ICON/스코프). 슬라이스 B·OSC 효과는 실 초음파 rig 필요분만 분리(부분 HW-gated).
- 회귀: 직접-초음파(LCD/Modbus) ceiling·ICON_RUN 무회귀 매 슬라이스 확인.
