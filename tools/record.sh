#!/usr/bin/env bash
# Records a gameplay clip headlessly: the bot picks a dude and plays the level.
#
# Usage: tools/record.sh [theme 0-2] [character 0-2] [output basename]
# Produces <basename>.mp4 (1280x720, 60 fps) and <basename>.gif (480x270, 20 fps preview).
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
PALETTE=$(mktemp --suffix=.png)
trap 'rm -f "$PALETTE"' EXIT

"$BIN" --headless --autoplay --quit-after-clear \
  --theme "$THEME" --character "$CHARACTER" --raw-out - |
  ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgra -s 1280x720 -r 60 -i - \
    -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart \
    "$OUT.mp4"

GIF_FILTER="fps=20,scale=480:270:flags=lanczos"
ffmpeg -loglevel error -y -i "$OUT.mp4" -vf "$GIF_FILTER,palettegen=stats_mode=diff" "$PALETTE"
ffmpeg -loglevel error -y -i "$OUT.mp4" -i "$PALETTE" \
  -lavfi "$GIF_FILTER[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" \
  "$OUT.gif"
echo "wrote $OUT.mp4 and $OUT.gif"
