#!/usr/bin/env python3
"""Level 10 - Sun Mirrors (SPEC.md 10). Twist: Light Beams. Prototype: Sunstone Lance.

A tall marble observatory climbed bottom to top: the lower court (start,
bottom left), floor 2 with its mezzanine, the Monk gallery, four dome
floors around a central light well, and the summit with the exit on the
observatory's eye. Floors alternate direction and ladders join them.

Sunbeams come in through roof slits and sun pipes. Mirror statues turn a
step when shot (from the left: anticlockwise, from the right: clockwise)
and send a beam on; a sun door opens after 15 lit frames in a row.

Changes from the spec, so the route plays as intended in this engine:
- The map starts solid and the rooms are carved out of it; the summit
  keeps row 0 as its ceiling.
- Ladder shafts are 2 blocks wide (the ladder in the right column).
- The vault's ladder is at x30 under the cracked disc (not x34, where a
  hole in floor 2 would let you drop in without the secret); the disc
  keeps the ladder's top block until it opens.
- The mezzanine runs to x36 (the step ledge is x37-38) so there is room to
  shoot M4 from its right, and its west end over x1-6 is solid so the room
  behind SD2 can only be reached through SD2.
- The Monk gallery has a one-way walkway (row 55, x6-86) over the floor:
  you cross above the Monks while they patrol below. The second Monk
  patrols x8-38 instead of standing at x75, where his shield would stop the
  reflected beam on its way to SD3; the first starts at x52 and patrols
  x48-70 so he walks into B3 at x60.
- The Nova line's balconies are at x42-46 (the spec's x50-57 hole would cut
  the Monk off from the beam at x60).
- The stone sun SDS is 1 x 4 (rows 2-5) so it stands on the summit floor.
- The cursed mirror and the moths are given by the block they stand in.
"""

from lib import Level

W, H = 100, 90
L = Level(W, H,
          name="STAGE 10 - SUN MIRRORS", episode=2, theme="marble_observatory",
          music="theme_brass_bells", weapon="sunstone_lance", par=420, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def deco(kind, x, y, w=1, h=1, **keys):
    L.at("deco", x, y, kind=kind, w=w, h=h, **keys)


# --- Rooms ----------------------------------------------------------------------------
room(30, 90, 1, 5)        # 8. summit, ground 6
room(10, 90, 7, 13)       # 7. dome floor 4, ground 14
room(10, 90, 15, 23)      # 6. dome floor 3, ground 24
room(92, 98, 19, 23)      # 6a. moon niche (secret 2)
L.clear(91, 91, 21, 23)   # its door SDL
room(10, 90, 25, 33)      # 5. dome floor 2, ground 34
room(10, 98, 35, 43)      # 4. dome floor 1, ground 44
room(1, 98, 47, 57)       # 3. Monk gallery, ground 58
room(1, 98, 62, 71)       # 2. floor 2, ground 72
room(26, 34, 73, 77)      # 2a. vault (secret 1), ground 78
room(1, 97, 79, 85)       # 1. lower court, ground 86

# The light well: every dome floor is a grate from x41 to x59; the summit's
# eye is a grate too.
for row in (14, 24, 34):
    L.row(41, 59, row, "%")
L.row(49, 51, 6, "%")

# Ladders between the floors (shafts 2 blocks wide, the ladder on the right).
def ladder(x, top, bottom, shaft=None):
    if shaft:
        room(x - 1, x, shaft[0], shaft[1])
    L.col(x, top, bottom, "H")


ladder(97, 72, 85, (72, 78))   # court -> floor 2
ladder(3, 58, 65, (58, 61))    # behind SD2 -> gallery
ladder(78, 58, 71, (58, 61))   # the dark passage -> gallery
ladder(95, 44, 57, (44, 46))   # gallery -> dome floor 1
ladder(15, 34, 43, (34, 34))   # dome floor 1 -> 2
ladder(88, 24, 33, (24, 24))   # dome floor 2 -> 3
ladder(12, 14, 23, (14, 14))   # dome floor 3 -> 4
ladder(85, 7, 13)              # dome floor 4 -> under the summit hatch
L.put(85, 6, "H")              # (the hatch HS covers x84-86 of row 6)
L.put(84, 6, ".")
L.put(86, 6, ".")

# --- 1. Lower court (x1-97, ground 86) --------------------------------------------------
L.fill(44, 45, 79, 82, "#")    # the wall; SD1 below it
on(3, 86, "P")
on(12, 86, "W")
L.at("beam", 12, 79, "SLIT", dir=6)  # a decorative slit: a pool of light on the box
L.at("beam", 20, 79, "B1", dir=6)
L.at("mirror", 20, 84, "M1", angle=0)
deco("reflection", 7, 83, 2, 3, text="odd", rect=(7, 83, 8, 85))
deco("42", 33, 85, 3, 1, text="XLII")  # the sundial's 42 hour lines
L.at("sundoor", 44, 83, "SD1", w=2, h=3, latch=1)
on(60, 86, "h")
on(70, 86, "m")
gem((6, 85), (16, 85), (26, 84), (30, 84), (50, 85), (56, 84), (80, 85), (90, 84))

# --- 2. Floor 2 (x1-98, ground 72) --------------------------------------------------------
L.row(1, 6, 66, "#")           # the mezzanine's west end, sealed
L.row(7, 28, 66, "=")          # the mezzanine
L.row(29, 31, 66, "%")         # its grate
L.row(32, 36, 66, "=")
L.row(37, 38, 69, "=")         # the step up to it
L.col(6, 62, 62, "#")          # the wall with SD2
L.at("sundoor", 6, 63, "SD2", latch=1)
on(96, 72, "c")
on(80, 72, "h")
L.at("beam", 70, 62, "B2", dir=6)
L.at("mirror", 70, 70, "M2", angle=4)
L.at("mirror", 30, 70, "M3", angle=2)
L.at("mirror", 30, 64, "M4", angle=6)
L.at("wraith", 50, 68)
on(20, 66, "m")
gem((90, 71), (86, 71), (64, 71), (58, 70), (24, 71), (16, 71), (12, 65), (24, 65), (34, 65), (37, 68))

# 2a. Vault (secret 1): the cracked disc under M3, the ladder under the disc.
L.put(29, 72, ".")
L.put(31, 72, ".")
ladder(30, 72, 77)
L.at("sundoor", 29, 72, "DISC", crack=1)
L.put(28, 77, "3")             # letter N
L.put(31, 75, "B")
gem((26, 77), (27, 76), (29, 77), (33, 77), (34, 77), (26, 74), (27, 74), (32, 73), (33, 73), (34, 74))

# 2b. The dark passage (Rocco line).
L.at("dark", None, None, "PASSAGE", rect=(76, 58, 80, 71))
L.at("wraith", 78, 66)
L.at("wraith", 78, 62)

# 2c. Balconies (Nova line): a hole in the gallery floor and three ledges.
room(42, 46, 58, 61)
L.row(45, 46, 68, "=")
L.row(42, 43, 64, "=")
L.row(45, 46, 60, "=")
gem((45, 67), (42, 63), (46, 59), (44, 61))

# --- 3. Monk gallery (x1-98, ground 58) ---------------------------------------------------
L.col(90, 47, 54, "#")         # the wall; SD3 below it
L.at("sundoor", 90, 55, "SD3", latch=1)
L.row(6, 86, 55, "=")          # the walkway over the Monks
on(5, 58, "c")
on(20, 58, "W")
L.at("beam", 60, 47, "B3", dir=6)
L.at("monk", 52, 57, patrol="48..70")
L.at("monk", 24, 57, patrol="8..38")
on(85, 58, "h")
gem((10, 54), (16, 54), (28, 54), (36, 54), (50, 54), (58, 54), (66, 54), (74, 54), (82, 54), (93, 57))

# --- 4. Dome floor 1 (x10-98, ground 44) --------------------------------------------------
on(92, 44, "c")
L.at("beam", 39, 36, "B4", dir=7)  # the sun pipe
L.at("mirror", 45, 42, "M5", angle=1)
L.at("wraith", 70, 40)
on(25, 44, "h")
gem((80, 43), (74, 42), (64, 43), (30, 43), (20, 42), (12, 43))

# --- 5. Dome floor 2 (x10-90, ground 34) --------------------------------------------------
L.at("cursedmirror", 20, 33, stare=30)
L.at("mirror", 55, 32, "M6", angle=2)
L.put(55, 33, "C")             # in M6's pedestal: only there while M6 is lit
L.at("moths", 50, 30, count=5)
on(75, 34, "W")
L.put(60, 30, "1")             # letter G
gem((24, 33), (32, 33), (40, 32), (66, 33), (80, 33), (86, 32))

# --- 6. Dome floor 3 (x10-90, ground 24) --------------------------------------------------
on(86, 24, "c")
on(80, 24, "T")
L.at("mirror", 45, 22, "M7", angle=6)
L.at("wraith", 30, 20)
L.at("moths", 50, 20, count=5)
L.at("sundoor", 91, 21, "SDL", latch=0)
on(95, 24, "$")
on(97, 24, "m")
gem((70, 23), (62, 22), (36, 23), (22, 22), (93, 23), (94, 22), (96, 22), (97, 21), (93, 20), (98, 22), (16, 23), (56, 23))

# --- 7. Dome floor 4 (x10-90, ground 14) --------------------------------------------------
L.at("mirror", 55, 12, "M8", angle=6)
L.at("wraith", 70, 10)
L.at("monk", 30, 13)
on(40, 14, "h")
L.put(20, 9, "2")              # letter U
gem((16, 13), (26, 13), (46, 13), (64, 13), (76, 13), (82, 12))

# --- 8. Summit (x30-90, ground 6) ----------------------------------------------------------
L.at("sundoor", 84, 6, "HS", w=3, h=1, latch=1)
L.at("sundoor", 45, 2, "SDS", h=4, latch=1, opens="HS")
on(50, 6, "X")
on(88, 6, "Q")
on(65, 6, "m")
gem((34, 5), (38, 4), (56, 5), (60, 4), (70, 5), (74, 4), (78, 5), (82, 4))

L.write(__file__, "10_sun_mirrors.txt")
