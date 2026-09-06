# 원격 hold-to-run 워치독 — HW 벤치 결과 (2026-09-06)

> **문서 요약**: 브랜치 `feat/hold-to-run-wdt`(main +11: 날짜 `16db144` · 도구 정정 `0a9c527` · horn 수정 `96dc7d5` 포함)를 실보드에 올려
> spec `2026-09-06-remote-hold-to-run-design.md` §8 의 H-0~H-16 을 실행했다. **실행 16항목 전건 PASS**(H-11·H-15 는 설계상 이연).
> 벤치가 **기존 결함 1건을 찾아 같은 브랜치에서 고쳤다** — horn 모드가 진행 중인 초음파 런을 세우지 않던 것(H-12, `96dc7d5`). 핵심 실측: **트립 608 ms**(T=600 + 폴), 30 s 상한이
> keep 을 이기고(H-8), E-STOP·패널 탭·STOP 어느 정지 뒤에도 keep 이 재기동을 못 한다(H-6/H-13/H-7),
> 구 펌웨어는 START=2/3 에 무반응(H-0). **STD 빌드에서도 hold 가 산다**(§0 A 실증).
> 벤치가 드러낸 관측 5건을 §4 에 적었다 — 특히 "침묵 뒤 첫 프레임 거부"
> 와 "warm-up 중 무음 거부" 는 원격기가 START=2 재시도를 갖는 이유의 실측이다.

## 1. 환경

| 항목 | 값 |
|---|---|
| 보드 | hw-revA, `comm_mode=ETH_STATIC` 192.168.1.199, `model_type=2`(std → on-time ceiling 미적용, 30 s 절대 상한만), `output_power=77` |
| 플래시 전 펌웨어 | `405f95e` REMOTE(비트맵 이전) — H-0 대상 |
| 시험 빌드 | STD `V3.0.0_260906` 66664 B / REMOTE `V3.1.0R!_260906` 66984 B (`16db144`) |
| 게이트 | PC8 극성 반전(미실장), `REMOTE_EN=1` 열림 |
| 소켓 | `gds_us_remote` 가 halt 로 반납(세션 내 재접속 없음) |
| 도구 | `docs/superpowers/tools/mb_tcp.py`(인라인 스크립트), mon = RS-485 어댑터 115200 8N1 캡처 |
| 규칙 | `nc -z` 금지 · 생존 판정 = FC03 · mbpoll 불가 (bench-results 2026-09-05 §4) |

## 2. 결과

| # | 원격기 | 내용 | 결과 | 실측 |
|---|---|---|---|---|
| H-0 | HV-8 | 구 펌웨어(405f95e)에 START=2·3 | **PASS** | 각 48회 폴 `US=0`, `0x1B` 잔류 2/3, mon 무반응, 51칸 읽기 무응답 |
| H-1 | HV-11 | STD: `0x32`·51칸·52칸 | **PASS** | `0x32=0x0001`, 51칸 응답, 52칸 무응답, `0x2A=0`(STD 잔류) |
| H-1 | HV-11 | REMOTE: 동일 | **PASS** | `0x32=0x0001`, `0x2A=0x5201`, `0x2B=1` |
| — | — | STD hold 스모크(§0 A) | **PASS** | 3 s 19/19 `US=1`, keep 중단 후 정지, `[mb] hold wdt trip` |
| H-2 | HV-1 | hold 15 s, keep 150 ms | **PASS** | keep 100회, `US=1` 100/100 (horn 수정 후 5 s 회귀 34/34, 트립 603 ms) |
| H-3 | HV-2 | keep 중단 → 정지 | **PASS** | **608 ms**(마지막 keep 기준, 10 ms 폴), mon trip |
| H-4 | HV-3 | 탭 START=1 폴링만 | **PASS** | 30.09 s 자연 정지(30 s 상한) — 워치독 무간섭 |
| H-5 | HV-4 | IDLE 에서 keep 만 5 s | **PASS** | 32회, `US=0` |
| H-6 | HV-5 | hold 중 E-STOP → 해제 → keep | **PASS** | 0.91 s E-STOP: `US=0`·bit1·`0x2B=4`; 2.86 s 해제: `0x2B=1` 자가 복귀; 이후 21 s keep 에 `US=0` |
| H-7 | HV-6 | hold 중 STOP → keep | **PASS** | keep 12회 `US=0` |
| H-8 | HV-7 | hold+keep 35 s | **PASS** | 30.01 s 정지, 이후 keep 34회 `US=0`, mon trip 없음(조용한 세션 종료) |
| H-9 | HV-9 | keep 550 ×3 / 650 ×3 | **PASS** | 550: [560,1110,1652] ms 전부 생존 / 650: [658,…] 첫 keep 전 트립 |
| H-10 | HV-10 | keep 직후 cfg 쓰기(FRAM 저장) | **PASS** | keep 20회 + OUT_POWER 토글 3회, `US=1` 20/20 |
| H-11 | HV-12 | 게이트 스위치 OFF | 이연 | PC8 = 초기 설정 스위치(spec §0) |
| H-12 | HV-13 | hold 중 LCD horn ON (수정 전) | **결함 발견** | 5.41 s horn ON(bit6) — **진행 런 계속**. 게이트 2곳이 새 START 만 막고 진행 런은 안 세움 = hold-to-run 이전부터의 구멍(레거시 SYS_HORN 은 RUN 분기 자체 배제) → 사용자 지적 → **`96dc7d5` 수정**(horn 모드 중 매 tick RUN_RELEASE, E-stop 패턴) |
| H-12′ | HV-13 | hold 중 horn ON (수정 후, `0x30` 경로) | **PASS** | horn ON → **13 ms** 정지, 이후 keep 10회 `US=0`, bit6=1. LCD 경로도 실증(재실행 중 LCD horn ON 이 hold 를 193 ms 에 세움) |
| H-12″ | — | 탭 런 중 horn ON (수정 후) | **PASS** | horn ON → **16 ms** 정지 |
| H-13 | HV-14 | hold 중 패널 RUN 탭 | **PASS** | 1.06 s 정지(mon `us_command=0`→`3` 대칭 정지), 이후 24 s keep 에 `US=0` |
| H-14 | HV-16 | keep 10/s 소크 10 분 | (아래 §3) | |
| H-15 | HV-15 | RTU | 이연 | 9600 산술(spec §5) |
| H-16 | — | 회귀 (horn 수정 후 재확인 PASS) | **PASS** | 탭 START/STOP · OUT_POWER 80→77 read-back · `0x31=0xFA01` · **START=99 → STOP=1 지연 없음**, `0x1B` 잔류 0 |

mon 대조: hold 기동마다 `set_pot` 1회(START=2 수락 시에만), keep 은 pot 무접촉, 트립마다 `[mb] hold wdt trip` 1회.

## 3. H-14 소크 (PASS)

horn 수정(`96dc7d5`) 플래시 후, LCD horn down OFF 저장 상태에서 **614 s · 24 사이클**(hold 25 s + keep 10/s → keep 중단 → 트립 → 다음 사이클).

| 항목 | 값 |
|---|---|
| keep 총 | **6000회** (≈9.8/s, `mb.write` 왕복 포함) |
| `US=1` 관측 | **6000/6000** (hold 중 정지 0) |
| 트립 | 24/24, 루프 종료 기준 502/510/517 ms (min/avg/max) = 마지막 keep 기준 ≈610 ms |
| 기동 실패 | 0 |
| 종료 STATUS | `0x0000`, OUT_POWER 77 |
| I2C | STATUS 에 fault 비트 0; mon `[i2c] err` 0 (소크 구간 mon 은 `set_pot` 1줄만 잡혀 캡처 결손 의심 — FC03 STATUS 가 1차 증거) |

1차 소크(수정 전)는 사이클 1 뒤 LCD 가 `horn down=1` 을 다시 보내 2~15 가 무효였다(§4-5). 그 사이클 1 도 horn ON 뒤 25 s 완주 = H-12 결함의 재현.

## 4. 벤치가 드러낸 기존 거동 (이 브랜치 결함 아님 — 원격기 공유 대상)

1. **침묵 뒤 첫 프레임은 게이트가 거부한다.** 10 s 이상 요청이 없으면 `REMOTE_EN` 이 `DIS_LINK(3)` 로 닫히고, 다음 프레임은 `remote_en_step` 이 tick 첫머리에서 **직전 tick 의 요청 스탬프**를 보므로 그대로 거부된다(mon `[mb] gate closed(state=3): blocked=0x1B`). 그 다음 프레임부터 열린다. 원격기는 16/s 폴로 침묵을 안 만들지만, **유휴 마스터의 첫 명령은 항상 한 번 버려진다** — 명령 전 읽기 1회로 프라임하면 회피.
2. **부팅 warm-up 중 START 는 무음 거부.** `start_allowed()` 의 `main_state==0` 조건. 부팅 ~7 s 에 보낸 START=2 가 로그 없이 무시됐다(H-2 첫 시도 FAIL 의 원인 — 플래시·부팅·시험을 한 호출에 묶어서 너무 일렀다). 탭 START=1 도 동일. **원격기의 "START=2 재시도" 가 정확히 이 구간을 덮는다.**
3. ~~horn 모드는 진행 런을 세우지 않는다~~ → **결함으로 판정하고 이 브랜치에서 수정**(`96dc7d5`, 사용자 승인). 레거시 SYS_HORN 은 RUN 분기 자체를 배제하므로 포트의 "새 START 만 차단" 은 이탈이었다. 수정 후 hold(13 ms)·탭(16 ms)·LCD 경로(193 ms, 우연 관측) 전부 정지 확인.
4. **rig 과부하 신호 1회, 자동 복구** — 재실행 중 STATUS `0x0004`(OVLD) 가 한 번 서서 START 가 `app_overload_active()` 게이트에 막혔고(H-16a·H-12 1차 재실행 FAIL 의 원인), 수 초 뒤 RESET→SEEK 자동 체인으로 `0x0000` 복귀. 실 초음파 rig 의 OSC 보드 신호이고 이 브랜치와 무관. **START 거부는 무음**이라 STATUS 전체를 같이 읽어야 원인이 보인다.
5. **LCD 조작이 벤치를 오염시킨다** — 소크 사이클 2~15 무효(LCD SETUP 저장이 `horn down=1` 을 다시 보냄), 재실행 1차도 같은 원인. 벤치 중 LCD 는 손대지 않는다.

벤치 도구 교훈: **keep 데드라인의 기준점은 START=2 송신 순간**이어야 한다. 기동 확인(+50 ms + 왕복) 뒤에 잡으면 첫 keep 이 arm 후 ~615 ms 에 가서 550 ms 케이스가 오판된다(H-9 첫 실행). `mb_hold.py` 도 같은 결함이 있어 **`0a9c527` 로 정정**(기준점 = START 송신 순간 + 침묵 회피용 링크 프라임 읽기 1회).

## 5. 보드 마감 상태

REMOTE `V3.1.0R!_260906`(`96dc7d5` 빌드, horn 수정 포함) 플래시. cfg 무변경(OUT_POWER 77 복원 확인). horn down = LCD 에서 OFF 저장. 게이트 열림(극성 반전, 배포 금지 그대로).

## 6. 다음

`--no-ff` 머지 + 태그 `hw-revA_fw-stage-hold-wdt` → `app_modbus_core.h` 값을 계약 확정으로 `gds_us_remote`·`gds_us_hmi` 통보(§4 1·2 포함) → 소켓 반환.
