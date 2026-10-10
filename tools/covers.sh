#!/usr/bin/env bash
# Draws the box covers (docs/media/cover_front.jpg, cover_back.jpg) from the
# game's own art, with four of the README screenshots on the back.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MEDIA="$ROOT/docs/media"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$ROOT/build" --target gunrunners_cover -j > /dev/null

shots=()
for name in level21 level43 motor_pool level49; do
  ffmpeg -loglevel error -y -i "$MEDIA/$name.jpg" "$TMP/$name.png"
  shots+=("$TMP/$name.png")
done
"$ROOT/build/gunrunners_cover" "$TMP" "${shots[@]}"
for side in front back; do
  ffmpeg -loglevel error -y -i "$TMP/cover_$side.png" -q:v 2 "$MEDIA/cover_$side.jpg"
done
echo "wrote $MEDIA/cover_front.jpg and $MEDIA/cover_back.jpg"
