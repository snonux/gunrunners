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
  across, its centre following a thumb that goes far out so turning round
  is quick), and on the right Fire in the corner, Jump left of it, Switch
  runner above and Pause at the top. Drawn
  faintly so the level shows through.
- **Made to hit without looking:** the controls hug the physical screen's
  edges, so on phones wider than 16:9 they sit in the black bars beside the
  picture, under the thumbs. A touch up to 1.2 radii outside a button
  presses the button whose rim is nearest; Pause needs a closer touch. A
  thumb keeps its button however far it drifts and switches only when it
  slides right onto another one.
- **Swipes, on the right half in a level:** a quick swipe up (80 units in
  under 220 ms) jumps and holds jump until the finger lifts, so a thumb on
  Fire can jump without letting go; a quick swipe down switches runner.
  Slower moves are not swipes.
- **Quick save and load:** QUICK SAVE and QUICK LOAD in the pause menu
  use their own slot (`quick.sav`), apart from the five save slots. A
  keyboard or gamepad can trigger them directly (F5 and F9 by default,
  rebindable in CONTROLS).
- **In menus, cutscenes and tallies:** a d-pad, OK (confirm, skip) and
  BACK. BACK never fires or pauses, so it can't quit the title by accident.
- **CONTROLS** on the title screen and in the pause menu has TOUCH PAD:
  SMALL / MEDIUM / LARGE and EDIT TOUCH LAYOUT. The layout editor shows the
  play controls over the dimmed screen: drag the stick, Jump, Fire, Switch
  and Pause anywhere (they stay on screen), RESET puts them back, DONE
  saves. Both are saved in the profile (`touch.layout` holds the offsets),
  and RESET TO DEFAULTS in CONTROLS clears them. The same menu rebinds
  keys and gamepad buttons.
- The overlay hides when a key or gamepad is used and comes back with the
  next touch. A tap shorter than a frame still counts.

## Gamepads

Bluetooth and USB pads go through SDL's GameController API, as on Linux
(`src/frontend/controls.cpp`): plug in or pair one at any time, even
mid-level, and it works with the same layout (X jump, A/B/RB/RT fire, Y
switch runner, Start pause), which CONTROLS can rebind. Pressing a button or pushing the stick hides
the touch overlay. Android reports a pad's Back/Select as the system back
key, so on Android it pauses or backs out of a menu instead of cycling the
theme. `tests/gamepad_test.cpp` plugs a virtual pad in and out to check
all of this (on Linux, SDL 2.24 or newer).

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

Releases go out through [snonux/fdroid](https://github.com/snonux/fdroid).
Nobody pushes tags by hand any more: running `.github/workflows/release.yml`
on main with a tag such as `v1.2.3` creates that tag at main (it must match
the version in `CMakeLists.txt`), builds the APK with the release key and
attaches `gunrunners-vX.Y.Z.apk` to the GitHub release, and the Linux
tarball `gunrunners-vX.Y.Z-linux-x86_64.tar.gz` next to it. The F-Droid repo
picks the APK up from there with the store text in
`fastlane/metadata/android/`. Running it for a tag that already exists
rebuilds that release; `linux_only` rebuilds only the tarball, so an APK
F-Droid already serves stays the same. A pushed tag still works too.

- **Version:** `project(Gunrunners VERSION x.y.z)` in `CMakeLists.txt`.
  The APK's versionCode is `x * 10000 + y * 100 + z`, so it grows with
  every release.
- **Changelog:** `fastlane/metadata/android/en-US/changelogs/default.txt`,
  at most 500 characters, rewritten before each release.
- **Icon:** `tools/icons.sh` draws it with the game's own art.

### One-time setup (done)

The release key is the app's identity on every phone; never lose or
replace it. It lives in `~/.config/gunrunners/release.jks`, with
`android/key.properties` (gitignored) pointing at it, and both are kept in
foostore under `keys/f-droid/gunrunners-release.jks.txt` as attachments.
The workflows read it from the `ANDROID_*` repository secrets.

On a new machine, restore the key from foostore; to set the secrets again:

```fish
set e keys/f-droid/gunrunners-release.jks.txt
mkdir -p ~/.config/gunrunners
foostore read $e/gunrunners-release.jks > ~/.config/gunrunners/release.jks
foostore read $e/key.properties > android/key.properties
chmod 600 ~/.config/gunrunners/release.jks android/key.properties

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
git commit -am "Release vX.Y.Z"; and git push
gh workflow run release.yml -f tag=vX.Y.Z; and sleep 5; and gh run watch
```

Or in the browser: Actions, Release, *Run workflow* on main, enter the tag.
A Claude session does the same through the GitHub API after pushing the
version bump.
