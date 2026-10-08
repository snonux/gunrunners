# Gunrunners

A jump-n-shoot platformer that plays like Duke Nukem II. Pick one of three
runners (Dash, Rocco or Nova) and blast your way to the exit.

This is a **proof of concept**: one level, three playable characters, three
enemy types, Duke II's weapons, item boxes and bonus tally, synthesized sound
and music, and three switchable visual themes that correspond to the design
directions in [docs/DESIGN.md](docs/DESIGN.md).

![Gameplay: Dash in Neon Overdrive](docs/media/gameplay_preview.gif)

The game renders at 1280x720 with smooth, anti-aliased vector art, soft
glow lighting and parallax backdrops (no chunky pixels). The full gameplay
videos (with sound) are made with `tools/record.sh`, see below.

| Select screen | Lost Temple | Station Zero |
|---|---|---|
| ![](docs/media/select_screen.png) | ![](docs/media/lost_temple.png) | ![](docs/media/station_zero.png) |

## Build and run

Developed and tested on Linux. Needs a C++17 compiler, CMake 3.16+, SDL2 and
Cairo:

```sh
sudo apt install build-essential cmake libsdl2-dev libcairo2-dev   # Debian/Ubuntu
sudo dnf install gcc-c++ cmake SDL2-devel cairo-devel              # Fedora
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/gunrunners
```

Play with the keyboard or a gamepad (both work at the same time):

| Keyboard | Gamepad | Action |
|----------|---------|--------|
| Left / Right (A / D) | left stick or d-pad | walk, pick a runner |
| Up (W) | stick or d-pad up | climb a ladder, aim up, pull your legs in on a hang bar; hold to look up |
| Down (S) | stick or d-pad down | crouch, aim down from a hang bar; hold to look down |
| Z / Space | A (bottom face button) | jump (tap for a short hop), down+jump drops from a hang bar |
| X / Ctrl | X or B, RB, right trigger | fire; with the flamethrower, down+fire is a jetpack |
| C | Y | switch to the next runner, right where you stand |
| Esc / P | Start | pause menu: resume, save, load, change runner, quit |
| Enter | A (or Start) | confirm in menus |
| Esc / Backspace | B | back out of a menu (Esc on the title screen quits) |
| T | Back / Select | cycle theme (Neon Overdrive, Lost Temple, Station Zero) |

Gamepads use SDL2's GameController API, so Xbox, PlayStation, Switch Pro,
8BitDo, Steam Deck and most generic USB or Bluetooth pads work with the
same layout (button names above are Xbox style; on PlayStation A is Cross
and X is Square). Pads can be plugged in or out while the game runs. A pad
SDL does not recognise can be added with the `SDL_GAMECONTROLLERCONFIG`
environment variable (a mapping line from a tool such as
`sdl2-jstest` or the SDL GameController DB).

Useful flags: `--theme N`, `--character N`, `--skip-menu`, `--autoplay` (the
bot plays), `--level PATH`, `--fullscreen`, `--no-audio`, `--save-dir PATH`,
`--trace` (prints the player state every logic frame), `--press LIST`
(scripted button presses for headless menu tests). Run `--help` for all of
them.

Tests: `ctest --test-dir build` runs the savegame round-trip test (play,
save, load into a fresh world, save again, compare).

## Gameplay: how close to Duke Nukem II

The player logic is a port of RigelEngine's `game_logic/player.cpp`
(`src/game/player.cpp`), so movement matches Duke II frame by frame:

- Game logic runs at Duke's 15 Hz on an 8 px cell grid; rendering
  interpolates between logic frames at 60 fps, so it still looks smooth.
- The same jump arc (with short hops when you let go early, air control and
  the occasional somersault), falling speeds, landing recovery after a long
  fall, turning on the spot, and 1-cell stair stepping.
- Ladders, hang bars (pipes) with aiming down and pulling the legs in, and
  the flamethrower jetpack.
- Duke's weapons: the blaster plus pick-ups for laser (pierces), rockets
  (splash damage) and flamethrower, with limited ammo; rapid fire as a
  timed power-up. Shot speeds, damage and muzzle positions per stance come
  from Rigel's tables.
- Health with mercy frames (blinking, then flashing white), Duke's death
  animation and respawn at the last checkpoint.
- Breakable item boxes colour-coded like Duke's (blue: health and merch,
  green: weapons, white: power-ups and keys), gems, an access card that
  opens a force field, the letters G-U-N (collect them in order for 100000),
  floating score numbers and the end-of-level bonus tally (no damage, all
  bots, every weapon, all merch, all gems).
- Enemies only wake up once they have been on screen, as in Duke II.
- Rigel's dead-zone camera with look up/down.

The runners differ in health and jump height; Rocco starts with rockets and
Nova with the laser.

## Beyond Duke Nukem II

- **Switch runners mid-level.** Press C (gamepad Y) to cycle Dash, Rocco
  and Nova on the spot, or pick one from the pause menu. Position, weapon,
  ammo and items carry over; health keeps the same share of the new
  runner's hearts.
- **Turbo Mode.** A white item box with an orange core (`T` in level files)
  maxes out every stat for 15 seconds: health refilled and no damage taken,
  double walking speed, an 11-cell jump, fire on every frame, double damage.
  Afterimages, a hot glow and a HUD timer show it; it warns before running out.
- **Virus.** A floating green germ (`V`) that makes you sick for 8 seconds:
  half walking speed, a 5-cell jump, no rapid fire, half damage. You turn
  green and bubble. Shoot it from a distance for 250 points. Turbo cures
  it, and touching a virus during Turbo burns the Turbo off instead.
- **Savegames.** Five slots, from the pause menu (save or load) and the
  LOAD GAME button on the title screen. A save keeps your runner, score,
  health, weapon, ammo, items, letters, Turbo/Virus timers and what is left
  of the level (enemies, boxes, items, checkpoints, the force field).
  Loading puts you on the last solid ground you stood on. Saves live in
  `$XDG_DATA_HOME/gunrunners/saves` (usually `~/.local/share/gunrunners/saves`),
  one small text file per slot; `--save-dir` overrides that.

## Sound and music

All sound effects and the three music tracks (menu, level, and the victory
fanfare) are synthesized at startup in `src/audio/synth.cpp` from
oscillators, noise, filters and envelopes: original material, GPL like the
rest of the game, no samples and no Duke assets. The level track is an
A-minor synthwave loop with drums, bass, pads, an arpeggio and a lead. The
mixer (`src/audio/audio.cpp`) plays through SDL2's audio device.

## Recording gameplay without a display

The game can run headless (SDL's software renderer, no display or GPU needed)
and stream raw frames, so clips can be recorded in CI or a container:

```sh
tools/record.sh 0 2 out/neon_nova   # theme 0, Nova -> out/neon_nova.mp4 (720p60, AAC audio) + .gif
```

In headless mode `--audio-out PATH` writes the game's audio to a WAV file,
mixed 800 samples per frame so it stays in sync with the video;
`record.sh` muxes the two with ffmpeg.

The bot (`src/frontend/bot.cpp`) drives the normal input path, browses the
select screen, picks the requested runner and plays the level to the exit. Runs
are deterministic, so the same command always produces the same clip.

## Relationship to RigelEngine

[RigelEngine](https://github.com/lethal-guitar/RigelEngine) is a modern
reimplementation of the Duke Nukem II engine. Gunrunners ports its game
logic where that helps, with attribution in each file (both projects are
GPL-2.0-or-later):

| RigelEngine | Gunrunners |
|---|---|
| `game_logic/player.cpp` (state machine, jump arc, ladders, pipes, jetpack, firing, death) | `src/game/player.cpp` |
| `engine/movement.cpp`, `engine/collision_checker.cpp` | `src/game/collision.cpp` |
| `game_logic/camera.cpp` | `src/engine/camera.cpp` |
| weapon, item box and bonus rules | `src/game/world.cpp` |

We did not fork the engine as a whole, for two reasons:

1. **It is built around Duke Nukem II's data files.** Levels, sprites, actor
   behaviour and the game logic itself are loaded from or keyed to the
   original `NUKEM2.CMP` content. Using it for an original game would mean
   either producing assets in Duke 2's formats or rewriting most of
   `assets/`, `data/` and `game_logic/`.
2. **Rendering.** RigelEngine is built to reproduce Duke II's 320x200
   pixel look. Gunrunners is aiming for modern HD 2D graphics instead, so
   its renderer would not carry over either.

So the engine around the ported logic is our own: the module split
(`base / data / assets / engine / game / frontend`) mirrors RigelEngine's,
but levels are text files, art is drawn in code and the renderer is HD.

Unlike Duke Nukem II (and RigelEngine), the look is not low-res pixel art:
all graphics are original vector art drawn with Cairo at startup
(`src/assets/art.cpp`) and composited with SDL2 on the GPU, with additive
glow, particles, parallax and a vignette. No Duke Nukem assets are used.

## Layout

```
src/base       math, deterministic RNG
src/render     SDL2 renderer wrapper, Cairo vector helpers, text
src/assets     vector-drawn sprites, tiles and backdrops for each theme
src/audio      synthesized sound effects and music, mixer
src/data       level loader, themes, character stats
src/engine     camera
src/game       world simulation: player (Rigel port), collision, enemies,
               shots, item boxes, effects, HUD
src/frontend   character select / level / bonus tally modes, autoplay bot
levels/        text level files (format in levels/README.md)
tools/         record.sh, mklevel.py (generates levels/level1.txt)
docs/          design directions and media
```

## Next steps

- Pick a design direction and replace the programmer art with artist-made
  HD sprites (PNG or SVG; the renderer already works with textures).
- More of Duke II's enemy roster and hazards (conveyor belts, elevators,
  destructible walls), secret areas, a boss.
- More levels and an episode map.
- A proper level editor workflow (e.g. Tiled `.tmx` import).

## License

Gunrunners is free software under the GNU General Public License, version 2
or (at your option) any later version. See [LICENSE](LICENSE).
