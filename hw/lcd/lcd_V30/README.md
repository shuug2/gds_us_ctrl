# DGUS LCD 자산 정본 (lcd_V30)

> **문서 요약**: 업체별 DWIN DGUS 패널 자산의 정본. 업체 폴더마다 패널에 올리는 `DWIN_SET/` 완본과 그 소스(`32_*` 배경 → `32.icl`, `48*` 아이콘 → `48.icl`, `TFT/` 툴 프로젝트)를 둔다. 펌웨어는 하나이며, 업체 간 차이는 배경·아이콘·좌표뿐이고 **페이지 ID·VP·키 값(계약)은 공유**한다(`fw/include/dgus_lcd.h`). Git LFS 로 관리(`.gitattributes`). 2026-10-04 `hw/lcd/dgus/` 를 대체.

## 업체 ↔ 펌웨어 BRAND

| 폴더 | BRAND (`fw/include/define.h`) | 계열 | 상태 |
|---|---|---|---|
| `gdsonic/` | `GDSONIC` | 검은색 | 정본 기준 세트 |
| `dongin/` | `DIAMT` | 검은색 | 작업 중 (bin·48.icl = gdsonic, 32.icl 만 다름) |
| `moohan/` | `MOOHAN` | 밝은색 | p3·p9 `LV_TIME` 바(0x1184~0x1197) 없음 — 의도 |
| — | `POWERTECH`, `MAKETECH` | 검은색 | 미반입 |
| — | `SAMA`, `SONOSYS` (신규) | 밝은색 | 미반입, 모델명 규칙 미정 |

업체 = BRAND 1:1. 업체가 늘면 폴더 하나 + 이 표 한 줄.

## 규칙

- **`DWIN_SET/` 은 평평하게 유지, 파일 개명 금지** — DWIN 은 파일명/번호로 로드한다. 패널 갱신 = 이 폴더를 그대로 SD(FAT32) 의 `DWIN_SET` 으로 복사하거나 DGUS 툴로 다운로드.
- 편집은 DGUS 툴(`TFT/` 프로젝트)에서 하고, 내보낸 `DWIN_SET/` 을 여기에 덮어쓴다. ⚠ 아래 "변경 이력"의 bin 직접 패치는 툴 프로젝트에 아직 반영되지 않았다 — 툴에서 다시 내보내기 전에 같은 수정을 툴에 먼저 할 것.
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

- **2026-10-04 — 3세트 `14ShowFile.bin` p27 모드 아이콘 VP 0x140b → 0x140c** (gdsonic/dongin @0x5747, moohan @0x51e7, 1바이트). 0x140b 는 터치 return 값(0 시리얼/1 이더넷/2 전환)이라 아이콘이 "마지막 누른 버튼"을 보여 재부팅 후 이더넷이어도 시리얼로 보였다(2026-09-13 증상). 펌웨어는 `DISP_COMM_MODE` 0x140c 에 쓴다. 구 `hw/lcd/dgus/` 의 2026-05-31 수정과 바이트 동일 — 패치 후 `gdsonic/DWIN_SET/14ShowFile.bin` == 구 저장소본. (구 README 는 이 수정을 "show-config auto-load" 로 적었으나 실제 차이는 VP 1바이트였다.)
- 이전 이력: 구 `hw/lcd/dgus/` (git 히스토리, 커밋 `0eafe68`).
