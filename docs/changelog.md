# Changelog

## [Unreleased]

### 2026-06-02 — Stage D slice 1 HW 기능검증(6a) PASS + Task 6 분리(6b calibration HW-gated)

실보드 HW 검증 진행. 전압 가변 소스 부재로 Task 6을 **6a(기능/구조, 지금 가능)** + **6b(신호 calibration, HW 준비 후)** 로 분리(사용자 결정 = measure-first). 6a 전체 PASS → slice 1(compute, 출력 deferred)은 설계대로 통합 준비 완료. compute 수식은 호스트 단위테스트가 전 범위 커버.

- **트레이스 빌드 명령 정정** (`879f43e`): plan Task 6 / RESUME의 `-DCMAKE_C_FLAGS="-DREG_TRACE"`가 캐시를 덮어써 툴체인 `CMAKE_C_FLAGS_INIT`의 CPU 플래그(-mcpu/-mthumb/-mfpu/-mfloat-abi)를 날려 ARM-mode 빌드 실패 → CPU 플래그를 함께 전달하도록 정정. 트레이스 ELF 0-warning, FLASH 28.45%/RAM 10.57%.
- **6a 기능/구조 검증 PASS** (REG_TRACE 빌드, 사용자+보드):
  - 6a-1 compute liveness: `[reg] ch0/ch1/scaled/band` ~500ms cadence. floating(신호 미연결) 시 ch0=3~5→scaled=18/24/30(**×6 라이브 확인**), 실 SENS_OUT 연결 후 ch0=0→**scaled=0(floor SCALE-05)→band=21(no-match off)** = scale 2분기+lookup off 경로 라이브 확인.
  - 6a-2 무회귀: `[boot]/[lcd] ready/[cfg]` 배너 정상 + LCD 네비게이션 락업 없음.
  - 6a-3 OSC 안전: **PB2/PB10/PB14 idle-HIGH 스코프 확인**(active-LOW=off, C6), OSC 무구동, PB12/PB13 미구동.
  - 6a-4 LCD provider live: idle scaled=0 → **VAR_POWER=0 표시 정상**(provider 라이브, garbage/stuck 아님).
- **6b 신호 calibration (DEFERRED, HW 준비 후)**: ① `>>2` 12→10bit 정규화 + 2.56V↔3.3V 레퍼런스 도메인 보정(DP2 first-cut→실측) ② ch0_avg/adc_scaled_value 물리단위 바인딩(B-UNITS/B3) ③ ADC offset/gain ④ OSC 출력 경로·비트매핑·극성(B-OSC-MAP/B-SEAM) + PB12/PB13 방향 확정. 전압 가변 + 실 초음파 구동 필요.
- **설명 문서 신규** (`docs/superpowers/analysis/2026-06-02-m16-to-stm32-port-explained.md`): M16→STM32 포팅을 compute(알고리즘) + I/O(핀) 두 축으로 정리한 핸드오프/teaching 문서(권위 소스 아님, 참조 명시).

### 2026-06-01 — Stage D slice 1 구현 완료 (레귤레이션 코어 compute, 출력단 DEFERRED) — HW 검증 대기

브랜치 **`feat/stage-d-regulation-core`** (main 미머지). spec(`…/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`) → plan(`…/plans/2026-05-31-stage-d-slice1-regulation-core.md`) → inline 구현. **compute 파이프라인만**(OSC 물리 구동 = B-SEAM, 벤치 측정까지 DEFERRED). 빌드 0-warning, 호스트 단위테스트 PASS, **cpp-reviewer APPROVED**(차단 0). **남은 것 = 실보드 HW 검증(REG_TRACE) → 머지/태그.**

- **신규 `app_reg_calc.{c,h}` (HAL-free, 호스트 테스트)**: `reg_scale`(SCALE-04/05/06: `in≥1000→1000` / `in<3→0` / else `in×6`, 의도적 불연속) + `reg_output_level`(C2: 21엔트리 strictly-less lookup, no-match→21) + `reg_lookup_table[24]`(§4.3 byte-exact). 순수함수 분리 = spec §12 호스트 TDD를 실제 실행 가능하게(레포에 테스트 하네스 부재 → `fw/test/Makefile` + assert 하네스 신설). TDD RED→GREEN.
- **신규 `adc1.{c,h}` 드라이버**: ADC1 init(12-bit, SW 트리거, 폴링) + `adc1_read(ch)` raw 12-bit; semantic 채널 enum→ADC_CHANNEL_8/9(PB0/PB1). cpp-reviewer L1/L2: `HAL_ADC_Start`/`PollForConversion` 실패 시 `Error_Handler`(타임아웃 스테일 샘플 누산 방지, 드라이버 기존 체크와 일관).
- **신규 `app_reg.{c,h}` 상태 모듈**: `app_reg_tick()` 2ms `sys_tick` 델타 게이트 → ch0/ch1 교대 1변환/틱, `>>2` 정규화, mean-of-10/50 누산·커밋 → 최신 `ch0_avg`에 scale+lookup → `lcd_measure_t` 발행(curr_amp=ch0_avg, curr/max/last_power=adc_scaled_value; freq/energy/status=0). OSC GPIO 미구동.
- **통합**: `main.c` `app_reg_init()`(app_init 뒤) / `app.c` `app_reg_tick()`(슈퍼루프 step 3) / `app_lcd.c` `app_lcd_measure()`→`app_reg_measure()`(스텁 라이브화).
- **board.c 안전수정**: `CTRL_OSC_OUT_PINS = PB2|PB10|PB14`(옛 마스크 PB10|PB12|PB13|PB14|PB15 정정 — PB2 누락·PB15 spurious) + 확정 3채널 idle-HIGH(active-LOW=off, C6); PB12/PB13(OSC2/3←PC4/PA7, 이미지상 입력 C7)은 미설정(B-OSC-MAP 측정 대기).
- **빌드**: `CMakeLists` + `stm32f4xx_hal_adc.c`, vendor include `SYSTEM`(LL ADC 헤더 `-Wunused-parameter`가 앱 0-warning 게이트 깨던 것 차단); `hal_conf.h` `HAL_ADC_MODULE_ENABLED`(앱레벨 config, I2C/TIM 선례 동일). **FLASH 26.86→28.39% / RAM 10.16→10.55%**.
- **의도적 spec 정제 3건**: ①순수함수 `app_reg_calc`로 분리(호스트 테스트). ②`adc1`은 얇은 페리페럴 계층(정규화/누산은 app_reg). ③`reg_output_level`은 band index만 반환 — band→thermometer 패턴바이트 맵(C-LADDER)은 출력단(B-SEAM)과 함께 DEFERRED(slice 1에서 미표시·미구동 = YAGNI, pure-add로 후속 가능).
- **다음**: Task 6 HW 검증(`-DREG_TRACE` 빌드 → PB0 주입 → ch0/scaled/band 추종 + LCD bar 확인 + PB2/10/14 idle-HIGH + PB15 NC 확인) → 머지/PR + 태그.

### 2026-05-31 (후속) — Stage D slice 1 분석 완료 + spec 초안 (레귤레이션 코어, 코드 변경 ✗)

ATmega16 레귤레이션 코어를 STM32F410으로 흡수하는 Stage D slice 1 착수. disasm-adjudicated 다중소스 분석(55-agent workflow, 3.7M tokens) → spec 초안. **코드 변경 없음**(분석+설계 단계, spec 리뷰 대기).

- **검증된 분석** (`docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md`): raw disasm(`M16_reverse/out/01_disassembly.asm`)을 ground-truth로 v001 디컴파일·재구성본을 교차검증(두 C파일은 hypothesis, "N소스 합의" 추론 금지). 29 register-fact(bucket A) + 7 open decision(bucket B). **v001 5건 정정**: ①스케일링 `>>6`→**×6 곱셈**(`1ac2 ldi r30,0x06`+`call 0x2158`, 시프트 명령 0개; 자기일관: `>>6`은 1000 clip 도달 불가) ②free-run→**단발 SW re-arm**(ADATE=0) ③Timer1 0xB1FF→**0xFFB1** ④램프 6단→**10단** ⑤compare `>`→**`<`**(strictly-less). 검증 파이프라인(SRAM→SRAM, ~2.05ms Timer0 틱): 2ch 단발 ADC(ch0÷10@0x01B9/ch1÷50@0x018D) → clip(≥1000)/floor(<3)/×6 → 21엔트리 내림차순 lookup thermometer → 패턴. advisor 최종검증: bucket-B 미승격 확인(C9 OSC identity는 register_fact 태그였으나 bucket B로 강등). 메모리 `project_m16_regulation_verified`.
- **핵심 미확정 B-SEAM**: 레귤레이션 패턴이 OSC 5핀에 닿는 물리 경로는 이미지에 없음(디컴파일은 board-dead PORTD로 라우팅; Timer0 ISR은 3 바이너리 플래그만 active-LOW로 PC7/PB1/PB0=STM32 PB14/PB2/PB10 미러). PC4/PA7(OSC2/3=STM32 PB12/PB13)는 이미지상 입력. **사용자 결정 = measure-first**(벤치 스윕으로 실제 구동경로·방향 확정 후 출력단 구현).
- **spec 초안** (`docs/superpowers/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`, **리뷰 대기**): compute 파이프라인(신규 `app_reg.c`+`adc1.c`) + `app_lcd_measure()` 라이브화 + board.c 안전수정(마스크 정정+확정 3채널 idle-HIGH, PB12/PB13 제외)만; OSC GPIO 구동은 DEFERRED(§9). 결정 DP1=슈퍼루프 폴링 ADC / DP2=raw 카운트+측정보정(AVR 10-bit@2.56V vs F410 12-bit@3.3V 도메인 해저드, `>>2` 정규화 1차안) / DP3=spec→리뷰→plan→구현. 순수함수 `reg_scale`/`reg_output_level`은 TDD 단위테스트 대상.
- **다음**: 사용자 spec 리뷰 → `writing-plans` → 구현(compute 먼저, 출력단은 측정 결과 수령 후).

### 2026-05-31 — STD comm 표시결함 해결(DGUS 에셋 root) + LCD 브랜치 HW 검증 완료 (통합/태그 대기)

comm_mode 표시결함의 **root는 firmware 아니라 DGUS 패널 에셋**(page 27/23 위젯이 page-show 시 `DISP_COMM_MODE`(0x140c) auto-load 안 함; page 25는 함)임을 **교차테스트**(STD comm 진입을 page 25로 repoint→정상)로 확정 → 사용자가 에셋만 수정해 정상화 = 진단 입증. firmware는 coherent 수정 A/B/C/D 적용. 상세: `docs/superpowers/analysis/2026-05-31-std-comm-page27-display-port-faithful.md`, 핸드오프 `docs/superpowers/2026-05-31-HANDOFF-lcd-comm-display-resolved.md`.

- **Fix A/B/C/D (커밋 `a0a631c`)**: A=부팅 `temp_comm_mode=0xFF` sentinel(`app_lcd.c init_mode`, 헤더 의도 정합) / B=`commit_comm_mode_and_ether` 0xFF early-return 손상가드(`app_lcd_input.c`, comm 미방문 STD-page SAVE 시 comm_mode=255+0.0.0.0 손상 차단) / C=`dgus_set_page` **후** comm 아이콘 재기록(`app_lcd_render.c`, page-27 auto-load 부재 방어 — 에셋 root fix로 redundant지만 **사용자 결정으로 defense-in-depth 유지**) / D=serial 분기 `DISP_EN_DHCP=0`(stale dhcp echo 제거). 머지빌드 0-warning, **FLASH 26.86%/RAM 10.16%**.
- **에셋 fix (커밋 `0eafe68`)**: `hw/lcd/dgus/14ShowFile.bin` page 27/23 comm-mode 아이콘 표시설정 (root fix, 레포 반영).
- **§4 전체 HW 재검증 PASS** (트레이스 빌드 + 머지 PR 바이너리 양쪽 플래시): ① STD comm 진입 표시 정상(`page=27 tcm=1`, ethernet O) ② **토글 0x140B serial→ethernet→dhcp 아이콘 정상 갱신·사라짐 없음**(이번 세션 핵심 실패분 해소; `data=0/1/2 → page=23/27/27 tcm=0/1/2`, serial=STDC23/eth·dhcp=STDE27 2-레이아웃은 패널 의도, 지난 세션 `page=9` 이탈 재현 안 됨) ③ DHCP 저장→전원사이클→`boot cm=2 ip=192.168.1.128` 영속+IP 정상 ④ Fix A 부팅 `tcm=255` sentinel ⑤ Fix B IP 0.0.0.0 손상 없음.
- **최종 리뷰**: cpp-reviewer **APPROVED** (A/B/C/D + `#ifdef LCD_TRACE_RX` 게이팅, 차단 이슈 0; B 오스킵 없음·C idempotent/무플리커/언더플로우가드 확인).
- **진단 스캐폴딩**: `LCD_TRACE_RX` 게이트 트레이스 **유지**(머지 컴파일아웃 검증), `fw/build-trace/` gitignore.
- **다음**: main 통합(머지/PR) + 태그 `hw-revA_fw-stage-lcd` → Stage D slice 1.

### 2026-05-27 (후속 3, WIP 체크포인트) — STD 이더넷/comm_mode 영속 + comm_mode 표시 버그

T10 항목 ⑤ 중 사용자 요청 "STD도 MULTI처럼 ether/comm_mode 영속" 작업. 상세: `docs/superpowers/analysis/2026-05-27-std-ether-comm-mode-persist-display.md`. **세션 종료 체크포인트 — 표시 버그 미수정, 다음 세션 이어감.**

- **작업1 (완료·HW검증, 미커밋→이 커밋에 포함)**: `data_save_commit()` STD 분기에 `commit_comm_mode_and_ether()` 추가 → STD에서도 comm_mode/ether 영속. 검증: `commit cm temp=1 cfg=0`→커밋, 전원사이클 후 `boot cm=1`, DHCP는 `boot cm=2`로 FRAM 영속 확인. = 사용자가 본 "ethernet enable 저장 안됨"은 영속 실패가 아니라 표시 버그.
- **작업2 (root cause 확정, 미수정)**: DHCP 저장+전원사이클 후 STDE에 **serial+dhcp 동시 체크**. 원인 = `temp_comm_mode` 섀도우 생명주기 결함: (1) 부팅 zero-init→`temp_comm_mode=0`(0xFF sentinel 아님), (2) seed가 `==0xff` 게이트라 부팅 후 첫 진입 스킵, (3) serial 분기가 `DISP_EN_DHCP` 미클리어(stale), (4) **추가 위험**: `commit_comm_mode_and_ether`가 0xFF 미가드 → comm 페이지 미방문 저장 시 comm_mode=255 손상(latent, MULTI에도 잠재, 작업1이 STD로 확장).
- **다음 세션 = coherent 수정 A~D** (spec/plan + samd20 comm-섀도우 대조 후): A 부팅 `temp_comm_mode=0xFF` 초기화 / B commit 0xFF 가드(손상방지) / C seed 자동정상화 / D serial 분기 `DISP_EN_DHCP=0`.
- **진단 스캐폴딩(LCD_TRACE_RX 게이트, 머지 컴파일아웃)**: `app.c` `boot cm=.. ip=..`, `app_lcd_input.c` commit `commit cm temp=.. cfg=..`. 유지/제거 결정 보류.

### 2026-05-27 (후속 2) — SETUP_MODEL 롱프레스 진입 불가 발견·수정 (코드 변경 O, **HW 검증 PASS**)

T10 항목 ⑥ 검증 중 **SETUP에서 모델 셋업 페이지 진입 키가 안 먹힘** 발견. systematic-debugging Phase 1~4. 상세 finding: `docs/superpowers/analysis/2026-05-27-lcd-setup-model-longpress.md`.

- **Root cause**: DGUS 모델 키(VP 0x1084)가 **터치-다운/업 양쪽 모두 value 0**을 올리고 **release=2를 영영 안 보냄**(LCD_TRACE_RX 진단 빌드 3회 캡처: 4초 홀드=2이벤트 둘 다 0, auto-repeat 없음). 포팅된 `long_press_released()`는 samd20 verbatim으로 `data==2`를 릴리스로 가정 → 업(0)을 "또 다른 press"로 오인 → **롱프레스 절대 미완성**. 포팅 회귀 아님(이 패널 설정이면 samd20도 동일 실패 구조) = 패널 DGUS 터치 설정 ↔ 펌웨어 프로토콜 가정 불일치.
- **수정 (P2, 펌웨어측 — 사용자 결정)**: `long_press_released(vp, data16)` 재설계 — 같은 VP의 **연속 두 data=0을 press→release로 페어링**, 간격 ≥ KEY_HOLD_MS(2000ms)면 발화(2초 가드 유지). `data==2` 하위호환 유지. `key_press_vp`로 키잉(0x1084/0x1094 무간섭). `app_lcd.h`에 `key_press_vp` 필드 추가. 빌드 0-warning, FLASH 26.80%/RAM 10.16%.
- **HW 검증 PASS**: ① 모델 키 2초 홀드 → 모델 셋업 진입 ✓, ② 짧은 탭 → 미진입(가드 유지) ✓.
- **보류 (P1, 패널측)**: DGUS `.bin`을 0x1084 업에서 value 2 올리도록 재설정(펌웨어 무수정·verbatim 유지) — DGUS 에디터+패널 재플래시 필요라 미채택.
- **flag(차단 아님)**: 토글 desync 위험(0x1084 다운 드롭 시 spurious 진입; DMA RX로 완화). 0x1094 동일거동 여부 미확인(같은 로직 공유).

### 2026-05-27 (후속) — HW 검증 중 버그 2건 발견·수정 (코드 변경 O, **HW 검증 PASS**)

T10 HW bench 검증 착수 → energy 슬라이더 드래그가 DGUS RX를 영구 wedge시킴을 발견. 추적 결과 **연결된 버그 2건**을 수정. 둘 다 LCD 포팅 로직이 아닌 하부 결함이며, RX wedge가 다른 하나를 가리고 있었음. 상세 finding: `docs/superpowers/analysis/2026-05-27-lcd-hw-verify-rx-wedge.md`. spec/plan: `docs/superpowers/{specs,plans}/2026-05-27-usart1-dma-rx-hardening*`.

- **버그 1 — USART1 DGUS RX 영구 wedge (Phase2/StageA 드라이버 결함, LCD 브랜치가 노출)**: 1바이트 `HAL_UART_Receive_IT`가 오버런/재무장 실패 시 RXNEIE=0으로 영구 정지(SR ORE=1, RXNE 갇힘). 트리거 = per-rx 블로킹 `mon_printf`(무한 타임아웃)가 VP 플러드 중 루프 포화. **수정**: USART1 RX를 **DMA2 Stream2 Ch4 circular free-running**으로 전환(오버런 면역, `usart1_rx_pop` NDTR 기반, 시그니처 불변 → 파서/app 무수정) + per-rx 트레이스 `#ifdef LCD_TRACE_RX` 게이트(default off) + mon TX 타임아웃 50ms. 커밋 a2c37b4→82cdb4c(+리뷰 20b3380). cpp-reviewer APPROVE.
- **버그 2 — LCD 입력 값 추출 off-by-one (LCD 브랜치 T6, RX fix 후 노출)**: DGUS 0x83 응답의 VP주소 뒤 READ_LEN(워드수) 바이트를 값 MSB로 오독(`data16=(data[0]<<8)|data[1]`). SAVE(값1)→256으로 읽혀 CANCEL 분기 실행(영속 안 됨), 전 numeric 입력이 garbage. **수정**: `data16=(data[1]<<8)|data[2]`(READ_LEN 건너뜀, samd20 dgus_lcd.h DATA_H=5/L=6 일치). 1줄. 커밋 d6c681f.
- **HW 검증 PASS**: ① `run_page_confirmed=1`(무회귀). ② SETUP energy 편집→SAVE→**전원사이클→영속**(`[cfg] energy=567`). 공격적 슬라이더 드래그에도 **RX 무wedge**(CR3 DMAR=1, SR ORE=0, NDTR/tail 전진). `[lcd-hook] set_pot power=55 dac=12`(output_power 정확 추출). ⑩ 훅 배선 OK. ⑧ N/A(전원 공유). ⑨ 재해석(패널이 idle에 LEN=3 `5A A5 03 82 4F 4B` 프레임 연속 스트리밍 → 파서가 설계상 거부 → dgus_rx_drop_count 항상 비0이나 무wedge가 진짜 신호).
- **잔여 T10(미검증)**: ③ CANCEL 복귀, ④ 전 페이지 네비 무락업, ⑤ STD ether 미영속 퀴크, ⑥ F3 HAND→MULTI, ⑦ NM/GW IP옥텟 시드 퀴크. 이들 통과 후 main PR + 태그 `hw-revA_fw-stage-lcd`.
- **향후 조사 후보(코딩 차단 아님)**: 패널이 스트리밍하는 VP 0x4F4B `5A A5 03 82 4F 4B`(LEN=3, WR)의 정체/이유.

### 2026-05-27 — LCD 전체 동작 포팅 (samd20→STM32) 구현 완료 (코드 변경 O, **HW 검증 대기**)

- **브랜치**: `feat/stage-lcd-full-behavior` (main에서 분기, BOOT0 `9bbc505`+LCD `d5860a3` cherry-pick 선반영). 사용자 지시: "samd20은 동작이 정확하게 되는 코드" → 충실 포팅.
- **범위**: parse_lcd_comm(터치/키 입력+설정편집+DATA_SAVE/CANCEL) + change_lcd_page(페이지 렌더) + 주기 표시 스텝머신(LV_OUTPUT/LV_TIME 바, VAR_*, ICON) + 문자열 포맷터. 와이어/FRAM 계층은 Stage B 재사용. 측정값은 **스텁 provider**(전부 0 → 바 빈/값 0), HW/제어 결합은 **named 스텁 훅**(set_pot/us_command/comm_reconfigure/ether_apply/horn = Stage C/D 통합점).
- **구현(spec→plan→10태스크, 각 빌드-게이트+커밋)**: T1 VP 매크로 / T2 `app_config_save_all` / T3 포맷터(time/energy/addr/ip/pdd) / T4 서브시스템 state+측정스텁+훅 / T5 `change_lcd_page` / T6 입력 디스패치 코어 / T7 DATA_SAVE/CANCEL+comm/ether / T8 표시 스텝머신+아이콘+에러페이지 / T9 app.c 배선+SYS_PIC_NOW 루프가드. T1-T4 직접, T5-T9 subagent-driven(태스크별 리뷰+samd20 원본 대조).
- 신규 소스: `src/app_lcd_str.c`(167) `app_lcd_render.c`(223) `app_lcd_input.c`(802) `app_lcd_disp.c`(290), `app_lcd.c`(165) 확장. **빌드 0-warning, FLASH 27.9%(35684B)/RAM 9.3%**. config 소유권을 LCD 서브시스템으로 이동(`app_lcd_cfg()`).
- **보존된 samd20 퀴크(verbatim, 임의수정 ✗)**: F3 HAND 저장→MULTI 페이지 복귀. F4 cancel = full FRAM reload(live comm/ether는 저장前 미변경이라 no-op). STD 경로 저장은 comm_mode/ether **미영속**(addr/speed/parity만). `ether_select_field`가 항상 `temp_ether_ip[3]`로 시드(samd20 copy-paste 버그). KEY_MULTI RESET 에러클리어가 samd20 `sys_status==SYS_ERROR` 추가게이트 생략(에러경로 미도달, Stage D 재조정).
- **Stage B 콜드부팅 핸드셰이크 무회귀** (`dgus_wait_ready`/`ensure_run_page` 그대로; init_mode는 change_page 호출로 리팩터, 기본설정 RUN 페이지 렌더 byte-identical 검증). 회귀 여부는 첫 부팅서 실증.
- **잔여 cleanup(옵션)**: `app_lcd_input.c` 802줄(800 가이드라인 2줄 초과) — 분할 시 ether FSM을 `app_lcd_input_ether.c`로 절단 권장. 그린 빌드 리스크로 지금은 보류.
- **다음 = HW bench 검증 (T10, 사용자와)**. 통과 시 RESUME/NEXT_STEPS "done" 갱신 + PR + 태그 `hw-revA_fw-stage-lcd`. spec `docs/superpowers/specs/2026-05-27-stage-lcd-full-behavior-port-design.md`, plan `docs/superpowers/plans/2026-05-27-stage-lcd-full-behavior.md`.

#### HW bench 검증 체크리스트 (USART6 mon @115200; spec §13)
- [ ] 부팅 → RUN 페이지 렌더 (Stage B 패리티), `run_page_confirmed=1` (Stage B 경로는 inspection상 불변, 첫 부팅서 실증).
- [ ] SETUP 진입 → 현재 리밋 표시 → 값 편집 → SAVE → 전원 사이클 → 값 영속 (FRAM determinism).
- [ ] CANCEL → 값이 FRAM 값으로 복귀.
- [ ] 전 setup/comm/ethernet 페이지 네비 — 락업/SYS_PIC_NOW 루프 없음 (mon trace 관찰).
- [ ] **STD 모델(type=2)에서 STDE 이더넷 편집 SAVE/reload → comm_mode/ether 미영속 = samd20 동작과 일치 확인** (의도된 동작).
- [ ] **HAND 저장 후 MULTI 페이지로 복귀(F3)** — 편집한 페이지와 다른 페이지 표시, 의도된 UX인지 사용자 확인.
- [ ] **NM/GW 필드 선택 시 IP 마지막 옥텟이 초기값으로 표시**(`temp_ether_ip[3]` 시드, verbatim) — samd20과 일치 확인.
- [ ] **패널만 전원 사이클**(MCU 유지) → SYS_PIC_NOW 재init이 루프 없이 복구되는지 (200ms 가드가 정상 케이스를 안 막는지).
- [ ] 터치-헤비 네비 중 `dgus_rx_drop_count()` 관찰 (RX 링 오버플로 없음).
- [ ] 훅이 RUN/SEEK/RESET/SAVE에 `[lcd-hook]` 로그 — 하드웨어 미구동 배선 확인.
- [ ] (옵션) `LCD_DEMO_MEASURE_RAMP` 빌드 → LV_OUTPUT/LV_TIME 바 스윕, VAR_* 증가.

### 2026-05-26 — BOOT0 해결 + LCD 콜드부팅 핸드셰이크 픽스 (코드 변경 O, HW 검증)

- **BOOT0 이슈 해결**: BOOT0(U2.60)→GND 연결. 콜드부팅 시 플래시 앱 직접 실행. HW 검증: `reset halt` PC=0x080045c0/MSP=0x20008000, mon 정상. force-jump 워크어라운드 폐기 (fallback으로 문서 보존). `docs/NEXT_STEPS.md §5.7`, `docs/superpowers/RESUME.md`, memory `project_board_boot0_workaround` 갱신.
- **LCD 콜드부팅 레이스 픽스**: 증상 = 콜드 전원 사이클 시 로고 화면만 잔류(동작화면 미전환). 근본원인 = DGUS 패널이 MCU보다 늦게 부팅 → 단발 `set_page(run)`(전원-on 후 ~1050ms)이 패널 준비 전 도착·유실 → 패널이 power-on 기본 페이지(로고)에 잔류. ST-LINK reset 경로는 패널이 이미 부팅돼 있어 미재현(비대칭).
  - 수정 = blind 1s delay 대체 **readiness 게이트**(`dgus_wait_ready`: SYS_PIC_NOW(0x0014) read에 패널이 응답할 때까지 폴링, 최대 4s) + **read-back 확인/재전송**(`app_lcd_ensure_run_page`: SYS_PIC_NOW가 run 페이지 확정할 때까지 최대 8회 재전송). samd20의 주석처리된 `check_lcd_comm` 핸드셰이크(`ref/samd20/main.c:4933-5022`) 의도 복원.
  - UX 보존: readiness 게이트 후 **로고 1초 dwell → run 페이지**(samd20 `set_page(0)→delay 1000→run` 동일).
  - 신규 드라이버 API: `dgus_read_word`/`dgus_wait_ready`(`drivers/dgus_lcd.c`), `app_lcd_run_page`/`app_lcd_ensure_run_page`(`src/app_lcd.c`). 튜닝 상수 `DGUS_BOOT_READY_TIMEOUT_MS`/`DGUS_LOGO_DWELL_MS`/`DGUS_PAGE_CONFIRM_*`(`include/dgus_lcd.h`).
  - HW 검증: 콜드 전원 사이클 → 로고 ~1초 → 동작화면 전환 확인. mon `[lcd] ready=1`/`run_page_confirmed=1`. 빌드 0-warning, FLASH 22.57%.

### 2026-05-25 (후속) — ATmega16 보드-진실 정정 + 소스오브트루스 전환 (코드 변경 ✗, 문서만)

- Stage D brainstorming 중 사용자 보드 직독으로 **OSC 드라이브 매핑 확정**: `CTRL_OSC0..4` ← mega16 **PB1/PB0/PC4/PA7/PC7** (V30 PB2/PB10/PB12/PB13/PB14). 분석 §7 #1(OSC 트리거 물리핀 미해소) **해소**. **PA0(ADC ch0) = 출력 초음파 세기 피드백**(레귤레이션 입력) 확정.
- **소스 오브 트루스 전환**: 1차 = 사용자 보드 측정 + 행동 재분석, 2차(약함) = `m16_conv_v001.c` 디컴파일(**함수명·동작주석 무시**). 코드 정밀추적 결과 누진 드라이브 레벨이 RAM 변수(`disp_*`) + 본문부재 `output_direct_process()`로만 라우팅됨을 확인 → 디컴파일이 출력단을 못 보여줌.
- **뒤집힌 본문 결론**(이제 무시): "IPC=PB0/PB1/PA7/PC4/PC7"(§1), "PB0/PB1/PC7=상태표시 IPC"(§3), "누진패턴→dead display로만 감"(§4). 실제 IPC는 명령선(PA4/5/6)+M_OVLD(PC0) 별개 그룹.
- **측정 대기 (Stage D 차단)**: 분석 **§0.1 B1~B5** — ① 5 OSC핀 방향(in/out; PC4·PA7 코드상 입력) ② 극성·레벨↔비트매핑 ③ 레귤레이션 전달함수 ④ ADC ch1(PA1) 정체 ⑤ `g_main_state` 0/1 두 출력모드. 사용자가 측정+행동재분석 제공 예정.
- **"ref verbatim 포팅" 전제 폐기**. Stage D 결정분: 1슬라이스(피드백 루프, 모듈경계로만 분할), 코드-포팅 먼저+머지前 실측, 안전경계 옵션은 측정 후 재논의.
- 갱신 문서: `analysis/atmega16-io-behavior.md`(§0.1 정정 절 + 배너 + §7 #1 CLOSED 표기), `pinmap.md`(CTRL_OSC 식별 확정/방향·극성 대기, Q8b/Q11), `NEXT_STEPS.md` §1.1, `RESUME.md`, memory `project_atmega16_absorption`.

### 2026-05-25 — ATmega16 FW 동작↔I/O 신호 분석 완료 (코드 변경 ✗)

- 산출물: **`docs/superpowers/analysis/atmega16-io-behavior.md`** (kickoff §6 deliverable). 소스 = `ref/atmega16/m16_conv_v001.c`(디컴파일, 비판적 독해) + `hw/schematics/usw_ctrl_v26_samd20.pdf`(OLD V26, page4=ATmega16 U6, pdftoppm 600/1200dpi 크롭 판독, netlist 없음) + `hw/schematics/USW_CTRL_V30.asc`(NEW netlist).
- **아키텍처 확정**(사용자): ATmega16 = 트리거 생성 + 전류 모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드. IPC = 순수 GPIO(`hw_init`이 USART/SPI/TWI 전부 비활성).
- **디컴파일러 noise 정정**: C 헤더의 핀-역할 주석 대부분 추측. `display_*`(PORTD+PB2/3/4)는 V26 미연결(×) = dead legacy 7-seg. `BUZ_M16`/`U1.xx` net 주석 오류 다수.
- **HIGH 확정**: PA4=`M_START`(START in, 3중확인), PC0=`M_OVLD`(overload out, 3중확인). 제어루프: ADC(PA0=`SENS_OUT`) → `lookup_table` 스케일 → `output_level_process` 누진 5비트(0x01→0x1F) ↔ V30 `OSC_OUT0..4` 1:1. ISR cadence: ADC~208µs/Timer0~2ms/Timer1~10ms.
- **미해소(최우선 HW-verify)**: ① OSC 트리거 물리 출력핀 ② ADC ch0/ch1 정체(PA1 미연결인데 ch1 처리) ③ PC1 방향 충돌. 분석 §7.
- 다음 = (선행)HW-verify 패스 → **Stage D Ultrasonic regulation 흡수**(분석 §8). `docs/NEXT_STEPS.md` §1.1 + `docs/superpowers/RESUME.md` 갱신, memory `project_atmega16_absorption` 갱신.

### 2026-05-25 — Stage A main 머지 + repo 자산 정리

- `feat/stage-a-lcd-io` (33 commits) → `main` **`--no-ff` 머지** (`4651453`), 태그 **`hw-revA_fw-stage-a`**. 머지 후 빌드 재검증 FLASH 18.94% / RAM 8.37% ✅ (브랜치 수치 일치).
- 머지 전 정리: 미커밋 `docs/requirements.md` 보강분(시스템 아키텍처 + F1~F6/FW1~FW3) 커밋(`e322644`); stale untracked `docs/NEXT_STEPS.md`·`docs/superpowers/RESUME.md` → `docs/superpowers/historical/` 아카이브(`cea0c3a`, 머지 시 untracked 충돌 회피 + Phase 1~7 로드맵 보존).
- repo 자산 track(`25fb41f`): `ref/`(레거시 SAMD20/ATmega16 포팅 source 8.8 MB), `fw/cube/gds_usctrl.ioc`(핀 config), `AGENTS.md`, `docs/fw_analysis.md`, `.claude/settings.json`·`.codex/hooks.json`(RESUME 자동로드 SessionStart 훅). `graphify-out/` 는 `.gitignore` 추가(재생성 산출물).
- worktree `gds_us_ctrl-stageA` + 브랜치 `feat/stage-a-lcd-io` 삭제 — 현재 `main` 단독, working tree clean. origin push ✗ (로컬만).
- 다음 슬라이스 **Stage B (LCD app 데이터 사전 셋업)** 사용자 확정 → `docs/NEXT_STEPS.md` §4 + `docs/superpowers/RESUME.md` 갱신.

### 2026-05-06 — Stage A LCD I/O Bring-up 완료 (DGUS LCD on USART1, Task 1–13)

- USART1 (PA9/PA10, AF7, 115200 8N1) raw 드라이버 신설 — `fw/drivers/usart1.c`. HAL `Receive_IT` 1-byte 무장 + 64-byte ring buffer + 폴링 TX (10 ms timeout). `HAL_UART_RxCpltCallback` + `HAL_UART_ErrorCallback` 두 vendor weak 함수 instance-USART1 분기 override.
- DGUS 프로토콜 레이어 신설 — `fw/drivers/dgus_lcd.c`. samd20 9 함수 풀 패리티 (`dgus_reset_lcd / set_page / write_u16 / write_u32 / write_bytes / write_u16_array / write_text / read_var`) + 4-state RX 파서 상태머신 (`PS_IDLE → PS_GOT_5A → PS_GOT_HEADER → PS_COLLECTING`) + WR-echo 헬퍼 (`dgus_is_echo`) + 관측성 카운터 (`dgus_rx_drop_count`, `dgus_tx_timeout_count`, `usart1_rx_drop_count`, `usart1_rx_error_count`).
- samd20 결함 5건 명시 회피 (spec §3.7) — LEN [4,26] 검증, 50 ms 벽시계 timeout (sys_tick_get_ms), HAL_UART_Init Error_Handler, 폴링 TX 자연 직렬화, RX ring drop-not-overwrite 카운터.
- 데모 동작: 부팅 시 banner 3줄 (`[boot] gds_us_ctrl stage-a-lcd ready` + `[lcd] usart1@115200 ring=64 prio=5` + `[lcd] init ok, set_page=9, uptime VP=0x1110`) → `dgus_set_page(LCD_RUN_STD)` → 1 Hz 로 `dgus_write_u16(VAR_POWER, secs)` + mon `[t=N ms] hello uptime=N` cadence + LCD echo passive RX 로깅.
- 빌드 결과: FLASH **24,824 B / 128 KB (18.94%)** ✅ (spec §6.2 acceptance < 30 KB), RAM **2,744 B / 32 KB (8.37%)** ✅ (acceptance < 4 KB). 신규 export 심볼 (`usart1_init`, `dgus_init`, `dgus_set_page`, `dgus_write_u16`, `dgus_rx_poll`, `USART1_IRQHandler`, `HAL_UART_RxCpltCallback`, `HAL_UART_ErrorCallback`) 모두 `.text` 확인. warning 0.
- HW 검증: ST-LINK V3 + STM32F410RBT 보드 flash → mon banner 3줄 + 1 Hz hello uptime 출력 ✅. USART1 wire-level 명령 valid 인식 ✅ (LCD 가 매 set_page/write_u16 명령에 DGUS WR-echo `5A A5 03 82 4F 4B` 응답, GPIOA AFRH=0x770 / BRR=0x341 / CR1=0x202C 정상). PC=`app_loop_iter` Thread mode, fault 미히트 ✅.
- **scope 정정 (2026-05-06)**: 초기 spec §6.3 6c "LCD 페이지 시각 전환" 이 samd20 ref 의 `init_lcd_mode` 흐름 (`send_model_str` + DISP_*/LV_*/ICON_* VP 11+ 사전 셋업) 의존을 누락. spec §0.3 OUT OF SCOPE 에 "LCD application 데이터 사전 셋업 — Stage B" 명시 추가, §6.3 6c/6d/6f acceptance 를 wire-level 입증 + mon-side cadence + drop counter 정밀 정의 로 완화. **Stage A 코드 자체 정상**, samd20 레퍼런스 분석 결과 Stage B 슬라이스로 분리.
- spec/plan 정정 패턴 (Task 8 sys_tick_get_ms / Task 9 sys_tick_init,__enable_irq / Task 10 mon_writeln 미러): 발견 → spec/plan 정정 commit → 코드 first-time commit. 모든 substantive Task (4, 6+7, 8, 9, 10) spec compliance reviewer ≥ 12/12 ✅ APPROVE, severity CRITICAL/HIGH/MEDIUM/LOW 0.
- Code quality reviewer (Task 4-10 묶음, code-reviewer agent): HIGH 1 (`HAL_UART_ErrorCallback` 미구현 → ORE 시 RX 영구 정지) + LOW 3 (헤더 가드 `#pragma once` 통일, irq.c mid-file include, `extern Error_Handler` → `#include "clock.h"`) 모두 fix 완료.
- `feat/stage-a-lcd-io` 브랜치 → main 머지 완료 (2026-05-25, `4651453`, tag `hw-revA_fw-stage-a`).
- 산출 spec: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`
- 산출 plan: `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-06-RESUME.md`

### 2026-05-05 — Phase 1+2 Bootstrap 완료 (Chunks 1–13)

- CubeMX UI 의존 제거 결정. HAL/CMSIS는 `fw/vendor/` in-tree read-only 카피로 동결.
- 디렉토리 재편: `fw/cube/` → `fw/vendor/` + `fw/{src,include,drivers,openocd}/` 신설.
- 빌드 시스템: CMake + arm-none-eabi-gcc 툴체인 + Ninja, OpenOCD/ST-LINK V3 flash/debug.
- **Phase 1**: HSI ×12 = **96 MHz** SystemCoreClock 부팅. 빈 main while 루프 도달 GDB 검증 완료 (PC=0x80002ea, MSP=0x20008000).
- **Phase 2**: TIM11 1 kHz 틱 (PSC=95, ARR=999, NVIC priority 5), USART6 115200 8N1 monitor (PC6/PC7 AF8, blocking TX), PB3 1 Hz heartbeat (50% duty).
- 빌드 결과: FLASH 22 060 B / 128 KB (16.83%), RAM 2 520 B / 32 KB (7.69%).
- HW 검증: ST-LINK V3 + STM32F410RBT 보드에서 banner `[boot] gds_us_ctrl phase2 ready` + 1 Hz `[t=N ms] hello` + PB3 LED 점멸 모두 관찰. fault 미히트.
- 발견 + 해소된 결함:
  - `TIM_AUTORELOADPRELOAD_DISABLE` (no underscore) 매크로 오타 → `TIM_AUTORELOAD_PRELOAD_DISABLE` (commit `de0c35f`).
  - CMake genex `$<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>` semicolon 처리 → 각 flag를 별도 genex로 분리 (commit `8d67a7d`).
  - Cortex-M4 r0p1 6 HW BP 한계 — fault handler 모두 + main 동시 break 시 7개째 거부. 디버그 시 break 갯수 제한.
- `STM32_TOOLCHAIN` env var stale 경로 이슈 — 빌드 시 `env -u STM32_TOOLCHAIN cmake ...` 우회.
- `feat/phase1-2-bootstrap` 브랜치(33 commits) → main 머지 (merge commit on 2026-05-05).
- 산출 spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`
- 산출 plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-05-RESUME.md`

### 2026-04-26 — 분석 세션
- 프로젝트 구조 생성 (`fw/`, `hw/`, `docs/`, `ref/`)
- `CLAUDE.md` 작성 — 통합 목표 (SAMD20 + ATmega16 → STM32F410RBT)
- `ref/samd20/`, `ref/atmega16/` 이전 코드 보관
- CubeMX `.ioc` 분석 → `docs/pinmap.md` 작성
  - W5500, DGUS LCD, I2C EEPROM, 5채널 OSC, TIM5 PWM 등 핀 매핑 확정
- graphify로 코드 분석: 340 노드 / 851 엣지 / 39 커뮤니티
- SAMD20 `main()` 슈퍼루프 추적 — 10-state job_state 머신 구조 파악
- ATmega16 `m16_conv_v001.c` 분석 — 독립 서브 컨트롤러, GPIO 시그널링
- `docs/requirements.md` 작성 — 시스템 아키텍처 (이전 → 신규), F1~F6
- `docs/NEXT_STEPS.md` 작성 — 회로도 미해결 질문 + 포팅 단계별 계획

### 2026-04-26 — FW 분석 마무리 + 회로 답변 통합
- **이전 MCU 간 GPIO 시그널 매핑 확정** (출처: `user_board.h:75-103` + 사용자 회로 동작 확인)
  - `M_START = PB13` (out, active LOW) → ATmega16 PA4 (초음파 출력 개시)
  - `M_SEEK  = PB11` (out, active LOW) → ATmega16 PA5 (공진 주파수 탐색)
  - `M_RESET = PB12` (out, active LOW) → ATmega16 PA6 (초음파 초기화)
  - `M_OVLD  = PB10` (in)               ← ATmega16 PC0 (과부하 알람)
- 통합 시 위 4개 GPIO는 **내부 함수/플래그**로 대체 (us_start/us_seek/us_reset/g_overload)
- 회로 미해결 항목 후속 답변 통합:
  - Q2-잔(PC1) = 무시, Q3(PC4) = 외부 입력, Q5(disp_led_pattern) = 단순 표시 출력
  - Q6(I2C_POT) = 외부 디지털 포텐셔미터 칩 → `i2c_adc_*` 그대로 포팅
  - Q7 = `.ioc`에 PA0 Net Name **`US_PWM_OUT`** 추가 완료
  - Q8 = CTRL_OSC0~4 = 외부 제어 신호 출력 (활성 극성은 추후 회로도 확정)
  - Q9 = TIM11 = 1 ms 시스템 틱
- `docs/fw_analysis.md` 신규 작성 — 포팅 참조용 종합 분석 (job_state 10단계, parse_lcd_comm, decode_comm, W5500/DHCP/TCP, 글로벌 상태, 인터럽트 인벤토리, F410 변환 규칙)
- `docs/pinmap.md` 갱신 — PA0 Net Name, CTRL_OSC0~4 외부 출력 표기, 부록 A(이전 MCU GPIO), 부록 B(결정), 부록 C(잔여)
- `docs/NEXT_STEPS.md` 갱신 — 회로 답변 반영, **개발 순서 4스테이지 재구성** (A: LCD+IO → B: Modbus → C: Ethernet → D: mega16 흡수)
- `docs/requirements.md` FW3 우선순위 = 4스테이지 순서로 재정렬

### 2026-04-26 — 잔여 회로 항목 결정
- **Q4 (7-segment) = 삭제**: DGUS LCD가 모든 표시 흡수. ATmega16 `disp_led_pattern` / 7-seg PORTB·PORTD 출력 폐기
- **Q8b (CTRL_OSC0~4 활성 극성) = 미정**: Stage D `do_control()` 구현 시 회로도 + 실측으로 확정
- **Q10 (ADC1 IN9 채널 추가) = 미정**: Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 확인
- 위 결정 사항을 `docs/pinmap.md` (부록 B/C), `docs/fw_analysis.md` §13, `docs/NEXT_STEPS.md` 잔여 표에 반영

### 모든 회로 미해결 항목 종결 — 다음은 Phase 1 (CubeMX 보정) → Phase 2 (빌드 시스템 + 슈퍼루프 골격) → Stage A (LCD + IO)

### 2026-04-26 — Phase 1+2 brainstorming 진행 + 일시 중단
- `superpowers:brainstorming` 스킬로 Phase 1+2 설계 진행
- 결정 완료: 범위(Phase 1+2) / 통합방식(Hand-written CMake) / 데모(TIM11+UART hello) / 레이아웃(현 구조) / HAL(in-tree) / 플래시(OpenOCD+ST-LINK) / 아키텍처(Phased Modular)
- **새 제약 기록**: USART6 이중 역할 (Phase 2~Stage A: Monitor / Stage B 이후: Modbus RTU). `pinmap.md` USART6 섹션 + Monitor 모듈 backend 교체 가능 인터페이스 설계 반영
- 설계 6 섹션 중 **섹션 1/6 (디렉토리+모듈 경계) 사용자 승인 대기 중 일시 중단**
- 재개 가이드: `docs/superpowers/RESUME.md` 작성 — 결정 사항 / 대기 질문 / 남은 5 섹션 / 작업 재개 절차 모두 포함
- `.claude/settings.json` 신규 — **SessionStart 훅 추가**: 다음 세션 시작 시 RESUME.md를 자동으로 시스템 메시지(사용자 가시) + additionalContext(Claude 컨텍스트)로 주입. 사용자는 한 번에 컨텍스트 복원 가능
