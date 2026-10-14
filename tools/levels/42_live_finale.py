#!/usr/bin/env python3
"""Level 42 - The Live Finale (SPEC.md 42). Twist: Season Recap.
Prototype: The Encore.

The stage (x 0-119) read left to right: the stage door (start), six set
wagons in their slots along the turntable, backstage with the refill,
and the ON AIR door into the arena (x 120-159), where Lance Marquee, the
Encore Engine and the race to the master switch end the show. A `@ wagon`
brings back an earlier level's system inside its slot: when the runner
stands past its trigger the turntable turns for 60 frames (the world holds
still), the wagon locks in and its trick runs for 450 frames on the
ratings ticker; a runner still in the slot then gets a RERUN (60 frames in
the rest state) and another 450. Everything is in world_finale.cpp.

Changes from the spec, so it plays in this engine:
- The screen shows 11 block rows, so the arena is lower: Lance's podium
  hovers with its top at row 32 (spec 27) and sinks to row 35 for his cue
  cards; the Encore Engine is 8 x 8 blocks (x 144-151, rows 32-39, spec
  8 x 12) with its 2 x 2 core on its front face, (144, 37) (spec (147,
  33)), where a standing runner's shots reach it before the robot's bulk
  stops them.
- Off the Air's high deck is at row 31 as in the spec (Nova and Skye).
- The floor under the first beat sign (x 11-13) dips a block (ground 41)
  so a runner who falls off the signs can walk back under S1 and climb
  again (the spec's 2-block gap under S1 has no room for a runner).
- The station's Ad Break rolls in at x 55 (spec 56) and the ceiling plate
  is at x 59 (spec 58), so the runner coming down from the plate lands
  beside the Ad Break, not on it.
- A wagon rolls in once the runner stands on the ground past its trigger
  (the studio's trigger at x 94 is over the toon pit, where the runner is
  air-running).
- The plates are `@ plate ID x y zone= dir=up|down` (a gravzone takes the
  flip 8 frames later); the monitors are `@ portal ID x y to=ID` (2 x 3
  blocks, x y the top-left; up on its bottom row).
- The ON AIR door, the trapdoors, Off the Air's decks and the switch's
  glass cover are script layers (`style=station`, drawn by the finale).
  The door opens again for a runner who respawns at the arena checkpoint.
- The master switch is the exit (157, 35); it works once the glass cover
  is broken in Off the Air.
- The Censor Bars' paths are at rows 29 and 27 (spec 28 and 26), so they
  are on screen from the wall top and the vines.
- Secrets are not counted (the engine has no secret rects).
- `@ step` lines are the bot's way through: `act=plate` (stand on the
  plate until gravity turns), `act=ceiling` (walk the ceiling to the plate
  at x and wait), `act=hop arg=X,Y` (up into a monitor), `go`.
"""

from lib import Level

W, H = 160, 44
L = Level(W, H, name="STAGE 42 - THE LIVE FINALE", episode=6, theme="main_stage", music="theme_medley",
          weapon="encore", par=540, flags="")


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
        assert L.get(x - 1, y) in ".=^1234", (x, y, L.get(x - 1, y))


def script(lid, x0, y0, x1, y1, solid, tile="#"):
    L.at("layer", None, None, lid, rect=(x0, y0, x1, y1), driver="script", solid=solid, style="station", tile=tile)


def step(x, y, act="go", **kw):
    L.at("step", x, y, act=act, **kw)


L.ground(0, W - 1, 40)
L.col(0, 0, 39, "#")
L.col(W - 1, 0, 39, "#")

# --- 1. The stage door (x 0-7) -------------------------------------------------------------
put(2, 39, "P")
put(5, 39, "W")                          # on the pedestal: "Contestants, your arsenal!"
L.at("deco", 4, 38, kind="pedestal")
put(7, 39, "h")
gems((1, 37), (3, 37), (4, 36), (6, 37))

# --- 2. The neon and temple wagons (x 8-43) ------------------------------------------------
L.at("wagon", None, None, "neon", kind="neon", slot=(8, 25), trigger=4)
L.clear(11, 13, 40, 40)                  # the dip under S1 (ground 41)
for sid, x0, row in (("S1", 11, 37), ("S2", 14, 34), ("S3", 17, 31)):
    L.at("layer", None, None, sid, rect=(x0, row, x0 + 2, row), driver="beat", beats=(1, 2, 3), bars=1,
         style="sign", color="pink" if sid == "S2" else "cyan")
L.fill(20, 23, 30, 39, "#")              # the neon wall (top ground 30)
L.at("censorbar", 10, 29, path=((10, 29), (24, 29)))
put(12, 30, "$")                         # off S3 at the top of the beat
L.at("wagon", None, None, "temple", kind="temple", slot=(26, 43), trigger=22)
L.clear(29, 40, 40, 42)                  # the chasm, pit floor ground 43
L.row(29, 32, 42, "^")
L.row(37, 39, 42, "^")
ladder(40, 40, 42)
L.at("vine", 32, 30, "V1", len=8, amp=50)
L.at("vine", 37, 30, "V2", len=8, amp=50)
L.at("censorbar", 28, 27, path=((28, 27), (42, 27)))
put(34, 42, "2")                         # U, on the chasm floor's safe patch
put(36, 42, "$")
put(42, 39, "m")
gems((9, 39), (12, 36), (15, 33), (18, 30), (21, 28), (23, 28), (25, 39), (31, 36), (35, 36), (38, 36))

# --- 3. The station and western wagons (x 44-80) -------------------------------------------
put(44, 39, "c")
put(46, 39, "h")
L.at("wagon", None, None, "station", kind="station", slot=(45, 61), trigger=41)
L.at("gravzone", None, None, "GZ", rect=(45, 22, 61, 39), dir="down")
L.fill(45, 61, 20, 21, "#")              # the ceiling slab (walkable underside, row 22)
L.fill(52, 54, 26, 39, "#")              # the barrier
L.at("plate", 48, 39, "PU", zone="GZ", dir="up")
L.at("plate", 59, 22, "PD", zone="GZ", dir="down")
L.at("adbreak", 55, 39, wagon="station")  # rolls in after the station's flip
put(47, 23, "1")                         # G, on the ceiling left of the barrier
put(50, 22, "$")
put(61, 39, "c")
put(62, 39, "W")
L.at("wagon", None, None, "western", kind="western", slot=(63, 79), trigger=59)
L.at("fuse", None, None, "FZ", path=((64, 39), (70, 39)), to="BR1", speed=0.5, bot=1, stand=61)
L.at("barrel", 70, 39, "BR1", radius=3, damage=8, hurt=2, flash=12)
L.fill(72, 73, 28, 39, "#")
L.at("breakable", rect=(72, 28, 73, 39), hp=1, by="explosion")
L.at("adbreak", 66, 39, wagon="western")   # the barrel's blast reaches it
put(75, 39, "$")                         # behind the western wall
put(78, 39, "h")
put(80, 39, "c")
gems((45, 38), (50, 38), (49, 23), (56, 23), (57, 38), (59, 38), (63, 38), (65, 37), (71, 37), (74, 37),
     (76, 38), (79, 37))

# --- 4. The toon and studio wagons (x 81-109) ----------------------------------------------
put(81, 39, "h")
L.at("wagon", None, None, "toon", kind="toon", slot=(81, 96), trigger=77)
L.clear(84, 94, 40, 42)                  # the cliff gap, pit floor ground 43
ladder(94, 40, 42)
L.at("double", 83, 39, runner="dash")
put(89, 42, "$")                         # in the toon pit
put(96, 39, "c")
put(98, 39, "h")
L.at("wagon", None, None, "studio", kind="studio", slot=(98, 109), trigger=94)
L.fill(104, 105, 24, 39, "#")            # the shutter wall
L.at("portal", 100, 37, "CAM1", to="CAM2", label="CAM 1")
L.at("portal", 107, 37, "CAM2", to="CAM1", label="CAM 2")
L.at("double", 103, 39, runner="rocco")
L.at("double", 109, 39, runner="nova")
L.row(108, 109, 36, "=")                 # the light gantry (Nova, +4)
put(108, 35, "$")
put(106, 39, "m")
gems((82, 38), (85, 39), (88, 39), (91, 39), (97, 38), (99, 37), (102, 38), (109, 35))

# --- 5. Backstage (x 110-119) --------------------------------------------------------------
L.row(110, 112, 36, "=")                 # the prop shelf
L.fill(113, 114, 38, 39, "#")            # the road case (+2)
L.at("refill", 111, 39)                  # full hearts and pool
put(112, 39, "W")
put(111, 35, "3")                        # N, on the prop shelf
put(115, 39, "c")                        # the arena checkpoint
put(116, 39, "T")
put(118, 39, "Q")
L.at("duck", 118, 39, costume="host")
L.fill(119, 119, 20, 35, "#")            # the proscenium wall over the ON AIR door
script("onair", 119, 36, 119, 39, solid=0)
L.at("deco", 119, 35, kind="42", text="STAGE 42", w=1, h=1)
gems((110, 38), (113, 36), (117, 38), (117, 36))

# --- 6. The arena (x 120-159) --------------------------------------------------------------
for tid, x0 in (("TD1", 128), ("TD2", 138), ("TD3", 148)):
    L.clear(x0, x0 + 2, 40, 42)          # the trap under it, floor ground 43
    script(tid, x0, 40, x0 + 2, 40, solid=1)
L.fill(155, 158, 36, 39, "#")            # the master-switch console (ground 36)
L.fill(153, 154, 38, 39, "#")            # the road case step
script("cover", 157, 34, 157, 35, solid=1)
put(157, 35, "X")                        # the master switch
script("dstep", 123, 37, 125, 37, solid=0, tile="=")
script("dmid", 126, 35, 152, 35, solid=0, tile="=")
script("dhigh", 130, 31, 154, 31, solid=0, tile="=")
L.at("lance", 124, 32, arena=(120, 28, 158, 39), path=((124, 32), (150, 32)), engine=(144, 32),
     desk=158, restart=(122, 39), drops=((130, 39), (140, 39)), turbo=(134, 39))

# The bot's way through.
step(48, 39, "plate")
step(59, 22, "ceiling")
step(100, 39, "hop", arg=(107, 39))

n = sum(row.count("g") for row in L.grid)
assert n == 38, n
L.write(__file__, "42_live_finale.txt")
