#!/usr/bin/env python3
"""Level 41 - Sweeps Week (SPEC.md 41). Twist: Audience Vote.
Prototype: Applause Cannon.

One stage floor (ground 40) read left to right: the rehearsal (start,
BIG HEADS), four voted segments (the warm-up stage, the stands, the
crowd walkway, the turntable stage) and the stage door (exit). At each
`@ vote` checkpoint (c1-c4) the studio audience votes on the rule up to
the next one: a friendly, a neutral and a nasty option weighted by the
style score of the segment just played; the vote at c5 picks the exit
fanfare. Everything is in world_sweeps.cpp.

Changes from the spec, so it plays in this engine:
- `@ vote` names its checkpoint by position (`@ vote ID x y`), not
  `at=cID`; `virus=X,Y` is where that segment's V comes in.
- The chicken run's balcony is one row lower (row 35, the Comics at row
  34) so the Comics are on screen from the stage floor.
- The Cue Card Drones fly at rows 35-37 (the spec's rows 31-33 are above
  the top of the view from the audience pit, where enemies sleep and shots
  die); segment 4's drone at row 35.
- The Plant on G's seat stands at (80, 32): the seat is a seat back two
  blocks wide (x 80-81) standing on the top aisle (row 33) over the aisle
  block under it (row 34), `@ breakable hp=2` for both rows, so a crouched
  shot from the aisle breaks it (a shot can't hit the floor it flies over).
- The hollow's way out is a ladder at x 81 (rows 35-39) instead of the
  step at row 37 and two +3 climbs (the Virus's jump only rises 3 cells,
  and a +3 step is out of most runners' reach); `@ seatladder 81 34` turns
  the seat's block over it into ladder once the seat is broken, so the
  ladder's top reaches the aisle. The bonus entrance `B` is at (78, 37), by
  the ladder's left, and G at (80, 39).
- The Nova balcony (one-way row 30 x 74-88) is 4 blocks over the aisle as
  in the spec: only Nova and Skye reach it, from the top of the seat (3
  blocks) once its Plant is gone.
- The Mannequin of the crowd stands on its tier at (71, 35); (71, 37) is
  inside the tier.
- "42 dB" (`@ deco kind=42`) sits at (24, 31), under the view's top from
  the stage floor (row 28 is above it).
- The duck's bow tie is `@ duck 12 37 costume=bowtie`, drawn only.
- `@ arena rect=` marks the chicken run and the turntable stage, where
  several Comics throw and several Plants stand at once; elsewhere only
  one of each. Enemies of a segment past the runner's sleep until its
  ballot ends.
- The ballot's 90 + 30 frames are read as 60 Hz ticks (22 + 8 frames at
  15 Hz): the runner covers a segment in about 70 frames, so with 120 the
  rule came in a segment late and the enemies ahead slept through it.
  Every segment past the runner's last vote sleeps until that ballot ends.
- DOUBLE GEMS adds 500 a gem (the score doubles; the count stays).
- SLIPPERY FLOORS: a quarter cell a frame faster each frame pushing, an
  eighth slower letting go (a quarter both ways slides about 2 cells, not
  the spec's 4).
- LIGHTS OUT is a darkness drawn over the view with a light 10 cells
  round the runner (the `dark` flag's sectors need breakers).
- A chicken's knockback is 4 cells (Rocco 2), 10 under SLIPPERY FLOORS.
- "One Plant standing and one Comic throwing per screen" is one at a time
  outside the arenas.
- QUACK is on the ballot once the profile holds every duck of levels 1-40.
- The c5 fanfare ballot settles at the exit if the runner gets there
  first.
- Secrets are not counted (the engine has no secret rects).
"""

from lib import Level

W, H = 170, 48
L = Level(W, H, name="STAGE 41 - SWEEPS WEEK", episode=6, theme="sweeps_week", music="theme_big_band",
          weapon="applause_cannon", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gems(*pts):
    for x, y in pts:
        put(x, y, "g")


def ladder(x, top, bottom):
    """A ladder from the ledge row `top` down to `bottom`; the block to its
    left must never be solid."""
    L.col(x, top, bottom, "H")
    for y in range(top, bottom + 1):
        assert L.get(x - 1, y) in ".=^1", (x, y, L.get(x - 1, y))


GEMS = []

L.ground(0, W - 1, 40)
L.col(0, 0, 39, "#")
L.col(W - 1, 0, 39, "#")

# --- 1. Rehearsal (x 0-27): BIG HEADS -------------------------------------------------------
L.row(8, 12, 38, "=")                    # the host's desk
put(2, 39, "P")
put(6, 39, "h")
put(10, 37, "W")
put(12, 37, "Q")
L.at("duck", 12, 37, costume="bowtie")
put(14, 39, "m")
L.at("comic", 20, 39)
put(27, 39, "c")
L.at("deco", 24, 31, kind="42", text="42 dB", w=1, h=1)
gems((4, 39), (8, 37), (11, 37), (17, 39), (23, 39), (25, 39))
L.at("vote", 27, 39, "c1", virus=(36, 37))

# --- 2. Segment 1: the warm-up stage (x 28-61) ---------------------------------------------
L.clear(45, 48, 40, 45)                  # the orchestra pit
L.row(45, 47, 45, "^")                   # hot stage lights
ladder(48, 40, 45)
L.fill(52, 55, 38, 39, "#")              # risers
L.row(38, 44, 35, "=")                   # the chicken run's balcony
for x in (38, 41, 44):
    L.at("comic", x, 34)
L.at("arena", rect=(30, 28, 44, 39))
put(40, 39, "h")
L.at("comic", 57, 39)
put(58, 39, "m")
put(60, 39, "W")
put(62, 39, "c")
gems((31, 39), (34, 39), (37, 39), (43, 39), (46, 38), (50, 39), (53, 37), (55, 37))
L.at("vote", 62, 39, "c2", virus=(70, 33))

# --- 3. Segment 2: the stands (x 62-97) ----------------------------------------------------
L.ground(66, 69, 38)
L.ground(70, 73, 36)
L.ground(74, 85, 34)
L.ground(86, 89, 36)
L.ground(90, 93, 38)
L.ground(94, 97, 40)
L.clear(78, 82, 35, 39)                  # the hollow under the top aisle
L.row(80, 81, 33, "#")                   # G's seat
L.at("breakable", rect=(80, 33, 81, 34), hp=2, by="any")
L.at("seatladder", 81, 34)
ladder(81, 35, 39)
put(78, 37, "B")
put(80, 39, "1")                         # G
L.at("audience_plant", 80, 32)
L.at("audience_plant", 70, 35)
L.at("audience_plant", 92, 37)
L.row(74, 88, 30, "=")                   # the Nova balcony
put(84, 29, "m")
put(86, 29, "2")                         # U
put(64, 39, "h")
gems((67, 37), (72, 35), (75, 33), (77, 33), (83, 33), (88, 35), (91, 37), (95, 39))
gems((74, 29), (76, 29), (77, 29), (79, 29), (80, 29), (82, 29), (85, 29), (88, 29))
for x, y, look in ((68, 37, "pilot"), (71, 35, "mannequin"), (76, 33, "cop"), (88, 35, "bouncer"),
                   (66, 37, "fan"), (73, 35, "fan"), (79, 33, "fan"), (84, 33, "fan"),
                   (86, 35, "fan"), (90, 37, "fan"), (93, 37, "fan")):
    L.at("crowd", x, y, look=look)
L.at("vote", 98, 39, "c3", virus=(106, 37))

# --- 4. Segment 3: the crowd walkway (x 98-131) --------------------------------------------
L.clear(103, 126, 40, 41)
L.clear(104, 125, 42, 43)               # the audience pit, ground 44
put(98, 39, "c")
put(100, 39, "h")
L.at("cuedrone", 106, 36, path=((104, 36), (125, 36)))
L.at("cuedrone", 114, 35, path=((104, 35), (125, 35)))
L.at("cuedrone", 124, 37, path=((104, 37), (125, 37)))
L.at("comic", 114, 43)
put(125, 43, "3")                        # N
put(128, 39, "W")
put(130, 39, "m")
gems((101, 39), (103, 41), (106, 43), (109, 43), (112, 43), (117, 43), (120, 43), (123, 43),
     (126, 41), (129, 39))
for x in (107, 111, 116, 121):
    L.at("crowd", x, 43, look="fan")
L.at("vote", 132, 39, "c4", virus=(140, 37))

# --- 5. Segment 4: the turntable stage (x 132-163, an arena) --------------------------------
put(132, 39, "c")
put(134, 39, "T")
L.at("turntable", 136, 40, w=10, dir="r")
L.at("turntable", 148, 40, w=10, dir="l")
L.at("comic", 140, 39)
L.at("cuedrone", 146, 35, path=((138, 35), (156, 35)))
L.row(159, 161, 38, "=")                 # the stage-side box seat
L.at("audience_plant", 160, 37)
L.at("arena", rect=(132, 28, 163, 39))
put(158, 39, "h")
put(162, 39, "c")
gems((137, 39), (139, 39), (143, 39), (145, 39), (149, 39), (152, 39), (155, 39), (157, 39))
L.at("vote", 162, 39, "c5", fanfare=1)

# --- 6. The stage door (x 164-169) ---------------------------------------------------------
put(165, 39, "h")
put(167, 39, "X")

n = sum(row.count("g") for row in L.grid)
assert n == 48, n

L.header["tip"] = "THE AUDIENCE VOTES ON A RULE AT EVERY CHECKPOINT: PLAY IN STYLE AND THEY VOTE KINDER"  # HINTS: the level's first 12 seconds
L.write(__file__, "41_sweeps_week.txt")
