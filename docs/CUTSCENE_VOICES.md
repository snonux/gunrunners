# Cutscene voices

Every line in the opening, the level briefings, the episode endings, the
finale and the bonus-level stings is spoken. The runners use the same
voices as their in-game lines ([VOICES.md](VOICES.md)); everyone else has
a voice of their own:

| Speaker | Who | Voice |
|---|---|---|
| `DASH`, `ROCCO`, `NOVA` | the runners | their in-game voices (`tools/voices/refs.py`) |
| `MAX` | the client's hologram | Maxine's voice, lower, ring-modulated |
| `MAXINE` | Maxine Vale, the director | urgent and sure of herself |
| `ZERO` | Station Zero's AI | calm, polite, in a cold metal room |
| `LANCE` | Lance Marquee, the host | big game-show energy |
| `S` | the voice in the static | a weak radio signal |
| `NARRATOR` | the nature documentary | hushed wonder |
| `BOARD` | the network board | the boss's boss |
| `ANNOUNCER` | MaxTV's announcer | title cards, channel idents, the Halcyon ad |

## Cues

- `cue N say=SPEAKER "text"` shows the subtitle box and speaks the line.
- `cue N voice=SPEAKER "text"` only speaks it: the words are already on
  screen (a title card, a channel ident, an ad).

A spoken line is heard to the end: the scene holds the frame before the
next `say` or `voice`, before a `fade=out` and before the shot ends until
the line is over. Pressing a button cuts the line and moves on to the
next one (or the next shot); three quick presses or a held button skip the
cutscene. The music
ducks under every line, the typing blips stay quiet under a spoken one,
and the ASSISTS options apply: RUNNER VOICES sets the volume (OFF leaves
subtitles only, as before) and SUBTITLES OFF hides the box under a spoken
line.

## Recording

The scripts reuse `fx.py` and `generate.py` from the runner voice lines
(`tools/voices/`, [VOICES.md](VOICES.md)) and its Python setup.

A clip's id is the cutscene's name and an FNV-1a hash of `SPEAKER|text`
(`CutsceneVoices::clipId`), so editing a line's words makes it a silent
subtitle until it is recorded again; nothing plays the old words.

```sh
# once: the non-runner reference voices (the runners' come from refs.py)
tools/voices/cutscene_refs.py REFS_DIR
# every line not recorded yet (masters: one ogg per line per voice)
tools/voices/cutscene_generate.py cutscenes REFS_DIR MASTERS_DIR --part 1/2 &
tools/voices/cutscene_generate.py cutscenes REFS_DIR MASTERS_DIR --part 2/2
# the game's banks: voices/cutscenes/<voice>.ogg and .txt
tools/voices/cutscene_pack.py cutscenes MASTERS_DIR voices/cutscenes
```

`cutscene_lines.py` has a `SPOKEN` table for lines whose spelling trips
the speech model (`"Mwa-ha-ha."`, a trailing dash). The masters and refs
are kept in the project's shared folder under
`gunrunners-poc/voices/cutscene_masters/` and `cutscene_refs/`.
`ctest -R cutscene_voices` checks that every line is recorded and that a
line waits for the one before it.
