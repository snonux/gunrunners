#!/usr/bin/env bash
# Build the browser version: gunrunners.js, .wasm and .data in web/build,
# next to a copy of the website's play page (docs/play/index.html). Serve
# that directory over HTTP; file:// will not load it.
#
# Usage: web/build.sh   (needs emsdk activated: emcc and emcmake on PATH)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
[[ -f "$HERE/deps/prefix/lib/libcairo.a" ]] || "$HERE/build-deps.sh"
emcmake cmake -S "$HERE/.." -B "$HERE/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build"
version="v$(sed -nE 's/^project\(Gunrunners VERSION ([0-9.]+).*/\1/p' "$HERE/../CMakeLists.txt")"
sed "s/@VERSION@/$version/" "$HERE/../docs/play/index.html" > "$HERE/build/index.html"
echo "Built $HERE/build (try: python3 -m http.server -d $HERE/build, then open /index.html)"
