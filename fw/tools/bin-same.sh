#!/bin/sh
# 두 모델을 빌드해 .bin sha256 을 기준과 비교. 사용: bin-same.sh baseline | bin-same.sh
set -eu; cd "$(dirname "$0")/../.."
./fw.sh >/dev/null
MODEL=remote ./fw.sh >/dev/null
cur=$(shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin | cut -d' ' -f1 | paste -sd' ' -)
base=fw/.bin-baseline
[ "${1:-}" = baseline ] && { printf '%s\n' "$cur" >"$base"; echo "baseline: $cur"; exit 0; }
[ "$cur" = "$(cat "$base")" ] && echo "SAME  $cur" || { echo "DIFF  base=$(cat "$base")  cur=$cur"; exit 1; }
