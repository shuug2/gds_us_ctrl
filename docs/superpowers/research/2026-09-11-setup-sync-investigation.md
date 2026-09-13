# SETUP 값 양방향 동기 조사 — 리모트(gds_us_remote) ↔ 컨트롤러 LCD (2026-09-11)

> **요약**: 사용자 관찰 두 문장을 코드로 판정했다.
> **방향 ① 리모트→컨트롤러 LCD: 맞다.** `app_modbus_apply_writes()` 는 cfg 대입 + FRAM 저장만 하고, LCD VP 에코는 6개(EN_ENERGY/EN_MULTI/EN_SAFTY/MODEL_FREQ/MODEL_TYPE/WORK_CNT)에만 있다. DELAY·TRIGGER·OUT_POWER·ON_TIME·ENERGY·MULTI_*·TIMEOVER·RUN_MODE·CAL·HORN 은 VP 쓰기가 없고, LCD 숫자는 `app_lcd_change_page()` 진입 1회 렌더뿐(주기 갱신 `app_lcd_disp_step()` 은 측정값·바·아이콘 전용). 그래서 **SETUP 페이지에 머무는 동안은 안 바뀌고, 페이지를 나갔다 들어오면 바뀐다.** STD 는 RUN 페이지(9)에도 D/W/H·RUN_MODE·SAFTY·LIMIT 텍스트가 있어 RUN 화면조차 stale 이 된다.
> **방향 ② 컨트롤러 LCD→리모트: 코드상 틀리다(조건부).** LCD 터치는 cfg 를 즉시 쓰고(SAVE 전), `mirror_live()` 가 매 tick 그것을 싣고, 리모트는 50 ms 폴 → 500 ms 스로틀로 setup 오버레이를 재렌더하며 세션 IDLE 이면 라이브를 그대로 그린다. 안 보이는 경우는 (a) 리모트가 그 필드를 **편집 중(DIRTY)** 이라 shadow 를 유지하고 `장치에서 변경됨` 라벨만 붙는 경우, (b) 모델/모드 변경으로 세션이 강제 취소된 경우, (c) 리모트가 RUN 화면(설정값 미표시)에 있거나 링크가 stale 인 경우다. 실기에서 어느 경우였는지는 확인 필요.
> **"SAVE 시 상대 화면 갱신"이 막히는 층**: ①은 컨트롤러 `apply_writes` 의 LCD 에코 부재(+ 페이지 재렌더 부재). ②는 막히지 않는다 — 리모트 SAVE 는 FC06 배치라 컨트롤러엔 "SAVE" 이벤트가 따로 없고 FC06 하나가 곧 저장이다.

---

## 0. 조사 대상·전제

- 컨트롤러: `/Users/tknoh/dev/work/gds_us_ctrl/.claude/worktrees/refactor-byte-identical` (브랜치 `refactor/byte-identical`, main 과 `.bin` 동일 — `3f82c06`).
- 원격기: `/Users/tknoh/dev/work/gds_us_remote` (`a2ae507`, 2026-09-09 "SAFE·HORNDOWN 세션 필드 + 컨트롤러 LCD horn 에코 부재 기록").
- "스탠다드 모드" = `cfg->model_type == 2` (SYS_STD). 펌웨어 빌드 모델(STD/REMOTE)과는 다른 축 — §A.4 에서 구분.
- 아래 `파일:라인` 은 전부 위 두 트리 기준. 추측은 **[추측]** 표시.

---

## A. 방향 ① 리모트 → 컨트롤러 LCD

### A.1 FC06 apply 체인이 하는 일 — 레지스터별 (`fw/src/app_modbus.c` `app_modbus_apply_writes`)

| 레지스터 | 분기 라인 | cfg 대입 | FRAM save | **LCD 에코** |
|---|---|---|---|---|
| `0x0A DELAY1` | 453-457 | `limit_delay_time1` (≤500) | ✓ | **없음** |
| `0x0B DELAY2` | 458-462 | `limit_delay_time2` | ✓ | 없음 |
| `0x0C DELAY3` | 463-468 | `limit_delay_time3` | ✓ | 없음 |
| `0x0D TRIGGER2` | 469-473 | `limit_trigger_time2` | ✓ | 없음 |
| `0x0E TRIGGER3` | 474-478 | `limit_trigger_time3` | ✓ | 없음 |
| `0x06 OUT_POWER` | 479-484 | `output_power` [50,100] | ✓ | **없음** (pot 도 안 씀 — START/페이지 진입 때만) |
| `0x07 ON_TIME` | 485-489 | `limit_on_time` | ✓ | 없음 |
| `0x08 ENERGY` | 490-492 | `limit_energy` | ✓ | 없음 |
| `0x0F~0x12 MULTI_T1/T2/O1/O2` | 493-514 | 각 필드 | ✓ | 없음 (LCD 의 T1>T2 강제도 없음) |
| `0x09 TIMEOVER` | 515-520 | `limit_out_time` (≤10) | ✓ | 없음 |
| `0x13 RUN_MODE` | 521-524 | `run_mode` as-is | ✓ | **없음** — 페이지 전환도 없음 |
| `0x14 EN_ENERGY` | 525-530 | `energy_ctrl` | ✓ | ✓ `dgus_write_u16(DISP_ENERGY_EN)` :529 |
| `0x15 EN_MULTI` | 531-535 | `multi_ctrl` | ✓ | ✓ `DISP_MULTI_EN` :534 |
| `0x16 EN_SAFTY` | 536-551 | `f_safty` 0/1 정규화 | ✓ | ✓ `DISP_SAFTY` :546 |
| `0x30 HORN_CMD` | 552-559 | (cfg 아님) `app_horn_set_mode` | ✗ | **없음** — `DISP_HORNDOWN` 미갱신 (원격기 CLAUDE.md:181 이 "컨트롤러 수정 대상"으로 기록) |
| `0x17 MODEL_FREQ` | 560-569 | `model_freq` | ✓ | ✓ `app_lcd_send_model_str` :568 (모델명 문자열만; `MODEL_FREQ` VP 자체는 안 씀) |
| `0x18 MODEL_TYPE` | 570-585 | `model_type` | ✓ | ✓ `send_model_str` :584 (`sys_mode`·런페이지 미재파생 — 주석 561-563) |
| `0x2E/0x2F CAL/FREQ_CAL` | 586-593 | `cal_val`/`freq_cal_val` | ✓ | 없음 (`VAR_CAL_VAL` VP 미갱신) |
| `WORK_CNTL=0` | 594-604 | `work_cnt=0` | ✓ | ✓ `app_lcd_set_work_cnt(0)` :603 |
| staged 9종(`0x1E~`) | 605-618 → `CFG_CTRL=1` 425-449 | `stg_apply_to_cfg` :363 | ✓ | 없음 (`COMM_ADDR` 등 VP 미갱신) |

- 에코가 있는 6개는 **samd20 충실**이다: 원본 `update_holding_reg(1)` 이 `send_lcd_data_var` 를 부르는 곳은 `ref/samd20/main.c:4520`(ENERGY_EN)·`4530`(MULTI_EN)·`4536`(SAFTY) 셋뿐이고, RUN_MODE 는 저장만(`4507-4510`). 즉 **값 레지스터의 LCD 에코 부재는 legacy 그대로**다.
- 저장은 전체 맵 `app_config_save_all(cfg)` :625 — **LCD 조작자의 미저장 편집까지 함께 FRAM 에 굳는다**(§A.3).

### A.2 SETUP 페이지 표시 중 cfg 가 바뀌면 화면이 따라오는가

- **렌더는 페이지 전환 1회**: `app_lcd_change_page()` `fw/src/app_lcd_render.c:163-240` 이 페이지별 VP 를 cfg 에서 채우고 `dgus_set_page` :215. 호출처 = 페이지 내비(`app_lcd_input.c:361/391/396/454/468/472`), SAVE/CANCEL(`app_lcd_comm.c:398/446`), 부팅·SYS_PIC_NOW(`app_lcd.c:247`). **cfg 변경 이벤트에 걸린 호출은 없다.**
- **주기 갱신은 측정값 전용**: `app_lcd_disp_step()` `fw/src/app_lcd_disp.c:194-250` — step 0~9 = LV_OUTPUT 바·LV_TIME 바·`VAR_POWER/AMP/FREQ/ENERGY`(:141-169) + ICON_RUN/DISP_REMOTE 엣지(:202-212). **LV_* 파라미터 VP 는 하나도 안 쓴다.** `app_lcd_tick()` `app_lcd.c:273-303` 도 fault 엣지 + disp_step 뿐.
- **결론**: SETUP 페이지(STD1/2D/2T/3)에 머무는 동안 Modbus 로 바뀐 DELAY/OUT_POWER/ON_TIME/… 는 **화면에 안 나타난다.** 페이지를 나갔다 다시 들어오면(`change_page` 재호출) cfg 에서 다시 채우므로 **그때 보인다.**
- **STD RUN 페이지도 stale**: `render_run_std()` `render.c:41-93` 이 `LV_DM_DELAY`·`DISP_RUN_MODE`·`DISP_SAFTY`·`LV_LIMIT_OUT_T`(:44-47) + `D : x.xx / W(E) : … / H : …` 텍스트(:49-92)를 cfg 로 **진입 시 1회** 채운다. 이후 DELAY1 을 원격이 바꿔도 `D :` 줄은 옛값. (예외: `DISP_SAFTY` 는 EN_SAFTY 에코 :546 로 갱신, `DISP_STD_DATA1` 은 TRIGGER 모드 SENSOR ON/OFF 만 `app_lcd_weld_sensor_text` :243-260 가 덧씀.)
- RUN_MODE 를 원격이 바꾸면: cfg 만 바뀌고 페이지는 그대로(LCD 의 `handle_run_mode` `input.c:463-474` 는 터치 경로 전용). 다음 STD2 진입에서 `goto_setup2` `input.c:353-354` 가 `cfg->run_mode` 로 2D/2T 를 고르므로 **재진입 시 반영**. RUN_STD 의 `DISP_RUN_MODE` 배지도 재렌더까지 stale.

### A.3 LCD 편집 중(shadow 존재) Modbus 가 같은 필드를 바꾸면 — 누가 이기나

`lcd_app_state_t` shadow 필드 목록 (`fw/include/app_lcd.h:99-104`): `temp_address, temp_speed_idx, temp_parity_idx, temp_comm_mode(0xFF 센티널), temp_ether_ip/nm/gw[4], temp_cnt_reset, temp_horndown`. **그 외 파라미터는 전부 cfg 직접 쓰기.**

| 파라미터 | LCD 터치 시 쓰는 곳 | 경유 | SAVE 시 | CANCEL 시 |
|---|---|---|---|---|
| MODEL_FREQ/TYPE, CAL, FREQ_CAL | `app_lcd_input.c:511-524` | **cfg 직접** | `save_all` | FRAM 재로드 |
| DELAY1/2/3, TRIGGER2/3 | :527-541 | cfg 직접(+클램프 에코) | 〃 | 〃 |
| MULTI_O1/O2, MULTI_T1/T2 | :544-563 | cfg 직접(T1>T2 강제 :552-555) | 〃 | 〃 |
| OUT_POWER, ON_TIME, ENERGY, TIMEOVER, SAFTY | :566-583 | cfg 직접 | 〃(+set_pot :369/377/386) | 〃 |
| ENERGY_EN, MULTI_EN | :586-597 | cfg 직접(토글+에코) | 〃 | 〃 |
| RUN_MODE | `handle_run_mode` :463-474 | cfg 직접 + 즉시 페이지 전환 | 〃 | 〃 |
| **comm addr/speed/parity** | `app_lcd_comm.c:46-80` | **shadow** `temp_*` | `commit_comm_serial_shadows` :259-276 (temp≠cfg 면 cfg←temp) | 센티널 재무장 :413 |
| **comm_mode, ether IP/NM/GW** | `comm.c:83-252` | **shadow** | `commit_comm_mode_and_ether` :292-330 | 〃 |
| **cnt_reset** | `input.c:397-400` | shadow | `commit_cnt_reset` :333-344 | — |
| **horndown** | `input.c:600-602` | shadow | `app_lcd_hook_horn(temp_horndown==1)` :388 | temp=0 :442 |

판정:
1. **cfg 직접 필드(대다수)**: 마지막에 쓴 쪽이 이긴다. LCD SAVE 는 `save_all(cfg)` :396 이라 원격이 그 사이에 넣은 값을 **덮지 않는다**(cfg 에 이미 들어 있음). LCD CANCEL 은 `app_config_load` :412 인데, 원격 FC06 의 `save_all` :625 이 그 시점 cfg 전체(LCD 미저장 편집 포함)를 FRAM 에 굳혔으므로 **CANCEL 이 LCD 편집을 되돌리지 못하는 부작용**이 있다(원격 쓰기 1건이 LCD 조작자의 "취소 예정" 편집을 영속시킴). 이건 stale-미러 레이스(`docs/changelog.md:66-79`)와는 별개의, 전체-맵 저장 자체의 성질이다.
2. **comm shadow(addr/speed/parity/comm_mode/ether)**: 페이지 진입 시 cfg 에서 시드(`render.c:106-110, 122-135`). 그 뒤 원격이 `CFG_CTRL=1` 로 comm 을 커밋해 cfg 가 바뀌면, LCD 조작자가 SAVE 를 누르는 순간 `temp_* != cfg` 가 되어 **LCD 의 옛 shadow 가 원격 커밋을 덮어쓴다**(`comm.c:266-271, 308-326`). 이 경로는 LCD 가 comm 페이지를 열어 둔 채 원격이 커밋하는 경우에만 열린다.
3. **horndown**: STD SAVE 는 `temp_horndown` 을 무조건 적용(:388, `input.c:455-459` 주석 "저장 시 체크 안 건드리면 temp==0 이라 모드 이탈되는 legacy 거동"). 원격이 `HORN_CMD=1` 로 켠 horn 모드는 **LCD 조작자가 아무 SETUP 저장을 해도 꺼진다** — 2026-09-06 벤치가 실측(`docs/superpowers/plans/2026-09-06-hold-to-run-bench-results.md:64,72`).

### A.4 "스탠다드 모드" 조건의 코드상 의미

- **`cfg->model_type==2` (SYS_STD)**: LCD 페이지군이 STD1/2D/2T/3/C/E(`input.c:158-162, 333-401`), RUN 페이지 9 에 설정값 텍스트가 있다(§A.2). `run_mode` 분기는 STD 전용(`input.c:353-354`, `render.c:49/70`). HAND/MULTI RUN(3) 은 설정값 텍스트가 없어 RUN 화면 stale 증상은 STD 에서만 두드러진다. 원격기도 setup2 행·RUN_MODE 체크를 STD 에서만 보인다(`components/gds_ui/src/pure/setup_fields.c:279-290`). → **조건은 실질적 의미가 있다(증상이 STD 에서 더 넓게 보임)**, 다만 SETUP 페이지 stale 자체는 모델 무관.
- **빌드 모델 STD/REMOTE(`MODEL_REMOTE`)**: apply 체인은 모델 무관 공통(`app_modbus.c:116-119` 주석). 차이는 게이트 하나 — REMOTE 빌드에서 `s_ren.state != REN_ENABLED` 면 cfg 쓰기 전체를 **무음 거부**(`:398-402`, read-back 미러 복원이 유일한 관측 :392-393). 현재 보드 = REMOTE `V3.1.0R!` 극성 반전 → PC8 미실장 HIGH = 열림. 게이트가 닫혀 있었다면 방향 ① 은 "표시가 안 됨"이 아니라 **"쓰기 자체가 안 됨"** 으로 나타났을 것 — 원격기 화면에 `실패`/불일치가 떴을 것이다. 사용자 관찰이 "값은 들어갔는데 LCD 만 안 바뀜"인지 확인 필요(§C.3).

### A.5 방향 ① 판정

**맞다.** 원인 = `app_modbus_apply_writes()` 가 값 레지스터에 LCD VP 에코를 하지 않고(`app_modbus.c:453-524, 586-593, 605-618`), LCD 는 페이지 진입 1회 렌더(`render.c:163`)에만 의존하며 주기 갱신은 측정값 전용(`disp.c:194-250`)이기 때문. **페이지 재진입 시에는 반영된다.** legacy(samd20) 도 같은 거동(`main.c:4520/4530/4536` 3개만 에코).

---

## B. 방향 ② 컨트롤러 LCD → 리모트 화면

### B.1 LCD 터치로 SETUP 값을 바꾸면 cfg 는 언제 바뀌나

- **즉시** — SAVE 전. §A.3 표: MODEL·CAL·DELAY·TRIGGER·MULTI·OUT_POWER·ON_TIME·ENERGY·TIMEOVER·SAFTY·ENERGY_EN·MULTI_EN·RUN_MODE 전부 `app_lcd_input.c:511-597` 에서 `cfg->` 직접 대입. 파일 헤더 :6-7 이 "Config edits write app_lcd_cfg() directly (samd20 immediate-to-live)" 로 명시.
- 예외(shadow → SAVE 시 반영): comm addr/speed/parity, comm_mode, ether, cnt_reset, horndown(§A.3 표).
- 2026-08-30 원격기 실측("SAVE 전에 RUN_MODE 0→1 이 미러에 보였고 CANCEL 로 되돌아감", `gds_us_remote/docs/reference/modbus-contract.md:159-167`)과 **일치**. 그 문서의 2026-08-01 구판 서술("LCD 는 shadow, 미러는 cfg만", :171-176)은 현 코드에 맞지 않고 문서 자체가 뒤집힘 처리(:157).

### B.2 `mirror_live()` 가 매 tick cfg 를 싣는가

- ✓. `mirror_live()` `app_modbus.c:277-289` → `mirror_cfg_fields` :172-189(DELAY~TIMEOVER) + `mirror_disp_status` :207-215(MODEL/RUN_MODE/EN_*/CAL) + `mirror_stage_and_gate` :243-273(comm/eth/CAP/HORN). 호출 = RTU 분기 `:759`, TCP 분기 `:787` — **디코드 앞, 매 tick**.
- 따라서 컨트롤러는 LCD 편집값을 **저장 전이든 후든 이미 Modbus 로 내보내고 있다.** 방향 ② 의 미표시가 있다면 컨트롤러 탓은 아니다.

### B.3 원격기: FC03 폴 → 화면 반영 경로와 정책

경로(`gds_us_remote`):
1. `poll_session()` `main/app_main.c:1227-1242` — `gds_mb_read(0, 50)` → `gds_snapshot_parse` → `app.live_snapshot` 갱신. 주기 `POLL_PERIOD_MS 50` :52.
2. `publish_live` 계열 :844-876 — **500 ms 스로틀**(`OVERVIEW_REFRESH_MS 500` :62, 사유 주석 :844-851 폴 예산) 안에서 `app.setup != NULL` 이면 `gds_ui_setup_update_snapshot(app.setup, &live_snapshot, live_valid, …)` :871-873. **오버레이가 열려 있을 때만** 흐른다(:869-870).
3. `gds_ui_setup_update_snapshot` `components/gds_ui/src/ui_setup.c:926-957` → `gds_ui_stepper_page_update(setup1/setup2/comm)` + `gds_ui_profile_update` :935-938 → 모델/모드 플립 배너 :942-957.
4. `gds_ui_stepper_page_update` `ui_stepper_page.c:1194-1207` — `last_snap` 저장 → `gds_edit_on_live(&session, snapshot, valid)` :1206 → `render(page)` :1207.
5. `render` :415-… 각 행을 `gds_edit_display_raw` :468-469 로 그린다.

정책(`components/gds_ui/src/pure/edit_session.c`):
- **IDLE(세션 없음)**: `gds_edit_display_raw` :392-396 → **라이브 그대로**. → LCD 편집값이 500 ms 이내 원격 화면에 보인다.
- **ACTIVE(편집 중)**: `gds_edit_on_live` :169-212 — CLEAN 필드는 baseline·shadow ← 라이브(:203-206, **안 건드린 행은 따라온다**); DIRTY/FAILED/UNSENT 는 shadow 동결 + `라이브 != baseline` 이면 `conflict` 라치(:208-209) → 라벨 `장치에서 변경됨`(`gds_edit_field_status` :442, 우선순위 주석 헤더 :187-194); PENDING/STAGED/REJECTED 는 무시(:190-202). `display_raw` :398-402 는 세션 중 shadow 반환.
- **모델/모드 변경**: `on_live` :175-181 라치 → `render` `ui_stepper_page.c:415-431` 이 SAVING 이 아니면 세션을 `gds_edit_init` 으로 **강제 취소**하고 `모델 변경됨 · 편집 취소` / `모드 변경됨 · 편집 취소`(:427-430). `mode_conflict` 는 행 집합이 바뀌는 setup2 에서만(`edit_session.c:43-54, 178-181`).
- 문서: `docs/superpowers/plans/2026-08-17-p18-setup1-edit-session.md:96, 101, 236` — "CLEAN 필드: 라이브를 따라간다 … 표시 규칙 IDLE→라이브 … 실보드 ③ 컨트롤러 LCD 로 다른 필드 변경 → 화면 따라옴".
- RUN 화면(`run_presenter.h:101-126`)은 설정값을 **OUT_POWER 마커**(:108-110)와 모델명 외엔 안 그린다 — RUN 화면을 보고 있었다면 DELAY 등의 변화는 원래 보일 자리가 없다.

### B.4 방향 ② 판정

**코드상 틀리다(조건부).** 컨트롤러는 내보내고(§B.2), 원격기는 받아서 그린다(§B.3, IDLE 라이브 추종 500 ms). 관찰이 사실이라면 코드가 예측하는 후보 원인은:
- (a) 원격 setup 오버레이에서 **그 필드를 편집 중(DIRTY)** — shadow 유지 + `장치에서 변경됨` 라벨(`edit_session.c:208-209, 398-402`). "값이 안 바뀐다"로 보이는 정상 설계.
- (b) LCD 에서 **RUN_MODE 를 바꾼 뒤** 원격 setup2 세션이 `모드 변경됨 · 편집 취소` 로 리셋된 뒤 라이브 표시(`ui_stepper_page.c:427-430`) — 이건 "안 바뀜"이 아니라 "편집 날아감".
- (c) 원격이 RUN 화면 또는 오버레이 미개방(`app_main.c:869-873` 은 `app.setup != NULL` 일 때만) — 설정값이 보일 자리가 없다.
- (d) 링크 stale/단절(`live_valid=false` 면 `on_live` 무동작 :172-174, IDLE 표시는 `--` :393-395).
- **[추측]** 사용자가 본 상황이 (a) 일 가능성이 가장 크다 — LCD 와 원격 양쪽에서 같은 필드를 동시에 다루는 시험이었을 것.

---

## C. 판정 요약 · 후보 지점 · 미확인

### C.1 사용자 관찰 판정

| 문장 | 판정 | 근거 |
|---|---|---|
| "리모트에서 수정하면 컨트롤러 LCD 화면에 안 바뀜" | **맞다** (SETUP 페이지 체류 중 + STD RUN 페이지 텍스트; 재진입 시엔 바뀜) | `app_modbus.c:453-524/586-618` LCD 에코 없음, `render.c:163` 진입 1회 렌더, `disp.c:194-250` 측정값 전용 |
| "컨트롤러 LCD 에서 수정하면 리모트 화면에 안 바뀜" | **코드상 틀리다** — 세션 IDLE 이면 500 ms 내 라이브 표시. 편집 중(DIRTY) 필드는 shadow 유지+conflict 라벨(설계) | `app_lcd_input.c:511-597` 즉시 cfg, `app_modbus.c:759/787` 매 tick 미러, `app_main.c:871`, `edit_session.c:169-212, 392-402` |
| "SAVE 시 상대 화면에 보이면 좋겠다" | ①: 컨트롤러 LCD 에코/재렌더 부재가 막음. ②: 이미 SAVE 전부터 보임(막힘 없음). 원격 SAVE 는 FC06 배치라 컨트롤러엔 별도 SAVE 이벤트 없음(`ui_stepper_page.c:725-`, `edit_session.c:214-`) | — |

### C.2 "SAVE 시 상대 화면 갱신"을 만들 때 손댈 후보 지점 (선택 안 함)

**컨트롤러 측 (방향 ①)** — 공통 배경: 원격 "SAVE" = FC06 N건. 컨트롤러가 알 수 있는 유일한 시점은 `apply_writes` 의 각 cfg 분기(= 저장 시점과 동치).

| # | 지점 | 장 | 단 / 원칙 충돌 |
|---|---|---|---|
| C-1 | `apply_writes` 각 cfg 분기에 해당 VP 1줄 에코(`dgus_write_u16(LV_DM_DELAY, …)` 등) — 기존 EN_*/SAFTY 에코 :529/534/546 와 동형 | 최소 diff, 페이지 표시 중 즉시 반영(DGUS 는 표시 중 VP 쓰기에 반응 — 클램프 에코 `input.c:410/554` 가 같은 전제), 페이지 밖이면 VP RAM 에 남아 무해 | 15 분기 × 1줄; RUN_STD `D/W/H` **텍스트**는 VP 1개가 아니라 `render_run_std` 재호출 필요; **legacy 이탈**(samd20 은 3개만 에코) — 단 "LCD 에 없는 규칙을 원격에만 발명" 원칙과는 무관(원격 제약이 아니라 LCD 표시 보강) |
| C-2 | `apply_writes` `if (save)` :621 뒤에 현재 페이지가 SETUP/RUN_STD 면 `app_lcd_change_page(state->lcd_status)` 재렌더 | 1지점, 전 필드+텍스트 커버 | `dgus_set_page` 재전송(깜빡임·터치 중 페이지 리셋), `render_setup_main` 이 `set_pot` 호출(:101)·comm shadow 재시드(:106-110, temp_* 를 cfg 로 덮어 **LCD 조작자의 comm 편집 손실**) — 부작용이 원격 쓰기에 실려 나감; legacy 없음 |
| C-3 | `app_lcd_disp_step` 에 LV_* 재쓰기 step 추가(주기 갱신) | 이벤트 배선 불필요 | DGUS UART 대역(4 ms step 예산), 터치 클램프 에코와 경합 가능, 페이지별 VP 분기 필요; legacy 없음 |
| C-4 | `HORN_CMD` 분기 :552-559 에 `dgus_write_u16(DISP_HORNDOWN, …)` | 원격기가 이미 "컨트롤러 수정 대상"으로 기록(`gds_us_remote/CLAUDE.md:181`) | `DISP_HORNDOWN` 은 setup1 의 shadow 소스이기도 함(`input.c:600-602` 는 터치 값을 temp 에 넣음) — 에코가 temp 를 바꾸진 않지만 SAVE 시 `temp_horndown` 적용(§A.3-3)과의 의미 정합을 함께 정해야 함 |
| C-5 | (설계 메모) 원격 쓰기 `save_all` :625 이 LCD 미저장 편집까지 굳히는 부작용(§A.3-1) — 갱신과 별개지만 같은 함수 | — | 고치려면 필드별 저장 또는 LCD 편집 세션 인지 필요 = 큰 변경; 현 상태는 legacy 와 다름(samd20 은 필드별 `save_byte_fram`) |

**원격기 측 (방향 ②)** — 코드상 이미 됨. 손댈 곳이 있다면:

| # | 지점 | 장 | 단 |
|---|---|---|---|
| R-1 | 현 정책 유지(IDLE 라이브 추종, DIRTY 는 conflict 라벨) | 변경 0 | 실기 미표시 원인이 §B.4 (a)~(d) 중 무엇인지 먼저 확인해야 함 |
| R-2 | `OVERVIEW_REFRESH_MS 500` :62 단축 | 체감 개선 | 폴 예산(주석 :844-851, 18.8→10.4/s 실측 사례) |
| R-3 | DIRTY 필드도 라이브로 덮기(conflict 대신) | "항상 상대 값" | 조작자 편집값 소실 — 원격기 규율("보냈다/먹혔다 구분", `edit_session.h:20-25`)과 충돌. 비권장 근거 있음 |

### C.3 코드만으로 판정 불가 — 실기 확인 필요

1. **DGUS 위젯 라이브 반응**: 페이지 표시 중 `LV_*` 숫자 VP 를 쓰면 즉시 바뀌는가 — 클램프 에코(`input.c:410, 554`)가 그렇게 동작한다는 전제이나 전 위젯 확인은 안 됨. comm 아이콘은 페이지 23/27 에서 페이지 진입 시 자동 로드가 안 되는 quirk 가 있음(`render.c:218-227`) — 반대 방향(라이브 쓰기만 먹음)이라 C-1 에는 유리.
2. **방향 ② 관찰 상황**: 원격기가 어느 화면(RUN / setup 오버레이 어느 탭)이었나, 그 필드가 편집 중(DIRTY)이었나, `poll rate` 로그가 살아 있었나, 라벨에 `장치에서 변경됨` 이 떴었나.
3. **방향 ① 관찰 상황**: LCD 가 SETUP 페이지에 머문 상태였나(재진입 후에도 안 보였다면 코드 판정과 어긋남 → 게이트 닫힘 §A.4 또는 원격 쓰기 실패를 의심), 원격기 행 라벨이 `저장됨` 이었나 `실패`/`미시도` 였나.
4. **게이트**: REMOTE 빌드 `0x2B REMOTE_EN` 이 1 이었나(닫힘이면 cfg 쓰기 무음 거부 `app_modbus.c:399-402`).
5. STD RUN 페이지 `D/W/H` 텍스트가 원격 DELAY 쓰기 뒤 실제로 stale 로 보이는지(코드 판정 §A.2, 육안 미확인).

---

## D. 참고한 문서 위치

- 컨트롤러: `docs/superpowers/specs/2026-08-30-remote-parity-requirements.md:95-155`(B-5), `docs/superpowers/plans/2026-09-05-bench-results.md:20, 217-221`(MOD-1~7 — MOD-4 "type 쓰기 후 STD 화면 유지" 가 §A.1 MODEL_TYPE 행의 실측), `docs/changelog.md:66-79`(stale-미러), `docs/superpowers/plans/2026-09-06-hold-to-run-bench-results.md:64,72`(LCD SETUP 저장이 horn 재전송).
- 원격기: `docs/reference/modbus-contract.md:157-176`(미러 커밋 시점 — 2026-08-30 정정), `:474-480`(§3.4), `docs/reference/ctrl-lcd-structure.md §0`, `docs/superpowers/plans/2026-08-17-p18-setup1-edit-session.md:96-101, 236`, `CLAUDE.md:181`(horn 에코 부재).
- legacy: `ref/samd20/main.c:4500-4545`(update_holding_reg(1) LCD 에코 3곳).
