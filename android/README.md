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

## What the PoC showed

Tested in the Android emulator (Android 11, x86_64) running in software
emulation with no GPU and no hardware acceleration: the APK installs, unpacks
its 67 data files, bakes the art, starts SDL's audio thread with the
synthesized music (the emulator ran without sound output, so nothing was
heard), and shows the title screen. The on-screen gamepad drives the menus, the runner select and
the training stage (walking, picking up gems). Bluetooth gamepads use the same
SDL code as on Linux, but no pad was tested here.

The emulator manages about 8 fps, since it emulates the CPU and draws with a
software GPU. That says nothing about phone speed: the game logs its frame
rate to logcat every 10 seconds (`adb logcat -s SDL/APP`), which is the first
thing to check on a real phone. The work per frame is SDL texture blits of
pre-baked art, which any phone GPU handles; startup Cairo baking takes about
half a second on a desktop and should take a few seconds on a phone.

The largest texture is 2560x720, fine for every phone of the last decade
(OpenGL ES 2 only guarantees 2048, so very old devices could miss a backdrop).

Note for emulator testing: `adb shell input tap X Y` on this emulator in
landscape maps X through a 1920-wide space (x_app = (X - 180) * 1.1875), so
taps land to the right of where the screenshot says. The raw finger positions
SDL reports match that mapping, so the game's own conversion is not the cause.

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
