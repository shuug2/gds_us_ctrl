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
# 실체는 fw/CMakeLists.txt의 커스텀 타깃(:123 flash, :130 reset, :140 gdb).
# 이 스크립트가 하는 일은 아래 두 함정을 대신 피해주는 것뿐이다.
set -eu
cd "$(dirname "$0")/fw"

# 함정 1: STM32_TOOLCHAIN이 stale 경로를 가리켜 cmake가 실패한다 (~/dev/CLAUDE.md §7).
cm() { env -u STM32_TOOLCHAIN cmake "$@"; }

# 함정 2: CMakeLists.txt:91의 file(GLOB src/*.c drivers/*.c)는 configure 타임에 고정된다.
# .c가 추가/삭제된 브랜치로 전환한 뒤 증분 빌드만 하면 새 파일이 링크되지 않아
# undefined reference로 터진다 (2026-06-19 seek-reset 세션에서 실제로 당함).
# 목록을 스탬프와 비교해 달라졌을 때만 재구성한다.
sync_glob() {
    stamp=build/.src-glob
    list=$(ls src/*.c drivers/*.c 2>/dev/null || true)
    if [ ! -f "$stamp" ] || [ "$list" != "$(cat "$stamp")" ]; then
        echo "[fw.sh] 소스 목록 변경 감지 → cmake 재구성"
        cm -B build -G Ninja >/dev/null
        printf '%s\n' "$list" >"$stamp"
    fi
}

case "${1:-build}" in
build)    sync_glob; cm --build build ;;
flash)    sync_glob; cm --build build --target flash ;;
reset)    cm --build build --target reset ;;
gdb)      sync_glob; cm --build build --target gdb ;;
test)     make -C test ;;
reconfig) rm -f build/.src-glob; sync_glob; cm --build build ;;
*)        echo "사용법: $0 [build|flash|reset|gdb|test|reconfig]" >&2; exit 1 ;;
esac
