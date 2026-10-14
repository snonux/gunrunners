#!/usr/bin/env bash
# Makes the media of the web page (docs/index.html, served on
# https://gunrunners.snonux.foo): the six runner portraits, short gameplay
# clips with sound, and a screenshot of every level that has none in
# docs/media yet. Everything is drawn or played by the game itself, headless
# and deterministic, so a rerun gives the same files.
#
# Usage: tools/site-media.sh [portraits|clips|shots|all]   (default: all)
# Writes docs/site/runners/*.png, docs/site/video/*.mp4 (+ .jpg posters) and
# docs/site/shots/levelNN.jpg. Needs ffmpeg and ImageMagick (magick).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/build/gunrunners"
SITE="$ROOT/docs/site"
WHAT=${1:-all}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

build() {
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
  cmake --build "$ROOT/build" --target gunrunners gunrunners_cover -j"$(nproc)" > /dev/null
}

# The runners in their idle pose, trimmed to the art plus a small margin.
portraits() {
  mkdir -p "$SITE/runners"
  "$ROOT/build/gunrunners_cover" --portraits "$TMP"
  for png in "$TMP"/runner_*.png; do
    magick "$png" -trim +repage -bordercolor none -border 8 "$SITE/runners/$(basename "$png")"
  done
}

# clip NAME LEVEL CHARACTER FROM SECONDS: the bot plays a campaign level
# straight from its start; keep SECONDS of it from FROM seconds in, as 720p
# H.264 with AAC sound, plus a poster frame from its middle. The raw frames
# are piped (a few GB a clip), the sound comes as a WAV that is mixed in
# lockstep with the frames, so cutting both at FROM keeps them in sync. The
# game plays exactly FROM + SECONDS worth of frames: an ffmpeg that stopped
# reading early (-t) would kill it with SIGPIPE.
clip() {
  local name=$1 level=$2 character=$3 from=$4 secs=$5
  local frames=$(((from + secs) * 60))
  "$BIN" --headless --autoplay --skip-menu --no-cutscenes --start "$level" --character "$character" \
    --frames "$frames" --audio-out "$TMP/$name.wav" --raw-out - |
    ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgra -s 1280x720 -r 60 -i - -ss "$from" \
      -c:v libx264 -preset slow -crf 27 -pix_fmt yuv420p "$TMP/$name.mp4"
  ffmpeg -loglevel error -y -i "$TMP/$name.mp4" -ss "$from" -t "$secs" -i "$TMP/$name.wav" \
    -map 0:v -map 1:a -c:v copy -c:a aac -b:a 96k -shortest -movflags +faststart "$SITE/video/$name.mp4"
  ffmpeg -loglevel error -y -ss "$((secs / 2))" -i "$SITE/video/$name.mp4" -frames:v 1 -q:v 4 \
    "$SITE/video/$name.jpg"
}

clips() {
  mkdir -p "$SITE/video"
  clip rooftop_run 1 0 3 16
  clip maglev_express 6 2 3 16
  clip idol_mines 11 1 3 16
  clip gravity_lab 19 3 3 16
  clip kaiju_tonight 26 4 3 16
  clip starfall 43 5 3 16
}

# One frame of the bot's run for Canopy Road and each level of Episodes 4
# to 6 (docs/media has the others), TICK frames in. The bot heads for the
# bonus level early in some levels, so those take their frame before it
# gets there (or after it is back).
shot_tick() {
  case $1 in
    24) echo 200 ;;
    27 | 28 | 41) echo 400 ;;
    26) echo 700 ;;
    8 | 30 | 34) echo 1000 ;;
    *) echo 1500 ;;
  esac
}

shots() {
  mkdir -p "$SITE/shots"
  local n tick
  for n in 8 $(seq 22 42); do
    tick=$(shot_tick "$n")
    "$BIN" --headless --autoplay --skip-menu --no-cutscenes --start "$n" --character $((n % 6)) \
      --no-audio --frames $((tick + 1)) --screenshots "$tick" --screenshot-prefix "$TMP/shot${n}_" > /dev/null
    magick "$TMP/shot${n}_$tick.png" -quality 82 "$SITE/shots/level$(printf %02d "$n").jpg"
  done
}

build
case $WHAT in
  portraits) portraits ;;
  clips) clips ;;
  shots) shots ;;
  all) portraits; clips; shots ;;
  *) echo "usage: $0 [portraits|clips|shots|all]" >&2; exit 1 ;;
esac
echo "wrote the page media to $SITE"
