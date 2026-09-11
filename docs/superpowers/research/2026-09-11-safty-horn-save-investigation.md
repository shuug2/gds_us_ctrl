# SETUP1 SAFTY + HORNDOWN 동시 선택 → SAVE 시 "하나가 꺼짐" 조사 (읽기 전용, 2026-09-11)

> **요약**: 꺼지는 쪽은 **horn(SYS_HORN 모드)** 이고 **표시만이 아니라 실제 모드가 꺼진다**(`app_horn_mode_active()` 0, SOL 강제 OFF, START 게이트 해제, STATUS bit6 0). `f_safty` 는 SAVE 경로에서 절대 꺼지지 않으며 cfg=1·FRAM=1 로 저장된다. 원인은 SETUP1 **진입 시** 체크박스 VP 는 현재 모드(1)로 그리면서 shadow `temp_horndown` 은 0 으로 리셋하는 `fw/src/app_lcd_input.c:458-459` 와, STD SAVE 가 그 shadow 를 무조건 적용하는 `fw/src/app_lcd_comm.c:388` 의 조합이다 — **화면에 ✓ 로 보이는 "선택"이 SAVE 가 적용하는 값과 다르다.** 재현 전제 = SETUP 진입 시점에 horn 이 이미 ON(직전 SAVE 또는 Modbus `0x30`). 둘 다 OFF 에서 두 개를 새로 탭하면 코드상 둘 다 살아남는다. legacy(samd20)는 **다르다** — `case SETUP_PARAM`(`ref/samd20/main.c:3754-3771`)은 `temp_horndown` 을 건드리지 않아 horn 이 유지된다; 포트 주석이 인용한 `3617-3622` 는 **CANCEL 분기**다(`519d908` 에서 오인용, 2026-07-18). 이 브랜치(`feat/modbus-write-lcd-echo`)는 이 거동을 바꾸지 않았다(LCD 파일 변경은 .bin 동일 리팩토링 + `run_std_refresh` 추출, HORN 에코 1줄은 원격 쓰기 때만 실행). DGUS 자산 파싱으로 두 체크박스가 **패널 자체 토글**(증분조절 ++ 루프 0..1 + 업로드)임을 확정 — 코드만으로 못 정하는 것은 §G.

---

## 0. 조사 범위·전제

- 저장소: `/Users/tknoh/dev/work/gds_us_ctrl/.claude/worktrees/feat-modbus-write-lcd-echo`, HEAD `8567038`. 보드는 `_260906` = main `b61ef0f`(merge-base 확인: `git merge-base b61ef0f 8567038` = `b61ef0f`).
- 대조: `git show b61ef0f:fw/src/app_lcd_input.c` :517-519(DISP_SAFTY) / :536-538(DISP_HORNDOWN) / :541-556(SETUP_PARAM·_MOOHAN 진입 시드+리셋), `b61ef0f:fw/src/app_lcd_comm.c:386`(hook_horn), `b61ef0f:fw/src/app_lcd_render.c:57-61`(RUN_STD 렌더 DISP_SAFTY) — **현 HEAD 와 로직 동일**(아래 §E-5).
- 페이지 ID: `LCD_RUN_STD=9`, `LCD_SETUP_STD1=10` (`fw/include/dgus_lcd.h:34-35`). VP: `DISP_HORNDOWN 0x1209`(`:67`), `DISP_SAFTY 0x120d`(`:111`).

## A. 터치 → 상태 경로

| 체크박스 | 터치 시 펌웨어 | 즉시/shadow | LCD 에코(✓ 표시)는 누가 | 근거 |
|---|---|---|---|---|
| `DISP_SAFTY` | `cfg->f_safty = (data16==1)?1:0` | **cfg 즉시** | **패널 자체**. 펌웨어는 터치 후 에코 없음 | `app_lcd_input.c:581-583` |
| `DISP_HORNDOWN` | `state->temp_horndown = (data16==1)?1:0` | **shadow**(SAVE 시 적용) | **패널 자체**. 펌웨어 에코 없음 | `app_lcd_input.c:600-602` |

펌웨어가 두 VP 를 쓰는 지점 전수(`grep` 결과):
- `DISP_HORNDOWN`: 부팅 `app_lcd.c:242`(=0) / SETUP1 진입 `app_lcd_input.c:458`(=`app_horn_mode_active()`) / **이 브랜치 신설** Modbus 에코 `app_modbus.c:574`.
- `DISP_SAFTY`: RUN_STD 렌더 `app_lcd_render.c:52`(=`cfg->f_safty`) / Modbus 에코 `app_modbus.c:560`.
- SETUP1 진입 렌더 `render_setup_main`(`render.c:102-117`)은 **둘 다 쓰지 않는다** → SETUP1 의 SAFTY ✓ 는 VP RAM 잔존값(마지막 RUN_STD 렌더 또는 패널 토글).

**DGUS 자산 확인**(`hw/lcd/dgus/13TouchFile.bin`, 32B 레코드, Pic_ID +0 / TP_type +15 / 데이터 +16):
- `@0x930`: Pic_ID `0x000A`(=페이지 10 STD1), 영역 (572,321)-(769,386), Pic_Next `FF00`(페이지 점프 없음), type `0x02`(증분 조절), VP `0x1209`, 모드 `01`(++), 루프 `01`, step 1, min 0, max 1, 업로드 `01`.
- `@0x950`: Pic_ID 10, 영역 (301,320)-(498,385), type `0x02`, VP `0x120d`, ++ 루프 0..1 step 1, 업로드 `01`.
- 두 VP 의 터치 컨트롤은 파일 전체에서 **각 1건**(페이지 10 만).
- `14ShowFile.bin`: 변수 아이콘(type 00) 바인딩 — SETUP1 군집: `0x120d`@(309,328)·`0x1209`@(580,329)(터치 영역 안에 위치 ✓ 교차검증) / **RUN_STD 군집(바·VAR_* 위젯과 동일 블록, 오프셋 0x4ee0-0x4fe0)**: x=454 열에 `0x120c`(y16)·`0x120a`(54)·`0x120b`(92)·**`0x1209`(130)·`0x120d`(168)**·`0x120e`(206) → **RUN 페이지 9 에도 HORNDOWN·SAFTY 배지가 있다.**
- 결론: 탭 = 패널이 VP 를 0↔1 토글하고 그 값을 0x83 으로 올림(legacy `main.c:3846-3868` 의 `lcd_data==1 / else` 분기와 정합). 펌웨어는 값을 기록만 한다.

## B. SAVE 경로 (`data_save_commit`, `fw/src/app_lcd_comm.c:354-397`) — STD1 에서 위→아래

1. `:381-386` 분기 진입(`lcd_status==LCD_SETUP_STD1`).
2. `:386 app_lcd_hook_set_pot(cfg->output_power)` — I2C, 무관.
3. `:387 commit_cnt_reset()` — `temp_cnt_reset` 만.
4. **`:388 app_lcd_hook_horn(state->temp_horndown == 1u)`** → `app_lcd.c:88-92` → `app_horn_set_mode(down)` (`app_horn.c:39-56`): `m != s_mode` 일 때만 `io_sol_dn(false)`·`s_sol_last=0`·mon `[horn] mode=%u SOL_DN off`; 이후 `s_mode = m` **무조건**. 즉 **temp 가 0 이면 현재 ON 인 모드가 꺼진다.**
5. `:389 commit_comm_mode_and_ether()` — STD1 진입 시 `temp_comm_mode=0xFF`(`render.c:116`) → `:303-305` 가드로 즉시 return. 부수효과 없음.
6. `:390 commit_comm_serial_shadows()` — 진입 시 cfg 에서 시드(`render.c:112-114`) → 불일치 없으면 no-op.
7. `:391 lcd_status = LCD_RUN_STD`.
8. `:396 app_config_save_all(cfg)` — `f_safty` → `FRAM_ADDR_SAFTY`(`app_config.c:59`). horn 은 cfg 필드가 아니라 **FRAM 저장 없음**(설계, `app_modbus.c:567-568` 주석 "비영속").
9. `:398 app_lcd_change_page(LCD_RUN_STD)` → `render.c:183-184` → `app_lcd_run_std_refresh()`: `DISP_SAFTY ← cfg->f_safty`(`:52`), **`DISP_HORNDOWN` 은 쓰지 않음**(`:50-53`) → 페이지 9 HORN 배지는 VP 잔존값.
- `sys_status` 필드는 **미배선**(`app_lcd_input.c:67` 주석 "sys_status 필드는 미배선") — SYS_HORN 이 페이지 렌더를 덮는 경로 없음.

## C. 부수효과 교차 점검

- `app_horn.c` 전체: `s_mode/s_sol_last/s_prev_ms` 와 `io_sol_dn`·`app_reg_command(RUN_RELEASE)`(`:81-86`)만 건드림. **`f_safty`·DISP_SAFTY 접근 없음.**
- `f_safty` 저장이 horn 을 건드리는 곳 없음(`grep f_safty` → `app_lcd_input.c:582`, `app_config.c:59/115`, `app_modbus.c:212/550-560`, `app_weld.c:137` 소비).
- 주기 표시(`app_lcd_disp_step`, `app_lcd_disp.c:194-250`)는 `ICON_RUN`·`DISP_REMOTE(0x120e)`·바·VAR_* 만 씀 — 인접 VP 를 덮는 다중워드 쓰기 없음(`dgus_write_u32` 는 `LV_WORK_CNT`·`LV_ENERGY_VAL 0x1212`, 배열은 `0x1170-0x1197`·`0x1081-2`·`0x1085-6`·`VAR_POWER 3워드`).
- Modbus 미러 경합 없음: `mirror_live()` 가 `holding[0x30] = app_horn_mode_active()`(`app_modbus.c:253`)·`holding[0x16]=cfg->f_safty`(`:212`)를 **디코드 앞** 매 tick(`:782/:810`) 갱신하고, 슈퍼루프에서 LCD dispatch(`app.c:129-133`)가 `app_modbus_tick`(`app.c:155`)보다 앞이라 LCD SAVE 직후 tick 에 이미 동기. `HORN_CMD` 분기(`:566`)는 원격 FC06 이 왔을 때만 평가.

## D. 시뮬레이션 (초기 f_safty=0, horn=0, 페이지 9)

표기: `f`=cfg.f_safty, `t`=temp_horndown, `m`=app_horn_mode_active(), `V_S`=VP 0x120d 마지막 쓰기(주체), `V_H`=VP 0x1209 마지막 쓰기(주체).

### D-1. SAFTY → HORN → SAVE (둘 다 OFF 에서 새로 탭)
| 단계 | f | t | m | V_S | V_H | 근거 |
|---|---|---|---|---|---|---|
| 진입 SETUP_PARAM | 0 | **0** | 0 | 0(직전 RUN 렌더) | **0(FW :458, m 미러)** | `input.c:452-460` |
| SAFTY 탭 | **1** | 0 | 0 | 1(패널 토글) | 0 | `:582` |
| HORN 탭 | 1 | **1** | 0 | 1 | 1(패널) | `:601` |
| SAVE | 1 | 1 | **1** | 1(FW `render.c:52`) | 1(잔존) | `comm.c:388→app_horn.c:44-55`, `:396` FRAM f=1 |
| 재진입 | 1 | 0 | 1 | 1(잔존) | 1(FW :458) | — |
**결과: 둘 다 유지. 꺼지는 것 없음.**

### D-2. HORN → SAFTY → SAVE
순서만 바뀜, 결과 동일(둘 다 유지). 두 핸들러가 서로 다른 저장소(cfg vs shadow)를 쓰므로 간섭 없음.

### D-3. ★ 진입 시 horn 이 이미 ON (D-1 직후 재진입, 또는 원격 `0x30=1` 이후) → SAFTY 탭(또는 이미 ✓) → SAVE
| 단계 | f | t | m | V_S | V_H | 비고 |
|---|---|---|---|---|---|---|
| 진입 | 0/1 | **0**(:459 리셋) | 1 | 잔존 | **1(FW :458)** → 화면 ✓ | 화면 ✓ 이지만 shadow 0 |
| SAFTY 탭 | 1 | 0 | 1 | 1 | 1 | 사용자 눈에 "둘 다 ✓" |
| SAVE | 1 | 0 | **0** | 1(FW) | **1(잔존, stale)** | `:388 hook_horn(false)` → `app_horn.c:47-55` SOL OFF, mon `[horn] mode=0` |
| 페이지 9 | | | 0 | 배지 ✓ | **배지 ✓ (거짓)** | `run_std_refresh` 가 0x1209 안 씀 |
| 재진입 | 1 | 0 | 0 | ✓ | **☐(FW :458, m=0)** | 사용자 관찰 = "horn 이 꺼졌다" |
**결과: horn 만 꺼짐 — 모드(실상태) OFF, SETUP 체크박스 OFF, 페이지 9 배지는 stale ON.** 이 상태에서 horn 을 유지하려면 ✓→☐→✓ **두 번 탭**해야 t=1 이 된다(패널 토글이라 첫 탭은 0 을 올림).

### D-4. 진입 시 SAFTY 이미 ON → HORN 탭 → SAVE
f 는 cfg 에 그대로(터치 없으면 `:582` 미실행), V_S 잔존 1, horn ON. **꺼지는 것 없음.** → `f_safty` 는 SAVE 로는 꺼질 수 없다. 꺼지는 경로는 SAFTY 탭(패널 0 토글), Modbus `0x16`, CANCEL(FRAM 재로드 `comm.c:412`) 뿐.

## E. 판정

1. **어느 쪽**: horn. `f_safty` 는 아님.
2. **표시만 vs 상태**: **상태도 꺼진다.** `app_horn_mode_active()`=0 → START 게이트 열림(`app_reg.c:154`), weld 동결 해제(`app_weld.c:125`), STATUS bit6 `0x40`(`app_modbus_core.h:172`) 0, `holding[0x30]` 0(`:253`). 표시는 **불일치**: 페이지 9 HORN 배지 stale ✓, SETUP1 재진입 시 ☐. `f_safty`: cfg 1, FRAM 1(`app_config.c:59`), DISP_SAFTY 1.
3. **원인**: `fw/src/app_lcd_input.c:458-459`(VP 는 모드 미러, shadow 는 0 리셋 → 표시≠적용값) + `fw/src/app_lcd_comm.c:388`(STD SAVE 가 shadow 무조건 적용). 보조: `fw/src/app_lcd_render.c:50-53`(RUN_STD 재렌더가 0x1209 를 안 써 배지 stale).
4. **legacy 동일 여부: 다르다.**
   - samd20 `case SETUP_PARAM` `ref/samd20/main.c:3754-3771` / `SETUP_PARAM_MOOHAN` `:3772-3800`: `lcd_status` 설정 + `change_lcd_page` 만. `temp_horndown` 대입은 파일 전체에서 `:3622`(CANCEL) 와 `:3849/:3855`(터치) 뿐(`grep temp_horndown`). `DISP_HORNDOWN` 쓰기는 `:3228`(init) 과 `:3618/:3620`(CANCEL) 뿐.
   - legacy STD SAVE `:3457-3472`: `temp_horndown==1` → `SYS_HORN` 유지/진입(+SOL OFF, `re_start` 클리어), else → `SYS_RUN`. SAVE 뒤 temp 를 리셋하지 않으므로 **다음 SETUP 방문에서 안 건드리고 SAVE 해도 horn 유지**(D-3 에서 legacy 는 ON 유지).
   - legacy 의 동종 quirk 는 **CANCEL 뒤에만** 존재: `:3617-3622` 가 VP 를 `sys_status` 로 시드하고 `temp_horndown=0` → 그 다음 SAVE 가 horn 을 끈다. 포트는 이 CANCEL 동작을 **SETUP 진입마다** 수행하도록 옮겼다.
   - 오인용 출처: `519d908`(2026-07-18 feat(horn)) 커밋 메시지 "SETUP 재진입 시 체크박스 미러+shadow 리셋(3617-3622 …)" 및 코드 주석 `input.c:455-457` "legacy main.c:3617-3622 verbatim". `3511-3630` 은 `DATA_SAVE` 의 `else { // cancel }` 블록이며 `:3628` 이 `lcd_status = LCD_RUN_STD` 로 닫힌다.
   - 부수 소차: legacy 는 temp==1 SAVE 마다 SOL 강제 OFF(`:3459-3461`), 포트는 모드 무변화 시 무조작(`app_horn.c:44` `if (m != s_mode)`). 본 증상과 무관.
5. **이 브랜치 영향: 없음(기존 결함).**
   - `git diff --stat b61ef0f..8567038` 의 LCD 파일 변경 = `f02dc01`·`c0ed281` 등 ".bin 동일" 리팩토링(커밋 제목에 해시 명기) + `d6c3c14` `app_lcd_run_std_refresh` 추출(쓰는 VP 4개+텍스트 동일, `b61ef0f render.c:57-61` 대조).
   - `b61ef0f:app_lcd_input.c:541-556` 진입 시드+리셋, `:517-519/:536-538` 터치, `b61ef0f:app_lcd_comm.c:386` hook_horn — 현 HEAD 와 동일 로직.
   - 브랜치 신설 HORN 에코 `app_modbus.c:574` 는 `HORN_CMD` 분기(`:566`) 안 — 원격 FC06 도착 시 `apply_writes`(`:798/:810`)에서만 평가되고 `temp_horndown` 은 건드리지 않음(주석 명시). LCD 단독 조작에서는 실행되지 않는다.
   - 기존 문서 정합: `docs/superpowers/research/2026-09-11-setup-sync-investigation.md:77` 은 "원격이 `HORN_CMD=1` 로 켠 horn 은 LCD 가 아무 SETUP 저장을 해도 꺼진다"(2026-09-06 벤치 `plans/2026-09-06-hold-to-run-bench-results.md:64,72`)를 기록 — **같은 원인(:459 리셋 + :388 무조건 적용)이 LCD 로 켠 horn 에도 적용**된다는 것이 이번 발견.

## F. 수정 후보 (나열만, 선택 없음)

| # | 지점 | 내용 | 장 | 단 / legacy 관계 |
|---|---|---|---|---|
| F1 | `input.c:459` | `temp_horndown = app_horn_mode_active()` 로 시드(표시와 shadow 일치) | 1줄. 안 건드리면 유지, 한 탭 토글 | legacy 의 "CANCEL 뒤 SAVE 는 이탈" quirk 도 함께 사라짐(엄밀 legacy 이탈이지만 legacy 정상 경로와 일치) |
| F2 | `input.c:458-459` 삭제 | legacy verbatim — 진입 시 무조작, temp 는 CANCEL(`comm.c:442`)에서만 리셋 | legacy 충실 | 원격 `0x30` 변경 시 VP(에코 :574)와 temp 가 어긋나 원격-ON horn 은 여전히 LCD SAVE 로 꺼짐(setup-sync :77 문제 잔존). 부팅 후 첫 진입 VP 는 init 0 |
| F3 | `comm.c:388` + `app_lcd.h:104` | temp 를 3상태(0xFF=미터치)로 두고 미터치면 `hook_horn` 생략 | LCD·원격 어느 쪽이 켠 horn 도 SAVE 에 무관하게 생존(벤치 §4-5 오염 해소) | legacy "SAVE 는 항상 temp 를 적용" 이탈. 진입 시 0xFF 세팅 필요 |
| F4 | `render.c:50-53` | `run_std_refresh` 에 `DISP_HORNDOWN ← app_horn_mode_active()` 추가 | 페이지 9 배지 stale 제거(표시 정합) | 표시 전용 — 플래그 문제는 못 고침. legacy 는 RUN_STD 렌더에 0x1209 안 씀(`main.c:2958-2963`) |
| F5 | `input.c:455-457` 주석 | "3617-3622 = CANCEL 분기, SETUP_PARAM(3754-3771)은 무조작" 으로 정정 | 오인용 재발 방지 | 어느 안을 택해도 필요 |
| F6 | `app_horn.c` | 변경 불요 — 부수효과 범위(SOL·s_mode)가 이미 최소, `f_safty` 무접촉 | — | — |

## G. 실기 확인 항목 (코드만으로 판정 불가)

1. **재현 전제 확인**: SETUP 진입 직전 horn 이 ON 이었는가. mon 은 Modbus 가 USART6 점유 중 억제(`fw/include/mon.h:12-13`)이므로 `docs/superpowers/tools/mb_tcp.py` 로 FC03 `0x30`/`STATUS bit6(0x40)`/`0x16` 을 **진입 전·SAVE 후·재진입 후** 3회 읽어 표 D-3 과 대조.
2. **원격 쓰기 배제**: 원격기(`gds_us_remote`)·mbpoll 을 끊고 재현. 원격의 `SAFE`/`HORN` 은 세션 필드로 원격 SAVE 때만 `0x16`/`0x30` 을 쓰지만(`gds_us_remote/components/gds_ui/include/gds_ui/setup_fields.h:67-75`), 연결 상태에서 관찰했다면 write-back 가능성을 실측으로 제거.
3. **패널 토글 거동**: ✓ 인 HORN 을 한 번 탭하면 ☐ 가 되고 펌웨어가 에코하지 않는지(자산 type 0x02 루프 해석 검증). 두 번 탭해야 t=1 이 되는지.
4. **페이지 9 배지 stale**: D-3 SAVE 직후 RUN 화면 x≈454,y≈130 의 HORN 배지가 ✓ 로 남는지(모드 OFF 인데 ✓ = 표시/상태 불일치의 육안 증거). 사용자가 "꺼짐"을 본 화면이 페이지 9 배지인지 SETUP1 재진입인지 확인.
5. **SAFTY 쪽**: SAVE 뒤 `0x16` read-back 1 유지·재부팅 후 FRAM 복원 1 — 코드상 확정이나 사용자 보고("둘 중 하나")의 반대쪽 배제용 1회.
6. **legacy 대조(선택)**: samd20 보드가 있다면 D-3 를 그대로 수행해 horn 유지 여부 육안(코드상 유지).
