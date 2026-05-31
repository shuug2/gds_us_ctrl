# HANDOFF — STD comm 표시결함 해결(에셋) → LCD 브랜치 마감 + Stage D (2026-05-31)

> 브랜치 `feat/stage-lcd-full-behavior` (main 미머지, tip `9b399c2`, **firmware fix A/B/C/D + 문서 미커밋**).
> 다음 세션은 이 문서의 **체크리스트 순서대로** 진행하면 됨. 결정 필요 항목은 ⚑ 표시.

---

## 0. 무슨 일이 있었나 (1분 요약)

이번 세션: 버그B(STD ether/comm_mode 영속+표시)를 coherent **A/B/C/D**로 수정 → 트레이스 빌드로 HW 검증 중 **STD comm 페이지 표시결함** 발견(전원사이클 후 ethernet인데 serial로 표시 / 토글 시 아이콘 사라짐).

**근본원인 규명(검증완료)**: firmware 회귀 아님. render가 STDE(page 27)/MHE(page 25)에 **byte-동등** 출력(SAMD20와도 byte-동등). **교차테스트**로 확정 — STD comm 진입을 page 25로 repoint하니 정상 → **DGUS 패널 page 27 위젯이 page-show 시 `DISP_COMM_MODE`(0x140c) auto-load를 안 하는 것**이 root (page 25는 함).

**사용자 해결(2026-05-31)**: **코드 미변경, LCD 에셋(DGUS 패널)만 수정 → 정상 동작.** = 진단 최종 입증. (firmware의 Fix C는 같은 증상에 대한 우회였는데, 에셋이 root에서 고쳐졌으므로 이제 **C는 잉여**가 됨 — §2 결정.)

상세 조사기록: `analysis/2026-05-31-std-comm-page27-display-port-faithful.md`, `analysis/2026-05-27-std-ether-comm-mode-persist-display.md`.

---

## 1. ✅ 완료 — 수정된 DGUS 에셋 레포 반영 (커밋 `0eafe68`)

사용자가 `hw/lcd/dgus/14ShowFile.bin`(디스플레이 설정) 갱신본을 레포에 넣음 → 커밋 `0eafe68`. 변경 = **`14ShowFile.bin` 단일**(`git diff --stat` 확인: 22400→22400 bytes, 내용변경). page 27/23 comm-mode 아이콘 위젯 표시설정 fix.

**남은 권장(선택)**:
- **편집가능 DGUS 프로젝트 소스**가 별도 보관됐는지 확인 — 레포엔 컴파일된 `.bin`/`.icl` 출력만. 다음 에셋 편집은 출력에서 시작하게 되니, DWIN 프로젝트 원본 위치를 `hw/lcd/dgus/README.md`에 기록 권장.
- README에 "page 27/23 comm-mode 아이콘 표시설정 수정(`14ShowFile.bin`, 2026-05-31)" 한 줄 기록.

> 확인용: 에셋만으로 고쳐졌으므로, **firmware Fix C를 되돌려도(§2)** 표시 정상이어야 함 = §2 재검증의 핵심.

---

## 2. ⚑ Firmware fix A/B/C/D 처분 결정

working tree(미커밋) 4개 수정. 디스플레이 root가 에셋에서 해결됐으므로 C 재검토 필요:

| fix | 파일 | 내용 | 처분 |
|---|---|---|---|
| **A** | app_lcd.c init_mode | 부팅 `temp_comm_mode=0xFF` sentinel (헤더 의도 갭) | **유지** (섀도우 생명주기 정합, 독립 유효) |
| **B** | app_lcd_input.c commit_comm_mode_and_ether | 0xFF early-return 손상가드 | **유지** (STD-persist deviation이 연 손상경로 차단, SAMD20도 무가드 잠복) |
| **C** | app_lcd_render.c set_page 후 재기록 | page 27 auto-load 부재 우회 | ⚑ **재검토**: 에셋이 root 해결 → 잉여. **C 되돌리고 재검증**해서 정상이면 revert(SAMD20 byte-faithful 유지). 에셋fix가 불안하면 방어용으로 유지 가능(잉여-무해 설계). |
| **D** | app_lcd_render.c serial 분기 | `DISP_EN_DHCP=0` stale 제거 | **유지** (독립 유효, 1줄) |

**권장**: §1 에셋 커밋 후 **C revert → 머지빌드로 전체 재검증**(§4). 정상이면 C 빼고 A/B/D만. 그래야 firmware가 SAMD20 충실 유지 + 패널이 root fix.

> ⚠️ **세션 미해결이었던 "토글 아이콘 사라짐"은 에셋 수정으로 함께 해결됐는지 §4에서 반드시 재확인.** (당시 page 23 STDC 관여 의심 + `0x140B→page=9` 네비 + `tcm=2 cm=1` 섀도우 stale 관측 — 에셋fix로 표시가 정상이면 이들은 표시 아닌 별개 이슈인지 확인. 코드 트레이스로 토글 시 page 전이/ tcm 값 점검.)

---

## 3. 진단 스캐폴딩 정리 (머지 전)

- `LCD_TRACE_RX` 게이트 트레이스: app.c `boot cm`, input.c `commit cm`/`rx vp`, **render.c `page= tcm= cm=`(이번 세션 추가)**. 전부 `#ifdef LCD_TRACE_RX`라 머지 바이너리엔 컴파일 아웃. ⚑ **유지(향후 유용) vs 제거** 결정.
- `fw/build-trace/` = untracked 트레이스 빌드 디렉토리. **삭제** 또는 `.gitignore` 추가.
- CMakeLists는 이미 원복(트레이스 OFF, clean). 트레이스 재활성 = `add_compile_definitions`에 `LCD_TRACE_RX` 한 줄(빌드→원복).

---

## 4. 전체 재검증 (에셋 수정 + 최종 firmware로)

머지 바이너리(`build/`, 트레이스 OFF)로 플래시 후 전 모드 재검증:

| # | 절차 | 기대 |
|---|---|---|
| 진입 | HAND/MULTI/STD 각 comm 페이지 진입(전원사이클 후) | comm_mode대로 serial/ethernet/dhcp 아이콘 정상 |
| **토글** | serial→ethernet→dhcp 버튼(0x140B) | 각 아이콘 정상 갱신, **사라짐 없음** (이번 세션 실패분 — 재확인 필수) |
| 영속 | DHCP 저장→전원사이클→재진입 | eth O+dhcp O 유지 (`boot cm=2`) |
| 영속 | serial 저장→전원사이클 | serial만 (stale dhcp X) |
| 손상가드(B) | 셋업→STD3 값편집→comm 미방문 SAVE | `commit cm temp=255` skip, `boot cm` 불변(255 손상 X) |
| 무회귀 | 부팅/CANCEL/네비/SAVE영속 (기존 T10 ①~⑩) | 회귀 없음 |

> mon: USART6 `/dev/cu.usbserial-BG02DMWU` @115200. 캡처: `{ stty -f /dev/cu.usbserial-BG02DMWU 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-BG02DMWU > /tmp/lcd-mon.log &`. 정리: `tr -d '\000' < log | tr -s ' ' | grep '\['`. 플래시: `openocd -f openocd/stm32f410.cfg -c "program build/gds_us_ctrl.elf verify reset exit"`. 빌드: `env -u STM32_TOOLCHAIN cmake --build build`.

---

## 5. 마감 → 머지 → Stage D

1. §1~§4 통과 → **전체 브랜치 final 코드리뷰 1회**(cpp-reviewer): A/B/(C?)/D + 에셋 + 진단정리.
2. 커밋 정리: firmware fix(A/B/D[+C?]) + 에셋 fix + 진단/문서. (현재 미커밋 — 사용자 결정으로 보존 중.)
3. changelog/RESUME/NEXT_STEPS "done" 갱신.
4. `gh pr create`(→main) + 태그 `hw-revA_fw-stage-lcd`.
5. → **STEP 1 = Stage D slice 1 (레귤레이션 코어)** — RESUME §STEP 1 참조(spec-first, 측정 게이트 해제됨). LCD의 `lcd_measure_t` provider + 스텁 훅 실체화로 자연 통합.

---

## 6. 현재 상태 스냅샷 (다음 세션 진입 시)

- **브랜치**: `feat/stage-lcd-full-behavior` (main 미머지, 미푸시). **커밋 완료**:
  - `a0a631c` fix(lcd): firmware A/B/C/D (app_lcd.c/input.c/render.c + 게이트 트레이스)
  - `0eafe68` fix(lcd-asset): `hw/lcd/dgus/14ShowFile.bin` (page 27/23 아이콘 — **root fix, 레포 반영 완료**)
  - `docs(lcd)`: RESUME + analysis + 이 핸드오프 + `.gitignore`(build-trace)
- **에셋**: ✅ 레포 반영됨(§1 해소). 단 **편집가능 DGUS 프로젝트 소스**는 별도 보관 확인 필요(레포엔 컴파일된 `.bin`/`.icl`만 — 다음 편집은 출력에서 시작).
- **보드**: 트레이스 빌드 플래시됨(머지 아님). mon 캡처 백그라운드 가동 가능성(`pkill -f cu.usbserial-BG02DMWU`로 정리).
- **머지 빌드**: 0-warning, FLASH 26.86%/RAM 10.16% (트레이스 OFF).
- 확정사실(재증명 불요): comm 표시결함 = 패널 에셋(page 27/23) issue, firmware byte-faithful. 교차테스트 + 사용자 에셋fix로 입증.
