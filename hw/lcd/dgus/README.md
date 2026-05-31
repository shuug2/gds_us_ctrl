# DGUS LCD Project (DWIN T5L1) — GD-SONIC panel

Panel-side **source of truth** for the page layouts and VP (variable-pointer) ↔ on-screen-widget mapping
that the STM32 firmware drives over USART1. This is the exported DWIN **DWIN_SET** project folder.

- **Panel**: DWIN T5L1, **800×480**, model **C050WTC** (5.0" capacitive). HW config: `T5LCFG_C1_80480C050WTC-DGUS_cfg.cfg`.
- **Firmware side**: `fw/include/dgus_lcd.h` (VP + page-ID macros), `fw/drivers/dgus_lcd.c` (wire layer),
  `fw/src/app_lcd.c` (Stage B `init_lcd_mode` port).

## ⚠️ Do not restructure

DWIN loads files from the **flat** DWIN_SET folder by **exact filename / number**. Do **not** move files into
subfolders or rename them — it breaks re-flashing the panel. Keep the flat layout; this README is the only
addition. Re-flash via the DWIN DGUS tool or SD-card (`DWIN_SET` on a FAT32 card).

## File roles

| File(s) | Role |
|---|---|
| `NN.jpg` (`0.jpg`…`29.jpg`) | Page **background** loaded by page ID `NN` (page ID = file number) |
| `NN_name.jpg` (`09_std_run.jpg`, …) | Designer-named source images for the same pages (human reference) |
| `14ShowFile.bin` | **Display instruction config** — per-page VP→widget bindings (the VP map) |
| `13TouchFile.bin` | Touch / key config — touch areas → VP / key codes |
| `22_Config.bin` | DGUS config |
| `0_verdana_ASC.HZK`, `1_verdana.bin`, `23_나눔스퀘어.bin` | Fonts (ASCII verdana + Korean NanumSquare) |
| `32.icl`, `48.icl` | Icon libraries |
| `*.cfg` | T5L hardware config (resolution / panel) |
| `*.bak` | DWIN config backups (tiny) |

## Page ID ↔ firmware macro (`fw/include/dgus_lcd.h`)

Confirmed: the `NN.jpg` numbering matches the `LCD_*` page-ID macros exactly.

| Page ID | jpg | `dgus_lcd.h` macro |
|--------:|-----|--------------------|
| 0  | `00_logo` | `LCD_LOGO` |
| 1  | `01/02_setupmodel` | `LCD_MODEL_SETUP` |
| 3  | `03_nulti` / `04_multi` | `LCD_RUN_MULTI` = `LCD_RUN_HAND` (3) |
| 5  | `05/06_multi_setup` | `LCD_SETUP_MULTI` |
| 7  | `07_hand_setup1` / `08_hand_setup2` | `LCD_SETUP_HAND` |
| 9  | `09_std_run` | `LCD_RUN_STD` |
| 10 | `10/11_std_setup1` | `LCD_SETUP_STD1` |
| 12 | `12_std_setup2d` | `LCD_SETUP_STD2D` |
| 13 | `13_std_setup2t` | `LCD_SETUP_STD2T` |
| 15 | `15/16_std_setup3` | `LCD_SETUP_STD3` |
| 17 | `17_error` | `LCD_WARNING` |
| 19 | `19/20_multi_hand_setup2` | `LCD_SETUP_MH2` |
| 21 | `21/22_multi_hand_setup3` | `LCD_SETUP_MHC` |
| 23 | `23_standard_setup3` / `23_multi_hand_setup3` | `LCD_SETUP_STDC` |
| 25 | `25/26_multi_hand_setup3e` | `LCD_SETUP_MHE` |
| 27 | `27/28_multi_hand_setup3e` | `LCD_SETUP_STDE` |

(`_bg` suffix = background-only variant of the same page.)

## Notes for firmware work

- **Stage B observation explained**: `LV_DM_DELAY/WELD/HOLD` (`0x1203/4/5`) and `LV_TM_WELD/HOLD` are
  displayed on the **SETUP** pages, **not** the RUN_HAND (3) run page. `init_lcd_mode` pre-fills them so they
  are correct the moment the operator navigates to a setup page — matching samd20. This is why a freshly
  booted RUN_HAND screen shows the model string + icons but not the delay/weld/hold numbers.
- **Scope-b / SETUP porting**: to learn which VPs a given page renders (and widget types / formats), inspect
  `14ShowFile.bin` with the DWIN DGUS tool, or open the page in DGUS and read its variable list. That is the
  authoritative map for `change_lcd_page()`'s per-page VP sets.
- VP addresses the firmware writes are listed in `fw/include/dgus_lcd.h`; they must match the bindings in
  `14ShowFile.bin`.

## Asset changes

- **2026-05-31 — `14ShowFile.bin`**: fixed the **comm-mode icon display on pages 27 (STDE) / 23 (STDC)**.
  Root cause of the STD comm display defect was here, not in firmware: the page-27/23 widgets did **not**
  auto-load `DISP_COMM_MODE` (0x140c) on page-show, while page 25 (MHE) did — so a freshly-shown STD comm
  page rendered a stale serial/ethernet/dhcp icon. Confirmed by repoint cross-test (repointing STD comm
  entry to page 25 displayed correctly). Fixed by editing the page 27/23 widget show-config so the icon
  auto-loads on page-show, matching page 25. HW-verified PASS. Firmware-side defense (re-assert after
  `set_page`, "Fix C") is kept as redundant-but-harmless. See
  `docs/superpowers/analysis/2026-05-31-std-comm-page27-display-port-faithful.md`.
- This folder holds the **compiled/exported** DWIN output (`.bin`/`.icl`). Edit the page widgets in the
  **DWIN DGUS tool** (open the project, edit the page's variable/widget config), then re-export
  `14ShowFile.bin` and re-flash the panel. Keep the editable DWIN project source archived alongside the
  panel toolchain (not in this repo).
