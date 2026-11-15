#!/usr/bin/env bash
# models.txt → $OUT/controller/<FW 버전>/ (Flasher Portable PLUS 루트) + $OUT/lcd/<brand>/DWIN_SET (SD 카드 루트).
#            + $OUT/jflash/<FW 버전>/ (J-Flash PC 용: 프로젝트 + <brand>.hex).
# <FW 버전> = 보관본 경로 releases/<버전>/ 의 <버전> — 전 슬롯이 같은 버전이어야 한다.
# .cfg/.dat 헤더에 J-Flash 생성 시각(분 단위)이 들어가므로 재생성할 때마다 controller/ 의 SHA256SUMS 가 바뀐다. .hex 는 같은 bin → 같은 바이트.
# 슬롯 N → FLASHER.INI 의 ConfigFile/DataFile 인덱스 N-1 (0 은 접미어 없음).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# J-Flash V7.88 고정 — 구형 Flasher PLUS(FW V6.32g)에서 PASS 한 유일한 방식(2026-10-28, 결정 2026-11-15). V9.42 + Device.pex 는 단독 PROG 실패.
JFLASH="${JFLASH:-/Applications/SEGGER/JLink_V788/JFlash.app/Contents/MacOS/JFlashExe}"
PRJ="$HERE/gds_us_ctrl_f410.jflash"
OUT="$ROOT/../production/ctrl"   # 결과물은 저장소 밖 gdsonic/production/ctrl (이력 = 그 폴더 git)
CTRL_TOP="$OUT/controller"
LCD="$OUT/lcd"
JF_TOP="$OUT/jflash"
OBJCOPY="${OBJCOPY:-arm-none-eabi-objcopy}"
FLASH_BASE=0x08000000

[ -x "$JFLASH" ] || { echo "JFlashExe 없음 — JFLASH=<경로> 로 지정" >&2; exit 1; }
command -v "$OBJCOPY" >/dev/null || { echo "$OBJCOPY 없음 — OBJCOPY=<경로> 로 지정" >&2; exit 1; }

make_flasher_pair() {   # <brand> <bin>
    local log="$CTRL/jflash.log"
    "$JFLASH" -hide -jflashlog"$log" -openprj"$PRJ" -open"$2,$FLASH_BASE" \
        -savecfg"$CTRL/$1.cfg" -savedat"$CTRL/$1.dat" -exit \
        || { echo "J-Flash 실패: $log" >&2; exit 1; }
    grep -q "Data file saved successfully" "$log" || { echo "DAT 생성 실패: $log" >&2; exit 1; }
    rm -f "$log"
}

ver=$(awk '$1 ~ /^[0-9]+$/ { split($3, p, "/"); print p[2] }' "$HERE/models.txt" | sort -u)
[ "$(printf '%s\n' "$ver" | wc -l)" -eq 1 ] && [ -n "$ver" ] || { echo "슬롯마다 FW 버전이 다르다 — controller/<버전> 폴더 이름을 정할 수 없음: $(echo $ver)" >&2; exit 1; }
CTRL="$CTRL_TOP/$ver"
JFD="$JF_TOP/$ver"
rm -rf "$CTRL_TOP" "$LCD" "$JF_TOP"; mkdir -p "$CTRL" "$LCD" "$JFD"
cp "$PRJ" "$JFD/"
ini="[FILES]"
while read -r slot brand bin lcd; do
    case "$slot" in ''|\#*) continue ;; esac
    if [ "$slot" = - ]; then   # LCD 세트만 (Flasher 슬롯 없음)
        [ "$lcd" = - ] && { echo "LCD 전용  $brand  LCD 세트 없음 (-)"; continue; }
        [ -f "$ROOT/$lcd/DWIN_SET/14ShowFile.bin" ] || { echo "LCD 세트 없음 ($brand): $lcd/DWIN_SET" >&2; exit 1; }
        mkdir -p "$LCD/$brand"
        rsync -rc --exclude .DS_Store --exclude "*.bak" "$ROOT/$lcd/DWIN_SET/" "$LCD/$brand/DWIN_SET/"
        echo "LCD 전용  $brand  LCD ← $lcd"; continue
    fi
    [[ "$slot" =~ ^[0-9]+$ ]] && [ "$slot" -ge 1 ] && [ "$slot" -le 99 ] || { echo "잘못된 슬롯: $slot" >&2; exit 1; }
    [ -f "$ROOT/$bin" ] || { echo "FW 보관본 없음 (슬롯 $slot $brand): $bin" >&2; exit 1; }
    [ "$lcd" = - ] || [ -f "$ROOT/$lcd/DWIN_SET/14ShowFile.bin" ] || { echo "LCD 세트 없음 (슬롯 $slot $brand): $lcd/DWIN_SET" >&2; exit 1; }

    make_flasher_pair "$brand" "$ROOT/$bin"
    "$OBJCOPY" -I binary -O ihex --change-addresses "$FLASH_BASE" "$ROOT/$bin" "$JFD/$brand.hex"
    idx=$((slot - 1)); sfx=""; [ "$idx" -gt 0 ] && sfx="$idx"
    ini+=$'\r\n'"DataFile$sfx = \"$brand.dat\""$'\r\n'"ConfigFile$sfx = \"$brand.cfg\""

    [ "$lcd" = - ] && { echo "슬롯 $slot  $brand  FW ← $bin  LCD 세트 없음 (-)"; continue; }
    mkdir -p "$LCD/$brand"
    rsync -rc --exclude .DS_Store --exclude "*.bak" "$ROOT/$lcd/DWIN_SET/" "$LCD/$brand/DWIN_SET/"
    echo "슬롯 $slot  $brand  FW ← $bin  LCD ← $lcd"
done < "$HERE/models.txt"
printf '%s\r\n' "$ini" > "$CTRL/FLASHER.INI"
rm -f "$CTRL"/*.pex   # PEX 삭제 — 구형 FW 가 STM32F4 Device.pex 를 못 돌린다

(cd "$CTRL" && shasum -a 256 *.cfg *.dat FLASHER.INI > SHA256SUMS)
(cd "$JFD" && shasum -a 256 *.hex *.jflash > SHA256SUMS)
(cd "$LCD" && find . -type f ! -name SHA256SUMS | sort | xargs shasum -a 256 | iconv -f UTF-8-MAC -t UTF-8 > SHA256SUMS)  # 파일명 NFC — Windows 대조용
cp "$HERE/README.md" "$OUT/"   # 생산라인용 설명 + 기준표 사본
echo "OK  $CTRL  $JFD  $LCD"
