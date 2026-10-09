#!/usr/bin/env bash
# Build Gunrunners (and its tests) on this machine.
#
# Usage: ./build.sh [-d|--debug] [-c|--clean] [-t|--test] [-r|--run]
#   -d  Debug build instead of Release
#   -c  wipe the build directory first
#   -t  run ctest after building
#   -r  start the game after building
#
# Needs a C++17 compiler, CMake 3.16+, SDL2 and Cairo dev packages
# (Fedora: sudo dnf install gcc-c++ cmake SDL2-devel cairo-devel).

set -euo pipefail

readonly SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="$SRC_DIR/build"

build_type=Release
clean=0
run_tests=0
run_game=0

usage() {
    sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

parse_args() {
    local arg
    for arg in "$@"; do
        case "$arg" in
            -d|--debug) build_type=Debug ;;
            -c|--clean) clean=1 ;;
            -t|--test)  run_tests=1 ;;
            -r|--run)   run_game=1 ;;
            -h|--help)  usage; exit 0 ;;
            *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
        esac
    done
}

# Fail early with a readable message instead of a CMake stack of errors.
check_dependencies() {
    local missing=()
    local tool
    for tool in cmake pkg-config g++; do
        command -v "$tool" >/dev/null || missing+=("$tool")
    done
    pkg-config --exists sdl2 || missing+=("SDL2-devel")
    pkg-config --exists cairo || missing+=("cairo-devel")

    if (( ${#missing[@]} )); then
        echo "Missing: ${missing[*]}" >&2
        echo "Fedora: sudo dnf install gcc-c++ cmake pkgconf-pkg-config SDL2-devel cairo-devel" >&2
        exit 1
    fi
}

# No -G here on purpose: an existing build directory keeps the generator it
# was first configured with (CMake errors out if we force a different one).
configure_and_build() {
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$build_type"
    cmake --build "$BUILD_DIR" -j "$(nproc)"
}

main() {
    parse_args "$@"
    check_dependencies

    if (( clean )); then
        rm -rf "$BUILD_DIR"
    fi

    configure_and_build

    if (( run_tests )); then
        ctest --test-dir "$BUILD_DIR" --output-on-failure
    fi

    echo "Built: $BUILD_DIR/gunrunners ($build_type)"

    if (( run_game )); then
        exec "$BUILD_DIR/gunrunners"
    fi
}

main "$@"
