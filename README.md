# Gunrunners

<p align="center">
  <img src="docs/media/cover_front.jpg" width="420" alt="Gunrunners box art, front">
  <img src="docs/media/cover_back.jpg" width="420" alt="Gunrunners box art, back">
</p>

A jump-n-shoot platformer that plays like Duke Nukem II. Pick one of three
runners (Dash, Rocco or Nova) and blast your way to the exit.

A 42-level campaign in six episodes: every level has its own twist, its
own prototype weapon, its own soundtrack, a bonus level behind a flickering
TV, secrets and a briefing cutscene. **Episode 1 (Levels 1-7) is complete**,
with its bonus levels and ending; Episode 2 is being built now.

| Rooftop Run: neon signs that are only solid on the beat | Club Laserdisc: bounce pads, bouncers and the bass drop |
|---|---|
| ![Rooftop Run](docs/media/rooftop_run.gif) | ![Club Laserdisc](docs/media/club_laserdisc.gif) |
| **Maglev Express: a fight along a moving train** | **Chopper Down: the Black Halo gunship** |
| ![Maglev Express](docs/media/maglev_express.gif) | ![Chopper Down](docs/media/chopper_down.gif) |

The game renders at 1280x720 with smooth, anti-aliased vector art, soft
glow lighting and parallax backdrops (no chunky pixels). Full gameplay
videos with sound are made with `tools/record.sh`, see below.

## Episode 1: Neon Overdrive

| # | Level | Twist | Prototype | Bonus level |
|---|---|---|---|---|
| 1 | Rooftop Run | neon signs that are solid only on the beat | Pulse Pistol | Cloud Nine (air jumps) |
| 2 | Glass Canyon | pulley gondolas: weigh one down to lift the other | Spark Disc | Free Fall |
| 3 | Club Laserdisc | subwoofer pads that launch you on the bass drop | Bass Cannon | Step on the Beat (you move only on the beat) |
| 4 | Blackout | a city in the dark: relight it sector by sector | Flare Gun | Echo Room (sonar) |
| 5 | Sludge Line | a rising and falling tide of sludge, gators in it | Bubble Gun | Duck Rapids |
| 6 | Maglev Express | a running train: get inside a car before each tunnel | Arc Caster | Light Trail |
| 7 | Chopper Down | a boss fight against a gunship and its searchlight | Lock-On Rockets | Pilot Seat (you fly) |

| | |
|---|---|
| ![Level 1, Rooftop Run](docs/media/level01.jpg) | ![Level 2, Glass Canyon](docs/media/level02.jpg) |
| ![Level 3, Club Laserdisc](docs/media/level03.jpg) | ![Level 4, Blackout](docs/media/level04.jpg) |
| ![Level 5, Sludge Line](docs/media/level05.jpg) | ![Level 6, Maglev Express](docs/media/level06.jpg) |
| ![Level 7, Chopper Down](docs/media/level07.jpg) | ![The level tally](docs/media/tally.jpg) |
| ![The title screen](docs/media/title.jpg) | ![A briefing cutscene](docs/media/cutscene.jpg) |

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

Or use `./build.sh` (`-t` also runs the tests, `-r` starts the game, `-d`
makes a debug build, `-c` starts from a clean build directory).

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
| F11 / Alt+Enter | | fullscreen on or off |

Fullscreen is borderless at your desktop's resolution: no window frame,
no mode switch, and the 1280x720 picture is scaled up with black bars if
the screen is not 16:9. Toggle it with F11, Alt+Enter, or the FULLSCREEN
item on the title screen and in the pause menu. The game remembers the
choice for the next start; `--fullscreen` or `--windowed` overrides it for
one run.

Gamepads use SDL2's GameController API, so Xbox, PlayStation, Switch Pro,
8BitDo, Steam Deck and most generic USB or Bluetooth pads work with the
same layout (button names above are Xbox style; on PlayStation A is Cross
and X is Square). Pads can be plugged in or out while the game runs. A pad
SDL does not recognise can be added with the `SDL_GAMECONTROLLERCONFIG`
environment variable (a mapping line from a tool such as
`sdl2-jstest` or the SDL GameController DB).

Useful flags: `--theme N`, `--character N`, `--skip-menu`, `--autoplay` (the
bot plays), `--start N` (campaign level N), `--no-cutscenes`,
`--cutscene NAME` (play one cutscene), `--level PATH` (just this level
file), `--fullscreen` / `--windowed`, `--no-audio`, `--cheats`, `--save-dir PATH`,
`--trace` (prints the player state every logic frame), `--press LIST`
(scripted button presses for headless menu tests). Run `--help` for all of
them.

Tests: `ctest --test-dir build` runs the savegame round-trip test (play,
save, load into a fresh world, save again, compare) on the training stage
and on every campaign level.

## The campaign

The title menu has NEW GAME, CONTINUE, LOAD GAME, LEVEL SELECT, ARSENAL,
BONUS CHANNEL, RERUNS and the TRAINING STAGE. A new game plays the opening
movie, then each level's briefing, the level, the tally and, after an
episode's last level, its ending. Cutscenes follow RigelEngine's movie model
(shots of animated clips at a frame delay, with cues for sound, subtitles,
freeze frames and fades); the scripts are data in `cutscenes/*.txt` and any
button skips them. The levels that are not built yet end the run with a
"to be continued" screen. The TRAINING STAGE is a free-play level for
trying the controls.

- **Prototypes and the Arsenal.** Each level hides its own prototype weapon
  in green `W` boxes. Reach the exit with it and it is logged to the
  Arsenal (title menu), with your best kill count.
- **Bonus levels.** A patch of TV static (`B`) is the way in: press up.
  "WE'LL BE RIGHT BACK!", a short level with crazy rules and a timer, and
  you are back where you were with everything you had. Beating it earns a
  Bonus Star (10000 on the tally) and a slot in the Bonus Channel.
- **Secrets.** A rubber duck in every level, candid cameras, hidden gem
  caches (`$`) and the number 42 somewhere.
- **Profile.** What you have unlocked is kept in `profile.txt` next to the
  savegames. Campaign saves remember their level, so LOAD GAME works from
  the title screen.

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

## Cheats (a little secret)

Pause the game and enter **up, up, down, down, left, right, left, right**
(arrow keys, WASD, the d-pad or the stick all work). A chime plays and the
pause menu gains a **CHEATS** entry:

| Cheat | What it does |
|---|---|
| GOD MODE | toggle: nothing hurts you (falling off the map still does) |
| FULL HEALTH | refills your hearts |
| PROTOTYPE + FULL AMMO | the level's prototype weapon, fully loaded |
| TURBO MODE | starts Turbo Mode on the spot |
| CURE THE VIRUS | ends an infection |
| ACCESS CARD | the card that switches off force fields |
| RAPID FIRE | the rapid-fire power-up |
| SKIP THE LEVEL | beams you out through the exit |

The code is remembered until you quit; `--cheats` shows the entry from the
start. Cheating has a price, as it should: a level you cheated in pays no
tally bonuses, and its score, time, Bonus Star and Arsenal entry are not
recorded in your profile. Savegames remember that you cheated.

## Sound and music

All sound effects and music are synthesized from oscillators, noise,
filters and envelopes: original material, GPL like the rest of the game, no
samples and no Duke assets.

Every level has its own soundtrack (`src/audio/music.cpp`). Each track is an
arrangement in a style of its own, with its own tempo, metre, key, chord
progression, drum groove, bass line and instruments, and a lead tune
composed from a motif: synthwave on the rooftops, a house track in the club,
a heartbeat score in the blackout whose layers come back as the power does,
dub in the sewers, a marimba and congas in the jungle, a slide guitar in the
desert, a waltz on the accordion, a noir trumpet in the rain, big band,
arena rock and more, up to a medley for the finale. Bonus levels, cutscenes
and the lift's lounge cover in Chopper Down get their own tracks too. Tracks render
in the background while a briefing plays.

The instruments are synthesized too: FM bells and electric piano, a
Karplus-Strong plucked string, marimba, kalimba, piano, organ, brass,
strings, choir, theremin, accordion and more, with a drum kit of kicks,
snares, toms, taiko, wood blocks, brushes and shakers.

Sound effects live in `src/audio/synth.cpp`, the cutscenes' named sounds and
character voice blips in `src/audio/synth_named.cpp`. The mixer
(`src/audio/audio.cpp`) plays through SDL2's audio device.

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
select screen, picks the requested runner and plays the level to the exit.
On campaign levels it plans with `src/frontend/planner.cpp`: it simulates
copies of the world with short input macros, guided by a distance field to
the next goal (the prototype, the access card, the bonus entrance, the
exit), so it copes with beat-timed platforms and other twists. Runs are
deterministic, so the same command always produces the same clip. Without
`--level`, `record.sh` records the campaign from the title screen.

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
src/frontend   title menu, campaign flow, cutscene player and clips,
               level / bonus tally modes, menus, autoplay bot and planner
levels/        text level files (format in levels/README.md)
cutscenes/     cutscene scripts
tools/         record.sh, mklevel.py (levels/level1.txt), levels/*.py
               (one generator per campaign level), covers.sh and cover/
               (the box art, drawn with the game's own runners)
docs/          design directions and media
```

## Next steps

- Episodes 2-6 (Levels 8-42), episode by episode, each with its twists,
  prototypes, bosses, bonus levels and cutscenes.
- Artist-made HD sprites to replace the programmer art (PNG or SVG; the
  renderer already works with textures).
- A level editor workflow (e.g. Tiled `.tmx` import).

## License

Gunrunners is free software under the GNU General Public License, version 2
or (at your option) any later version. See [LICENSE](LICENSE).
