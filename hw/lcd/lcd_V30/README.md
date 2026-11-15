# DGUS LCD 자산 정본 (lcd_V30)

> **문서 요약**: 업체별 DWIN DGUS 패널 자산의 정본. 업체 폴더마다 패널에 올리는 `DWIN_SET/` 완본과 그 소스(`32_*` 배경 → `32.icl`, `48*` 아이콘 → `48.icl`, `TFT/` 툴 프로젝트)를 둔다. 펌웨어는 하나이며, 업체 간 차이는 배경·아이콘·좌표뿐이고 **페이지 ID·VP·키 값(계약)은 공유**한다(`fw/include/dgus_lcd.h`). Git LFS 로 관리(`.gitattributes`). 2026-10-04 `hw/lcd/dgus/` 를 대체.

## 업체 ↔ 펌웨어 BRAND

| 폴더 | BRAND (`BRAND=<소문자> ./fw.sh`, 목록 `fw/include/define.h`) | 계열 | 상태 |
|---|---|---|---|
| `gdsonic/` | `GDSONIC` | 검은색 | 정본 기준 세트 |
| `dongin/` | `DIAMT` | 검은색 | 작업 중 (bin = gdsonic, 32.icl·48.icl 다름 — 빨간 레벨 바) |
| `moohan/` | `MOOHAN` | 밝은색 | p3·p9 `LV_TIME` 바(0x1184~0x1197) 없음 — 의도 |
| — (작업 원본 `powertech/` 만) | `POWERTECH` | 검은색 ← gdsonic | 미반입 — 로고·레벨 바 그림 대기 |
| — (작업 원본 `maketech/` 만) | `MAKETECH` | 검은색 ← gdsonic | 미반입 — 2026-11-15 build(로고·빨간 바), 실기 대기 |
| — (작업 원본 `sama/`·`sonosys/` 만) | (펌웨어에 없음) | 밝은색 ← moohan | 미반입 — 2026-11-15 build(로고), 실기 대기, 모델명 규칙 미정 |

업체 = BRAND 1:1. 업체가 늘면 폴더 하나 + 이 표 한 줄 + `../tools/brands.json` 한 줄.

**V31 런타임 브랜드**: V31 펌웨어는 BRAND 빌드 없이 `22_Config.bin` 의 **VP `0x1500`**(파일 오프셋 = VP×2 = `0x2A00`)에 든 브랜드 번호를 부팅·패널 재기동 때 읽는다 — 1 GDSONIC · 2 MOOHAN · 3 DIAMT · 4 POWERTECH · 5 MAKETECH (0·미지·읽기 실패 → GDSONIC, 6 SAMA · 7 SONOSYS 는 예약). 2바이트 상위 바이트 먼저(`00 0N`) — 벤치 B-1 확정(2026-11-15). ⚠ 이 칸 때문에 업체 세트의 `22_Config.bin` 은 gdsonic 과 2바이트 다르다(같음 검사의 예외). 정본 `fw/include/app_brand.h`, 설계 `docs/superpowers/specs/2026-11-15-lcd-brand-id-design.md`.

**파생 규칙 (사용자 2026-11-15)**: 같은 색 계열은 기준 세트에서 파생한다. 검은색 = gdsonic + 로고(`32.icl` #0)·레벨 바(`48.icl` #11), 밝은색 = moohan + 로고. 파생 세트는 `../tools/lcdset.py build <업체>` 로 만들고 `check`·`report` 로 확인한다(`../tools/README.md`).

## 규칙

- **`DWIN_SET/` 은 평평하게 유지, 파일 개명 금지** — DWIN 은 파일명/번호로 로드한다. 패널 갱신 = 이 폴더를 그대로 SD(FAT32) 의 `DWIN_SET` 으로 복사하거나 DGUS 툴로 다운로드.
- 편집은 DGUS 툴(`TFT/` 프로젝트)에서 하고, 작업 원본(`~/dev/work/gdsonic/gds_lcd_V30/<업체>/`) 폴더 전체를 여기에 동기화한다(`rsync -rc --delete --exclude .DS_Store --exclude Thumbs.db`). 반영 전 p21/23/25/27 모드 아이콘 = 0x140c, 통신 버튼 = 0x140b 인지 확인.
  - 도구: `../tools/lcdset.py check <업체>` (계약 검사) · `sync-preview <업체>` (아래 미리보기 + 함정 경고를 자동으로 함).
  - **반드시 미리보기 먼저**: `rsync -rcn --delete --exclude .DS_Store --exclude Thumbs.db --itemize-changes <작업원본>/<업체>/ <여기>/<업체>/ | grep -vE '^\.(d|f\.\.T)'` — 시각만 다른 줄을 빼고 남는 줄(내용 변경 `>fc`·신규 `>f+++`·삭제 `*deleting`)을 하나씩 확인한 뒤 실행.
  - ⚠ **한글 파일명 함정**: `DWIN_SET/23_나눔스퀘어.bin` 은 작업 원본이 macOS NFD, 저장소(gdsonic·dongin)가 NFC 로 이름 바이트가 달라 rsync 가 다른 파일로 본다 → 미리보기에 `*deleting …23_나눔스퀘어.bin` 이 뜨고, 그대로 실행하면 **정본에서 폰트가 빠질 수 있다**. 이 Mac 의 `/usr/bin/rsync` 는 openrsync 라 `--iconv` 가 없다 — 내용이 같으면(`cmp`) `--exclude '23_*.bin'` 을 붙여 건너뛴다.
  - ⚠ **역행 함정**: 작업 원본이 저장소보다 오래된 파일이 있으면 동기화가 되돌린다(예: 2026-11-15 이전 dongin 작업 원본의 `14ShowFile.bin` = p27 0x140b 구판, `48.icl` = 06-22 판). 미리보기의 `>fc` 줄은 양쪽 날짜·SHA 를 보고, 저장소 쪽이 의도된 수정본이면 작업 원본을 먼저 저장소 판으로 맞춘다. 2026-11-15 dongin 두 파일은 맞춤 완료.
- 펌웨어가 쓰는 VP 는 `fw/include/dgus_lcd.h` 가 정본. 세트를 바꾸면 그 VP 바인딩이 유지되는지 확인.

## 파일 역할 (`DWIN_SET/`)

| 파일 | 역할 |
|---|---|
| `14ShowFile.bin` | 페이지별 표시 위젯 ↔ VP 바인딩. 헤더 `DGUS_2`, 페이지 테이블 @0x10+4·page (count LE16, offset BE16), 32 B 레코드 `5A type FFFF len VP(BE) X Y …` |
| `13TouchFile.bin` | 터치 영역 → VP/키 값. 레코드 32 B (sync_return 0x08 은 48 B) |
| `22_Config.bin` | VP 초기값 |
| `32.icl` / `48.icl` | 배경 / 아이콘 라이브러리 (JPEG 묶음) |
| `NN.jpg` | 페이지 NN 배경 |
| `0_*.HZK`, `1_verdana.bin`, `23_나눔스퀘어.bin` | 폰트 |
| `*.cfg`, `*.bak` | T5L 하드웨어 설정 (800×480, C050WTC) |

페이지 ID ↔ 펌웨어: 0 LOGO · 1 MODEL_SETUP · 3 RUN_MULTI/HAND · 5 SETUP_MULTI · 7 SETUP_HAND · 9 RUN_STD · 10 SETUP_STD1 · 12/13 SETUP_STD2D/2T · 15 SETUP_STD3 · 17 WARNING · 19 SETUP_MH2 · 21 SETUP_MHC · 23 SETUP_STDC · 25 SETUP_MHE · 27 SETUP_STDE.

## 변경 이력

- **2026-11-15 — 3세트 `22_Config.bin` VP 0x1500 브랜드 번호 기록** (V31 런타임 브랜드, ctrl `7495b5d`): gdsonic `00 01` · moohan `00 02` · dongin `00 03`. 각 파일 오프셋 0x2A01 한 바이트만 바뀌었다(0x2A00 = 00). 작업 원본과 같은 파일이다. `lcdset check` 업체 번호 PASS. 실기: ctrl 벤치 B-1/B-2(gdsonic 세트 + `00 02` → MH-)로 형식 확인. moohan·dongin 세트 자체의 실기는 대기.
- **2026-10-18 — gdsonic `48.icl` #56(시리얼 ON) 수정 + dongin 아이콘·배경 갱신.** gdsonic `48.icl` 의 56번 칸에 57번(시리얼 OFF)과 같은 그림이 들어 있어 ON/OFF 가 구분되지 않았다 — 56번 항목만 `48_icon_red/48.icl` 의 정상 항목(원본 `56_serial_on.jpg` 와 픽셀 동일, red/green 원본 동일)으로 교체하고 오프셋 표·크기(@0x08)·CRC(@0x06, 0x08~끝 Modbus CRC16 LE)를 재계산했다. 나머지 58개 항목은 바이트 동일. 작업 원본(`DWIN_SET/48.icl`·`48_icon_green/48.icl`)도 같은 파일로 교체. 같은 날 gdsonic 작업 원본의 `DWIN_SET/120.lib`(10-13 신규) + `TFT/` 6개(p01·03·05·23·25·27) 반영 — gdsonic 정본 = 작업 원본 일치. dongin: `32.icl` = 작업 원본 10-18 판, `48.icl` = `48_icon_red/48.icl`(빨간 레벨 바 #11, 정상 시리얼 ON #56), `32_bg_dongin/00_logo.jpg` 추가. dongin `14ShowFile.bin` 은 작업 원본(p27 0x140b 구판)을 쓰지 않고 저장소 수정본(= gdsonic) 유지.
- **2026-10-04 — 3세트 `14ShowFile.bin` p27 모드 아이콘 VP 0x140b → 0x140c** (gdsonic/dongin @0x5747, moohan @0x51e7, 1바이트). 0x140b 는 터치 return 값(0 시리얼/1 이더넷/2 전환)이라 아이콘이 "마지막 누른 버튼"을 보여 재부팅 후 이더넷이어도 시리얼로 보였다(2026-09-13 증상). 펌웨어는 `DISP_COMM_MODE` 0x140c 에 쓴다. 구 `hw/lcd/dgus/` 의 2026-05-31 수정과 바이트 동일 — 패치 후 `gdsonic/DWIN_SET/14ShowFile.bin` == 구 저장소본. (구 README 는 이 수정을 "show-config auto-load" 로 적었으나 실제 차이는 VP 1바이트였다.)
- **2026-10-13 b — moohan 재동기화**(22:40 내보내기): 13Touch·14Show + TFT 4개(p19/21/23/25 프로젝트) 갱신. 아이콘 0x140c·통신 버튼 0x140b·계약(LV_TIME 바·0x1100 키 없음 외 gdsonic 동일) 유지 확인.
- **2026-10-13 — moohan 원본 폴더 전체 동기화**(작업 원본 10-13 22:17 내보내기). 툴에서 p27 아이콘 0x140c 반영 — gdsonic 툴 출력도 이 폴더와 바이트 동일해져 10-04 bin 패치가 툴 프로젝트에도 들어감. moohan 은 13Touch·14Show·22Config·48.icl 갱신 + 디자인 원본 `gds_lcd_v25_mh1.pn` 추가. 계약: gdsonic 대비 LV_TIME 바·p3/p9 0x1100 키 없음(의도) 외 동일.
- 이전 이력: 구 `hw/lcd/dgus/` (git 히스토리, 커밋 `0eafe68`).
