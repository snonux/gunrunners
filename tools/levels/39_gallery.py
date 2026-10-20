#!/usr/bin/env python3
"""Level 39 - The Gallery (SPEC.md 39). Twist: Monitor Hop.
Prototype: Echo Mic.

A hub of closed rooms in three bands linked only by studio monitors: band C
(ground 56) the edit suite, the hallway, studio B and the cable corridor;
band B (ground 36) the panning room, the tape library and the main gallery
with the exit; band A (ground 16) the forgotten studio and the virus room.
Up in front of a monitor (feet on its bottom row) hops to the one it shows;
panning monitors drop the runner where their camera is looking. Shots fly
through monitors. Everything is in world_gallery.cpp.

Changes from the spec, so it plays in this engine:
- Entering a monitor is up only (walking into one doesn't hop), so a runner
  walking past never hops by accident.
- The force field at x 83 is a solid wall (it never opens).
- A monitor without `to=` is a dead one (the CAM 42 label).
- The static monitor B in the forgotten studio is the bonus door `B` on the
  floor in front of it (36, 15).
- Secrets are not counted (the engine has no secret rects); the forgotten
  studio, the tape library and the virus room are still off the route.
- `@ step` lines are the bot's way through: `act=hop arg=X,Y` (the block in
  front of the target monitor), `act=pan lo= hi= arg=ROW` (waits until the
  camera is over blocks lo-hi).
"""

from lib import Level

W, H = 140, 60
L = Level(W, H, name="STAGE 39 - THE GALLERY", episode=6, theme="gallery",
          music="theme_synth_pulse", weapon="echo_mic", par=420)


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gems(*pts):
    for x, y in pts:
        put(x, y, "g")


def room(x0, x1, top, ground):
    L.fill(x0, x1, top, ground - 1, ".")


def portal(pid, x, y, **kw):
    L.at("portal", x, y, pid, **kw)


# Everything solid, then the rooms carved out.
L.fill(0, W - 1, 0, H - 1, "#")
# Band C (rows 40-55, ground 56).
room(1, 20, 40, 56)       # R1 edit suite
room(23, 59, 40, 56)      # R2 hallway
room(61, 97, 40, 56)      # R3 studio B
room(99, 138, 40, 56)     # R4 cable corridor
# Band B (rows 20-35, ground 36).
room(1, 45, 20, 36)       # R5 panning room
room(47, 59, 20, 36)      # R6 tape library
room(61, 69, 20, 36)      # R6 landing
room(71, 138, 20, 36)     # R7 main gallery
L.fill(83, 83, 20, 35, "#")   # the force field, closed for good
L.fill(95, 95, 20, 31, "#")   # G1 | G2, doorway rows 32-35
L.fill(117, 117, 20, 31, "#")  # G2 | G3
# Band A (rows 2-15, ground 16).
room(1, 40, 2, 16)        # R8 forgotten studio
room(71, 100, 2, 16)      # R10 virus room

# --- R1 edit suite -------------------------------------------------------------------------------
L.row(8, 12, 53, "=")     # the mixing desk
put(3, 55, "P")
put(10, 52, "W")
put(12, 52, "Q")
L.at("duck", 12, 52, costume="headphones")
put(5, 55, "m")
L.at("deco", 19, 45, kind="cam0", text="CAM 0", w=2, h=2)
portal("M1", 16, 53, to="M1b", label="CAM 2")
gems((2, 52), (6, 54), (9, 52), (14, 54))

# --- R2 hallway ----------------------------------------------------------------------------------
portal("M1b", 24, 53, to="M1", label="CAM 1")
L.at("wrangler", 34, 55)
L.at("wrangler", 50, 55)
put(40, 55, "h")
put(30, 55, "m")
portal("M2N", 44, 49, to="M5b", label="CAM 6", oneway=1)
portal("CAM42", 52, 49, label="CAM 42")
L.at("deco", 52, 48, kind="42", text="CAM 42", w=1, h=1)
portal("M2", 57, 53, to="M2b", label="CAM 3")
gems((28, 54), (32, 54), (38, 54), (42, 54), (46, 54), (48, 52), (54, 54), (56, 52))

# --- R3 studio B ---------------------------------------------------------------------------------
portal("M2b", 62, 53, to="M2", label="CAM 2")
L.fill(74, 82, 54, 55, "#")   # the riser
put(65, 55, "c")
put(72, 55, "h")
L.at("boomop", 78, 53)
portal("MS", 86, 53, to="MR8", label="NO SIGNAL")
portal("M3", 95, 53, to="M3b", label="CAM 5")
gems((64, 52), (68, 54), (70, 54), (76, 52), (80, 52), (84, 54), (90, 54), (93, 54))

# --- R4 cable corridor -----------------------------------------------------------------------------
portal("M3b", 100, 53, to="M3", label="CAM 3")
L.row(108, 136, 53, "=")      # the catwalk
put(103, 55, "c")
put(106, 55, "W")
put(104, 54, "h")
for x in (112, 126, 134):
    L.at("wrangler", x, 55)
portal("MV", 118, 50, to="MVb", label="VIRUS WARNING", oneway=1, virus=1)
portal("M5", 137, 53, to="M5b", label="CAM 6")
gems((110, 52), (114, 52), (116, 52), (122, 52), (124, 52), (128, 52), (130, 52), (132, 52))

# --- R5 panning room ------------------------------------------------------------------------------
portal("M5b", 42, 33, to="M5", label="CAM 5")
put(40, 35, "c")
put(12, 35, "h")
L.at("boomop", 22, 35)
L.at("camspot", None, None, "CS8", path="49,35;67,35", speed="1/2")
portal("M6", 4, 33, to="CS8", label="CAM 8")
gems((8, 34), (16, 34), (28, 34), (32, 34), (36, 34), (44, 32))

# --- R6 tape library and landing ------------------------------------------------------------------
L.row(50, 58, 33, "=")
L.row(50, 58, 30, "=")
for x in range(50, 59):
    L.at("treasure", x, 32, score=2000, art="candid_tape")
    L.at("treasure", x, 29, score=2000, art="candid_tape")
for x in (52, 55):
    L.at("treasure", x, 35, score=2000, art="candid_tape")
portal("MT", 47, 33, to="8,35", label="CAM 7", oneway=1)
put(63, 35, "W")
put(66, 35, "2")                                  # U
L.at("camspot", None, None, "CS11", path="73,35;94,35", speed="1/2")
portal("M8", 67, 33, to="CS11", label="CAM 11")

# --- R7 main gallery ------------------------------------------------------------------------------
put(76, 35, "T")
portal("MG", 72, 33, to="63,35", label="CAM 10", oneway=1)
put(87, 35, "c")
L.at("camspot", None, None, "CS9", path="100,35")
portal("M9", 92, 33, to="CS9", label="CAM 9")
put(98, 35, "h")
L.at("wrangler", 104, 35)
L.at("floormgr", 110, 35)
L.at("boomop", 113, 35)
put(120, 35, "m")
L.at("boomop", 122, 35)
L.at("floormgr", 128, 35)
L.at("wrangler", 133, 35)
L.row(128, 132, 30, "=")      # the light rig
L.col(127, 30, 35, "H")       # and its ladder
put(130, 29, "3")                                 # N
L.at("deco", 126, 22, kind="onair", text="ON AIR", w=2, h=1)
put(137, 35, "X")
gems((74, 34), (78, 34), (80, 34), (86, 34), (90, 34), (100, 34), (106, 32), (114, 32), (124, 32), (129, 29),
     (131, 29), (135, 34))

# --- R8 forgotten studio, R10 virus room ------------------------------------------------------------
portal("MR8", 4, 13, to="MS", label="CAM 4")
put(20, 15, "1")                                  # G
put(36, 15, "B")
gems(*[(x, 15) for x in (8, 11, 14, 17, 23, 26, 29, 32)], (12, 12), (28, 12))
portal("MVb", 72, 13, label="VIRUS")
put(76, 15, "$")
portal("MR10", 98, 13, to="124,52", label="CAM 13", oneway=1)

# The bot's way through.
for x, y, act, kw in [
        (17, 55, "hop", {"arg": "25,55"}), (58, 55, "hop", {"arg": "63,55"}), (96, 55, "hop", {"arg": "101,55"}),
        (138, 55, "hop", {"arg": "43,35"}), (5, 35, "pan", {"arg": "35", "lo": 62, "hi": 67}),
        (68, 35, "pan", {"arg": "35", "lo": 85, "hi": 94}), (100, 35, "go", {}), (120, 35, "go", {})]:
    L.at("step", x, y, act=act, **kw)

L.header["tip"] = "PRESS UP IN FRONT OF A MONITOR TO HOP TO THE ONE IT SHOWS"  # HINTS: the level's first 12 seconds
L.write(__file__, "39_gallery.txt")
