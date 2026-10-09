# Gunrunners on Android (proof of concept)

The normal game, unchanged, built for Android with SDL2's Android
backend. This is a proof of concept on its own branch, not a finished port:
see "What a full port still needs" below.

## Build

Needs the Android SDK (`ANDROID_HOME`) with platform 35, build-tools 35 and
NDK 27.3.13750724, a JDK 17+, and `meson` + `ninja` for the first run.

```sh
android/build.sh            # -> android/app/build/outputs/apk/release/app-release.apk
adb install -r android/app/build/outputs/apk/release/app-release.apk
```

The first run of `build.sh` calls `build-deps.sh`, which fetches SDL2,
Cairo and pixman into `android/deps/` and cross-compiles Cairo and pixman
for arm64-v8a (phones) and x86_64 (emulators). The release APK is signed
with the debug key so it installs directly; it is about 10 MB.

## How it fits together

| Piece | Desktop | Android |
|---|---|---|
| Entry point | `main()` in `src/main.cpp` | the same `main()`, built as `libmain.so`; SDL's Java activity (`org.libsdl.app.SDLActivity`, from the SDL2 source tree) calls it |
| Build | top-level `CMakeLists.txt` | the same file; `if(ANDROID)` pulls in `android/android.cmake` (SDL2 from source, prebuilt static Cairo) |
| Rendering | SDL2 renderer, OpenGL | SDL2 renderer, OpenGL ES 2; the 1280x720 logical size is letterboxed to the screen |
| Art | baked with Cairo at startup | the same; Cairo is built without fontconfig/FreeType, so text uses Cairo's built-in font (see below) |
| Audio | synthesized, SDL audio | the same code; SDL plays through AAudio/OpenSL ES |
| Levels, cutscenes | read from the source tree | packed into the APK as assets with a manifest, unpacked to internal storage on start (`src/frontend/android_data.cpp`) |
| Saves, profile | `$XDG_DATA_HOME/gunrunners/saves` | the app's internal storage (`/data/data/io.github.snonux.gunrunners/files/saves`) |
| Input | keyboard, gamepads | on-screen gamepad (`src/frontend/touch_controls.cpp`), Bluetooth/USB gamepads through the same SDL GameController code, the back key pauses / backs out |

The on-screen gamepad also works on the desktop with `--touch` (for a
Linux touch screen, or to try the layout).

## What a full port still needs

- **Font.** Without fontconfig, Cairo's toy font API falls back to its
  built-in stroke font. It is readable and looks fine, but it is not DejaVu
  Sans. To match the desktop: build Cairo with FreeType, ship
  `DejaVuSans-Bold.ttf` in the APK and create the font face with
  `cairo_ft_font_face_create_for_ft_face` in `Renderer::text` (and the two
  other `cairo_select_font_face` calls).
- **Touch controls tuning.** The layout is fixed and the d-pad sits where
  the camera keeps the runner (lower left). Wants: placement and size per
  screen, a floating stick, transparency in levels, an options screen, and
  menus you can tap directly.
- **App lifecycle.** SDL already pauses the loop in the background; the
  game should also open its pause menu and save the profile on
  `SDL_APP_WILLENTERBACKGROUND`, since Android may kill a background app.
- **Data loading.** Unpacking to internal storage keeps the game's file
  code unchanged. Cleaner: read levels and cutscenes through `SDL_RWops`.
- **Store release.** A real signing key, an adaptive launcher icon, a
  version scheme, an App Bundle (`bundleRelease`), and testing on a range
  of phones (GPU drivers, notches and aspect ratios, 60 vs 90/120 Hz
  displays).
- **CI.** A job that runs `android/build.sh` so the port does not rot as
  the desktop game changes.
