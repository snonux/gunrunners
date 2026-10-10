#!/usr/bin/env python3
"""Level 19 - Gravity Lab (SPEC.md 19). Twist: Gravity Switches. Prototype: Grav Grenade.

A hub of nine 40 x 20 test chambers in a 3 x 3 grid. The route snakes through
all nine: along the bottom row west to east (C1, C2, C3), up through a hole
into the middle row and back west (B3, B2, B1), up through a hole into the
top row and east again (A1, A2, A3), where the exit hangs on the ceiling.

Every chamber is a `@ gravzone` with its own down; its wall switch (`@ switch
kind=panel`: press up on it, or shoot it) turns it over, and everything in it
falls the other way. A runner entering a chamber takes its down at once;
outside the chambers (in the doorways and slab holes) it keeps the one it has.

Changes from the spec, so it plays in this engine:
- Hanging spikes are the `v` tile (spikes on a ceiling, points down).
- B1's switch alcove is a shaft the whole height of B1 (x 2-8), walled off
  at x 9 above the Test Gate, so flipping B1 there drops the runner up
  through the hole at x 4-7.
- The closet's bonus entrance is there as soon as the closet is open (S_CL
  only turns the closet over, to get back out).
- A3's floor stash is a low room x 106-125 under a roof at row 18, its door
  the fake wall at x 106.
"""

from lib import Level

W, H = 130, 70
L = Level(W, H,
          name="STAGE 19 - GRAVITY LAB", episode=3, theme="station_gravlab",
          music="clinical_bleeps", weapon="grav_grenade", par=200, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


# --- The hub: solid everywhere but the chambers ---------------------------------------------------
L.fill(0, W - 1, 0, H - 1, "#")
COLS = {1: (2, 41), 2: (44, 83), 3: (86, 125)}
ROWS = {"A": (2, 21), "B": (24, 43), "C": (46, 65)}
for r, (y0, y1) in ROWS.items():
    for c, (x0, x1) in COLS.items():
        L.clear(x0, x1, y0, y1)
# B1's ceiling is row 27: the Rocco duct (rows 24-26) runs over it.
L.fill(2, 41, 27, 27, "#")

# Doors (3-row openings in the walls) and slab holes.
for wx in (42, 43):
    L.clear(wx, wx, 63, 65)   # C1|C2 floor side
    L.clear(wx, wx, 46, 48)   # C1|C2 ceiling side
    L.clear(wx, wx, 41, 43)   # B2|B1 floor side
    L.clear(wx, wx, 24, 26)   # B2|duct ceiling side
    L.clear(wx, wx, 2, 4)     # A1|A2 ceiling side
for wx in (84, 85):
    L.clear(wx, wx, 63, 65)   # C2|C3
    L.clear(wx, wx, 46, 48)
    L.clear(wx, wx, 24, 26)   # B3|B2
    L.clear(wx, wx, 41, 43)
    L.clear(wx, wx, 19, 21)   # A2|A3
    L.clear(wx, wx, 2, 4)
L.clear(120, 123, 44, 45)     # C3 up into B3
L.clear(4, 7, 22, 23)         # the duct up into A1
L.clear(4, 7, 27, 27)         # B1's alcove up into the duct

# The chambers, each with its own down and its switch.
ZONES = [
    ("C1", 2, 46, 41, 65, "down", "S_C1"), ("C2", 44, 46, 83, 65, "down", "S_C2"),
    ("C3", 86, 46, 125, 65, "down", "S_C3"),
    ("B3", 86, 24, 125, 43, "up", "S_B3"), ("B2", 44, 24, 83, 43, "up", "S_B2"),
    ("B1", 2, 28, 41, 43, "down", "S_B1"),
    ("A1", 2, 2, 41, 21, "up", "S_A1"), ("A2", 44, 2, 83, 21, "up", "S_A2a"),
    ("A3", 86, 2, 125, 21, "down", "S_A3"),
]
for zid, x0, y0, x1, y1, d, sw in ZONES:
    L.at("gravzone", None, None, zid, rect=(x0, y0, x1, y1), dir=d, switch=sw)
L.at("gravzone", None, None, "DUCT", rect=(2, 24, 41, 26), dir="up")
# Orange arrows on the back walls, two to a chamber.
for zid, x0, y0, x1, y1, d, sw in ZONES:
    for ax in (x0 + 6, x1 - 6):
        L.at("deco", ax, (y0 + y1) // 2, kind="arrow")

# --- 1. C1, the empty chamber (x 2-41) --------------------------------------------------------------
put(4, 65, "P")
L.at("deco", None, None, kind="desk", rect=(9, 65, 11, 65))
L.at("deco", 10, 64, kind="mug")
put(12, 65, "h")
L.at("switch", 20, 64, "S_C1", kind="panel")
put(30, 46, "W")
put(35, 46, "m")
gem((24, 46), (26, 46), (28, 46), (33, 46), (38, 46))

# --- 2. C2 and C3, Flip Walkers (x 44-125) -------------------------------------------------------------
put(45, 65, "c")
L.at("switch", 50, 64, "S_C2", kind="panel")
L.at("flip_walker", 55, 65, "FW1")
L.at("flip_walker", 70, 65, "FW2")
put(60, 46, "1")                                  # letter G, on C2's ceiling
L.fill(74, 80, 62, 62, "=")                       # the Nova ledge, hung on chains
L.at("deco", None, None, kind="chain", rect=(74, 46, 74, 61))
L.at("deco", None, None, kind="chain", rect=(80, 46, 80, 61))
gem((74, 61), (75, 60), (76, 59), (78, 59), (79, 60), (80, 61))
L.at("flip_walker", 95, 65, "FW3")
L.at("flip_walker", 105, 65, "FW4")
L.at("flip_walker", 112, 65, "FW5")
put(100, 65, "h")
put(100, 46, "Q")                                 # the duck, glued to C3's ceiling in a lab coat
L.at("switch", 118, 64, "S_C3", kind="panel")
gem((48, 62), (52, 62), (64, 62), (68, 62), (90, 62), (98, 62), (108, 62), (116, 62))

# --- 3. B3 and B2, the probe corridor (x 125 -> 44) ------------------------------------------------------
put(121, 24, "c")
put(100, 24, "W")
put(90, 24, "m")
L.at("gravity_probe", 112, 32, "GP1")
L.at("gravity_probe", 90, 32, "GP2")
L.at("gravity_probe", 60, 34, "GP3")
L.at("switch", 116, 25, "S_B3", kind="panel")
L.at("switch", 70, 25, "S_B2", kind="panel")
L.fill(44, 60, 24, 24, "v")                       # spikes hanging from B2's ceiling
put(66, 24, "C")                                  # the camera at the observation window
L.at("deco", None, None, kind="window", rect=(64, 24, 68, 24))
put(60, 43, "h")
gem((118, 24), (114, 24), (108, 24), (104, 24), (96, 24), (82, 24), (76, 24), (72, 24))

# --- 4. B1, the Test Subject chamber (x 41 -> 2) -----------------------------------------------------------
put(40, 43, "c")
L.at("deco", 40, 40, kind="42", text="CHAMBER 42")
L.fill(18, 32, 37, 38, "#")                       # the overhang
L.fill(18, 32, 39, 39, "v")                       # and its spikes
L.at("test_subject", 30, 43, "TS1", carrier=1)
L.at("test_subject", 15, 43, "TS2")
L.fill(9, 9, 28, 40, "#")                         # the alcove's wall
L.at("testgate", 9, 41, "TESTGATE", h=3, opens="TS1,TS2")
L.at("switch", 5, 41, "S_B1", kind="panel")

# --- 5. A1 (x 5 -> 41) ---------------------------------------------------------------------------------
put(30, 2, "h")
L.at("switch", 22, 3, "S_A1", kind="panel")
gem((10, 2), (14, 2), (18, 2), (26, 2), (34, 2), (38, 2))
# Secret 1, the upside-down closet in A1's bottom-right corner.
L.fill(34, 41, 13, 14, "#")                       # its ceiling
L.fill(34, 34, 15, 18, "#")                       # its wall
L.at("testgate", 34, 19, "CLOSET", h=3, hits="S_A1")
L.at("gravzone", None, None, "CLOSET", rect=(35, 15, 41, 21), dir="up", switch="S_CL")
put(38, 15, "2")                                  # letter U
L.at("switch", 40, 16, "S_CL", kind="panel")
put(36, 15, "B")

# --- 6. A2 and A3, the control room spiral (x 44-125) ---------------------------------------------------
put(45, 2, "c")
L.at("switch", 58, 3, "S_A2a", kind="panel")
L.fill(54, 70, 14, 15, "#")                       # LG1
put(62, 13, "T")
put(60, 13, "h")
L.at("switch", 68, 12, "S_A2b", kind="panel")
L.fill(64, 80, 5, 7, "#")                         # the hanging block
put(75, 8, "W")
L.at("gravity_probe", 78, 15, "GP4")
L.at("switch", 80, 9, "S_A2c", kind="panel")
L.at("switch", 98, 20, "S_A3", kind="panel")
L.fill(100, 105, 18, 18, "=")                     # the Nova ledge
gem((100, 17), (101, 16), (102, 16), (103, 16), (104, 16), (105, 17))
# Secret 2: the floor stash behind a wall that is not there.
L.fill(106, 125, 18, 18, "#")
L.fill(106, 106, 19, 21, "#")
L.at("fakewall", 106, 19, h=3)
put(112, 21, "$")
put(114, 21, "m")
put(110, 2, "3")                                  # letter N
put(120, 2, "X")

L.write(__file__, "19_gravity_lab.txt")
