#!/usr/bin/env python3
"""Level 9 - Hall of Traps (SPEC.md 09). Twist: Your Traps Too. Prototype: Snare Bolas.

A temple hub. The entrance hall (start, top middle) sits over the hub; the
west wing (blades), the east wing (stones) and the deep wing (a collapsing
floor over spikes) open off it, each with a stone key at its far end. The
key door at the entrance hall's east end takes all three and opens on the
final hall, where spikes and blades run on a 60-frame cycle, and the exit.

Every trap fires from a plate (a red glyph in the floor), and plates fire
for any weight: the runner, a Stone Guardian, a beetle. Traps hurt
whatever is in their path, so a Guardian walking over a plate can do the
job for you.

Changes from the spec, so the route plays as intended in this engine:
- The map starts solid and the rooms are carved out of it.
- Ladder shafts are 2 blocks wide (the ladder in the right column) and the
  floor holes at the top of the deep wing's pit ladders are open.
- The east stairs are one-way stone ledges, so the lower floor runs under
  them to the foot of the stairs (x136-138).
- The west lower hall reaches x8, where the key room's ladder comes up.
- Dart Faces hang in the row under the ceiling (row 39) so they can be shot;
  the candid camera is in the silent face's mouth, one row below it.
- Catch slots are drawn at the grooves' ends rather than cut into the floor,
  except the east upper floor's (x125-126, the top of the stairs).
"""

from lib import Level

W, H = 140, 60
L = Level(W, H,
          name="STAGE 9 - HALL OF TRAPS", episode=2, theme="sandstone_traps",
          music="theme_creeping_drums", weapon="snare_bolas", par=420, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def deco(kind, x, y, w=1, h=1, **keys):
    L.at("deco", x, y, kind=kind, w=w, h=h, **keys)


def torch(x, y):
    deco("torch", x, y, 1, 2)


# --- Rooms ----------------------------------------------------------------------------
room(56, 84, 1, 11)        # 1. entrance hall, ground 12
room(44, 54, 6, 11)        # 1a. glyph room (secret 1)
L.clear(55, 55, 9, 11)     # its hidden door (a plate closes it again at load)
room(56, 84, 14, 25)       # 2. hub, ground 26
room(46, 54, 15, 19)       # 2a. incense niche (secret 2), ground 20
room(32, 54, 21, 25)       # 3. west wing: blade hall, ground 26
L.clear(55, 55, 23, 25)    # west door
room(30, 31, 21, 32)       # the hole to the lower hall
room(8, 29, 21, 23)        # 3a. blade gauntlet, ground 24
room(1, 7, 21, 37)         # its drop shaft into the key room
room(8, 31, 27, 32)        # 3b. west wing: lower hall, ground 33
room(1, 9, 33, 37)         # 3c. west key room, ground 38
room(86, 124, 18, 25)      # 4. east wing: upper floor, ground 26
L.clear(85, 85, 23, 25)    # east door
room(125, 138, 18, 26)     # catch slot (ground 27) and the stairwell
room(127, 138, 27, 37)
room(86, 138, 33, 37)      # 4b/4c. east wing: lower floor and key room, ground 38
room(86, 87, 26, 32)       # ladder shaft between the floors
room(20, 70, 39, 47)       # 5. deep wing, ground 48, ceiling 38
room(30, 59, 49, 57)       # the pit under the collapse floor, ground 58
room(65, 66, 26, 38)       # hub floor hole and shaft down to the deep wing
room(86, 138, 1, 11)       # 6. final hall, ground 12
room(81, 82, 12, 13)       # entrance hall floor hole down to the hub

# --- 1. Entrance hall (x56-84) ---------------------------------------------------------
L.col(82, 12, 25, "H")
L.on(57, 12, "P")
deco("skeleton", 58, 11)
L.on(60, 12, "W")
L.on(62, 12, "h")
L.on(84, 12, "c")
torch(59, 7)
torch(76, 7)
L.at("plate", 63, 12, "PL0")
L.at("trap", None, None, "RS0", kind="stone", groove="66..80", row=11, dir="l", plate="PL0")
L.at("guardian", 70, 11, sentry=1, dir="l")
L.at("keydoor", 85, 9, keys=3, h=3)
deco("glyph", 84, 6, 1, 2, text="3 KEYS")
gem((67, 11), (69, 11), (72, 11), (74, 11), (76, 11), (78, 11))

# --- 1a. Glyph room (secret 1) -----------------------------------------------------------
L.put(49, 10, "2")                           # letter U
L.on(52, 12, "$")
L.on(46, 12, "m")
torch(50, 7)

# --- 2. Hub (x56-84) -------------------------------------------------------------------
L.row(59, 61, 23, "=")
L.row(56, 58, 20, "=")
L.col(66, 26, 47, "H")
L.at("breakable", rect=(55, 17, 55, 19), hp=3, by="any", look="mark")
L.on(60, 26, "c")
L.on(64, 26, "h")
L.at("plate", 70, 26, "DP")
L.at("secret", None, None, rect=(55, 9, 55, 11), plate="DP", presses=1)
L.at("wake", None, None, plate="DP", presses=3)
L.put(72, 23, "B")
L.put(60, 22, "1")                           # letter G
L.at("deco", 70, 20, kind="42", w=2, h=1, text="IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII")
deco("glyph", 56, 21, 1, 1, text="I")
deco("glyph", 84, 21, 1, 1, text="II")
deco("glyph", 65, 23, 2, 1, text="III")
torch(62, 18)
torch(78, 18)
gem((57, 19), (61, 22), (74, 25), (76, 25), (78, 25), (80, 25))

# --- 2a. Incense niche (secret 2) --------------------------------------------------------
L.on(50, 20, "$")
L.on(48, 20, "m")
gem((47, 19), (49, 19), (51, 19), (52, 18), (53, 19), (54, 19))

# --- 3. West wing: blade hall (x32-54) -------------------------------------------------
L.col(31, 26, 32, "H")
L.on(32, 26, "W")
L.at("guardian", 40, 25, patrol="33..53")
L.at("plate", 38, 26, "PW1")
L.at("plate", 48, 26, "PW2")
L.at("trap", 38, 26, "BW1", kind="blade", plate="PW1")
L.at("trap", 48, 26, "BW2", kind="blade", plate="PW2")
torch(44, 22)
gem((35, 25), (42, 25), (44, 25), (46, 25), (51, 25), (53, 25))

# --- 3a. Blade gauntlet (x8-29, the Rocco line) ----------------------------------------
for i, x in enumerate((12, 16, 20, 24, 27)):
    L.at("trap", x, 24, "BG%d" % (i + 1), kind="blade", cycle=30, phase=i * 6)
L.put(18, 23, "Q")                           # the duck, in an explorer's fedora
gem((10, 23), (14, 23), (22, 23), (29, 23))

# --- 3b. West wing: lower hall (x8-31) -------------------------------------------------
L.at("guardian", 20, 32)
L.at("plate", 16, 33, "PW3")
L.at("plate", 24, 33, "PW4")
L.at("trap", 16, 33, "BW3", kind="blade", plate="PW3")
L.at("trap", 24, 33, "BW4", kind="blade", plate="PW4")
L.on(12, 33, "m")
torch(28, 29)
gem((10, 32), (14, 32), (19, 32), (21, 32), (27, 32), (29, 32))

# --- 3c. West key room (x1-9) ----------------------------------------------------------
L.col(9, 33, 37, "H")
L.at("stonekey", 4, 37, "K1")
L.on(7, 38, "c")
L.on(2, 38, "h")
gem((1, 37), (3, 37), (5, 37), (6, 34))

# --- 4. East wing: upper floor (x86-124) -------------------------------------------------
L.col(87, 26, 37, "H")
L.row(104, 106, 23, "=")                     # the niche ledge: the stone rolls under it
L.on(95, 26, "W")
L.at("plate", 100, 26, "PL1")
L.at("trap", None, None, "RS1", kind="stone", groove="88..124", row=25, dir="r", plate="PL1")
L.at("scarabs", 115, 25, count=12, carrier=1)
L.on(124, 26, "h")
torch(98, 20)
torch(118, 20)
gem((92, 25), (97, 25), (102, 25), (105, 22), (108, 25), (110, 25), (112, 25), (114, 25))

# --- 4a. Stairs (x125-138) ---------------------------------------------------------------
L.ground(125, 126, 27, 32)                   # the catch slot at the top
L.row(127, 129, 29, "=")
L.row(130, 132, 32, "=")
L.row(133, 135, 35, "=")
L.on(128, 29, "m")

# --- 4b. East wing: lower floor (x88-138) ------------------------------------------------
L.at("plate", 121, 38, "PL2")
L.at("trap", None, None, "RS2", kind="stone", groove="98..123", row=37, dir="r", plate="PL2")
for x in (112, 115, 118):
    L.at("guardian", x, 37, dir="r")
L.at("scarabs", 100, 37, count=8)
torch(110, 34)
gem((101, 35), (105, 35), (109, 37), (126, 37), (128, 37), (130, 37), (132, 37), (134, 37))

# --- 4c. East key room (x88-97) ----------------------------------------------------------
L.at("stonekey", 90, 37, "K2")
L.on(93, 38, "T")
L.on(95, 38, "c")

# --- 5. Deep wing (x20-70) -------------------------------------------------------------
L.row(32, 57, 57, "^")
L.col(31, 48, 57, "H")
L.col(59, 48, 57, "H")
L.clear(30, 30, 48, 48)
L.clear(58, 58, 48, 48)
for x0 in (32, 39, 46, 53):
    L.at("collapse", rect=(x0, 48, x0 + (5 if x0 < 53 else 4), 48))
for x0 in (32, 38, 44, 50, 56):
    L.row(x0, x0 + 2, 44, "=")               # wall carvings to hop up on
L.on(62, 48, "W")
L.at("dartface", 35, 39, phase=0)
L.at("dartface", 49, 39, phase=10)
L.at("dartface", 56, 39, phase=20)
L.at("dartface", 42, 39, silent=1)
L.put(42, 40, "C")
torch(64, 42)
gem((33, 47), (36, 47), (39, 47), (41, 47), (43, 47), (47, 47), (49, 47), (51, 47), (54, 47), (56, 47))

# --- 5a. Deep key room (x20-30) ----------------------------------------------------------
L.at("stonekey", 22, 47, "K3")
L.on(24, 48, "T")
L.on(26, 48, "h")
L.on(28, 48, "c")
torch(21, 42)

# --- 6. Final hall (x86-138) ---------------------------------------------------------------
for x0 in (92, 102, 112, 122):
    L.fill(x0, x0 + 3, 9, 11, "#")           # ledges, ground 9
for i, x in enumerate((87, 88, 89, 90)):
    L.at("plate", x, 12, "D%d" % (i + 1))
L.at("drums", plates="D1,D2,D3,D4")
for i, x0 in enumerate((96, 106, 116)):
    L.row(x0, x0 + 5, 12, "#")
    L.at("trap", None, None, "SF%d" % (i + 1), kind="spikes", rect=(x0, 11, x0 + 5, 11), cycle=60, phase=0,
         tell=12, active=16, start=92)
for i, x0 in enumerate((92, 102, 112, 122)):
    L.at("trap", None, None, "BF%d" % (i + 1), kind="blade", rect=(x0, 6, x0 + 3, 8), cycle=60, phase=30,
         tell=12, active=8, start=92)
L.on(88, 12, "h")
L.on(103, 9, "m")
L.put(123, 5, "3")                           # letter N
L.on(135, 12, "X")
torch(90, 4)
torch(130, 4)
gem((93, 8), (94, 8), (98, 11), (100, 11), (104, 8), (108, 11), (110, 11), (113, 8), (114, 8), (118, 11),
    (120, 11), (124, 8))

L.write(__file__, "09_hall_of_traps.txt")
