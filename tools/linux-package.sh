#!/usr/bin/env bash
# Package Gunrunners for Linux: a Release build plus its levels, cutscenes
# and font in one tarball that runs from wherever it is unpacked.
#
# Usage: tools/linux-package.sh [OUTDIR]   (default: out/)
# Output: OUTDIR/gunrunners-vX.Y.Z-linux-<arch>.tar.gz
#
# The game links SDL2 and Cairo from the system (packages libsdl2-2.0-0 and
# libcairo2 on Debian/Ubuntu, SDL2 and cairo on Fedora); the C++ runtime is
# linked in statically so the binary also runs on older distributions.

set -euo pipefail

readonly SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly OUT_DIR="$(mkdir -p "${1:-$SRC_DIR/out}" && cd "${1:-$SRC_DIR/out}" && pwd)"
readonly BUILD_DIR="$SRC_DIR/build-package"

version="$(sed -n 's/^project(Gunrunners VERSION \([0-9.]*\).*/\1/p' "$SRC_DIR/CMakeLists.txt")"
readonly NAME="gunrunners-v$version-linux-$(uname -m)"
readonly STAGE="$BUILD_DIR/$NAME"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
cmake --build "$BUILD_DIR" --target gunrunners -j"$(nproc)"

rm -rf "$STAGE"
mkdir -p "$STAGE"
install -m 755 -s "$BUILD_DIR/gunrunners" "$STAGE/gunrunners"
cp -r "$SRC_DIR/levels" "$SRC_DIR/cutscenes" "$SRC_DIR/fonts" "$STAGE/"
cp "$SRC_DIR/LICENSE" "$SRC_DIR/README.md" "$STAGE/"

tar -C "$BUILD_DIR" -czf "$OUT_DIR/$NAME.tar.gz" "$NAME"
echo "$OUT_DIR/$NAME.tar.gz"
