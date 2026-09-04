# HW 없이 진행 가능한 잔여 작업 — 전수 조사 + 실행 계획

> **문서 요약**: 보드가 없는 상태에서 `gds_us_ctrl` 의 열린 항목 **22건**을 전수 조사해 [HW-불요]/[HW-게이트]/[사용자-보류]/[타-repo] 로 분류했다. **[HW-불요] 는 6건**이고 그중 코드 변경은 **2건뿐**이다 — ① 2026-07-02 감사가 남긴 stale 주석 정정(감사 3곳 중 1곳은 이미 해소, 2곳 생존 + 같은 계열 4곳 추가 발견, 전부 바이너리 무변화) ② **조사 중 발견한 잠복 결함**: `app_modbus.c` 의 Modbus START 진폭 pot write 가드가 게시(publish) 시점 문제로 **항상 FALSE** 라 2026-06-28 I2C_POT 실구동 머지 이후 원격 START 가 pot 을 한 번도 쓰지 않는다(samd20 main.c:4401 이탈, 1줄 fix). 나머지 4건은 문서: rsb 스택의 changelog 누락, 진입 문서 4종의 IWDG 미반영, 벤치 체크리스트 stale 3곳, 로컬 전용 브랜치 정리(6개 중 `feat/symmetric-stop` 은 main 의 조상 = 자명한 삭제 대상; 5개는 사용자 판단). **`ponytail:` 주석은 0건**, 감사 defer Minor 는 전건 코스메틱(YAGNI 종결). 두 HW-게이트 브랜치(`feat/remote-status-bits`·`feat/iwdg-watchdog`)는 **`git merge-tree` 충돌 0**이므로 벤치는 throwaway 통합 브랜치 1회로 가능하다 — 이것과 pot fix 채택·백업 브랜치 처리·문서 정렬 위치가 §5 의 사용자 결정 4건이다. **큐가 비지는 않았지만 얇다** — 코드 2건 합쳐 ±20줄, 문서 4건 합쳐 1세션 미만.

**작성**: 2026-09-04 (조사 세션, Fable — 코드 무변경·git 무변경)
**조사 기준 체크아웃**: `feat/iwdg-watchdog` `00e4027` (⚠ 세션 스냅샷의 "현재 브랜치 = `feat/remote-status-bits`" 는 stale — 실제 HEAD 는 iwdg 였다. 따라서 작업 트리의 CLAUDE.md/NEXT_STEPS/HANDOFF/RESUME 는 **main 판(2026-08-17)** 이고, 2026-09-04 판 진입 문서는 `feat/remote-status-bits` 에만 있다.)
**약칭**: rsb = `feat/remote-status-bits`(main `2f74611` + 20), iwdg = `feat/iwdg-watchdog`(main + 4). 둘 다 origin 동기.

---

## 0. 전제 (실행자 필독)

- **코드 수정 전 사용자 컨펌** — §3 의 각 항목에 "컨펌 필요" 표기. 컨펌 없이 진행 가능한 것은 주석·문서뿐.
- **main 머지·태그 금지** — rsb·iwdg 는 둘 다 HW 벤치 게이트. 이 문서는 벤치 전 준비만 다룬다.
- **라인 번호는 드리프트한다** — rsb 와 main 의 `app_modbus.c` 는 194줄 어긋난다(rsb = main + 194). 편집 전 반드시 인용 문자열로 `grep -n` 재확보.
- 바이너리 무변화 판정 = 편집 전후 `./fw.sh` 후 `md5 fw/build/gds_us_ctrl.bin` 비교(`CMakeLists.txt:122-124` 가 `.bin/.hex` 를 생성한다). `MODEL=remote` 빌드도 따로.
- 검증 명령: `./fw.sh`(0-warning) + `./fw.sh test`(rsb 16 스위트 / iwdg·main 14 스위트 — 아래 §6 불일치 참조).

---

## 1. 전수 분류표

출처: `docs/NEXT_STEPS.md` §1.2·§1.3·§2.2, 루트 `HANDOFF.md`(rsb 판 §미해결·main 판 §Not Yet Done), `docs/superpowers/RESUME.md` 최상단, `docs/changelog.md`, `docs/requirements.md` FW3-6, `.superpowers/sdd/progress.md`, rsb `CLAUDE.md` 열린 항목, 코드 grep.

| # | 항목 | 분류 | 근거 (파일:라인) | 규모 | 거동 변화 | 컨펌 |
|---|---|---|---|---|---|---|
| 1 | **Modbus START 진폭 pot write 복원** (조사 중 발견) | **[HW-불요 코딩 / HW 검증]** | `fw/src/app_modbus.c:143-153`(main) = rsb `:337-347`; 게시 `app_reg.c:390`; 순서 `app.c:152`→`:161`; legacy `ref/samd20/main.c:4393-4402` | −10줄 +3줄, 1파일 | **있음** (원격 START 시 pot 실기록 — legacy 복원) | **필요** |
| 2 | stale 주석 정정 — 감사 3곳 + 동계열 4곳 | [HW-불요] | 아래 §3-B 표 | 6파일 ≈12줄, 바이너리 동일 | 없음 | 불필요(주석) |
| 3 | rsb 스택 `docs/changelog.md` 항목 부재 | [HW-불요·문서] | `git diff main..rsb -- docs/changelog.md` = 0줄; 관례 = 브랜치 위에서 작성(ponytail-cleanup·IWDG 선례) | 1항목 ≈15줄 | 없음 | 불필요 |
| 4 | 진입 문서 4종 IWDG 미반영 + 벤치 체크리스트 stale | [HW-불요·문서] | rsb `CLAUDE.md:144`, `docs/NEXT_STEPS.md:61,101`, `HANDOFF.md:31-34`, `RESUME.md:3`("18커밋"), 체크리스트 `:3`("7커밋")·`:287`(§7-3) | 5파일 ≈10줄 | 없음 | 위치만(§5 D-4) |
| 5 | 로컬 전용 브랜치 정리 | [HW-불요·git] | §4 | 명령 1~7개 | 없음 | 5개는 **필요** |
| 6 | 벤치 빌드 전략(통합 1회 vs 순차 2회) 확정 | [HW-불요·결정] | `git merge-tree --write-tree rsb iwdg` = 충돌 0, 겹치는 파일 `fw/CMakeLists.txt` 뿐(다른 hunk) | 결정 1건 | 없음 | **필요**(§5 D-3) |
| 7 | rsb 통합 HW 벤치 (S·M·B34·MOD·CAL·NET·FA·A) | [HW-게이트] | `plans/2026-09-04-remote-parity-bench-checklist.md` | — | — | — |
| 8 | IWDG T-3 HW 벤치 V-1~V-6 | [HW-게이트] | `plans/2026-09-04-iwdg-watchdog.md` T-3, spec §6.2 | — | — | — |
| 9 | PC8 인터록 실장 PCB (회로 수정) | [HW-게이트·사용자] | rsb HANDOFF §벤치 실행에 필요한 것 | — | — | — |
| 10 | 전류 0.60A 실측 + energy-exit 실전류 (3회째 이월) | [HW-게이트] | NEXT_STEPS §2.3-a, HANDOFF Not Yet Done | — | — | — |
| 11 | `app_eth` STATIC_UP 링크 재폴링 부재 | [HW-게이트·이연] | `fw/src/app_eth.c:258-262` (`case ETH_STATIC_UP: … break;` no-op); KA 10s(`app_modbus_tcp.c:30,91`) + 앱 유휴 12s(rsb `3ce1ead`)로 완화 | ≈15줄이나 **케이블 분리 벤치 없이 검증 불가** | 있음 | 이연 |
| 12 | KA 무송신-피어 잔여 | [사용자-보류·YAGNI] | sdd ledger "Modbus 실질 무해" | 0 | — | — |
| 13 | 감사 defer Minor(T1 경계벡터/NULL, T2 desync 로그·mon 50ms, T3 코스메틱) | [사용자-보류·YAGNI] | `.superpowers/sdd/progress.md:16-22` 전건 DEFER | 0 | — | — |
| 14 | `irq.c:10` HardFault 레지스터 덤프 TODO | [이연·YAGNI] | `fw/src/irq.c:10`; IWDG 가 `while(1)` 을 외부에서 끊는 설계(iwdg plan §0.2) | 0 | — | — |
| 15 | 컨트롤러 측 `MODEL_TYPE` 차단 여부 | [타-repo 판단 대기] | rsb `app_modbus.c:515-528` 주석("거부가 필요해지면 `app_estop_active() \|\| us_on_status` 로 막는 것이 그 자리") | 1줄 | 있음 | `gds_us_remote` |
| 16 | HMI SP1 Task 8 실보드 E2E | [타-repo] | `~/dev/work/gds_us_hmi` | — | — | — |
| 17 | `gds_us_remote` 상의 / `gds_us_hmi` 통보 (계약·IWDG 리셋) | [타-repo·벤치 후] | rsb HANDOFF §세션 간 협업; iwdg plan 함정 10 | — | — | — |
| 18 | 6b signal calibration 잔여 | [사용자-보류] | NEXT_STEPS §2.2 | — | — | — |
| 19 | B-SEAM 잔여 | [사용자-보류] | NEXT_STEPS §2.2 | — | — | — |
| 20 | `ponytail:` 의도적 단축 주석 | **해당 없음** | `grep -rn 'ponytail:' fw/` = 0건 | 0 | — | — |
| 21 | `app_reg.h:42` "no-op" (감사 stale 1/3) | **이미 해소** | `d035802`(2026-07-04, slice4)에서 "SEEK/RESET = app_seek_reset로 위임(D1 이후)" 로 교체됨 | 0 | — | — |
| 22 | FW3-6 "단위 테스트·통합 테스트·첫 릴리즈 태그" | [HW-게이트] | `docs/requirements.md:92`; 단위=host 스위트 존재, 통합=벤치, 태그=벤치 후 | 0 | — | — |

**[HW-불요] 합계 = #1~#6.** 코드 = #1·#2, 문서 = #3·#4, git = #5, 결정 = #6.

---

## 2. [HW-불요] 실행 순서와 이유

```
① #5-a  feat/symmetric-stop 삭제            (자명, 0 위험 — 문서의 "로컬 전용 5개" 가 참이 된다)
② #2    stale 주석 정정 (rsb)                (바이너리 동일, 컨펌 불요, #1 과 같은 파일이라 먼저)
③ #1    Modbus START pot fix (rsb)          (★ 컨펌 후. 거동 변화 — 별도 커밋으로 이등분 지점 확보)
④ #3    rsb changelog 항목                  (#1 결과를 포함해서 한 번에)
⑤ #4    진입 문서 4종 + 체크리스트 정렬       (①~④ 의 결과를 기록하므로 마지막)
⑥ #5-b  백업 브랜치 5개 처리                 (사용자 결정, 아무 때나)
⑦ #6    벤치 빌드 전략                       (결정만 — 실행은 벤치 당일)
```

- ②→③ 순서: 주석 커밋은 바이너리 동일 증명이 가능하고, fix 는 최초 거동 변화. 합치면 회귀 시 이등분 지점을 잃는다(iwdg plan §0.3 와 같은 원칙).
- 전부 **rsb 위**에서(§5 D-4). 근거: 2026-09-04 판 진입 문서·체크리스트·`app_modbus.c` 최신본이 rsb 에 있고, #1 의 HW 검증 항목이 rsb 벤치(Modbus START)에 자연히 포함된다. iwdg 는 4커밋 그대로 둔다(독립 revert 가능 유지).
- 작업 전 `git checkout feat/remote-status-bits` + `./fw.sh reconfig`(rsb 는 `app_cfg_stage.c` 등 신규 `.c` 보유 — GLOB 함정).

---

## 3. 항목별 실행 지시

### 3-A. #1 Modbus START 진폭 pot write 복원 — ★ 컨펌 필요

**원인** (조사로 확정, 코드 사실만):

| 사실 | 근거 |
|---|---|
| Modbus START 수락 시 `g_reg.us_run_status = src` 만 바뀌고 `g_measure.us_run_status` 는 **즉시 게시되지 않는다** | `fw/src/app_reg.c:195`(설정) vs `:390`(게시, `app_reg_tick` 안에서만) |
| `app_reg_tick` 은 매 iter `app_modbus_tick` **앞**에 돈다 | `fw/src/app.c:152` → `:161` |
| 가드 `app_lcd_measure()->us_run_status == US_COMM` 는 `&g_measure` 를 읽는다 | `app_lcd.c:23-27` → `app_reg.c:275-277` |
| → START 를 수락한 그 iter 의 가드는 **항상 직전 게시값(IDLE)** 을 봐서 FALSE, 같은 블록에서 START 레지스터가 소거되므로 재시도 없음 | `app_modbus.c:143-154`(main) |
| 이 사실은 2026-06-12 리뷰가 이미 알고 "log stub 이라 무해" 로 적었다 | `be9c60c`, 주석 `:147-151` "harmless while set_pot is a log stub" |
| 2026-06-28 I2C_POT 실구동 머지(`hw-revA_fw-stage-i2c-pot`)로 stub 이 아니게 됐는데 가드는 재검토되지 않았다 | `app_lcd.c:31-39` = `i2c_pot_set_dac()` 실호출 |
| FC06 OUT_POWER 쓰기 경로도 pot 을 쓰지 않는다 | `app_modbus.c:184-189` |
| **결과**: 원격이 OUT_POWER 를 바꾼 뒤 START 해도 U4 wiper 는 부팅/LCD 값에 머문다. 벤치가 못 잡은 이유 = OUT_POWER 가 늘 55/56(부팅값과 동일) | — |
| legacy 는 comm START 수락 분기 안에서 무조건 pot write | `ref/samd20/main.c:4393-4402` |
| LCD RUN-press 경로는 가드 없이 무조건 write — Modbus 만 다르다 | `app_lcd_input.c:216-217`, `:240-242` |

> ⚠ 메모리 `project_i2c_pot_amplitude` 의 "START 경로가 set_pot 확실 호출(app_modbus.c:117) … write 실행+ACK 확정" 은 **추론 오류**다 — 근거였던 `s_err_count=0` 은 부팅 write(`app.c:65`) 한 번만으로도 성립한다. 메모리 정정은 주 세션 판단(이 문서는 손대지 않음).

**수정안 (ponytail — 가드 삭제, LCD 경로와 동형화)**: rsb `fw/src/app_modbus.c` (`grep -n 'stub hook logs until'` 로 위치 재확보)

```c
    } else if (g_mb.holding[MB_REG_START] == 1u) {
        app_reg_command(US_CMD_START, (uint8_t)US_COMM);
        /* samd20 comm START 는 같은 자리에서 진폭 pot 을 쓴다(main.c:4400-4401).
         * LCD RUN-press 경로(app_lcd_input.c:217/242)와 동형 — 무조건 write. 거부된
         * START 여도 출력이 없어 무해(멱등 1바이트). 구 가드는 g_measure 가 app_reg_tick
         * 에서만 게시돼 같은 iter 에 항상 FALSE 였다(2026-06-12 리뷰 NOTE, 2026-09-04 fix). */
        app_lcd_hook_set_pot(cfg->output_power);
        g_mb.holding[MB_REG_START] = 0u;
```

- 삭제: `if (app_lcd_measure()->us_run_status == (uint8_t)US_COMM) {` + NOTE 8줄 + 닫는 `}`.
- 대안(비추천): `app_reg` 에 라이브 접근자 신설 후 가드 유지 — API 1개 추가, 얻는 것 없음(거부 시 write 회피뿐, LCD 는 이미 안 함).
- **거동 변화**: 원격(RTU/TCP) START 마다 I2C 1바이트 write 1회 추가(best-effort, `i2c_pot_set_dac` 는 err_count 만 올림). legacy **복원**이지 이탈 아님.
- **위험**: I2C1 버스가 죽어 있으면 50 ms 타임아웃 1회 추가 — IWDG 마진 계산(단일 iter 최악 2.6 s)에 흡수됨. 부팅·LCD 경로가 이미 같은 write 를 한다.
- host 테스트: 없음(글루). 검증 = **벤치 1항목 추가** — 체크리스트 §3 S 섹션 뒤에:

  | S-P | Modbus START 진폭 pot write | comm=ETH_STATIC + RS-485 mon 청취 상태에서 `mbpoll -m tcp -a 1 -t 4 -r 28 192.168.1.199 1` | mon 에 `[lcd-hook] set_pot power=NN dac=NN` 1줄 + STATUS bit0=1 → ceiling 정지. 실패 시 의심 = 이 커밋 |

- 커밋 subject: `fix(modbus): 원격 START 시 진폭 pot 기록 복원 — stale 게시 가드 제거 (samd20 main.c:4401 복원, 사용자 승인)`. body 에 위 원인 표 요지 + 거동 변화 + "LCD 경로 동형".
- 완료 판정: our-code 0-warning(STD·REMOTE 양쪽) + host 16 무회귀 + `git diff` 가 그 블록 1곳뿐 + 체크리스트 행 추가.

### 3-B. #2 stale 주석 정정 — 컨펌 불요 (바이너리 동일)

전부 rsb 기준. 편집 후 `md5 fw/build/gds_us_ctrl.bin` 전후 동일 확인(`build-remote/` 도).

| 파일 | 위치(인용으로 재확보) | 현재 | 정정 | 근거 |
|---|---|---|---|---|
| `fw/src/app_modbus.c` | `effect = no-op this slice` (rsb :328-329 / main :134-135) | "RESET → effect = no-op this slice (RESET->SEEK chain + error machine deferred, spec §3.3)" | "RESET → app_reg_command 가 app_seek_reset 에 위임: RESET→SEEK 자동 체인 + 물리 OSC 구동 + fault 클리어(tag seekreset / `29803ae`)" | 2026-06-19 머지, 2026-07-05 물리 구동 |
| `fw/src/app_modbus.c` | `/* no-op, deferred */` (rsb :333 / main :139) | SEEK 호출 뒤 "no-op, deferred" | "SEEK 단발 → app_seek_reset 위임" 또는 주석 삭제 | 동일 |
| `fw/src/app_modbus.c` | `stub hook logs until` (rsb :338-345) | set_pot NOTE 8줄 | **#1 fix 가 삭제** — #1 미채택 시 "stub" 문구만 "실구동(i2c_pot.c)" 로 | `hw-revA_fw-stage-i2c-pot` |
| `fw/src/app_lcd_render.c:15-16` | `no real I2C this stage — U4 identity is open Q F2` | 헤더 | "→ `app_lcd_hook_set_pot()` = U4 @0x28 실구동(drivers/i2c_pot.c). 칩 스케일/극성은 6b" | 동일 |
| `fw/src/app_lcd_render.c:110` | `(F2 open)` | 트레일링 | `(I2C_POT 실구동)` | 동일 |
| `fw/src/app_lcd_input.c:242` | `(stub, F2)` | 트레일링 | `(I2C_POT 실구동)` | 동일 |
| `fw/src/app_lcd.c:25-26` | `Cycle/freq/energy/status stay 0 until slice 2` | 헤더 | "app_reg 가 게시하는 라이브 측정값(진폭·주파수·에너지·상태 전부)" | slice 2 이후 전부 라이브 |
| `fw/test/Makefile:2-4` | 스위트 목록 | 15개 나열(`cfg_stage` 누락) | `… horn_fsm / cfg_stage / remote_en_fsm.` (BIN_STG·BIN_REN = 16) | `grep -c '^BIN_'` = 16 |

- `fw/include/app_reg.h:42` 는 이미 정정됨(#21) — 건드리지 말 것.
- `fw/src/irq.c:10` TODO 는 stale 아님(열린 TODO) — 유지.
- 커밋 subject: `docs(code): 2026-07-02 감사 stale 주석 정정 — seek/reset 위임·I2C_POT 실구동 반영 (바이너리 동일)`. body 에 md5 전후.
- 완료 판정: `.bin` md5 동일(STD·REMOTE) + 0-warning + `git diff --stat` 가 위 6파일 주석 줄만.

### 3-C. #3 rsb `docs/changelog.md` 항목 신설

`[Unreleased]` 최상단(iwdg 항목은 iwdg 브랜치에 있으므로 rsb 에서는 첫 항목). 골격 — 해시는 `git log --oneline main..feat/remote-status-bits` 로 채움:

```
### 2026-09-04 — 원격기 기능 동등성 스택 (요구사항 2026-08-30 A·B-1~B-5·C) — HW 벤치 대기

브랜치 `feat/remote-status-bits` (base = main `2f74611`, 20커밋 + 이번 세션분). 벤치 = `plans/2026-09-04-remote-parity-bench-checklist.md`.

- **제품 모델 축 STD/REMOTE** (`2f74611` main / 브랜치 `0e723de`): H/W 동일, `MODEL=remote ./fw.sh` → `build-remote/`, 버전 `V3.0.0`/`V3.0.0R`. 게이트는 REMOTE 전용.
- **원격 활성화 게이트 = PC8 물리 인터록** (`98c60f1` 편입 + `0e723de` 재설계): active-LOW+풀업 fail-safe, 만료 없음, `DIS_ESTOP` 만 래치·`DIS_LINK` 자동 복귀. T-5(LCD 활성화) 폐기.
- **STATUS 관측 비트** SENSOR 0x20 · HORN 0x40 (`4c92e0c`).
- **F-A comm/eth staging+commit** `0x1E~0x29` (`86a292a`) · **CFG_CAP** `0x31` (`a9ba247`) · **calibration** `0x2E/0x2F` + int16 계약 + 클램프 (`f206b3a`) · **EN_SAFTY 0/1 정규화** (`c5c2f7e`, samd20 이탈·사용자 승인) · **work_cnt 리셋 가드 32비트** (`0ab2608`, samd20 이탈·사용자 승인).
- **B-5 모델 선택·주파수 원격 쓰기** (`deb48bb`, 가드 없음 = LCD 동형·사용자 결정) · **B-4 HORN_CMD** `0x30` (`4c4b792`).
- **TCP 연결 자동 복구** 앱 유휴 12s + 코드리뷰 6건 (`3ce1ead`).
- (#1 채택 시) **Modbus START 진폭 pot write 복원** (`<hash>`): 2026-06-12 NOTE 가드가 게시 시점 문제로 항상 FALSE → I2C_POT 실구동(2026-06-28) 이후 원격 START 가 pot 미기록. 가드 삭제 = LCD 경로 동형, samd20 main.c:4401 복원.
- ⚠ 레지스터 여유 7칸(FC03 상한 57). 게이트: 0-warning / host 16 / FLASH 50.38 %.
```

- 완료 판정: 20커밋 전부가 위 불릿 중 하나에 매핑(docs 커밋 제외).

### 3-D. #4 진입 문서 4종 + 체크리스트 정렬 (rsb)

최소 편집. 각 줄은 인용으로 재확보.

| 파일 | 위치 | 편집 |
|---|---|---|
| `CLAUDE.md` | `:144` "4. **전류 0.60A 실측** / **6b·B-SEAM**(사용자 보류) / **IWDG**" | IWDG 를 분리해 새 줄: "5. **IWDG 워치독 = 코드-완료**(`feat/iwdg-watchdog`, main+4, origin 동기; spec/plan `2026-09-04-iwdg-watchdog*`) — **T-3 HW 벤치 V-1~V-6 대기**. rsb 와 `merge-tree` 충돌 0" |
| `CLAUDE.md` | `:148` "**5개는 로컬 전용**(의도적으로 보이나 미확인)" | §4 결과로 교체("`feat/symmetric-stop` 삭제됨(main 조상); 백업 5개 = <결정>") |
| `docs/NEXT_STEPS.md` | `:61` IWDG 항목 | 앞에 "✅ **CODE-COMPLETE 2026-09-04** (브랜치·spec·plan) — 남은 것 T-3 벤치. " 추가, 뒤의 "착수 시 … 설계 필요" 삭제 |
| `docs/NEXT_STEPS.md` | `:101` "**2026-08-16 현재 우선순위**: ① remote-gate T-5 …" | "**2026-09-04 현재 우선순위**: ① **통합 벤치**(rsb 체크리스트 + IWDG V-1~V-6, §5 D-3 방식) → ② HMI Task 8 → ③ 전류 실측 → ④ 6b·B-SEAM(보류)" |
| `docs/NEXT_STEPS.md` | §2.1 `make -C fw/test test   # 5 스위트 PASS 기대` (main :94 / rsb :96) | "# rsb 16 / main 14 스위트" (겸사겸사) |
| `HANDOFF.md` | `:31-34` "## 미해결 2건" | 로컬 브랜치 항목을 §4 결과로 교체 + 3번째 불릿 "IWDG 브랜치 별도 존재 — 벤치 통합 방식 = §5 D-3" |
| `docs/superpowers/RESUME.md` | `:3` "main 위 18커밋" | "20커밋(+이번 세션분)" + 문단 끝에 1문장: "**IWDG 워치독은 별도 브랜치 `feat/iwdg-watchdog`(main+4) 에 코드-완료** — 같은 벤치 세션에서 V-1~V-6." |
| `plans/2026-09-04-remote-parity-bench-checklist.md` | `:3` "main 위 7커밋" | "main 위 20커밋(+)" |
| 〃 | `:287` "3. 남은 요구사항: B-5 · B-4 조작" | 삭제(둘 다 구현됨 `deb48bb`/`4c4b792` — MOD·B34-7 항목이 이미 있다) |
| 〃 | §3 말미 | 새 섹션 "### W — IWDG (iwdg 브랜치, §5 D-3 통합 빌드 시)" 1행: "V-1~V-6 = `plans/2026-09-04-iwdg-watchdog.md` T-3 절차 그대로. V-3 hang 주입은 throwaway 편집·커밋 금지." + (#1 채택 시) S-P 행 |

- 완료 판정: `grep -n IWDG` 가 4종 모두에서 "코드-완료/벤치 대기" 문맥으로 잡힘; 체크리스트에 "7커밋"·"남은 요구사항" 없음.
- 커밋 subject: `docs: 진입 문서 정렬 — IWDG 코드-완료 반영 + 로컬 브랜치 실측 + 체크리스트 stale 3곳`.

---

## 4. 로컬 전용 브랜치 6개 처리안

실측(2026-09-04):

| 브랜치 | main 과의 관계 | 판정 | 처리 |
|---|---|---|---|
| `feat/symmetric-stop` `8b0943a` | **main 의 조상** (`git merge-base --is-ancestor` = yes, 머지 `6252566`). `git log main..feat/symmetric-stop` = 0 | **자명** | `git branch -d feat/symmetric-stop` (`-d` 가 통과한다 = 머지됨의 증거) |
| `backup/pre-d5-slice-b` `a5d984f` (main 에 없는 5커밋) | 조상 아님 — D5 reconcile(2026-07-04 d)이 **cherry-pick** 으로 b' 를 재구축했고 b' 는 `0cc34a8` 로 머지(tag `-physio-b`) | 내용은 main 에 등가 반영·HW 검증 완료 | 사용자 판단(§5 D-2) |
| `backup/pre-d5-slice-d` `d70600c` (40) | 위와 동일, d' = `46055c9`(tag `-physio-d`) | 〃 | 〃 |
| `backup/pre-d5-ch1` `27b6888` (7) | 위와 동일, ch1' = `e973721`(tag `-power-ch1`) | 〃 | 〃 |
| `feat/physical-io-slice-a` `0e2408b` (7) | 조상 아님 — a ⊂ c ⊂ d, d' 로 흡수 | 〃 | 〃 |
| `feat/physical-io-slice-c` `7483d77` (25) | 〃 | 〃 | 〃 |

**`feat/symmetric-stop` diff 가 "3파일 2+/27−" 로 보이는 이유**: `git diff main..feat/symmetric-stop` 은 "main 에서 그 브랜치로 가려면 무엇을 바꾸나" 이고, main 이 그 뒤 `2f74611`(MODEL 축)로 `define.h` 22줄·`CMakeLists.txt` 6줄·`.gitignore` 1줄을 **추가**했으므로 역방향 diff 에는 그 추가분이 삭제로 보인다. 브랜치 고유 내용은 0 — 반대 방향 `git diff feat/symmetric-stop..main --stat -- fw/` 가 정확히 같은 3파일 27+/2− 인 것이 증거.

**덤 (origin 동기지만 역할 종료)**:
- `refactor/ponytail-cleanup` — main 에 머지됨(`4d9a0f4`). 로컬 `git branch -d` 자명. origin 삭제(`git push origin --delete`)는 사람 터미널(SSH) — 사용자 판단.
- `feat/remote-enable-gate` `bc2067c` — rsb 에 머지됨(`98c60f1`), main 에는 아직 없음. **rsb 가 main 에 들어간 뒤** 로컬+origin 삭제. 지금은 유지.

문서 정합: `feat/symmetric-stop` 을 지우면 rsb `CLAUDE.md:148`·`HANDOFF.md:34` 의 "로컬 전용 5개" 가 **그대로 참**이 된다(현재는 6개라 부정확).

---

## 5. 사용자 컨펌이 필요한 결정

| # | 결정 | 선택지 | 추천 | 근거 |
|---|---|---|---|---|
| **D-1** | Modbus START 진폭 pot write 복원(#1) 채택 + 위치 | (a) 채택, rsb 위 별도 커밋 (b) 채택, main 에서 새 브랜치 `fix/modbus-start-pot` (c) 보류(주석만 "stub 아님" 으로) | **(a)** | 거동 변화라 HW 검증이 필요한데 rsb 벤치가 Modbus START 를 어차피 돌린다. (b) 는 HW-게이트 브랜치를 셋으로 늘린다. (c) 는 원격기 동등성 요구사항(원격 OUT_POWER→START)과 정면 충돌하는 결함을 알고도 두는 것 |
| **D-2** | 백업 브랜치 5개(`backup/pre-d5-*`·`feat/physical-io-slice-a/c`) | (a) 삭제 (b) origin 푸시(보관) (c) 로컬 유지(현상) | **(a)** | D5 reconcile 산출물 b'/d'/ch1' 이 2026-07-04 HW 검증·머지·태그됐고 2개월간 참조 0. origin 에는 처음부터 없었다 = "origin 백업" 이 의도였던 적이 없다. reflog 90일이 안전망. (b) 는 origin 에 죽은 브랜치 5개 |
| **D-3** | 벤치 빌드 전략 | (a) throwaway 통합 브랜치(`git checkout -b bench/2026-09 feat/remote-status-bits && git merge feat/iwdg-watchdog`) → 플래시 1회, 체크리스트 S…A + W → PASS 후 main 에 rsb·iwdg **각각** `--no-ff`+태그, bench 브랜치 삭제 (b) 순차: iwdg 벤치→머지→rsb 재베이스→벤치(플래시 2회) (c) iwdg 4커밋을 rsb 위로 rebase(force-push) | **(a)** | `merge-tree` 충돌 0. 브랜치 구조·푸시 이력 무변경, IWDG 독립 revert 가능 유지. IWDG V-3 hang 주입은 어느 방식이든 throwaway 편집 |
| **D-4** | 진입 문서 정렬·changelog·이 문서의 커밋 위치 | (a) rsb (b) main docs-only 커밋(`ec1b972` 선례) | **(a)** | 2026-09-04 판 문서가 rsb 에만 있어 main 에 쓰면 머지 시 docs 충돌(관례상 해소 가능하나 무의미). iwdg 는 손대지 않는다 |
| D-5 (통보) | `feat/symmetric-stop` 삭제 · `refactor/ponytail-cleanup` 로컬 삭제 | — | 실행 | 둘 다 `git branch -d` 가 통과 = main 조상. origin 의 ponytail-cleanup 삭제만 사람 터미널 |

---

## 6. 이번에 할 수 없는 것 (오판 방지)

| 항목 | 이유 |
|---|---|
| rsb·iwdg **main 머지·태그** | 둘 다 HW 벤치 게이트. #1 fix 도 rsb 벤치에 동승 |
| IWDG V-1~V-6, rsb S~A, S-P(pot write) | 보드. A 섹션은 PC8 PCB 까지 |
| `gds_us_remote`·`gds_us_hmi` 계약 문서 갱신 | 양쪽 규율 = 벤치 PASS 후. IWDG "리셋 = TCP 단절" 통보도 벤치 후 |
| `MODEL_TYPE` 원격 쓰기 차단(#15) | `gds_us_remote` 사용자 판단 대기. 자리는 rsb `app_modbus.c:527` 주석 |
| `app_eth` STATIC_UP 링크 재폴링(#11) | 코딩은 가능하나 **케이블 분리 벤치 없이 검증 불가** + KA·유휴 타임아웃으로 완화됨 → 이연(YAGNI) |
| KA 무송신-피어 / 감사 defer Minor / HardFault 덤프 | 전건 코스메틱 또는 실질 무해 — 일감 만들지 않음 |
| 전류 0.60A · 6b · B-SEAM · HMI Task 8 | HW-게이트 / 사용자 보류 / 타-repo |
| 메모리 `project_i2c_pot_amplitude` 정정 | 부수 작업 — 주 세션이 #1 컨펌 시 함께 판단 |

**문서-코드 불일치 (기록만, 이 문서 범위 밖)**:
- `plans/2026-09-04-iwdg-watchdog.md` §0.1 "host 16 스위트" — iwdg 는 main 기반이라 실제 **14**(`grep -c '^BIN_' fw/test/Makefile`). rsb 기반이면 16. T-3 docs 커밋 때 한 단어.
- 세션 스냅샷 "현재 브랜치 = rsb" ≠ 실제 HEAD iwdg — 작업 전 `git branch --show-current` 확인.
