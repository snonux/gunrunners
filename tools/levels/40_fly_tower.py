#!/usr/bin/env python3
"""Level 40 - The Fly Tower (SPEC.md 40). Twist: The Cue Sheet.
Prototype: Pyro Rig.

A tower read bottom to top: the stage wings (start, bottom left, ground
118), four zigzag lower catwalks joined by ladders, the fly gallery's
trusses, the pyro wall shaft and the grid deck with the hatch to the
lighting bridge (exit, top left, row 11). Every catwalk is a one-way slab
unless marked solid; falling off one lands on the one below. Each zone has
a looping cue list (`@ cuelist`) whose `@ cue` lines fire pyro, fly
trusses in and out, blast confetti, switch followspots and drop the towel;
the cue board at the top of the screen shows the next three. Everything is
in world_flytower.cpp.

Changes from the spec, so it plays in this engine:
- Catwalks C1 (x 24-64) and C2 (x 10-64) run under the foot of the ladders
  they lead to, so a runner stands on something under every ladder.
- Zone 3's rect (rows 50-83) and zone 4's (rows 24-51) overlap on L3 inside
  the shaft; there the pyro wall (the later list) wins.
- C5's east column (x 43) is a one-way block, so the +3 from ledge I passes
  up through it; the rest of C5 is the solid slab.
- The grid deck's blocks beside the ladder at x 42 (x 41, rows 12-13) are
  one-way, so a climber fits (the ladder rule).
- Lance's smoke machine is a 10,000-point `@ treasure` (the engine has no
  `@ pickup`).
- Truss cues name the stop they fly to (`stop=0` home, 1 up); TRUSSES OUT
  sends all three home. Cue Q40 (the pyro wave) is a `kind=wave` cue with
  `on=`, `off=` and `offsets=`; Q47 is a `kind=pyro` cue on all six
  columns with `dur=45` and `warn=90`.
- The Q99 cannon's fuse box is the block behind its nozzle (x 25, rows
  20-21, where a standing runner's shots and stream fly).
- Secrets are not counted (the engine has no secret rects).
- `@ step` lines are the bot's way through: `act=ride arg=TRUSS` (board it
  from here once it is home, ride it up), `act=jet` (the jetpack lane), `go`.
"""

from lib import Level

W, H = 80, 120
L = Level(W, H, name="STAGE 40 - THE FLY TOWER", episode=6, theme="fly_tower",
          music="theme_arena_rock", weapon="pyro_rig", par=420, flags="")


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
        assert L.get(x - 1, y) in ".=^", (x, y, L.get(x - 1, y))


def catwalk(x0, x1, row):
    L.row(x0, x1, row, "=")


def cue(cid, lst, t, kind, label, **kw):
    L.at("cue", None, None, cid, list=lst, t=t, kind=kind, label=label, **kw)


# --- 1. Stage wings (rows 104-119) ---------------------------------------------------------
L.fill(0, W - 1, 118, 119, "#")          # the stage floor
L.row(18, 23, 118, ".")                  # TR1's trap
catwalk(24, 64, 104)                     # C1
put(4, 117, "P")
L.at("pyro", 1, 117, "PY1", h=8, w=2, dir="up")
L.at("platform", 18, 118, "TR1", w=6, mode="truss", stops="118,104")
put(21, 117, "W")                        # rides TR1
put(60, 117, "m")
put(14, 117, "h")
put(60, 103, "c")
put(58, 103, "W")
gems((30, 117), (40, 117), (50, 117), (30, 103), (40, 103), (50, 103))
L.at("cuelist", None, None, "cl1", zone="0,104,79,119", period=330)
cue("Q1", "cl1", 45, "pyro", "PYRO STAGE LEFT", target="PY1")
cue("Q2", "cl1", 120, "truss", "FLY IN TRUSS 1", target="TR1", stop=1)
cue("Q3", "cl1", 300, "truss", "FLY OUT TRUSS 1", target="TR1", stop=0)

# --- 2. Lower catwalks (rows 84-103) ---------------------------------------------------------
catwalk(10, 64, 97)                      # C2
catwalk(10, 56, 90)                      # C3
catwalk(30, 73, 83)                      # C4
catwalk(74, 78, 92)                      # the spot perch
ladder(63, 97, 103)
ladder(11, 90, 96)
ladder(56, 83, 89)
L.at("followspot", 76, 91, "FS1")
L.at("rigger", 20, 96)
L.at("rigger", 36, 82)
put(40, 89, "h")
put(24, 96, "m")
put(35, 93, "1")                         # G
put(31, 82, "Q")
L.at("duck", 31, 82, costume="headset")
put(70, 82, "c")
gems((30, 96), (40, 96), (50, 96), (56, 96), (20, 89), (30, 89), (48, 89), (44, 82), (52, 82), (64, 82))
L.at("cuelist", None, None, "cl2", zone="0,84,79,103", period=240)
cue("Q11", "cl2", 0, "spot", "FOLLOWSPOT PICK UP", target="FS1", on=1)
cue("Q12", "cl2", 150, "spot", "BLACKOUT", target="FS1", on=0)

# --- 3. Fly gallery (rows 50-83) --------------------------------------------------------------
catwalk(46, 73, 72)                      # L1
catwalk(4, 41, 62)                       # L2
catwalk(4, 75, 52)                       # L3, the shaft's floor too
catwalk(56, 70, 64)                      # the batten
catwalk(48, 50, 68)                      # Nova's line
catwalk(46, 47, 64)
L.at("platform", 74, 83, "TR2", w=6, mode="truss", stops="83,72")
L.at("platform", 42, 72, "TR3", w=4, mode="truss", stops="72,62")
L.at("platform", 0, 62, "TR4", w=4, mode="truss", stops="62,52")
L.at("confetti", 72, 71, "CF1", dir="l", len=10)
L.at("confetti", 40, 61, "CF2", dir="l", len=10)
L.at("pyro", 5, 61, "PY3", h=9, w=2, dir="up")
L.at("sandbag", 62, 63)
L.at("sandbag", 30, 51)
L.at("rigger", 50, 71)
L.at("followspot", 12, 61, "FS2")
put(60, 71, "h")
put(70, 71, "m")
put(20, 61, "T")
put(8, 59, "2")                          # U
L.at("deco", 52, 71, kind="42", text="42 LB", w=1, h=1)
put(38, 51, "c")
gems((56, 71), (64, 71), (68, 71), (36, 61), (28, 61), (24, 61), (16, 61), (49, 67), (47, 63), (10, 51),
     (18, 51), (26, 51))
L.at("cuelist", None, None, "cl3", zone="0,50,79,83", period=300)
cue("Q30", "cl3", 20, "truss", "FLY IN TRUSS 2", target="TR2", stop=1)
cue("Q31", "cl3", 70, "pyro", "PYRO STAGE LEFT", target="PY3")
cue("Q32", "cl3", 120, "truss", "FLY IN TRUSS 4", target="TR4", stop=1)
cue("Q33", "cl3", 170, "confetti", "CONFETTI", target="CF1,CF2")
cue("Q34", "cl3", 200, "truss", "FLY IN TRUSS 3", target="TR3", stop=1)
cue("Q35", "cl3", 280, "truss", "TRUSSES OUT", target="TR2,TR3,TR4", stop=0)

# --- 4. Pyro wall (shaft x 44-75, rows 21-51) --------------------------------------------------
LEDGES = [(46, 49), (52, 46), (58, 43), (64, 40), (70, 37), (64, 34), (58, 31), (52, 28), (46, 25)]  # A-I
for x, row in LEDGES:
    catwalk(x, x + 3, row)
for i, x in enumerate((50, 56, 62, 68)):
    L.at("pyro", x, 51, "PY%d" % (i + 4), h=31, w=2, dir="up")
L.at("pyro", 44, 51, "PY8", h=31, w=2, dir="up")
L.at("pyro", 74, 51, "PY9", h=31, w=2, dir="up")
catwalk(76, 79, 30)                      # the smoke machine's perch
L.at("treasure", 78, 29, score=10000, art="smoke_machine")
put(46, 51, "W")
put(44, 51, "h")
gems((47, 48), (48, 48), (53, 45), (54, 45), (59, 42), (60, 42), (65, 39), (66, 39), (71, 36), (72, 36),
     (66, 33), (60, 30), (54, 27), (48, 24))
L.at("cuelist", None, None, "cl4", zone="44,24,79,51", period=300)
cue("Q40", "cl4", 0, "wave", "PYRO WAVE", target="PY4,PY5,PY6,PY7", on=20, off=40, offsets="0,15,30,45")
cue("Q47", "cl4", 240, "pyro", "PYRO FULL STAGE", target="PY4,PY5,PY6,PY7,PY8,PY9", dur=45, warn=90)

# --- 5. Grid (rows 0-23) ------------------------------------------------------------------------
L.fill(8, 43, 22, 23, "#")               # C5 and the crawl space's floor
L.row(43, 43, 22, "=")                   # C5's east end: one-way
L.row(43, 43, 23, ".")
L.fill(7, 19, 17, 17, "#")               # the crawl space's ceiling
L.fill(7, 7, 18, 21, "#")                # and its back wall
L.fill(19, 19, 18, 21, "#")              # the wall Q99 breaks
L.at("breakable", rect=(19, 18, 19, 21), hp=6, by="any")
L.fill(2, 79, 12, 13, "#")               # the grid deck
L.fill(70, 79, 12, 12, "=")              # one-way over the right lane
L.fill(70, 79, 13, 13, ".")
L.fill(41, 41, 12, 13, "=")
ladder(42, 12, 21)
put(40, 21, "c")
put(36, 21, "W")
L.at("pyro", 24, 21, "Q99", h=6, w=2, dir="l", fuse=2)
put(12, 21, "3")                         # N
put(9, 19, "B")
L.at("rigger", 24, 11)
L.at("sandbag", 12, 11)
put(34, 11, "h")
L.at("towel", 20, 2, land=11)
put(10, 11, "m")
put(4, 11, "X")
gems((28, 21), (32, 21), (60, 11), (52, 11), (46, 11), (38, 11), (29, 11), (16, 11))
L.at("cuelist", None, None, "cl5", zone="0,0,79,23", period=450)
cue("Q42", "cl5", 60, "towel", "THE ANSWER", once=1)
cue("Q99", "cl5", -1, "cut", "(CUT)", target="Q99")

# The bot's way through.
for x, y, act, kw in [
        (16, 117, "ride", {"arg": "TR1"}), (60, 96, "go", {}), (14, 89, "go", {}), (58, 82, "go", {}),
        (72, 82, "ride", {"arg": "TR2"}), (47, 71, "ride", {"arg": "TR3"}), (8, 61, "ride", {"arg": "TR4"}),
        (36, 51, "go", {}), (44, 51, "jet", {"arg": "21"})] + \
        [(x + 1, row - 1, "go", {}) for x, row in LEDGES] + [(40, 21, "go", {})]:
    L.at("step", x, y, act=act, **kw)

L.write(__file__, "40_fly_tower.txt")
