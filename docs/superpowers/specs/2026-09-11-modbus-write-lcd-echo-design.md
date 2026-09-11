# Modbus FC06 쓰기 → LCD VP 에코 — 설계 spec (2026-09-11)

> **문서 요약**: 원격기(gds_us_remote)가 FC06 으로 SETUP 파라미터를 쓰면 컨트롤러는 cfg 대입 + FRAM 저장까지 하지만
> **LCD 에 새 값을 써 주지 않아** 조작자가 그 페이지에 머무는 동안 옛 숫자가 남는다(조사 `research/2026-09-11-setup-sync-investigation.md` §A).
> 이 spec 은 사용자가 확정한 **방법 A** — `app_modbus_apply_writes()` 의 cfg 분기마다 이미 있는 3개 에코(`DISP_ENERGY_EN`/`DISP_MULTI_EN`/`DISP_SAFTY`)와
> **동형**으로 `dgus_write_u16(VP, 값)` 1줄씩 추가 — 를 코드 좌표까지 내려 확정한다. 에코 대상은 **SETUP 숫자 13분기(14 VP 쓰기)** + **STD RUN 페이지(9) 텍스트 재기록 1호출**(`render_run_std` 를
> 공개 함수 `app_lcd_run_std_refresh()` 로 승격, `if (save)` 뒤 1줄) + 판정 대기 3건(HORN 체크박스 · CAL 2개 · MODEL 선택 2개, 전부 **포함 권고**). staged comm 레지스터는 제외(shadow 기반·커밋 시점 상이).
> 추가 UART 지연은 apply 1회당 **최악 100 B ≈ 8.7 ms**(115200 8N1) — hold 워치독 적층 579 → **587.7 ms < 600** 으로 예산 안. FLASH ≈ **+240 B**(0.18 %). samd20 은 3곳만 에코했으므로 **의도적 legacy 이탈**이며, host 테스트 불가 영역(HAL 글루)이라 게이트는 **HW 벤치**(§5).

작성 2026-09-11 · 작업 트리 `.claude/worktrees/feat-modbus-write-lcd-echo`(브랜치 `feat/modbus-write-lcd-echo`, `refactor/byte-identical` tip `3f82c06` 위) · 아래 `파일:라인` 은 전부 이 트리 기준으로 직접 확인한 값. 구현 plan = `plans/2026-09-11-modbus-write-lcd-echo.md`.

---

## 0. 확정 결정 (2026-09-11 사용자)

| # | 결정 | 귀결 |
|---|---|---|
| 1 | **방법 A** — `apply_writes` 분기 안 VP 에코(조사 §C.2 C-1). 페이지 재렌더(C-2)·주기 재쓰기(C-3)는 **기각** | `dgus_set_page` 재전송 없음(깜빡임·터치 리셋 없음), `render_setup_main` 의 `set_pot`/comm shadow 재시드 부작용을 원격 쓰기에 싣지 않는다 |
| 2 | **LCD 편집 중 충돌은 범위 밖** — 양쪽이 같은 값을 동시에 편집하는 경우의 우선권·병합은 다루지 않는다 | cfg 직접 필드는 "마지막에 쓴 쪽이 이긴다"(조사 §A.3-1) 그대로. DGUS 입력 위젯이 편집 중 값을 덮어쓰는지는 **실기 확인 항목**(§5 E-12)으로만 남긴다 |
| 3 | **§3.3 판정 항목 전부 포함** — HORN_CMD→`DISP_HORNDOWN`, CAL/FREQ_CAL→MODEL_SETUP 보정 VP(raw int16 캐스트), MODEL_FREQ/TYPE→선택 표시 VP | Task 3 실행. 각 1줄, 기존 진입 시드와 동형 |
| 4 | **ENERGY 에코 스케일 = 페이지 렌더와 동일(raw)** — `LV_ENERGY_VAL`(u32 raw) + `LV_ENERGY_EDIT`(u16 raw), `render_setup_main` :102-103 동형 | 부팅 시드 `app_lcd.c:240` 의 `/10` 불일치(R4)는 **이번 범위 밖, 기록만**. 사람이 SETUP 페이지에 머무는 동안 보이는 값은 렌더 값이므로 그것과 맞춘다 |
| 5 | **PR #1(`refactor/byte-identical`) 위에 스택, 벤치 PASS 후 태그** — PR #1 머지 뒤 이 브랜치를 PR #2 로. 태그 `hw-revA_fw-stage-lcd-echo` 는 HW 벤치 §5.2 전건 PASS 시 | PR #1 의 ".bin 무변경" 주장을 깨지 않는다(§7) |

---

## 1. 배경 — 조사 결과 인용

- `app_modbus_apply_writes()` `fw/src/app_modbus.c:375-630` 은 cfg 대입 + `app_config_save_all` :625 만 하고, LCD VP 에코는 **6개**에만 있다: `EN_ENERGY→DISP_ENERGY_EN` :529 · `EN_MULTI→DISP_MULTI_EN` :534 · `EN_SAFTY→DISP_SAFTY` :546 · `MODEL_FREQ/TYPE→app_lcd_send_model_str` :568/:584(모델명 문자열만) · `WORK_CNTL→app_lcd_set_work_cnt(0)` :603. (조사 §A.1 표)
- LCD 숫자는 **페이지 진입 1회 렌더** `app_lcd_change_page()` `fw/src/app_lcd_render.c:163-240` 뿐이고, 주기 갱신 `app_lcd_disp_step()` `fw/src/app_lcd_disp.c:194-250` 은 측정값·바·아이콘 전용 — **LV_\* 파라미터 VP 는 하나도 안 쓴다.** (조사 §A.2)
- 그래서 SETUP 페이지에 머무는 동안 Modbus 로 바뀐 값은 화면에 안 나타나고, **STD 는 RUN 페이지(9) 도 stale** 이다 — `render_run_std()` `render.c:41-93` 이 `D/W(E)/H` 3줄 + `LV_DM_DELAY`/`DISP_RUN_MODE`/`DISP_SAFTY`/`LV_LIMIT_OUT_T` 4필드를 진입 시 1회 채운다. (조사 §A.2)
- 에코가 있는 3개는 samd20 충실(`ref/samd20/main.c:4520/4530/4536`) — 값 레지스터의 에코 부재는 **legacy 그대로**다. (조사 §A.1)
- DGUS 는 표시 중 VP 쓰기에 즉시 반응한다 — LCD 의 클램프 에코 `app_lcd_input.c:410/417/554/561` 이 그 전제 위에 있고 HW 검증됐다. 페이지 밖 VP 쓰기는 VP RAM 에 남아 무해하다(`change_page` 가 "VP 쓰기 → `set_page`" 순서로 동작하는 것 자체가 그 증거, `render.c:175-215`). (조사 §C.2 C-1, §C.3-1)

---

## 2. 범위

### 2.1 In

- `fw/src/app_modbus.c` `app_modbus_apply_writes()` — cfg 분기 13개에 VP 에코 1~2줄 + `if (save)` 뒤 STD RUN 재기록 1줄 + (판정 후) HORN/CAL/MODEL 에코 5줄.
- `fw/src/app_lcd_render.c` — `render_run_std`(static always_inline, :41-93) 를 공개 함수 `app_lcd_run_std_refresh(void)` 로 승격 + `app_lcd_change_page` 의 호출 1곳 교체 + 미사용 `buf` 제거(:171). `fw/include/app_lcd.h` 선언 1줄.
- 문서: 이 spec · plan · `docs/changelog.md [Unreleased]` · `docs/requirements.md` FW3 항목 1줄 · `fw/include/define.h` 빌드 날짜(플래시 직전, 관례).

### 2.2 Out (명시적 제외)

| 항목 | 근거 |
|---|---|
| **staged comm 9종**(`0x1E~0x27`, `CFG_CTRL=1` 커밋) → `COMM_ADDR`/`COMM_SPEED`/`COMM_PARITY`/`COMM_*_TXT`/`COMM_IP_TXT`… | COMM 페이지는 **shadow(`temp_*`) 기반**이다 — 페이지 진입 시 cfg 에서 시드(`render.c:106-110, 122-135`)하고 SAVE 시 `temp_* != cfg` 면 cfg←temp 로 되쓴다(`app_lcd_comm.c:266-271, 308-326`). 커밋 시점도 staged 쓰기가 아니라 `CFG_CTRL=1`(`app_modbus.c:425-449`)이다. 여기서 cfg 값을 VP 에 에코하면 위젯과 shadow 가 어긋나고, 조작자의 SAVE 가 원격 커밋을 **옛 shadow 로 덮어쓰는** 부작용(조사 §A.3 판정 2, "부작용 ①")은 그대로 남는다. 고치려면 shadow 재시드 = 다른 설계 → 이번 범위 밖 |
| RUN_MODE 쓰기 시 **SETUP2D↔2T 페이지 전환** | 기각된 C-2 계열(페이지 조작). 조작자가 SETUP2 에 있는 동안은 페이지 자체가 모드 표시이고, 재진입 시 `goto_setup2` `input.c:353-354` 가 새 `run_mode` 로 페이지를 고른다. RUN_STD 의 배지·텍스트는 §3.2 로 갱신 |
| OUT_POWER 에코 시 `set_pot` | 표시 동기가 목적. pot 은 START/페이지 진입/SAVE 때 쓴다(`input.c:567-569` 주석, `app_modbus.c:340-344`) — 기존 결정 유지 |
| MULTI_T1>T2 강제(LCD `input.c:552-555`) 를 Modbus 경로에 추가 | 기존 편차(조사 §A.1 표 "LCD 의 T1>T2 강제도 없음"). 표시 동기와 무관 |
| 원격 쓰기 `save_all` 이 LCD 미저장 편집까지 굳히는 부작용(조사 §A.3-1, C-5) | 같은 함수지만 다른 문제. 필드별 저장 또는 편집 세션 인지 = 큰 변경 |
| LCD 편집 중 충돌 해소 | §0-2 |
| 원격기(gds_us_remote) 코드·계약 문서 | 계약 무변경(§5.3). 벤치 PASS 후 통보만 |

---

## 3. 에코 대상 표

판단 기준 = **"그 페이지에 사람이 머물러 있는 동안 값이 보이는가."** VP 상수는 `fw/include/dgus_lcd.h`(`LV_*` :61-66, :105-118 · `DISP_*` :67-69, :109-113 · `MODEL_*` :79-80 · `VAR_*CAL_VAL` :132-133 · `DISP_STD_DATA*` :120-122).

### 3.1 SETUP 숫자 — 13분기, 14 VP 쓰기 (Task 1)

| 레지스터 | 분기 `app_modbus.c` | cfg 필드 (타입) | LCD 가 그리는 VP · 렌더 호출 | 값 변환 | 보이는 페이지 | 에코 코드 (분기 안, cfg 대입 직후) |
|---|---|---|---|---|---|---|
| `0x0A DELAY1` | :453-457 | `limit_delay_time1` (u16) | `LV_DM_DELAY` — `dgus_write_u16` `render.c:183`(STD2D) · `:44`(RUN_STD) · 클램프 에코 `input.c:528` | 없음 | SETUP_STD2D(12) · RUN_STD(9) | `dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);` |
| `0x0B DELAY2` | :458-462 | `limit_delay_time2` | `LV_DM_WELD` `render.c:184` · `input.c:531` | 없음 | STD2D · (RUN_STD `W :` 줄, §3.2) | `dgus_write_u16(LV_DM_WELD, cfg->limit_delay_time2);` |
| `0x0C DELAY3` | :463-468 | `limit_delay_time3` | `LV_DM_HOLD` `render.c:185` · `input.c:534` | 없음 | STD2D · (RUN_STD `H :`) | `dgus_write_u16(LV_DM_HOLD, cfg->limit_delay_time3);` |
| `0x0D TRIGGER2` | :469-473 | `limit_trigger_time2` | `LV_TM_WELD` `render.c:188` · `input.c:537` | 없음 | SETUP_STD2T(13) · (RUN_STD `W :`) | `dgus_write_u16(LV_TM_WELD, cfg->limit_trigger_time2);` |
| `0x0E TRIGGER3` | :474-478 | `limit_trigger_time3` | `LV_TM_HOLD` `render.c:189` · `input.c:540` | 없음 | STD2T · (RUN_STD `H :`) | `dgus_write_u16(LV_TM_HOLD, cfg->limit_trigger_time3);` |
| `0x06 OUT_POWER` | :479-484 | `output_power` (u8) | `LV_OUT_POWER` `render.c:100` · `input.c:570` | 없음(u8→u16 승격) | SETUP_HAND(7)/MULTI(5)/STD1(10) | `dgus_write_u16(LV_OUT_POWER, cfg->output_power);` |
| `0x07 ON_TIME` | :485-489 | `limit_on_time` (u16) | `LV_MAX_ON_TIME` `render.c:99` · `input.c:573` | 없음 | HAND/MULTI/STD1 | `dgus_write_u16(LV_MAX_ON_TIME, cfg->limit_on_time);` |
| `0x08 ENERGY` | :490-492 | `limit_energy` (u32; wire 는 u16) | `LV_ENERGY_VAL` **u32** `dgus_write_u32` `render.c:102` + `LV_ENERGY_EDIT` **u16** `render.c:103` · 터치 입력 `input.c:575-577` | 없음(render_setup_main 기준) ⚠ `app_lcd.c:240` 부팅 시드는 `/10` — 기존 불일치, 페이지 렌더가 정본이므로 렌더를 따른다 | HAND/MULTI/STD1 · (RUN_STD `E :`) | `dgus_write_u32(LV_ENERGY_VAL, cfg->limit_energy);` `dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)cfg->limit_energy);` |
| `0x0F MULTI_T1` | :493-497 | `limit_mo_time1` | `LV_MO_TIME1` `render.c:195` · `input.c:551` | 없음 | SETUP_STD3(15)/MH2(19) · (RUN_STD `W :` multi) | `dgus_write_u16(LV_MO_TIME1, cfg->limit_mo_time1);` |
| `0x10 MULTI_T2` | :498-502 | `limit_mo_time2` | `LV_MO_TIME2` `render.c:196` · `input.c:554/561` | 없음 | STD3/MH2 | `dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);` |
| `0x11 MULTI_O1` | :503-508 | `limit_mo_out1` | `LV_MO_OUT1` `render.c:193` · `input.c:545` | 없음 | STD3/MH2 | `dgus_write_u16(LV_MO_OUT1, cfg->limit_mo_out1);` |
| `0x12 MULTI_O2` | :509-514 | `limit_mo_out2` | `LV_MO_OUT2` `render.c:194` · `input.c:548` | 없음 | STD3/MH2 | `dgus_write_u16(LV_MO_OUT2, cfg->limit_mo_out2);` |
| `0x09 TIMEOVER` | :515-520 | `limit_out_time` (u16, ≤10) | `LV_LIMIT_OUT_T` `render.c:105`(setup main) · `:47`(RUN_STD) · `input.c:579` | 없음 | HAND/MULTI/STD1 · RUN_STD | `dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);` |

전부 `dgus_write_u16(VP, cfg->필드)` 원형(ENERGY 만 u32+u16 두 줄) — 값 변환(÷10·×100)은 **어느 렌더에도 없다.** 클램프 뒤의 cfg 값을 쓰므로 원격이 `DELAY1=600` 을 써도 LCD 는 **500** 을 보인다(LCD 클램프 에코 `input.c:406-419` 와 같은 결과).

### 3.2 STD RUN 페이지(9) 텍스트 — 재기록 1호출 (Task 2)

`render_run_std()` `render.c:41-93` 가 쓰는 것과 영향 레지스터:

| RUN_STD 표시 | 쓰기 함수·바이트 | 영향을 주는 FC06 |
|---|---|---|
| `LV_DM_DELAY` | `dgus_write_u16` :44 (8 B) | DELAY1 (§3.1 에코와 중복 — 무해) |
| `DISP_RUN_MODE` 배지 | `dgus_write_u16` :45 | **RUN_MODE** |
| `DISP_SAFTY` | `dgus_write_u16` :46 | EN_SAFTY (이미 :546 에코) |
| `LV_LIMIT_OUT_T` | `dgus_write_u16` :47 | TIMEOVER (§3.1 중복) |
| `DISP_STD_DATA1` — `D : x.xx`(DELAY) / `SENSOR OFF`(TRIGGER) | `time2str`(≤6 B incl NUL, `app_lcd_str.c:27-62`)+`dgus_write_bytes` :52 / 11 B :74 → 프레임 ≤16 / 17 B | DELAY1 · RUN_MODE |
| `DISP_STD_DATA2` — `E : …`(energy_ctrl) / `W : …`(multi 면 T1+T2, 아니면 DELAY2 또는 TRIGGER2) | `energy2str`(≤7 B, `str.c:67-114`)/`time2str` + `dgus_write_bytes` :57/:64/:79/:86 → ≤17 B | **ENERGY · EN_ENERGY · EN_MULTI · MULTI_T1/T2 · DELAY2 · TRIGGER2 · RUN_MODE** |
| `DISP_STD_DATA3` — `H : …` | `time2str`+`dgus_write_bytes` :69/:91 → ≤16 B | **DELAY3 · TRIGGER3 · RUN_MODE** |

텍스트는 VP 1개가 아니라 **cfg 여러 필드의 함수**(run_mode × energy_ctrl × multi_ctrl 분기)라 항목별 에코로는 못 맞춘다. 두 안:

| 안 | 장 | 단 |
|---|---|---|
| ① `render_run_std` 를 공개 함수로 승격해 **`if (save)` 뒤 1줄 호출** (`app_lcd_run_std_refresh()`) — **채택** | 코드 1줄 + 함수 승격(본체 이동, 신규 로직 0). 분기 11개에 흩어 놓지 않는다. RUN_MODE·EN_* 조합 재계산이 기존 렌더 그대로라 **텍스트 규칙을 두 번 쓰지 않는다** | text 와 무관한 save(OUT_POWER·ON_TIME·MULTI_O*·CAL·MODEL·WORK_CNT·CFG_CTRL)에도 ≤82 B(7.1 ms) 가 나간다 — 드문 쓰기이고 예산 안(§4.3) |
| ② 영향 분기 11곳에 각각 텍스트 조립 | 필요할 때만 송신 | 11곳 × 조립 코드 = 규칙 중복. `render_run_std` 와 표류 위험 |

**페이지 게이트(`lcd_status == LCD_RUN_STD`)는 두지 않는다** — ⑴ 데이터/텍스트 VP 는 페이지 밖에서 써도 VP RAM 에 남아 무해하고 다음 표시 때 그 값이 나온다(`change_page` 의 "VP 쓰기 → `set_page`" 순서가 그 전제, `render.c:175-215`), ⑵ 경고 페이지→런 페이지 복귀 경로 5곳이 **재렌더 없이 `dgus_set_page(state->lcd_status)`** 만 한다(`input.c:103, 126, 130, 151, 266`). 게이트를 두면 조작자가 경고 페이지(17)에 있는 동안 도착한 원격 쓰기가 복귀 뒤 stale 로 남는다. ⑶ 1줄 vs 조건문 — 게이트는 7 ms 를 아끼려고 정합성 구멍을 사는 거래다. 
호출 위치는 `app_config_save_all(cfg)` **뒤**(FRAM 이 정본, LCD 는 표시).

승격 형태(§4.1): `static inline __attribute__((always_inline)) void render_run_std(const app_config_t *cfg, uint8_t *buf)` → `void app_lcd_run_std_refresh(void)`(내부에서 `app_lcd_cfg()` + 지역 `buf[20]`), `app_lcd_change_page` 는 `render_run_std(cfg, buf)` :179 를 `app_lcd_run_std_refresh()` 로 교체하고 미사용이 되는 `uint8_t buf[20];` :171 을 지운다(`-Wall` 미사용 변수 경고 방지). `always_inline` 이 풀려 `bl` 1개가 되고 본체는 한 벌만 남는다(두 벌 복제 시 ≈+350 B 를 피함).

### 3.3 컨트롤러 판정 필요 항목 (Task 3 — 판정 후 실행)

| # | 항목 | 현 상태 | 에코 코드 | 판정 재료 | **권고** |
|---|---|---|---|---|---|
| (a) | `0x30 HORN_CMD` → `DISP_HORNDOWN`(0x1209) | 분기 :552-559 `app_horn_set_mode()` 만. LCD 는 SETUP1 진입 시 `dgus_write_u16(DISP_HORNDOWN, app_horn_mode_active())` `input.c:458` 로 체크박스를 **실제 모드로 시드**하고 shadow `temp_horndown=0` :459. 원격기 CLAUDE.md:181 이 "컨트롤러 수정 대상"으로 기록(조사 §A.1 HORN 행·§C.2 C-4) | `dgus_write_u16(DISP_HORNDOWN, app_horn_mode_active());` — `app_horn_set_mode(...)` :559 직후 | 에코는 **표시만** 바꾸고 `temp_horndown` 은 안 건드린다. SAVE 시 `app_lcd_hook_horn(temp_horndown==1)` `app_lcd_comm.c:388` 가 shadow 를 무조건 적용하는 legacy 거동(체크 안 건드리면 OFF, `input.c:455-459` 주석; 2026-09-06 벤치 §4-5 실측)은 **이미 페이지 진입 시드에도 똑같이 존재**하는 불일치 — 에코가 새로 만드는 것이 아니다 | **포함.** 진입 시드 :458 와 정확히 동형 1줄. 원격기의 기록 항목을 닫는다 |
| (b) | `0x2E/0x2F CAL_VAL/FREQ_CAL_VAL` → `VAR_CAL_VAL`(0x1410)/`VAR_FREQ_CAL_VAL`(0x1412) | 분기 :586-593 cfg 만. MODEL_SETUP(1) 진입 `enter_model_setup` `input.c:328-329` 가 `dgus_write_u16(VAR_CAL_VAL, (uint16_t)cfg->cal_val)` **raw int16 캐스트**로 쓴다. 터치 입력도 raw `(int16_t)data16` :520/:523. 펌웨어 어디에도 ÷100 없음 → 표시 스케일은 **DGUS 자산 몫** | `dgus_write_u16(VAR_CAL_VAL, (uint16_t)cfg->cal_val);` / `dgus_write_u16(VAR_FREQ_CAL_VAL, (uint16_t)cfg->freq_cal_val);` | 클램프 `cfg_cal_from_wire` (`app_cfg_stage.c:107`, ±1000) 뒤의 cfg 를 쓰므로 LCD 도 클램프 값을 본다. 페이지가 롱프레스 진입(드묾)이라 가치는 낮으나 규칙 균일 | **포함.** 2줄, 진입 에코 :328-329 와 동형 |
| (c) | `0x17/0x18 MODEL_FREQ/TYPE` → `MODEL_FREQ`(0x1060)/`MODEL_TYPE`(0x1070) 선택 VP | 분기 :560-585 는 `app_lcd_send_model_str` 만. MODEL_SETUP 진입 `input.c:326-327` 이 두 VP 를 별도로 쓴다 = 선택 표시 VP 가 **따로 있다**(터치 입력 VP 와 동일 주소 — DGUS 는 0x82 쓰기 에코를 `dispatch` 가 `cmd != RD` 로 버리므로(`input.c:480`) 피드백 루프 없음) | `dgus_write_u16(MODEL_FREQ, cfg->model_freq);` :568 뒤 / `dgus_write_u16(MODEL_TYPE, cfg->model_type);` :584 뒤 | `sys_mode`·런페이지 미재파생은 기존 결정 그대로(:561-566 주석, 벤치 MOD-4). MODEL_TYPE 쓰기의 PC11 의미 변경(:571-582)은 **이 에코와 무관** | **포함.** 2줄, 진입 에코 :326-327 와 동형 |
| (d) | staged comm 9종 | — | — | §2.2 첫 행 | **제외** |

### 3.4 이미 에코가 있어 손대지 않는 분기

`EN_ENERGY` :529 · `EN_MULTI` :534 · `EN_SAFTY` :546 · `WORK_CNTL` :603(`app_lcd_set_work_cnt` → `dgus_write_u32(LV_WORK_CNT)` `app_lcd_disp.c:265-269`). `RUN_MODE` :521-524 는 별도 VP 에코 없이 §3.2 재기록이 `DISP_RUN_MODE` + 3줄을 갱신한다.

---

## 4. 설계

### 4.1 변경점 전수

| 파일 | 변경 | 줄 수 |
|---|---|---|
| `fw/src/app_modbus.c` :453-520 | 13분기에 `dgus_write_u16/u32` 14줄(§3.1 표) — cfg 대입 다음, `save = true;` 앞 | +14 |
| `fw/src/app_modbus.c` :621-626 | `if (save) { app_config_save_all(cfg); app_lcd_run_std_refresh(); }` | +1 (+주석) |
| `fw/src/app_modbus.c` :559 / :568 / :584 / :589 / :592 | (Task 3) HORN 1 · MODEL 2 · CAL 2 | +5 |
| `fw/src/app_lcd_render.c` :41-93 | `render_run_std` → `void app_lcd_run_std_refresh(void)` (cfg 를 `app_lcd_cfg()` 로, `buf`·`n` 지역화). 헤더 주석에 "change_page + Modbus 에코 공용, `set_page` 없음" | 이동 ±0, +3 |
| `fw/src/app_lcd_render.c` :171, :179 | `buf[20]` 삭제 · `render_run_std(cfg, buf)` → `app_lcd_run_std_refresh()` | −1, ±0 |
| `fw/include/app_lcd.h` :163 부근 | `void app_lcd_run_std_refresh(void);` | +1 |

`#include` 추가 없음 — `app_modbus.c` 는 이미 `dgus_lcd.h`(:26)·`app_lcd.h`(:13)·`app_horn.h`(:18) 를 포함한다.

### 4.2 호출 위치·순서 — 분기 안 vs 모아서

- **숫자 에코는 분기 안(cfg 대입 직후, `save = true` 앞)** — 기존 3개(:529/:534/:546)와 **동형**. "모아서" 안은 `if (save)` 뒤에서 어느 필드가 바뀌었는지 알아야 하므로 VP/값 쌍을 나르는 변수(또는 분기별 플래그)가 필요 = 새 상태 + 줄 수 증가 + 기존 3개와 이형. 분기 안이면 각 분기가 자기 VP 하나만 알고 끝난다.
- **STD RUN 텍스트는 `if (save)` 뒤 1호출** — 페이지 단위 재계산이라 "모아서"가 자연스럽다(§3.2 ①). `app_config_save_all` **뒤**.
- 순서 불변식 유지: `mirror_live()`(디코드 앞) ~ `apply_writes` 사이에 cfg 쓰기를 넣지 않는다(`app_modbus.c:759-763` 경고). 에코는 apply **안**(cfg 대입 뒤)이고 cfg 를 읽기만 하므로 불변식 무관.
- `HORN_CMD` 분기는 `save` 가 없으므로 RUN 재기록이 따라오지 않는다 — 옳다(horn 은 RUN_STD 텍스트에 없다).

### 4.3 지연 계산 — hold 워치독 예산

- `dgus_write_*` → `send_frame` `fw/drivers/dgus_lcd.c:72-89` → `usart1_send_blocking` `fw/drivers/usart1.c:79-83` = **`HAL_UART_Transmit` 폴링 블로킹**, 프레임당 10 ms 타임아웃(USART1 115200 8N1 `usart1.c:42`). 수신자 유무와 무관하게 TX 는 완료되므로 정상 지연 = 바이트 × 10 bit / 115200 = **86.8 µs/B**.
- 프레임: u16 = 6+2 = **8 B(0.69 ms)** · u32 = 10 B(0.87 ms) · 텍스트 ≤ 6+11 = 17 B(1.48 ms).
- apply 1회 추가 지연:

| 경우 | 바이트 | 지연 |
|---|---|---|
| 숫자 에코 1개(u16) | 8 | 0.69 ms |
| ENERGY(u32+u16) | 18 | 1.56 ms |
| RUN_STD 재기록: u16×4 (32) + DATA1 ≤17 + DATA2 ≤17 + DATA3 ≤16 | ≤82 | ≤7.12 ms |
| **최악(ENERGY + 재기록)** | **≤100** | **≤8.68 ms** |
| HORN/CAL/MODEL 에코(save 없음 또는 1 u16) | 8 | 0.69 ms(+재기록 7.1) |

- hold 워치독(spec `2026-09-06-remote-hold-to-run-design.md` §5): 최악 적층 150(P) + 150(유실 1) + 100(지터) + 131(원격기 실측 쓰기→keep 지연, 정상 FRAM 2 ms 포함) + 48(FRAM 최악 추가) = **579 ms**. TCP 는 FC06 응답을 apply **뒤에** 보내므로(`app_modbus_tcp.c:190-196` build→apply, `:225` send) 에코 시간이 그대로 응답 지연에 얹힌다 → **579 + 8.7 = 587.7 ms < 600**. 여유 21 → **12.3 ms**. 유지 신호(START=3) 자체는 cfg 분기가 아니라 에코 0 — 영향은 "hold 중 파라미터 쓰기"에만 있고, 그것이 §5 의 산정 시나리오다. RTU 는 응답 송신 뒤 apply(`app_modbus.c:772/775`)라 응답은 안 늦고 다음 프레임 처리만 ≤8.7 ms 늦는다.
- 병리 상한: UART 가 wedge 되면 프레임당 10 ms 타임아웃 → 재기록 7프레임 = 70 ms. **새 실패 부류가 아니다** — `change_page` 가 이미 페이지당 7~15 프레임을 같은 함수로 보낸다(`render.c:175-231`). 슈퍼루프 1회 8.7 ms 정지도 기존 FRAM `save_all`(2 ms 정상/50 ms 최악, `app_modbus.c:622-624`)·`change_page` 버스트와 같은 급.

### 4.4 legacy 관계 — 의도적 이탈

- samd20 `update_holding_reg(1)` (`ref/samd20/main.c:4343-`) 의 LCD 에코는 **`:4520` DISP_ENERGY_EN · `:4530` DISP_MULTI_EN · `:4536` DISP_SAFTY 셋뿐**이고 RUN_MODE 는 저장만(`:4507-4510`). 값 레지스터 에코 부재 = legacy 거동. 이 변경은 **의도적 legacy 이탈**(사용자 결정 2026-09-11 방법 A)이며 changelog 에 그렇게 적는다.
- "LCD 에 없는 규칙을 원격에만 발명하지 않는다"(`app_modbus.c:564, 579-580` 주석의 원칙)와는 **무관** — 원격에 제약을 더하는 것이 아니라 LCD 표시를 cfg 에 맞추는 것이고, LCD 자신이 이미 하는 일(클램프 에코 `input.c:410`, 페이지 진입 렌더)을 다른 트리거에서 한 번 더 하는 것이다. 레지스터 의미·주소·클램프·저장은 전부 무변경.

### 4.5 LCD 가 같은 값을 편집 중일 때

- `lcd_app_state_t` `fw/include/app_lcd.h:87-118` 의 shadow 는 `temp_address/speed_idx/parity_idx/comm_mode/ether_*/cnt_reset/horndown` :99-104 만. **SETUP 숫자는 cfg 직접**(`input.c:527-583`, 헤더 :6-7) → 에코가 덮는 것은 VP 표시값뿐이고 펌웨어 쪽 편집 상태는 없다. 다음 터치 확정이 오면 그 값이 cfg 에 들어가고 다음 tick 의 미러가 그것을 싣는다 — "마지막에 쓴 쪽"(조사 §A.3-1) 그대로.
- DGUS 입력 위젯(키패드 팝업)이 **편집 중** VP 쓰기를 받으면 입력란을 덮는지 / 확정 후 반영하는지는 패널 동작이라 코드로 판정 불가 → **실기 확인 항목 E-12**(§5.2). 결과가 어떻든 범위 밖(§0-2)이며 기록만 한다.

### 4.6 빌드 영향

- `refactor/byte-identical` 의 `.bin` 동일 게이트(`fw/tools/bin-same.sh`)는 **이 브랜치에 적용되지 않는다** — 바이너리가 바뀌는 첫 커밋이다. `fw/.bin-baseline` 파일은 존재하지 않으며 무관.
- FLASH 추정: `dgus_write_u16(VP, cfg->x)` 1줄 ≈ `ldrh`(2~4 B) + `movw`(4 B) + `bl`(4 B) = **10~12 B**. Task 1 = 14줄 ≈ 150 B · Task 2 = 승격 프롤로그/에필로그 + 호출 2곳 ≈ 30 B · Task 3 = 5줄 ≈ 55 B → **≈ +240 B ≈ 0.18 %**(REMOTE 50.90 → ≈51.1 %). 실측은 plan 각 Task 의 `size` 출력으로 기록.
- our-code 경고 0 유지(`-Wall -Wextra -Wundef -Wshadow`, `fw/CMakeLists.txt:19`) — 주의점은 `change_page` 의 `buf` 미사용(§3.2) 하나. host 17 스위트는 건드리는 파일이 전부 HAL 글루(`app_modbus.c`·`app_lcd_render.c`)라 **무변경 PASS** 여야 한다.
- `fw/tools/funclen.py` 기준 `app_lcd_run_std_refresh` 는 53줄 → 이미 `2026-09-08` spec §6 예외표에 `render_run_std 53` 으로 올라 있는 함수의 이름 변경이다(예외 유지, 신규 위반 아님). `app_modbus_apply_writes` 는 256 → ≈276(예외 유지).

---

## 5. 검증

### 5.1 왜 HW 벤치가 게이트인가

`app_modbus.c`·`app_lcd_render.c` 는 HAL 글루라 host 스위트에 링크되지 않는다(`fw/test/Makefile` 17 스위트 = 순수 FSM/코어만). DGUS 위젯의 라이브 반응(§1 마지막 항)도 코드로 못 판정한다. → 게이트 = **HW 벤치 §5.2 전건 PASS**, 통과 후 `hw-revA_fw-stage-lcd-echo` 태그(§7).

### 5.2 HW 벤치 항목

**환경(`plans/2026-09-05-bench-results.md` §4 인용, 위반 시 벤치 무효)**: 보드 = **REMOTE 빌드**(`MODEL=remote ./fw.sh flash`), `comm_mode=ETH_STATIC 192.168.1.199`, 게이트 = PC8 극성 반전으로 열림(`V3.1.0R!`). 🔴 `nc -z` 금지(구 펌웨어 소켓 고착) · 🔴 TCP connect ≠ MCU 생존(W5500 이 SYN 자체 응답) → 생존은 FC03 · 🔴 mbpoll 동작 불가 → **`docs/superpowers/tools/mb_tcp.py`**(wire 주소 0-based, `MB().read/r1/write`, `write` 는 read-back 반환) · 침묵 >10 s 뒤 첫 프레임은 게이트가 버림 → 명령 전 읽기 1회로 프라임(`2026-09-06-hold-to-run-bench-results.md` §4-1) · TCP 소켓 1개 = 클라이언트 1개만 · **벤치 중 LCD SAVE 는 항목이 요구할 때만**(SETUP 저장이 horn down 재전송, 같은 문서 §4-5). SWD halt 금지.

| # | LCD 위치 | 조작(`mb_tcp.py`) | 기대(육안 + read-back) | 근거 |
|---|---|---|---|---|
| E-0 | 임의 | `snap = m.read(0, 51)` 저장·출력 | 51칸. 마감 시 이 값으로 복원 | 잔재 방지 |
| E-1 | SETUP_STD1(10) | `m.write(0x06, 80)` | `LV_OUT_POWER` 숫자 **즉시 80**, read-back 80 | §3.1 OUT_POWER |
| E-2 | STD1 | `m.write(0x07, 700)` | `LV_MAX_ON_TIME` 700 | ON_TIME |
| E-3 | STD1 | `m.write(0x09, 5)` | `LV_LIMIT_OUT_T` 5 | TIMEOVER |
| E-4 | STD1 | `m.write(0x08, 1234)` | 에너지 표시(`LV_ENERGY_VAL`/`EDIT`) 1234 | ENERGY u32+u16 |
| E-5 | SETUP_STD2D(12) | `m.write(0x0A, 123)` → `m.write(0x0B, 234)` → `m.write(0x0C, 345)` | D/W/H 3칸 각각 즉시 갱신 | DELAY1/2/3 |
| E-5b | STD2D | `m.write(0x0A, 600)` | read-back **500** + LCD **500**(클램프 값) | 클램프 뒤 cfg 에코 |
| E-6 | SETUP_STD2T(13) | `m.write(0x0D, 222)` → `m.write(0x0E, 333)` | W/H 갱신 | TRIGGER2/3 |
| E-7 | SETUP_STD3(15) | `m.write(0x0F, 100)`·`(0x10, 200)`·`(0x11, 60)`·`(0x12, 70)` | T1/T2/O1/O2 갱신 | MULTI_* |
| E-8 | **RUN_STD(9)** | `m.write(0x0A, 150)` | `D : 1.50` 줄 + `LV_DM_DELAY` 즉시 갱신 | §3.2 재기록 |
| E-9 | RUN_STD | `m.write(0x13, 1)` → 관찰 → `m.write(0x13, 0)` | 1: `SENSOR OFF` / `W :`(TRIGGER2) / `H :`(TRIGGER3) + 배지 TRIGGER · 0: `D :`/`W :`(DELAY2)/`H :`(DELAY3) 복귀 | RUN_MODE |
| E-10 | RUN_STD | `m.write(0x14, 1)` → `m.write(0x14, 0)` | 2행 `E : <energy>` ↔ `W : …` 전환, `DISP_ENERGY_EN` 아이콘 동반 | EN_ENERGY(기존 에코 + 재기록) |
| E-11 | STD1 | `m.write(0x30, 1)` → `m.r1(0x1D) & 0x40` → `m.write(0x30, 0)` | horn-down 체크박스 ☑ → ☐, STATUS bit6 동반. **이후 LCD SAVE 누르지 않음** | §3.3(a) |
| E-12 | STD2D, `LV_DM_DELAY` 키패드 **열어 둔 채** | `m.write(0x0A, 111)` | **기록만**: 입력란이 111 로 덮이는가 / 확정 후 어느 값인가. 범위 밖(§0-2) | §4.5 실기 확인 |
| E-13 | MODEL_SETUP(1) — `SETUP_MODEL` 2 s 롱프레스 | `m.write(0x2E, 20)` → `m.write(0x17, 2)` → (간이 벤치 = E-stop 배선 없음 확인 후) `m.write(0x18, 1)` → `m.write(0x18, 2)` | CAL 20 표시 · 주파수 선택 30 kHz + 모델명 `GDS-30…` · 타입 선택 표시 이동. ⚠ MODEL_TYPE 은 PC11 의미를 바꾼다(체크리스트 MOD-6 경고) — E-stop 배선 리그에서는 생략 | §3.3(b)(c) |
| E-14 (선택) | LCD_WARNING(17) 경유 | `m.write(0x14,1)`·`(0x09,1)` → `m.write(0x1B,1)` → ~1 s 후 STATUS bit3(OVTIME) → 경고 페이지에서 `m.write(0x0A, 175)` → LCD `KEY_ERROR_RESET` 터치 | 런 페이지 복귀(`set_page` 만, `input.c:266`) 뒤 `D : 1.75` 가 **새 값** — 페이지 게이트 없음의 근거 검증 | §3.2 게이트 미채택 |
| R-1 | 임의 | 각 쓰기 뒤 `m.read(0, 51)` | 대상 칸만 바뀌고 나머지 = E-0 snap | FC03 미러 무회귀 |
| R-2 | — | E-1 뒤 전원 재인가 → `m.r1(0x06)` | 80 (FRAM 영속) → 마감 시 77 복원 | 저장 무회귀 |
| R-3 | RUN_STD | `python3 docs/superpowers/tools/mb_hold.py 192.168.1.199 150 2` 15 s → Ctrl-C | keep 중 `US=1` 연속, 손 뗌 후 **≈600 ms** 트립(`+5xx ms US=0`). 에코가 hold 경로에 없음을 확인 | §4.3 |
| R-4 | RUN_STD | `t=time.monotonic(); m.write(0x08, 1500); print(time.monotonic()-t)` ×5 | 왕복(FC06+FC03) **< 50 ms**(에코 최악 8.7 ms 포함) | §4.3 수치 |
| R-5 | STD2D | LCD 터치로 `LV_DM_DELAY` 편집 → `m.r1(0x0A)` | 500 ms 내 미러 반영(방향 ② 무변경) | 조사 §B |
| X | — | 마감: E-0 snap 으로 쓰인 칸 전부 `m.write` 복원(`OUT_POWER 77 · ON_TIME 750 · MODEL_FREQ 3 · MODEL_TYPE 2 · CAL 16 · FREQ_CAL 40 · HORN 0 · RUN_MODE/EN_ENERGY/DELAY/TRIGGER/MULTI = snap`) → `m.read(0,51) == snap` | 잔재 0 | `2026-09-05-bench-results.md` §5 |

### 5.3 원격기 세션 통보 문구(초안 — 벤치 PASS 후 발신)

> gds_us_ctrl `hw-revA_fw-stage-lcd-echo`: **Modbus 계약 변화 없음** — 레지스터 주소·의미·클램프·저장·`CFG_CAP/FEAT_CAP` 전부 무변경. 바뀐 것은 컨트롤러 **LCD 표시**만: FC06 으로 쓴 SETUP 값(DELAY/TRIGGER/OUT_POWER/ON_TIME/ENERGY/MULTI/TIMEOVER)이 LCD 가 그 페이지를 표시 중이면 즉시 갱신되고, STD RUN 페이지의 `D/W(E)/H`·RUN_MODE 텍스트도 즉시 재기록된다. `HORN_CMD(0x30)` 도 SETUP1 체크박스에 에코된다 — 원격기 CLAUDE.md 의 "컨트롤러 LCD horn 에코 부재" 항목은 닫아도 된다. 부작용: FC06 1건당 LCD UART 송신 ≤8.7 ms 가 apply 안에 추가돼 TCP 응답이 그만큼 늦는다(hold 워치독 적층 579→587.7 ms, T=600 안). staged comm(`0x1E~0x29`) 은 LCD COMM 페이지가 shadow 기반이라 에코 대상에서 제외했다(변화 없음). 원격기 쪽 조치 필요 없음.

---

## 6. 리스크

| # | 리스크 | 완화 |
|---|---|---|
| R1 | DGUS 일부 위젯이 페이지 표시 중 VP 쓰기에 반응하지 않을 수 있다(아이콘 변수의 pages 23/27 quirk 는 **반대 방향** — 라이브 쓰기만 먹음, `render.c:218-227`) | 클램프 에코 `input.c:410/554` 가 같은 위젯군(LV_*)에서 HW 검증됨. E-1~E-13 이 전 위젯을 육안 확인 |
| R2 | 원격 쓰기 시 TRIGGER 모드 RUN_STD 의 `DISP_STD_DATA1` 이 `SENSOR OFF` 로 재기록돼, weld 글루가 올린 `SENSOR ON`(`app_lcd_weld_sensor_text` `render.c:243-260`, 레벨 엣지에만 쓰기)을 다음 엣지까지 덮는다 | `change_page` 진입도 같은 일을 한다(기존). 가동 중 원격 파라미터 쓰기는 드묾. 게이트(`us_on_status`)를 두면 §3.2 의 1줄이 조건문이 된다 — 실기에서 문제 되면 후속 |
| R3 | apply 1회 ≤8.7 ms 추가 블로킹 → hold 여유 21→12.3 ms | §4.3 수치. R-3/R-4 로 실측. 원격기 산식 131 ms 는 자기 스택 포함값이라 실제 여유는 더 크다 |
| R4 | `LV_ENERGY_EDIT` 부팅 시드가 `/10`(`app_lcd.c:240`) 인 기존 불일치가 에코(렌더 기준 raw)와 나란히 보이게 된다 | 이번 범위 밖 관찰. E-4 결과에 기록만. 정정은 별건 |
| R5 | HORN 에코 뒤 조작자가 SETUP1 에서 SAVE 하면 shadow(`temp_horndown=0`)가 horn 을 끈다 — 체크박스 ☑ 표시와 어긋남 | legacy 거동 그대로(`input.c:455-459`, 2026-09-06 벤치 §4-5). 에코가 만든 것이 아님. E-11 은 SAVE 를 누르지 않는다 |
| R6 | 바이너리 변경 첫 커밋 — `refactor/byte-identical` 의 `.bin` 승계 근거가 끝난다 | 그래서 HW 벤치가 게이트(§5.1). 태그는 PASS 후 |

---

## 7. 실행

- 브랜치 `feat/modbus-write-lcd-echo`(현 worktree, `3f82c06` 위). 커밋 5개(plan Task 1~5 각 1개 + Task 5 의 날짜 커밋 1개) — Task 1 `feat(modbus): SETUP 숫자 13분기 LCD VP 에코` → Task 2 `feat(lcd): STD RUN 텍스트 재기록 공개 함수 + apply 뒤 1호출` → Task 3 `feat(modbus): HORN/CAL/MODEL 에코` → Task 4 `docs` → Task 5 `chore(version)` + 벤치 체크리스트(문서는 Task 4 커밋에 포함, 결과 문서는 벤치 세션이 `plans/2026-09-11-modbus-write-lcd-echo-bench-results.md` 로 신설).
- 게이트: 각 코드 커밋에서 STD `./fw.sh` + REMOTE `MODEL=remote ./fw.sh` our-code **경고 0** · `./fw.sh test` **17 스위트 PASS** · `size` 출력 기록.
- HW 벤치 §5.2 전건 PASS → `main` 에 `--no-ff` 머지 → 태그 **`hw-revA_fw-stage-lcd-echo`**(이 저장소 관례: HW 검증 통과 중간 스택). 릴리즈 번호는 올리지 않는다(기능 티어 불변, 날짜만 `_260911`). 벤치 PASS 전에는 원격기 통보(§5.3)·계약 문서 갱신 금지(세션 간 규칙).
- 벤치 FAIL 시: 위젯 무반응(R1)이면 해당 항목만 되돌리고 나머지 유지(분기 독립) — 이분탐색 불필요.
