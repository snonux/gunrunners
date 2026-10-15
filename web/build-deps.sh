#!/usr/bin/env bash
# Compile Gunrunners' native dependencies to WebAssembly: FreeType, pixman
# and Cairo as static libraries (the same versions as the Android build).
# SDL2 is Emscripten's own port, fetched by emcc on first use. No libcurl:
# cloud saves are left out of the browser build.
#
# Usage: web/build-deps.sh   (needs emsdk activated: emcc on PATH, plus
#        meson, ninja, curl, sha256sum)
# Output: web/deps/prefix/{include,lib}

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
readonly DEPS="$HERE/deps"
readonly SRC="$DEPS/src"
readonly PREFIX="$DEPS/prefix"
readonly WORK="$DEPS/build"
readonly CAIRO_VER=1.18.4
readonly CAIRO_SHA=445ed8208a6e4823de1226a74ca319d3600e83f6369f99b14265006599c32ccb
readonly PIXMAN_VER=0.44.2
readonly PIXMAN_SHA=6349061ce1a338ab6952b92194d1b0377472244208d47ff25bef86fc71973466
readonly FREETYPE_VER=2.13.3
readonly FREETYPE_SHA=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289

command -v emcc >/dev/null || { echo "emcc not found: activate emsdk first (source emsdk_env.sh)" >&2; exit 1; }

# fetch URL SHA256 DIR: download a release tarball once, check it, unpack it.
fetch() {
    local url="$1" sha="$2" dir="$3" file
    [[ -d "$SRC/$dir" ]] && return
    file="$SRC/$(basename "$url")"
    curl -fsSL -o "$file" "$url"
    echo "$sha  $file" | sha256sum -c --quiet
    tar xf "$file" -C "$SRC"
    rm -f "$file"
}

mkdir -p "$SRC" "$WORK"
fetch "https://cairographics.org/releases/pixman-$PIXMAN_VER.tar.gz" "$PIXMAN_SHA" "pixman-$PIXMAN_VER"
fetch "https://cairographics.org/releases/cairo-$CAIRO_VER.tar.xz" "$CAIRO_SHA" "cairo-$CAIRO_VER"
fetch "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VER.tar.xz" \
    "$FREETYPE_SHA" "freetype-$FREETYPE_VER"

cat > "$WORK/cross.txt" <<CROSS
[binaries]
c = 'emcc'
cpp = 'em++'
ar = 'emar'
ranlib = 'emranlib'
pkg-config = 'pkg-config'

[built-in options]
c_args = ['-O2']
default_library = 'static'

[host_machine]
system = 'emscripten'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'
CROSS

export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH=""
common=(--cross-file "$WORK/cross.txt" --prefix "$PREFIX" --libdir lib --buildtype release)

meson setup --wipe "$WORK/freetype" "$SRC/freetype-$FREETYPE_VER" "${common[@]}" \
    -Dzlib=disabled -Dbzip2=disabled -Dpng=disabled -Dharfbuzz=disabled -Dbrotli=disabled -Dtests=disabled
ninja -C "$WORK/freetype" install

meson setup --wipe "$WORK/pixman" "$SRC/pixman-$PIXMAN_VER" "${common[@]}" \
    -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled -Dopenmp=disabled
ninja -C "$WORK/pixman" install

meson setup --wipe "$WORK/cairo" "$SRC/cairo-$CAIRO_VER" "${common[@]}" \
    -Dfreetype=enabled -Dfontconfig=disabled -Dpng=disabled -Dzlib=disabled \
    -Dxlib=disabled -Dxcb=disabled -Dquartz=disabled -Ddwrite=disabled -Dtee=disabled \
    -Dglib=disabled -Dspectre=disabled -Dlzo=disabled -Dsymbol-lookup=disabled -Dtests=disabled
ninja -C "$WORK/cairo" install

echo "Deps ready in $PREFIX"
