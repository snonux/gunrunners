# Gunrunners on Android

The same game as on Linux, built for Android with SDL2's Android backend:
one APK for arm64-v8a (phones), armeabi-v7a (older 32-bit phones) and
x86_64 (emulators, Chromebooks), Android 7.0 (API 24) and newer. It is
published through [snonux/fdroid](https://github.com/snonux/fdroid).

## Build

Needs the Android SDK (`ANDROID_HOME`) with platform 35, build-tools 35 and
NDK 27.3.13750724, a JDK 17+, and `meson` + `ninja`.

```sh
android/build.sh            # -> android/app/build/outputs/apk/release/app-release.apk
adb install -r android/app/build/outputs/apk/release/app-release.apk
```

The first run calls `build-deps.sh`, which fetches SDL2 (a pinned commit),
FreeType, pixman and Cairo (checksummed release tarballs) into
`android/deps/` and cross-compiles FreeType, pixman and Cairo for the three
ABIs. Without `android/key.properties` the release APK is signed with the
debug key; see "Releasing".

## How it fits together

| Piece | Linux | Android |
|---|---|---|
| Entry point | `main()` in `src/main.cpp` | the same `main()`, built as `libmain.so`; SDL's Java activity (`org.libsdl.app.SDLActivity`, from the SDL2 source tree) calls it |
| Build | top-level `CMakeLists.txt` | the same file; `if(ANDROID)` pulls in `android/android.cmake` (SDL2 from source, the static libraries of `build-deps.sh`) |
| Rendering | SDL2 renderer, OpenGL | SDL2 renderer, OpenGL ES 2; the 1280x720 picture is letterboxed to the screen |
| Art | baked with Cairo at startup | the same |
| Font | DejaVu Sans Bold through fontconfig | the same font, shipped in `fonts/` and loaded through FreeType (`loadGameFont` in `src/render/vector.cpp`) |
| Audio | synthesized, SDL audio | the same code; SDL plays through AAudio/OpenSL ES |
| Levels, cutscenes | read from the source tree | packed into the APK as assets with a manifest, unpacked to internal storage on start (`src/frontend/android_data.cpp`) |
| Saves, profile | `$XDG_DATA_HOME/gunrunners/saves` | the app's private storage (`/data/data/org.buetow.gunrunners/files/saves`) |
| Input | keyboard, gamepads | on-screen gamepad, Bluetooth/USB gamepads (the same SDL GameController code), the back key |
| Leaving the app | | the pause menu opens over a running level and the profile is saved (`Game::suspend`), since Android may close a background app |

## Touch controls

`src/frontend/touch_controls.cpp`, tested by `tests/touch_test.cpp`.

- **In a level:** a floating stick on the left half of the screen (it
  centres wherever the thumb lands and keeps steering if the thumb slides
  across), and Jump, Fire, Switch runner and Pause on the right. Drawn
  faintly so the level shows through.
- **In menus, cutscenes and tallies:** a d-pad, OK (confirm, skip) and
  BACK. BACK never fires or pauses, so it can't quit the title by accident.
- **TOUCH PAD: SMALL / MEDIUM / LARGE** on the title screen and in the pause
  menu, saved in the profile.
- The overlay hides when a key or gamepad is used and comes back with the
  next touch. A tap shorter than a frame still counts.

The same overlay runs on Linux with `--touch` (for a touch screen, or to
try the layout).

## Testing

- `ctest --test-dir build` on Linux runs `touch_test` with the savegame
  tests.
- In the emulator: `adb install -r ...apk`, then `adb logcat -s SDL/APP`
  shows the data unpacking and the frame rate every 10 seconds.
- `adb shell input tap X Y` on the landscape emulator maps X through a
  1920-wide space (x_app = (X - 180) * 1.1875), so taps land right of where
  a screenshot says; real touches are not affected.

## Releasing

Releases go out through [snonux/fdroid](https://github.com/snonux/fdroid):
a `vX.Y.Z` tag runs `.github/workflows/release.yml`, which builds the APK
with the release key and attaches `gunrunners-vX.Y.Z.apk` to the GitHub
release; the F-Droid repo picks it up from there with the store text in
`fastlane/metadata/android/`.

- **Version:** `project(Gunrunners VERSION x.y.z)` in `CMakeLists.txt`.
  The APK's versionCode is `x * 10000 + y * 100 + z`, so it grows with
  every release.
- **Changelog:** `fastlane/metadata/android/en-US/changelogs/default.txt`,
  at most 500 characters, rewritten before each tag.
- **Icon:** `tools/icons.sh` draws it with the game's own art.

### One-time setup (Paul, fish)

The release key is the app's identity on every phone; keep it backed up.

```fish
mkdir -p ~/.config/gunrunners
set pw (openssl rand -hex 16)
keytool -genkeypair -noprompt -keystore ~/.config/gunrunners/release.jks -storetype PKCS12 \
  -alias gunrunners -keyalg RSA -keysize 4096 -validity 36500 \
  -dname "CN=Gunrunners" -storepass $pw -keypass $pw
printf 'storeFile=%s\nstorePassword=%s\nkeyAlias=gunrunners\nkeyPassword=%s\n' \
  ~/.config/gunrunners/release.jks $pw $pw > android/key.properties
chmod 600 ~/.config/gunrunners/release.jks android/key.properties
cp ~/.config/gunrunners/release.jks android/key.properties ~/.foostore-export/

mkdir -p .github/workflows; and git mv ci/workflows/release.yml ci/workflows/build.yml .github/workflows/
git commit -m "Enable the release and build workflows"; and git push

function get; sed -n "s/^$argv[1]=//p" android/key.properties; end
base64 -w0 (get storeFile) | gh secret set ANDROID_KEYSTORE
gh secret set ANDROID_KEY_ALIAS --body (get keyAlias)
gh secret set ANDROID_KEYSTORE_PASSWORD --body (get storePassword)
gh secret set ANDROID_KEY_PASSWORD --body (get keyPassword)
```

Optionally `gh secret set FDROID_DISPATCH_TOKEN` (a fine-grained token with
*Contents: read and write* on snonux/fdroid), so a release shows up in
F-Droid at once instead of within six hours.

### Each release (fish)

```fish
# bump project(Gunrunners VERSION ...) in CMakeLists.txt, rewrite the changelog
git commit -am "Release vX.Y.Z"; and git tag vX.Y.Z; and git push; and git push --tags
gh run watch
```
