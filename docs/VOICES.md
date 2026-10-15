# Runner voices

The runners comment out loud on what they see. Every runner has their own
voice; custom runners pick one of the six in the runner editor (VOICE).

## What gets said

- **Level lines** (`[level <file>]` in `voices/lines.txt`): three in every
  campaign level (1-42, DEEP SPACE 43-49), one or two in every bonus level
  and EXTRAS level. None in the Silent Picture, which has no sound.
- **Event barks** (`[event <name>]`): Turbo, the Virus, a new weapon, G-U-N
  spelled, boarding a vehicle, back from a checkpoint after dying, down to
  the last heart.
- **Catchphrases** (`[runner <voice>]`): the SAY A LINE hotkey (V, rebindable
  in CONTROLS) picks one of the runner's own or one of the level's lines.

## Triggers

| Trigger | Fires |
|---|---|
| `start` | two seconds into the level |
| `see <enemy>` | the first time an enemy with that key has its middle on screen |
| `at <x> <y>` | the first time map block (x, y) is well inside the view |
| `reach <x> <y> <r>` | the runner comes within r blocks of (x, y) |
| `time <s>` | s seconds into the level |
| `clear` | the runner reaches the exit (said over the tally) |

Each level line says once per run. A loaded save or a new run starts over;
the level's lines wait while its bonus level plays.

## Rules

- One line at a time, at least 3 s of quiet before a level line.
- A level line waits for its turn up to 10 s, then is dropped; `clear` cuts in.
- Barks only after 8 s of quiet, about every other time, each event at most
  once every 45 s, none in the first 8 s of a level.
- The hotkey speaks at once when nobody is talking (2 s between presses).
- The music ducks to 40% under a line.
- ASSISTS AND OPTIONS: RUNNER VOICES (OFF, QUIET, NORMAL, LOUD) and VOICE
  SUBTITLES (the line written at the bottom of the screen, on by default).
- `--voice-log` prints every trigger and every line said, with the logic
  frame: the bot runs check that lines fire where they should.

## Recordings

`voices/<voice>.ogg`: all of a voice's lines one after the other, Ogg Vorbis, mono,
22.05 kHz, about 20 kbit/s (one stream per voice, so the codec headers come
once, not 250 times), with `voices/<voice>.txt` saying where each clip starts;
decoded with stb_vorbis (`src/third_party/stb_vorbis.c`, public domain). The id is the
section and the line's number in it: `01_rooftop_run_2`, `event_turbo_1`,
`runner_rocco_3`. Reordering lines renames their recordings, so add new
lines at the end of a section.

`tools/voices/generate.py` makes them (it needs torch, chatterbox-tts,
faster-whisper, soundfile and ffmpeg; none of it is part of the build):

1. A reference per runner: a Kokoro-82M voice (Dash am_puck, Rocco am_onyx
   at 0.86 speed, Nova af_nova, Jade bf_isabella, Skye af_sky, Bolt
   am_michael) read by Chatterbox at high exaggeration, so the reference
   itself is excited (`tools/voices/refs.py`).
2. Every line cloned from it with Chatterbox Turbo, transcribed with
   faster-whisper and re-rolled (up to four takes) when the words don't
   match.
3. The runner's sound (`tools/voices/fx.py`): Rocco 8% lower with chest and
   gravel (the trailer voice), Bolt ring-modulated and tinny, the rest as
   cloned; then trimmed, levelled and encoded.

The takes (high quality, one file per line) are kept as masters outside
the repository, in the project's shared folder under
`gunrunners-poc/voices/masters/`. Existing takes are kept, so after adding
lines the same commands record only the new ones and rebuild the banks:

    tools/voices/generate.py voices/lines.txt REFS_DIR MASTERS_DIR
    tools/voices/pack.py voices/lines.txt MASTERS_DIR voices
