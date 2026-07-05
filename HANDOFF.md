# Handoff: 2026-07-05 벤치 세션 — weld 사이클 E2E 전항목 PASS + 벤치 수정 6커밋 (클럭 HSE·전류 표시 실동작)

**Generated**: 2026-07-05 (세션 마감, 풀배선 벤치)
**Branch**: `main` tip `3d2f414` (+이 docs 커밋) — **⚠ push 미실행** (사람 터미널에서 `git push origin main`)
**Status**: weld 사이클 E2E 종결. HW-gated 백로그 대폭 축소. 다음 = B-SEAM 잔여/6b 잔여/overload 실동작/HMI Task 8/M6-M8-M9 중 택일.

> **요약**: 사용자가 양손/센서/EMSW 배선을 완료한 풀배선 벤치에서 ⑴ **spec §7.3 weld 사이클 E2E 전항목 PASS**(1~6·8+SETUP 게이트; EMSW 해제-추종 d' 이월분 포함 종결) ⑵ 벤치 발견 결함/갭 **6건 즉석 수정→cpp-review(전건 APPROVE)→플래시→재검증**: SENSOR ON/OFF 동적 갱신(`b19823a`) / E-stop 경고 페이지+복귀 가드(`cbbfe19`, 리뷰 HIGH 1건 반영) / E-stop 부저+overload 아이콘-only(`c3b3f27`) / **RESET/SEEK 물리 OSC 구동**(`29803ae`) / **클럭 HSI→HSE**(`e72dbe4`) / **소비전류 표시 실동작**(`3d2f414`). ⑶ 세션 개시 때 보드에 **구형(stage-b~d) 펌웨어**가 실려 있던 것 발견·재플래시(양손 무반응 근본 원인).

## 핵심 성과 (다음 세션에 영향 주는 것)

1. **클럭 = HSE(16MHz X-tal) PLL 96MHz** (`fw/src/clock.c`, HSI 폴백+진단 플래그) — HSI +1.39% 편차로 인한 주파수 표시 underread와 전 타이밍 -1.3%가 **원천 제거**. `freq_cal_val=0 유지가 정답** (구 HSI 보정값 넣지 말 것). HSE_VALUE=16MHz는 CMake 주입.
2. **B-SEAM 최대 미지수 해소**: RESET/SEEK = 단순 active-LOW 레벨 미러(600ms 레그)를 실구동으로 실증 — SEEK 중 FREQ_IN이 34115→34508Hz 스윕-정착 = **스윕 주체는 OSC 보드측**. B-SEAM 잔여 = 스코프 파형 정밀 관측(폴라리티 sanity), PB12 용도(출력 구동 금지 유지), 진폭 추종.
3. **전류/W/에너지 표시 체계 확립**: PB1(ADC_CURR) 신호는 600mA→~15mV 초소신호로 정상 도달. ch1 취득=raw 12-bit×6(legacy 2.23V/4×누산 도메인 정합) + GAIN 7/5 **rig-fit**(앵커 600mA↔표시 0.60A) + EMA(τ≈400ms) + 표시=런 중 max 피크/정지 후 last 유지(legacy 4167/4191). 미세 트림=LCD CAL(cal_val). 정밀/다점 보정=6b.
4. **주파수 표시 = 스코프 대비 0.01%** (34984 vs 34980Hz, 보정 0).

## Not Yet Done (백로그)

- [ ] **B-SEAM 잔여** — 위 1항 참조 (준비물: 절연/차동 프로브. 매핑/스윕 주체/전기설정은 전부 해소됨)
- [ ] **6b 잔여** — 전류 정밀/다점 보정, ch0(레귤레이션) 도메인·물리단위, weld energy 절대 E2E+divisor(250), OUTERR 포팅(트리거 주석 해제+실 curr_amp)
- [ ] **[리뷰 MEDIUM, `3d2f414`] EMA↔에너지 적분 디커플링** — 표시용 EMA(τ400ms, 런-시작 리셋 없음)가 acc_energy→energy 종료 판정에도 물림 → 런 초기 언더리딩으로 energy-exit 지연 가능(weld 레그 ≤500ms와 동급). 권고=에너지 적분은 비필터 커밋 평균, EMA는 표시 전용으로 분리. **6b 에너지 절대 E2E와 함께 처리**(실신호 게이트 — 현 벤치 검증은 display-only 의도 충족, 명시 이연 결정 2026-07-05)
- [ ] **overload 실동작** — PB13 실신호 E2E (인프라 완비) + 리뷰 노트 LOW: `handle_key_multi` RESET의 OVLD/OUTERR 비트 테스트 휘발성(app_lcd_tick 미러 — 아이콘 클리어 누락 가능, 후속)
- [ ] **[HW 불요] modbus-tcp-hardening (M6/M8/M9)** — HMI 트리거 발화 유지, "RS-485 첫 write 간헐 무효" FW 원인 조사 포함
- [ ] **HMI SP1 Task 8** — gds_us_hmi 폴더 세션 (RS-485 어댑터 필요)
- [ ] (미세) clock 폴백 시 HSEON 잔류 정리 원라이너(리뷰 LOW) / seek_reset FSM default 분기 물리 스테이크 주석(리뷰 노트)

## Warnings / 벤치 노트 (다음 벤치 세션 필독)

1. **벤치 첫 단계 = 플래시↔ELF 대조**: `openocd read_memory 0x08000000` 벡터 vs 빌드 ELF — 이번 세션 양손 무반응의 근본 원인이 구형 펌웨어였음 (mbpoll류 증상 디버깅 전에 반드시).
2. **SWD 주소는 빌드마다 이동**: read 전 `arm-none-eabi-gdb -batch -q <elf> -ex "p/x &심볼"` 재확보. 중간에 stale 주소로 오판독 1회 발생.
3. **비침습 샘플러 확립**: `openocd -c 'init; for {...} { read_memory ... }; shutdown'` — halt 없이 ~1.4ms/샘플. 타이밍 실측/스윕 관측/캘리브레이션의 핵심 도구 (§7.3-8, seek 스윕, 전류 앵커 전부 이걸로).
4. **LCD 터치+B_START 웨지 1회** — 터치 프레임 자체가 미송신(RX는 write-ACK만) → 물리 전원사이클로 해소. 재발 시 같은 처방.
5. RX drop 카운터(dgus)는 write-ACK(len 3<min)를 세는 회계 — 큰 수치가 정상 (오진 주의).
6. 보드 **테스트 잔재 설정** 남음 — NEXT_STEPS §2.3-a 목록 참조, 운영 투입 전 복원.
7. 기존 벤치 함정 승계: mbpoll -r 1-based, START/STOP 별도 레지스터, 첫 write 간헐 무효, SWD halt 금지.

## Current State

- **FW main**: `3d2f414`(+docs), working tree clean 기대, **push 미실행**. 태그 신규 없음(벤치 수정은 main 직접 커밋 관례).
- **보드**: main `3d2f414` 플래시, 풀배선 리그(양손/센서/EMSW/전류 sense), RS-485 미접속, 테스트 잔재 설정.
- **리뷰**: 6커밋 전건 cpp-reviewer 통과 — 5건 APPROVE + `3d2f414` APPROVE-WITH-COMMENTS(MEDIUM 1=EMA↔에너지 적분 커플링, 위 백로그로 명시 이연; LOW=gain 단독으론 앵커 6카운트 아래 — cal_val 트림으로 흡수, 벤치 표시 0.6A 확인됨).

## Resume Instructions

- FW 벤치(스코프 있음) → **B-SEAM 잔여**(파형 관측) 또는 **overload 실동작**.
- FW 벤치(전류계) → **6b 전류 다점 보정** (여러 OUT_POWER에서 전류계↔표시 대조, gain/cal 정련).
- HW 없음 → **modbus-tcp-hardening** brainstorming (NEXT_STEPS §3 절차).
- HMI → gds_us_hmi 폴더 + 그쪽 HANDOFF.md (RS-485 연결).
