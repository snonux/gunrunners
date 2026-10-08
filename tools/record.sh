#!/usr/bin/env bash
# Records a gameplay clip headlessly: the bot picks a dude and plays the level.
#
# Usage: tools/record.sh [theme 0-2] [character 0-2] [output basename]
# Produces <basename>.mp4 (960x540, 60 fps) and <basename>.gif (320x180, 30 fps).
set -euo pipefail

THEME=${1:-0}
CHARACTER=${2:-2}
OUT=${3:-out/turbodudes_t${THEME}_c${CHARACTER}}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/build/turbodudes"

if [[ ! -x "$BIN" ]]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j
fi
mkdir -p "$(dirname "$OUT")"

RAW=$(mktemp)
trap 'rm -f "$RAW" "$OUT.palette.png"' EXIT

"$BIN" --headless --autoplay --quit-after-clear \
  --theme "$THEME" --character "$CHARACTER" --raw-out "$RAW"

RAWIN=(-f rawvideo -pix_fmt bgra -s 320x180 -r 60 -i "$RAW")
ffmpeg -loglevel error -y "${RAWIN[@]}" \
  -vf scale=960:540:flags=neighbor -c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p \
  "$OUT.mp4"
ffmpeg -loglevel error -y "${RAWIN[@]}" \
  -vf "fps=30,palettegen=max_colors=128:stats_mode=diff" "$OUT.palette.png"
ffmpeg -loglevel error -y "${RAWIN[@]}" -i "$OUT.palette.png" \
  -lavfi "fps=30[x];[x][1:v]paletteuse=dither=none:diff_mode=rectangle" \
  "$OUT.gif"
echo "wrote $OUT.mp4 and $OUT.gif"
