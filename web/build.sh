#!/usr/bin/env bash
# Build the browser version: gunrunners.html, .js, .wasm and .data in
# web/build (serve that directory over HTTP; file:// will not load it).
#
# Usage: web/build.sh   (needs emsdk activated: emcc and emcmake on PATH)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
[[ -f "$HERE/deps/prefix/lib/libcairo.a" ]] || "$HERE/build-deps.sh"
emcmake cmake -S "$HERE/.." -B "$HERE/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build"
echo "Built $HERE/build/gunrunners.html (try: python3 -m http.server -d $HERE/build)"
