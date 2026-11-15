#!/bin/sh
# fw.sh — gds_us_ctrl 펌웨어 빌드/플래시 단축 스크립트.
#
#   ./fw.sh            빌드 (기본)
#   ./fw.sh flash      빌드 + 플래시 (ST-LINK 필요)
#   ./fw.sh reset      보드 리셋만
#   ./fw.sh test       host 테스트 (fw/test)
#   ./fw.sh gdb        빌드 + GDB 접속
#   ./fw.sh reconfig   cmake 강제 재구성 후 빌드
#
# 보드 리비전은 HW_REV, 브랜드(업체)는 BRAND, 제품 모델(기능 티어)은 MODEL 환경변수로 고른다.
# 빌드 디렉토리 = V30 build-v30-<brand>-<model>/ · V31 build-v31-<model>/ (기본값도 항상 이름에 넣는다).
# V31 은 브랜드를 LCD 세트(VP 0x1500)가 런타임에 정한다 — BRAND 를 줘도 무시한다.
#   ./fw.sh flash                                       V30 GDSONIC CTRL   -> build-v30-gdsonic-ctrl/   (기본)
#   MODEL=remote ./fw.sh flash                          V30 GDSONIC REMOTE -> build-v30-gdsonic-remote/
#   BRAND=moohan MODEL=remote ./fw.sh flash             V30 MOOHAN  REMOTE -> build-v30-moohan-remote/
#   HW_REV=v31 MODEL=remote ./fw.sh flash               V31 REMOTE         -> build-v31-remote/
# 리비전·브랜드·모델 목록 = fw/include/define.h (오타는 #error 로 터진다).
#
# 실체는 fw/CMakeLists.txt의 커스텀 타깃(:155 flash, :162 reset, :172 gdb).
# 이 스크립트가 하는 일은 아래 두 함정을 대신 피해주는 것뿐이다.
set -eu
cd "$(dirname "$0")/fw"

# 함정 1: STM32_TOOLCHAIN이 stale 경로를 가리켜 cmake가 실패한다 (~/dev/CLAUDE.md §7).
cm() { env -u STM32_TOOLCHAIN cmake "$@"; }

# 조합별 빌드 디렉토리 — 서로의 오브젝트를 덮어쓰지 않게 분리한다.
# cmake 캐시가 HW_REV/BRAND/MODEL을 기억하므로 재구성 때마다 넘겨 캐시와 디렉토리를 일치시킨다.
lc() { printf '%s' "$1" | tr 'A-Z' 'a-z'; }
uc() { printf '%s' "$1" | tr 'a-z' 'A-Z'; }
hw_lc=$(lc "${HW_REV:-v30}");     hw_uc=$(uc "$hw_lc")
brand_lc=$(lc "${BRAND:-gdsonic}"); brand_uc=$(uc "$brand_lc")
model_lc=$(lc "${MODEL:-ctrl}");   model_uc=$(uc "$model_lc")
build=build-$hw_lc-$brand_lc-$model_lc
if [ "$hw_lc" = v31 ]; then
    [ -n "${BRAND:-}" ] && echo "[fw.sh] V31 은 BRAND 를 LCD 세트가 정한다 — BRAND=$BRAND 무시"
    brand_uc=NONE
    build=build-v31-$model_lc
fi

# 함정 2: CMakeLists.txt:123의 file(GLOB src/*.c drivers/*.c)는 configure 타임에 고정된다.
# .c가 추가/삭제된 브랜치로 전환한 뒤 증분 빌드만 하면 새 파일이 링크되지 않아
# undefined reference로 터진다 (2026-06-19 seek-reset 세션에서 실제로 당함).
# 목록을 스탬프와 비교해 달라졌을 때만 재구성한다.
sync_glob() {
    stamp=$build/.src-glob
    list=$(ls src/*.c drivers/*.c 2>/dev/null || true)
    if [ ! -f "$stamp" ] || [ "$list" != "$(cat "$stamp")" ]; then
        echo "[fw.sh] 소스 목록 변경 감지 → cmake 재구성 (HW_REV=$hw_uc BRAND=$brand_uc MODEL=$model_uc, $build/)"
        cm -B "$build" -G Ninja -DHW_REV="$hw_uc" -DMODEL="$model_uc" -DBRAND="$brand_uc" >/dev/null
        printf '%s\n' "$list" >"$stamp"
    fi
}

case "${1:-build}" in
build)    sync_glob; cm --build "$build" ;;
flash)    sync_glob; cm --build "$build" --target flash ;;
reset)    cm --build "$build" --target reset ;;
gdb)      sync_glob; cm --build "$build" --target gdb ;;
test)     make -C test ;;
reconfig) rm -f "$build/.src-glob"; sync_glob; cm --build "$build" ;;
*)        echo "사용법: $0 [build|flash|reset|gdb|test|reconfig]" >&2; exit 1 ;;
esac
