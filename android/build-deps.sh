#!/usr/bin/env bash
# Cross-compile Gunrunners' native dependencies for Android: FreeType,
# pixman, Cairo, Mbed TLS and libcurl as static libraries, one prefix per
# ABI. SDL2 is built by
# Gradle/CMake from source (android/deps/SDL2), so it is only fetched here.
#
# Usage: android/build-deps.sh [ABI...]   (default: arm64-v8a armeabi-v7a x86_64)
# Needs: ANDROID_NDK_HOME (or ANDROID_HOME with an NDK), meson, ninja, curl,
#        git, sha256sum.
# Output: android/deps/<ABI>/{include,lib}, android/deps/SDL2

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
readonly DEPS="$HERE/deps"
readonly SRC="$DEPS/src"
readonly API=24
readonly SDL_TAG=release-2.32.10
readonly SDL_COMMIT=5d249570393f7a37e037abf22cd6012a4cc56a71
readonly CAIRO_VER=1.18.4
readonly CAIRO_SHA=445ed8208a6e4823de1226a74ca319d3600e83f6369f99b14265006599c32ccb
readonly PIXMAN_VER=0.44.2
readonly PIXMAN_SHA=6349061ce1a338ab6952b92194d1b0377472244208d47ff25bef86fc71973466
readonly FREETYPE_VER=2.13.3
readonly FREETYPE_SHA=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
readonly MBEDTLS_VER=3.6.4
readonly MBEDTLS_SHA=ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110
readonly CURL_VER=8.18.0
readonly CURL_SHA=40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946
readonly CACERT_SHA=a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505

find_ndk() {
    if [[ -n "${ANDROID_NDK_HOME:-}" ]]; then echo "$ANDROID_NDK_HOME"; return; fi
    local sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    local ndk="$sdk/ndk/27.3.13750724"
    [[ -d "$ndk" ]] || ndk="$(find "$sdk/ndk" -mindepth 1 -maxdepth 1 \
        -type d -print 2>/dev/null | sort -V | tail -1)"
    [[ -n "$ndk" && -d "$ndk" ]] || { echo "No NDK: set ANDROID_NDK_HOME or ANDROID_HOME" >&2; exit 1; }
    echo "$ndk"
}

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

fetch_sources() {
    mkdir -p "$SRC"
    if [[ ! -f "$DEPS/cacert.pem" ]]; then
        curl -fsSL -o "$DEPS/cacert.pem" \
            https://curl.se/ca/cacert-2026-09-25.pem
    fi
    echo "$CACERT_SHA  $DEPS/cacert.pem" | sha256sum -c --quiet
    fetch "https://cairographics.org/releases/pixman-$PIXMAN_VER.tar.gz" "$PIXMAN_SHA" "pixman-$PIXMAN_VER"
    fetch "https://cairographics.org/releases/cairo-$CAIRO_VER.tar.xz" "$CAIRO_SHA" "cairo-$CAIRO_VER"
    fetch "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VER.tar.xz" \
        "$FREETYPE_SHA" "freetype-$FREETYPE_VER"
    fetch "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VER/mbedtls-$MBEDTLS_VER.tar.bz2" \
        "$MBEDTLS_SHA" "mbedtls-$MBEDTLS_VER"
    fetch "https://curl.se/download/curl-$CURL_VER.tar.xz" \
        "$CURL_SHA" "curl-$CURL_VER"
    if [[ ! -d "$DEPS/SDL2" ]]; then
        git clone -q --depth 1 -b "$SDL_TAG" https://github.com/libsdl-org/SDL.git "$DEPS/SDL2"
    fi
    local head
    head="$(git -C "$DEPS/SDL2" rev-parse HEAD)"
    [[ "$head" == "$SDL_COMMIT" ]] || { echo "SDL2 is at $head, expected $SDL_COMMIT" >&2; exit 1; }
}

# Meson cross file for one ABI.
write_cross_file() {
    local abi="$1" ndk="$2" out="$3" triple cpu family
    case "$abi" in
        arm64-v8a)   triple=aarch64-linux-android;     family=aarch64; cpu=aarch64 ;;
        armeabi-v7a) triple=armv7a-linux-androideabi;  family=arm;     cpu=armv7a ;;
        x86_64)      triple=x86_64-linux-android;      family=x86_64;  cpu=x86_64 ;;
        *) echo "unsupported ABI $abi" >&2; exit 2 ;;
    esac
    local bin="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
    cat > "$out" <<CROSS
[binaries]
c = '$bin/$triple$API-clang'
cpp = '$bin/$triple$API-clang++'
ar = '$bin/llvm-ar'
strip = '$bin/llvm-strip'
pkg-config = 'pkg-config'

[built-in options]
c_args = ['-fPIC', '-O2']
default_library = 'static'

[host_machine]
system = 'android'
cpu_family = '$family'
cpu = '$cpu'
endian = 'little'
CROSS
}

build_abi() {
    local abi="$1" ndk="$2"
    local prefix="$DEPS/$abi" work="$DEPS/build/$abi"
    mkdir -p "$work"
    write_cross_file "$abi" "$ndk" "$work/cross.txt"
    export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
    export PKG_CONFIG_PATH=""
    local common=(--cross-file "$work/cross.txt" --prefix "$prefix" --libdir lib --buildtype release)

    meson setup --wipe "$work/freetype" "$SRC/freetype-$FREETYPE_VER" "${common[@]}" \
        -Dzlib=disabled -Dbzip2=disabled -Dpng=disabled -Dharfbuzz=disabled -Dbrotli=disabled -Dtests=disabled
    ninja -C "$work/freetype" install

    # pixman's 32-bit ARM assembly is written for GNU as, which the NDK's
    # clang does not take; the C paths are plenty for baking art at startup.
    local pixman_arm=()
    [[ "$abi" == armeabi-v7a ]] && pixman_arm=(-Darm-simd=disabled -Dneon=disabled)
    meson setup --wipe "$work/pixman" "$SRC/pixman-$PIXMAN_VER" "${common[@]}" "${pixman_arm[@]}" \
        -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled -Dopenmp=disabled
    ninja -C "$work/pixman" install

    meson setup --wipe "$work/cairo" "$SRC/cairo-$CAIRO_VER" "${common[@]}" \
        -Dfreetype=enabled -Dfontconfig=disabled -Dpng=disabled -Dzlib=disabled \
        -Dxlib=disabled -Dxcb=disabled -Dquartz=disabled -Ddwrite=disabled -Dtee=disabled \
        -Dglib=disabled -Dspectre=disabled -Dlzo=disabled -Dsymbol-lookup=disabled -Dtests=disabled
    ninja -C "$work/cairo" install

    local toolchain="$ndk/build/cmake/android.toolchain.cmake"
    cmake -S "$SRC/mbedtls-$MBEDTLS_VER" -B "$work/mbedtls" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$API" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DENABLE_PROGRAMS=OFF \
        -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
        -DUSE_STATIC_MBEDTLS_LIBRARY=ON
    cmake --build "$work/mbedtls" --target install

    cmake -S "$SRC/curl-$CURL_VER" -B "$work/curl" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$API" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix" \
        -DBUILD_CURL_EXE=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
        -DBUILD_SHARED_LIBS=OFF -DHTTP_ONLY=ON -DCURL_USE_MBEDTLS=ON \
        -DCURL_USE_LIBPSL=OFF -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF \
        -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF -DUSE_LIBIDN2=OFF
    cmake --build "$work/curl" --target install
}

main() {
    local abis=("$@")
    (( ${#abis[@]} )) || abis=(arm64-v8a armeabi-v7a x86_64)
    local ndk
    ndk="$(find_ndk)"
    fetch_sources
    for abi in "${abis[@]}"; do
        build_abi "$abi" "$ndk"
    done
    echo "Deps ready in $DEPS"
}

main "$@"
