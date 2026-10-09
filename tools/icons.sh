#!/usr/bin/env bash
# Draws the app icon with the game's own art (gunrunners_cover --icon) and
# writes the Android launcher icons (adaptive layers + legacy PNGs per
# density) and the 512 px store icon.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RES="$ROOT/android/app/src/main/res"
STORE="$ROOT/fastlane/metadata/android/en-US/images"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$ROOT/build" --target gunrunners_cover -j > /dev/null
"$ROOT/build/gunrunners_cover" --icon "$TMP"

scale() { ffmpeg -loglevel error -y -i "$1" -vf "scale=$3:$3:flags=lanczos" "$2"; }

# density: adaptive layer size (108 dp) and legacy icon size (48 dp)
for d in mdpi:108:48 hdpi:162:72 xhdpi:216:96 xxhdpi:324:144 xxxhdpi:432:192; do
    IFS=: read -r name layer legacy <<< "$d"
    mkdir -p "$RES/mipmap-$name"
    scale "$TMP/icon_background.png" "$RES/mipmap-$name/ic_launcher_background.png" "$layer"
    scale "$TMP/icon_foreground.png" "$RES/mipmap-$name/ic_launcher_foreground.png" "$layer"
    scale "$TMP/icon.png" "$RES/mipmap-$name/ic_launcher.png" "$legacy"
done
mkdir -p "$STORE"
cp "$TMP/icon.png" "$STORE/icon.png"
echo "wrote the launcher icons in $RES and $STORE/icon.png"
