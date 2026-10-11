#!/usr/bin/env python3
"""Level 30 - Fiber Trench (SPEC.md 30). Twist: Buoyancy.
Prototype: Harpoon Line.

A descent in a Z under the sea: the shallow shelf, down the kelp slope into
the sunken ship's dark hold, out of its stern hatch and down the trench's
right wall, then back left across the trench on harpoon lines to the cable
station at the bottom left, where the exit is. The whole map is water: a
runner sinks slowly, every jump is a 4-cell swim stroke and three strokes
(Nova four) are all they get before touching ground, a line or an air
pocket again. Currents push swimmers (not Rocco); the downdraft over the
trench is stronger than three strokes, so the trench is crossed on lines:
crouch and fire to plant a harpoon, then shoot the anchor ring across.

Everything in world_trench.cpp.

Changes from the spec, so it plays in this engine:
- A line is walked level, at the height of the ground where its planted end
  is (`@ anchor x y spot=sx`: the planting spot), not as a slope, and it is
  not a hang bar from below.
- The trench's second ledge is 99-102 (spec 99-106), so the drop from the
  third ledge's left end to the shelf is clear of it. The Cable Eels guard
  swimmers rather than the walk: (103, 46) facing left over the trench,
  (95, 48) facing up beside the first line (spec (106, 47) and (94, 50),
  in the line's and the shot's way), (72, 52) facing down over the far
  ledge.
- An extra `W` on the shelf at (112, 51), so there is ammo for both lines
  after the hold's Anglerfish.
- The ship sits in open water (a deck at rows 22-23); the treasure's lip is
  joined to the rock at its right and the ceiling over the kelp lift is one
  row thick, so a runner can stand under it.
"""

from lib import Level

W, H = 120, 80
L = Level(W, H, name="STAGE 30 - FIBER TRENCH", episode=5, theme="deep_sea", music="theme_submerged",
          weapon="harpoon_line", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def carve(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


L.fill(0, W - 1, 0, H - 1, "#")               # rock, carved out below
L.at("fluid", None, None, kind="water", rect=(0, 0, 119, 79))

# --- 1. Shallow shelf (x 0-26) --------------------------------------------------------------------
carve(0, 26, 2, 11)
L.fill(14, 22, 10, 11, "#")                 # the wreck's deck (g10)
put(2, 11, "P")
put(10, 11, "m")
put(18, 9, "W")
put(24, 11, "h")
put(12, 4, "1")
gem((5, 11), (7, 9), (8, 8), (9, 9), (15, 9), (21, 9), (16, 6), (20, 6))

# --- 2. Kelp slope (x 27-56) ----------------------------------------------------------------------
carve(27, 34, 2, 15)
carve(35, 42, 2, 19)
carve(43, 50, 6, 23)
carve(51, 56, 6, 27)
carve(36, 43, 1, 1)                         # the ceiling over the lift's top is one row thick
L.row(39, 42, 4, "#")                       # the treasure's lip (joined to the rock at 43)
L.fill(43, 50, 2, 5, "#")
L.at("current", None, None, rect=(30, 4, 38, 15), dir="u", push="1/2")   # kelp lift
L.at("current", None, None, rect=(44, 8, 52, 22), dir="l", push="1/2")   # kelp drift
L.at("treasure", 40, 3, score=10000, art="maxtv_tape_waterproof")
L.at("box_jelly", 32, 10, rise=6, up=24, hold=30, down=24, rest=30)
L.at("box_jelly", 40, 14, rise=6, up=24, hold=30, down=24, rest=30, phase=40)
L.at("box_jelly", 47, 18, rise=6, up=24, hold=30, down=24, rest=30, phase=80)
put(54, 27, "c")
gem((31, 13), (33, 10), (35, 7), (37, 4), (31, 7), (36, 13), (39, 17), (46, 21), (49, 21), (51, 27))

# Secret 1: the Anglerfish cave, by the crack at x 49-50 rows 25-27 beside the sparkle.
carve(49, 50, 25, 27)
carve(36, 48, 25, 32)
put(37, 30, "B")
put(38, 32, "2")
put(45, 32, "$")
L.at("anglerfish", 40, 32, docile=1, dir=-1)
gem((42, 32), (43, 32), (44, 28), (46, 32), (47, 32), (47, 28), (39, 26), (41, 26), (43, 26), (45, 26))

# --- 3. Sunken ship (x 57-90) ---------------------------------------------------------------------
carve(57, 90, 6, 21)                        # open water over the wreck
carve(57, 89, 24, 33)                       # the hold (deck rows 22-23, floor g34)
carve(90, 90, 30, 33)                       # the stern hatch
L.at("dark", None, None, rect=(57, 24, 90, 33))
L.at("airpocket", None, None, rect=(60, 28, 80, 33))
put(62, 33, "T")
L.at("porthole", 64, 31)
put(66, 33, "h")
put(70, 33, "W")
L.at("anglerfish", 72, 33, dir=-1)
put(75, 33, "m")
L.at("anglerfish", 84, 33, dir=-1)
put(88, 33, "c")
gem((59, 33), (61, 33), (68, 31), (78, 33), (86, 33), (64, 18))

# --- 4. The trench (x 76-119) ---------------------------------------------------------------------
carve(91, 119, 30, 51)                      # outside the hatch, over the ledges and the shelf
carve(76, 103, 40, 79)                      # the trench and the abyss under it
L.fill(91, 98, 38, 39, "#")                 # ledge 1 (g38)
L.fill(99, 102, 42, 43, "#")                # ledge 2 (g42)
L.fill(107, 119, 46, 47, "#")               # ledge 3 (g46)
L.fill(104, 119, 52, 79, "#")               # the shelf (g52)
L.fill(88, 93, 54, 79, "#")                 # the pillar (g54)
L.at("current", None, None, rect=(76, 40, 103, 66), dir="d", push=1)    # the downdraft
L.at("abyss", None, None, rect=(76, 67, 103, 79))
L.at("anchor", 92, 51, spot=105)            # the first line: shelf -> pillar
L.at("anchor", 74, 54, spot=88)             # the second line: pillar -> far ledge
L.at("cable_eel", 103, 46, face="l")
L.at("cable_eel", 95, 48, face="u")
L.at("cable_eel", 72, 52, face="d")
put(98, 37, "h")
put(110, 45, "c")
put(116, 45, "m")
put(112, 51, "W")
put(90, 53, "h")
L.at("box_jelly", 82, 66, carrier=1, rise=20, up=40, hold=10, down=40, rest=30)
gem((94, 37), (101, 41), (109, 45), (114, 45), (100, 48), (96, 50), (84, 52), (80, 52))

# --- 5. The cable station (x 2-75) ----------------------------------------------------------------
carve(60, 75, 46, 55)                       # over the far ledge (g56)
carve(40, 59, 48, 59)                       # g60
carve(2, 39, 48, 63)                        # g64, the station and the water over its roof
L.fill(1, 31, 55, 55, "#")                  # the station's roof
L.fill(31, 31, 55, 59, "#")                 # its wall, the door at rows 60-63
L.at("airpocket", None, None, rect=(2, 56, 30, 63))
put(64, 55, "c")
put(66, 55, "W")
put(66, 51, "3")
put(50, 59, "m")
put(30, 63, "h")
L.at("junction", 28, 62)
put(20, 54, "Q")
L.at("duck", 20, 54, costume="diving_mask")
L.at("deco", 26, 58, kind="42", art="pressure_gauge", text="42 FATHOMS", w=1, h=1)
put(12, 63, "X")
gem((70, 55), (56, 59), (45, 59), (36, 63), (24, 63), (18, 63))

L.write(__file__, "30_fiber_trench.txt")
