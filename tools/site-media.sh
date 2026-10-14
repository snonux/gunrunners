#!/usr/bin/env bash
# Makes the media of the web page (docs/index.html, served on
# https://gunrunners.snonux.foo): the six runner portraits, short gameplay
# clips with sound, and the screenshots the page shows that docs/media does
# not have. Everything is drawn or played by the game itself, headless and
# deterministic, and the PNGs carry no timestamps, so a rerun gives the same
# files.
#
# Usage: tools/site-media.sh [portraits|clips|shots|trailers|all]   (default: all)
# Writes docs/site/runners/*.png, docs/site/icon.png, docs/site/video/*.mp4
# (+ .jpg posters), docs/site/shots/levelNN.jpg and a poster for each
# trailer in docs/trailers (docs/site/video/trailer_*.jpg). Needs cmake, ffmpeg and ImageMagick (magick).
# Like record.sh and covers.sh it (re)configures build/ as a Release build.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/build/gunrunners"
SITE="$ROOT/docs/site"
WHAT=${1:-all}

case $WHAT in
  portraits | clips | shots | trailers | all) ;;
  *) echo "usage: $0 [portraits|clips|shots|trailers|all]" >&2; exit 2 ;;
esac
for tool in cmake ffmpeg magick; do
  command -v "$tool" > /dev/null || { echo "$0: needs $tool" >&2; exit 1; }
done

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

build() {
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
  cmake --build "$ROOT/build" --target gunrunners gunrunners_cover -j"$(nproc)" > /dev/null
}

# The runners in their idle pose, trimmed to the art plus a small margin,
# and Rocco's head and shoulders on a square as the page's icon. No date or
# time chunks, so an unchanged portrait stays byte for byte the same.
portraits() {
  mkdir -p "$SITE/runners"
  "$ROOT/build/gunrunners_cover" --portraits "$TMP"
  for png in "$TMP"/runner_*.png; do
    magick "$png" -trim +repage -bordercolor none -border 8 \
      -define png:exclude-chunks=date,time "$SITE/runners/$(basename "$png")"
  done
  magick "$SITE/runners/runner_rocco.png" -gravity north -crop 100%x62%+0+0 +repage \
    -background none -gravity center -extent "%[fx:max(w,h)]x%[fx:max(w,h)]" -resize 64x64 \
    -define png:exclude-chunks=date,time "$SITE/icon.png"
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

# One frame of the bot's run for each level the page shows that docs/media
# has no picture of, TICK frames in. The frames are picked by hand: before
# (or after) the bot's trip to the bonus level, and for Toon Town
# Throwdown and The Transmitter in the middle of the boss fight.
SHOT_LEVELS="8 22 23 26 27 28 31 34 35 38 39 42"
shot_tick() {
  case $1 in
    27) echo 400 ;;
    26) echo 700 ;;
    8 | 34) echo 1000 ;;
    28) echo 3600 ;; # Buddy Boom: "PIE TIME!"
    35) echo 6400 ;; # the Halcyon Titan, overheating
    *) echo 1500 ;;
  esac
}

shots() {
  mkdir -p "$SITE/shots"
  local n tick
  for n in $SHOT_LEVELS; do
    tick=$(shot_tick "$n")
    "$BIN" --headless --autoplay --skip-menu --no-cutscenes --start "$n" --character $((n % 6)) \
      --no-audio --frames $((tick + 1)) --screenshots "$tick" --screenshot-prefix "$TMP/shot${n}_" > /dev/null
    magick "$TMP/shot${n}_$tick.png" -quality 82 "$SITE/shots/level$(printf %02d "$n").jpg"
  done
}

# A poster for each trailer in docs/trailers, from 12 seconds in: past the
# opening title, into the gameplay.
trailers() {
  local mp4 name
  mkdir -p "$SITE/video"
  for mp4 in "$ROOT"/docs/trailers/*.mp4; do
    name=$(basename "$mp4" .mp4)
    ffmpeg -loglevel error -y -ss 12 -i "$mp4" -frames:v 1 -q:v 4 "$SITE/video/trailer_${name#gunrunners_trailer_}.jpg"
  done
}

build
case $WHAT in
  portraits) portraits ;;
  clips) clips ;;
  shots) shots ;;
  trailers) trailers ;;
  all) portraits; clips; shots; trailers ;;
esac
echo "wrote the page media to $SITE"
