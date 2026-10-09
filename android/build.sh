#!/usr/bin/env bash
# Build the Gunrunners Android APK (proof of concept).
#
# Usage: android/build.sh [debug|release]   (default: release, debug-signed)
# Needs: ANDROID_HOME with platform 35, build-tools and NDK 27.3.13750724,
# JDK 17+, meson and ninja (for android/build-deps.sh on the first run).
# Output: android/app/build/outputs/apk/<type>/app-<type>.apk

set -euo pipefail

readonly HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly TYPE="${1:-release}"

: "${ANDROID_HOME:?set ANDROID_HOME to your Android SDK}"

if [[ ! -f "$HERE/deps/arm64-v8a/lib/libcairo.a" || ! -f "$HERE/deps/x86_64/lib/libcairo.a" ]]; then
    "$HERE/build-deps.sh"
fi

cd "$HERE"
echo "sdk.dir=$ANDROID_HOME" > local.properties
./gradlew --no-daemon "assemble${TYPE^}"
ls -l "app/build/outputs/apk/$TYPE/"*.apk
