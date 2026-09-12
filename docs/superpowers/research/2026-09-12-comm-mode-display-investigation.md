# 조사: LCD COMM 페이지 "재부팅 후 시리얼 표시" (2026-09-12, 읽기 전용)

## 요약 (사용자 3차 판별 반영 후 정정)

- **cfg / FRAM / 부팅 로드 = 정상.** 재부팅 후 `mb_tcp.py 192.168.1.199` 접속 성공 = `cfg->comm_mode` 가 ETH 로 저장·로드된다. 코드도 일치: `app_config_save_all` 이 `FRAM_ADDR_COMM_MODE(44)` 에 쓰고(`fw/src/app_config.c:78`), `app_config_load` 가 같은 주소를 클램프 없이 읽으며(`:144`), 부팅 뒤 `cfg->comm_mode` 에 쓰는 코드는 `commit_comm_mode_and_ether`(`fw/src/app_lcd_comm.c:309`)와 `app_config_load` 둘뿐이다. 페이지 라우팅(`fw/src/app_lcd_input.c:394`)도 `cfg->comm_mode` 로 STDE/MHE 를 고른다 — 사용자가 이더넷 페이지에 들어간 것이 그 증거.
- **문제는 표시 한 지점: 모드 선택 아이콘 VP `DISP_COMM_MODE(0x140c)`.** 펌웨어는 첫 진입에서 이 VP 에 **1(ethernet) 을 두 번 쓴다** — set_page 전(`fw/src/app_lcd_render.c:215`) + set_page 후 재기록 Fix C(`:160`, 호출 `:233-236`). 값 흐름: 부팅 `temp_comm_mode=0xFF`(`fw/src/app_lcd.c:216`) → SETUP1 진입 0xFF 유지(`render.c:116`) → COMM 진입 시드 `temp=cfg->comm_mode=1`(`render.c:135-136`) → `DISP_COMM_MODE=1`. **펌웨어가 0 을 쓰는 경로는 없다.**
- 따라서 **패널 측 결함 = 2026-05-31 에 규명된 것과 동일 증상**(analysis `2026-05-31-std-comm-page27-display-port-faithful.md`): page 27/23 의 var-icon 위젯이 page-show 시 VP 를 다시 그리지 않고, set_page 직후 즉시 쓰는 Fix C 재기록이 페이지 전환 중에 도착해 반영되지 않는다(레이스). 같은 페이지가 **활성인 상태**에서의 쓰기(버튼 터치 → `handle_comm_mode` `app_lcd_comm.c:102`)는 반영된다 — 사용자 관측("선택하면 이더넷으로 바뀜, 재부팅하면 시리얼")과 정확히 일치.
- **이 브랜치 무관 / 기존 잠복.** `git diff 3f82c06..6ec5b78 -- fw/` 의 hunk(에코 19줄 · `app_lcd_run_std_refresh` 승격 · F1 `temp_horndown` 시드 · 버전 문자열)는 0x140c/`temp_comm_mode` 경로에 닿지 않는다. 리팩토링 구간(b61ef0f..3f82c06)은 `.bin` 동일(baseline `d97b2b4` 의 fw/ 는 b61ef0f 와 diff 없음 = 기준점 유효). legacy samd20 은 set_page **전**에만 썼으므로(`ref/samd20/main.c:3159-3172`) 이 패널에서 같거나 더 나쁘게 보인다.
- **미확정(추측)**: 물리 패널에 실려 있는 에셋이 레포 `hw/lcd/dgus/14ShowFile.bin`(`eeba15a`, 05-31 page 27/23 수정본)인지. 레포 에셋은 4개 comm 페이지의 0x140c 위젯 레코드가 **완전히 동일**하다(아래 §5). 06-08 이후 "V30 에셋" 언급이 있어 디자이너 프로젝트로 재플래시됐다면 05-31 수정이 빠진 상태일 수 있다.

---

## 1. 판정 1~3 (FRAM · 부팅 로드 · 되돌리는 코드) — 정상, 짧게

| 항목 | 위치 | 결과 |
|---|---|---|
| SAVE → cfg | `fw/src/app_lcd_comm.c:292-330` `commit_comm_mode_and_ether` — `temp_comm_mode!=0xFF` 이고 cfg 와 다르면 대입 + ether 훅 | STD 계열 6페이지(STD1/2D/2T/3/STDC/STDE) 모두 호출(`:380-389`), MULTI/MHC/MHE 호출(`:363-370`), HAND 만 미호출(legacy 충실) |
| SAVE → FRAM | `fw/src/app_config.c:78` `fram_write_byte(FRAM_ADDR_COMM_MODE, cfg->comm_mode)` | 주소 44, `fw/include/fram.h:44`, 다른 필드와 겹침 없음(42-43 FREQ_CAL, 45- ETHER_IP) |
| 부팅 로드 | `fw/src/app_config.c:144` | **클램프 없음**(M3 클램프는 speed/parity 만 `:139,141`) — 1/2 그대로 로드. 읽기 실패 시에만 기본 0 + `fail++`(mon `[cfg] WARN`) |
| 부팅 뒤 `comm_mode=` 쓰기 전수 | `app_lcd_comm.c:309`(commit), `app_config.c:35`(defaults, load/cancel 경로) | `app_eth.c` 는 읽기만(`:104,167`), `app_modbus.c` 는 미러만(`:243`), staging 9종에 COMM_MODE 없음(`app_modbus.c:87-97`), 원격 계약도 `0x21 R only`(`gds_us_remote/docs/reference/modbus-contract.md:194`) |
| `save_all` 호출자 | `app_lcd_comm.c:396`, `app_modbus.c:644`, `app_weld.c:224` | 전부 **라이브 cfg** 를 쓰므로 0 을 만들 수 없음 |
| 브랜치 diff | `git diff b61ef0f..6ec5b78 -- fw/src/app_config.c fw/drivers/fram.c fw/include/fram.h fw/include/app_config.h` | **변경 0** |

사용자 실측(재부팅 후 TCP 접속 성공)과 일치 → 종결.

## 2. 표시 경로 — 값 흐름 (파일:라인)

부팅 후 첫 COMM 진입(STD 모델, cfg->comm_mode=1):

1. `app_lcd_init_mode` → `state->temp_comm_mode = 0xFFu` (`fw/src/app_lcd.c:216`), lcd_status=RUN_STD.
2. SETUP_PARAM → `handle_setup_param_enter` (`fw/src/app_lcd_input.c:451-461`) → `app_lcd_change_page(STD1)` → `render_setup_main` → `temp_comm_mode=0xff` (`fw/src/app_lcd_render.c:116`). F1 은 `temp_horndown` 만 건드림(`input.c:460`).
3. STD_SETUP_PARAM=4 → `lcd_status = (cfg->comm_mode==0) ? STDC : STDE` (`input.c:394`) → STDE(27).
4. `app_lcd_change_page(27)` (`render.c:168-246`):
   - `render_comm_page` (`:120-155`): `temp_comm_mode==0xff` → **`temp = cfg->comm_mode (=1)`** + ether 텍스트 시드 (`:135-147`).
   - STDE 분기 (`:210-217`): `temp!=0` → `DISP_COMM_MODE=1` (`:215`), `DISP_EN_DHCP=temp-1=0` (`:216`).
   - `dgus_set_page(27)` (`:220`).
   - Fix C `render_comm_tail` (`:157-165`, 호출 `:233-236`): `DISP_COMM_MODE = (temp==0)?0:1 → 1`, `DISP_EN_DHCP=0`.
5. 터치 시 `handle_comm_mode(1)` (`app_lcd_comm.c:100-119`): `DISP_COMM_MODE=1` (`:102`) + `temp=1` + `change_page(STDE)` 재렌더 → 위 4 와 **동일한 바이트**.

→ 진입(4)과 터치(5)는 같은 VP·같은 값을 쓴다. 차이는 **패널 페이지 상태**(4 = 10→27 전환 중, 5 = 27 활성) 하나. 표시가 4 에서만 틀리므로 값이 아니라 패널의 반영 타이밍/위젯 특성 문제.

`temp_comm_mode` 생명주기(0 이 새는 곳 없음): 0xFF 세트 = `app_lcd.c:216`(부팅·SYS_PIC_NOW 재init), `render.c:116,191,195,202`(STD1/HAND/MULTI, STD2D, STD2T, STD3/MH2 진입), `app_lcd_comm.c:413`(CANCEL). 0 세트 = `app_lcd_comm.c:92`(serial 버튼 터치만). `g_lcd` 는 static zero-init(`app_lcd.c:14`)이지만 첫 페이지 전에 `init_mode` 가 0xFF 로 덮는다(fix A).

VP 클로버 점검: 0x140c 인접에 쓰는 긴 프레임 없음 — `DISP_VERSION 0x1330`+20B(→0x1339), `COMM_*_TXT 0x1460~0x1497`, `LV_MO_* 0x1400~0x1403`, `MULTI_EN 0x140a`, 주기 표시(`app_lcd_disp.c`)는 0x1110~0x1197·0x1150~0x1154·0x120e 범위. 에코 신설 VP(LV_*, DISP_HORNDOWN 0x1209, MODEL_*, VAR_CAL_*)도 무관.

## 3. legacy 대조

`ref/samd20/main.c:3159-3172` (STDE/MHE 분기): `temp_comm_mode==0xff` 시드 → `DISP_COMM_MODE 0/1` + `DISP_EN_DHCP` → `set_lcd_page`. **set_page 뒤 재기록 없음.** 진입 nav(`:3937-3942`)는 `comm_mode` 로 STDC/STDE 선택 — 포팅과 동일. 05-31 분석이 이미 "SAMD20 도 같은 바이트 → 동일 잠복"으로 판정. 이번 포팅은 Fix C 로 legacy 보다 한 번 **더** 쓰는데도 안 되는 것이므로 legacy 도 같은 패널에서 serial 로 보인다.

## 4. 브랜치 관계

- `git diff 3f82c06..6ec5b78 -- fw/` = 5 파일 43+/12-: `app_modbus.c` 에코 `dgus_write_u16(LV_*/DISP_HORNDOWN/MODEL_*/VAR_*)` + `app_lcd_run_std_refresh()` 호출(`app_modbus.c:645`), `render.c` `render_run_std`→`app_lcd_run_std_refresh` 승격(본문 동일), `input.c:460` F1 `temp_horndown` 시드, `define.h` 날짜. 0x140c/`temp_comm_mode`/`commit_comm_mode_and_ether` 무접촉.
- 리팩토링 `b61ef0f..3f82c06`: `.bin` 동일 주장의 baseline `d97b2b4` 는 fw/ 변경 없음(tools/.gitignore 만) → 기준점 = b61ef0f 바이너리. `render_comm_page/render_comm_tail` 추출은 always_inline 순수 이동.
- **결론: 기존 결함(패널 측)이며 이 브랜치가 만든 것이 아니다.** `_260906` 에서도 같았을 것(사용자 미확인).

## 5. DGUS 에셋 (레포 `hw/lcd/dgus/`, 커밋 `eeba15a`)

`14ShowFile.bin` 페이지 테이블 해독(엔트리 = count(LE 2B)+offset(BE 2B), idx = page+2): page21@0x5520(4개) · 23@0x55a0(4) · 25@0x5620(5) · 27@0x56c0(5). 0x140c 레코드 4개(각 페이지 1개) **바이트 동일**:
`5a 00 ffff 000a 140c X=0x28 Y=0x3c Vmin=0 Vmax=1 Icon 0x38..0x39 lib=0x30(48.icl) mode=0 ...`
→ 값 0 = 아이콘 56(serial 표시), 1 = 아이콘 57(ethernet), 범위 밖 = 공백. 0x140d(DHCP) 도 25/27 동일(`Icon 0x3a..0x3b`).
`13TouchFile.bin`: 0x140b 는 **type 0x05(return key)** 로 값 0/1/2 세 버튼 — 입력 전용, 표시 아님. 0x140b 는 ShowFile 에 없음(표시는 0x140c/0x140d 만).
`22_Config.bin`(131076B = 전 VP 초기값 파일): 0x140b/0x140c/0x140d 초기값 **0** → 전원 인가 직후 패널 RAM 은 serial 상태. 펌웨어 쓰기가 **렌더에 반영**돼야 바뀐다.
→ 레포 에셋에는 페이지 간 차이가 없다(05-31 수정 반영 상태). **물리 패널의 에셋이 이것인지는 레포에서 확인 불가**(추측: 06-08 "V30 에셋" 재플래시 시 05-31 수정 소실 가능 — `changelog.md:576` "편집 DWIN 프로젝트 repo 부재").

## 6. 사용자 1분 판별

전원 재투입 → SETUP → 4(통신) 진입, 아이콘 serial 확인 후:
1. **같은 페이지에서 nav "4" 를 한 번 더 누르거나, "3" 으로 갔다가 "4" 로 복귀**(둘 다 `app_lcd_change_page(27)` 재실행 = set_page 27→27 + 0x140c=1 재기록). **ethernet 으로 바뀌면** = 값은 맞고 "페이지 전환 직후 쓰기가 반영 안 됨"(패널 측/Fix C 레이스) 확정. 여전히 serial 이면 → 트레이스 빌드(`LCD_TRACE_RX`, `render.c:238-244` `page= tcm= cm=`) 로 값 확인이 다음 단계.
2. (이미 관측) 이더넷 버튼 터치 → ethernet 표시 = 활성 페이지 쓰기는 반영.
3. 선택: DHCP 체크 상태도 같이 보라 — 0x140d 도 같은 레이스 대상(`:162`).

## 7. 수정 후보 (나열만)

- **A. 패널 에셋**: 물리 패널에 레포 `14ShowFile.bin`(`eeba15a`) 을 다시 올려 page 27/23 위젯이 page-show 시 VP 를 그리는지 확인. 디자이너 프로젝트(V30)에 05-31 수정을 반영. root fix, 펌웨어 무변경.
- **B. 펌웨어 지연 재기록**: `render_comm_tail` 의 즉시 쓰기 대신(또는 추가로) `app_lcd_tick` 에서 `lcd_status ∈ {21,23,25,27}` 이고 `sys_tick - last_set_page_ms ≥ ~100ms` 인 첫 tick 에 `DISP_COMM_MODE/EN_DHCP` 를 1회 재기록(1-shot 플래그). legacy 이탈이지만 Fix C 와 같은 성격의 방어.
- **C. 페이지 확인 후 쓰기**: comm 페이지 진입 시 `dgus_read_word(SYS_PIC_NOW)` 로 페이지 27 확인 뒤 재기록(`app_lcd_ensure_run_page` 패턴 `app_lcd.c:251-270`). 블로킹 read 가 슈퍼루프에 들어가는 비용 있음.
- **D. 주기 재기록**: comm 페이지에 있는 동안 표시 스텝머신(`app_lcd_disp.c`)에 0x140c/0x140d 를 저빈도(예 200ms) 재기록. 가장 단순·튼튼하나 UART 트래픽 소폭 증가.
- **E. 진단만**: `LCD_TRACE_RX` 빌드로 `[lcd] page=27 tcm=1 cm=1` 확인해 펌웨어 값 1 을 실증(05-31 방식).

## 8. 과거 이력 대조

- 2026-05-27(changelog:660-666): "ethernet 저장 안 됨" 으로 보였던 것도 영속 아니라 표시 결함 → 이번과 같은 패턴.
- 2026-05-31(changelog:649-653, analysis 문서): page 27/23 위젯 auto-load 부재 = 에셋 root. Fix A/B/C/D + 에셋 `eeba15a`. Fix C 단독 "진입 표시 성공" 1회 기록 — 이번엔 실패하므로 Fix C 는 타이밍 의존.
- 2026-06-13 j(`project_eth_dhcp_static_persist`): 반대 방향(DHCP 리스가 static 으로 굳음) — 영속 경로는 그때도 정상.
