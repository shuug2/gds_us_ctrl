# HW finding — STD comm 페이지(27) 표시 버그: firmware는 SAMD20 byte-충실, 패널-page 이슈 (2026-05-31)

> 브랜치 `feat/stage-lcd-full-behavior`. fix A/B/D(아래) 적용 후 트레이스 빌드로 HW 검증 중 발견.
> systematic-debugging Phase 1~5 + advisor 점검. **firmware 정상 확정, comm 표시 결함은 DGUS 패널 page 27 특성.**

## 한 줄 요약

STD comm-ethernet 페이지(DGUS page **27** STDE)가 전원사이클 후 첫 진입에서 **serial로 표시**(icon auto-load 실패). MULTI(page **25** MHE)는 정상. 트레이스로 firmware가 page 27/25에 **동일한 DISP_COMM_MODE VP 출력**을 보냄을 증명했고, SAMD20 원본과 **byte-동등**(기록 로직·순서·set_page 형식·주기refresh부재 모두 일치)임을 확인. → **포팅 회귀 아님. page 27 위젯이 page-show 시 VP auto-load를 안 하는 패널-프로젝트 특성**(page 25는 함). SAMD20도 같은 바이트를 보내므로 동일 잠복.

## 증상 (사용자 HW, STD 모드)

1. 전원사이클 → STD comm 페이지 진입 → **serial O / ethernet X / dhcp X** (영속값은 ethernet=cm1).
2. 그 상태에서 ethernet 선택(버튼) → serial X / ethernet O / dhcp X (정상).
3. SAVE 후 RUN 나갔다 들어와도(in-session) 유지.
4. **전원사이클하면 다시 serial 표시.**
5. dhcp 선택 → serial·ethernet 아이콘 둘 다 사라짐.
- **MULTI 모드는 1~5 전부 정상** (진입부터 ethernet O, 영속·표시 OK).

## Ground truth (LCD_TRACE_RX 트레이스)

진단: `[lcd] page=%u tcm=%u cm=%u`(change_page 말미, render.c) + 기존 boot/commit/rx.

```
[lcd] boot cm=1 ip=192.168.1.128     ← ethernet(cm=1) 영속 확인 (FRAM 정상)
[lcd] page=25 tcm=1 cm=1              ← MULTI eth comm(MHE) 진입 → 화면 ethernet O  ✓
[lcd] page=27 tcm=1 cm=1              ← STD eth comm(STDE) 진입 → 화면 serial      ✗
[lcd] rx vp=0x140B data=1            ← 사용자 ethernet 버튼
[lcd] page=27 tcm=1 cm=1              ← 같은 page 27, 이제 화면 ethernet O          ✓(live write)
```

→ page 27/25에 **동일 VP 출력**(DISP_COMM_MODE=1), set_page 대상만 다름. 25=정상, 27=serial.
→ 진입 render는 `DISP_COMM_MODE=1` 쓴 *뒤* set_page(27): page 27이 **show 시 auto-load 안 함** → serial 기본.
→ 버튼은 page 27 *활성 중* 기록: 위젯 업데이트 → ethernet O. (in-session 됨/전원사이클 첫진입 깨짐 패턴과 정확히 일치.)

## Firmware ↔ SAMD20 byte-동등 (포팅 회귀 배제)

| 항목 | SAMD20 (`ref/samd20`) | 포팅 (`fw/`) | 일치 |
|---|---|---|---|
| STDE/MHE DISP_COMM_MODE 기록 | main.c:3163-3169 | render.c:198-206 | ✓ |
| 기록 순서 (VP → set_page) | VP, then set_lcd_page @3172 | VP, then dgus_set_page @207 | ✓ |
| 주기 refresh가 DISP_COMM_MODE 재기록? | **없음** (grep: 3125/3127/3164/3167/3168 + 버튼 4043/4055/4064/4069 뿐) | **없음** (render + 버튼 input.c만) | ✓ |
| set_page 명령 형식 | `VP_LCD_SETPAGE ← 0x5A 01 00 page` (dgus_lcd.c:145) | 동일 (drivers/dgus_lcd.c:106) | ✓ |

→ comm-페이지 진입 바이트 시퀀스가 SAMD20와 동일. **page 27 표시 결함은 패널 page-27 위젯 특성**(page 25엔 있는 "VP auto-upload on page show" 플래그가 27엔 없음으로 추정). SAMD20도 동일 바이트 → 동일 잠복(STD+ethernet 희소 조합이라 미발견했거나 패널 .bin이 이후 변경).

## ✅ 결정적 교차 테스트 (firmware-vs-패널 확정, 2026-05-31)

**실험**: firmware 고정, STD comm 진입(input.c:247 data16==4)을 `STDC/STDE` → `MHC/MHE`로 1줄 repoint(진단 빌드, 미커밋). STD 모드에서 comm 진입.
**트레이스**: `rx vp=0x1020 data=4` → `[lcd] page=25 tcm=1 cm=1` (STD firmware가 page 25 렌더).
**화면 결과**: **ethernet O 정상 표시** ✅ (page 27일 땐 serial로 깨짐).

→ **firmware 동일·page 번호만 27→25 변경 → 정상.** 따라서 결함은 **패널 page 27**에 100% 국한. firmware 회귀 아님 확정. page 27 위젯은 page-show 시 DISP_COMM_MODE(0x140c) auto-load 안 함, page 25는 함.

### 미해소 (root cause 세부, 수정엔 불필요)
- 패널 .bin이 SAMD20 출하본과 동일한지(=패널 회귀 vs 원래 잠복)는 DGUS 에디터로 page 25 vs 27 위젯 설정(13TouchFile.bin/14ShowFile.bin) 대조해야 확정. 수정 방향엔 무관.

## 수정 옵션 (택1, 사용자 결정 대기)

- **A. Firmware 방어 재기록** *(in-repo, 즉시 테스트 가능)*: render comm 분기에서 **set_page *후*** DISP_COMM_MODE/EN_DHCP 재기록(버튼 경로처럼 활성-페이지 기록). page 27 강제 업데이트, page 25 무해. ⚠️ set_page 직후 기록이 너무 빠르면 패널이 페이지 전환 중이라 누락될 수 있음 → 소량 delay 필요할 수 있음(테스트). SAMD20 미적용 동작이므로 "방어적 deviation"으로 명시.
- **B. DGUS 패널 수정** *(root, 패널 .bin 재플래시 필요)*: page 27(및 23 STDC)의 comm-mode 아이콘 위젯에 page-show auto-load 활성화해 page 25와 일치. firmware는 byte-faithful 유지. DGUS 툴링 필요.

## 독립적으로 유효한 firmware 수정 (이번 브랜치, 이미 적용·빌드 0-warning)
이 표시 이슈와 **무관하게** 유효(섀도우 생명주기 + 손상 가드):
- **A(boot sentinel)** app_lcd.c init_mode `temp_comm_mode=0xFF` — 헤더 의도(app_lcd.h:86) 갭 해소.
- **B(commit guard)** app_lcd_input.c commit_comm_mode_and_ether early-return on 0xFF — STD-persist deviation이 연 손상경로(STD3 SAVE→cm=0xFF) 차단. SAMD20 3340도 무가드(동일 잠복).
- **D(serial dhcp clear)** render.c serial 분기 DISP_EN_DHCP=0.
- 상세 설계/근거: [[2026-05-27-std-ether-comm-mode-persist-display]].

## 진단 스캐폴딩 (머지 전 정리 대상)
- render.c `[lcd] page= tcm= cm=` (이번 추가), app.c `boot cm`, input.c `commit cm`/`rx vp` — 전부 `#ifdef LCD_TRACE_RX` 게이트. 머지 바이너리엔 컴파일 아웃. 유지/제거 결정 필요.

## 보류 신호
- `[lcd] page=3 tcm=255 cm=2` 1회 출현(부팅 cm=1과 1글자 차) = 시리얼 글리치 추정. cm=2가 깨끗이 재현되면 별도 신호로 재조사.

## Fix C 적용 결과 (2026-05-31, 부분 성공 — 토글 미해결)

**적용**: render.c `app_lcd_change_page` 말미, `dgus_set_page(page)` **후** comm 페이지(21/23/25/27)면 DISP_COMM_MODE(+ ethernet 페이지는 DISP_EN_DHCP) 재기록. (input.c 진단 repoint는 원복.) 빌드 0-warning(FLASH 27.04%).

**HW 결과**:
- ✅ **진입 표시 성공**: STD 전원사이클 → STD comm 진입 → 트레이스 `[lcd] page=27 tcm=1 cm=1` + **화면 ethernet O**. page 27인데도 page 25처럼 정상. (set_page 후 활성-페이지 재기록이 page 27 auto-load 부재를 우회.)
- ❌ **토글 실패 (미해결)**: STD comm 페이지에서 serial/ethernet/dhcp 버튼(0x140B) 토글 시 **serial·ethernet 아이콘 둘 다 사라짐**. 전원사이클하면 **serial로 초기화** 표시.

**토글 트레이스 관찰 (해석 보류, 사용자 코드리뷰 예정)**:
- 진입 `page=27 tcm=1 cm=1`(정상) 직후 토글 `rx vp=0x140B data=1` → 다음 트레이스가 `page=9`(RUN_STD) 출현 = **토글 네비/페이지 처리 이상** 의심(STDE↔STDC 스왑 경로 input.c:310-334가 page 23 STDC 또는 run page로 잘못 빠질 가능성).
- 한 진입에서 `page=27 tcm=2 cm=1` 관측 = tcm=2(DHCP)인데 cm=1(eth-static) 불일치 → **섀도우(temp_comm_mode) stale 엣지** 잔존(토글 후 이전 tcm가 다음 진입에 누수?).
- 토글은 **page 23(STDC, serial 페이지)** 를 관여시키는데 23은 page 27과 별개 패널 페이지(별도 `standard_setup3` 이미지) → page 23 고유 위젯 이슈 가능성(미테스트).

**가설 (미검증, 사용자 조사용)**:
1. page 23(STDC)도 page 27처럼 auto-load 부재 + Fix C 재기록이 serial(temp=0)일 때 DISP_COMM_MODE=0만 쓰고 page 23 위젯이 0을 "아이콘 없음"으로 렌더할 수 있음.
2. handle_comm_mode(input.c:303-336)의 페이지 스왑/재렌더 순서가 Fix C 재기록과 상호작용해 아이콘을 지울 수 있음(이중 기록 타이밍).
3. 토글 중 lcd_status가 STDC/STDE가 아닌 값으로 빠지는 네비 버그(page=9 RUN_STD 출현 근거).

> **사용자 결정 (2026-05-31)**: 여기서 중단. 사용자가 직접 코드 리뷰 후 재수정 예정. Fix A/B/C/D는 working tree에 적용·유지(미커밋). 진단 스캐폴딩(render `page=` 트레이스 + LCD_TRACE_RX 게이트)도 유지.

## ✅ RESOLUTION (2026-05-31) — 에셋 수정으로 해결, 진단 입증

사용자가 **firmware 코드 미변경, DGUS LCD 에셋(패널 page 27/23 위젯)만 수정 → 정상 동작** 확인. = 본 문서의 진단(firmware byte-faithful, root는 패널 page-27 auto-load 부재)을 **HW로 최종 입증**. firmware Fix C(set_page 후 재기록)는 같은 증상의 우회였고, 에셋에서 root가 고쳐졌으므로 **이제 잉여**(revert 후 재검증 권장 — handoff §2).

**후속(handoff `2026-05-31-HANDOFF-lcd-comm-display-resolved.md`)**:
- ⚑ 수정된 에셋이 레포 `hw/lcd/dgus/`에 **미반영** → 캡처·커밋 필요(handoff §1).
- ⚑ Fix C 처분(revert vs 유지) + 토글 재검증(handoff §2/§4).
- A/B/D는 독립 유효 → 유지.

## 머지 게이트 (해소됨)
표시 결함 root는 에셋에서 해결. 남은 건 **에셋 레포 반영 + C 처분 + 전체 재검증 + final 리뷰** → PR/태그 (handoff §1~§5).
