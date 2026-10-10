#!/usr/bin/env python3
"""Level 44 - Crash Garden (docs/DEEP_SPACE.md). Twist: Goo Walls. Prototype: Goo Gun.

Episode 7, DEEP SPACE, on the planet Vurr. Mostly vertical, left to right:
the crash site (start, top left) with one short goo wall, the drop into the
glowing garden, the first goo chimney up to the upper garden, the shaft
down to the lower canopy, the second goo chimney up to the canopy top and
the exit (top right).

Goo (`@ goo rect=`) coats the solid blocks in the rect. Jump into a goo wall
holding toward it and you stick to it, sliding slowly down (up holds you,
down slides fast, away lets go); jump kicks you off it, three cells away
from the wall and then a full jump, so you either steer back to the same
wall a jump higher or reach the other wall of a chimney. The Goo Gun's
blobs splat on any wall as goo for 10 seconds, and glue an alien's feet
for 3 seconds.

Secrets: the dry cliff over the shaft (x 120-129) has a shelf on top that
only Goo Gun patches reach (the gem cache and the 42); a goo pillar hangs
from the garden's ceiling (x 50-51) up into a hidden pocket with the
rubber duck.
"""

from lib import Level

W, H = 180, 50
L = Level(W, H,
          name="STAGE 44 - CRASH GARDEN", episode=7, theme="alien_garden",
          music="theme_crash_garden", weapon="goo_gun", par=300, flags="")

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


def goo(x0, y0, x1, y1):
    L.at("goo", rect=(x0, y0, x1, y1))


# --- 1. The crash site (x 1-30, ground 18) -------------------------------------------------
room(1, 30, 4, 17)
L.ground(18, 30, 13)           # the step: five blocks up, goo on its face
goo(18, 13, 18, 17)
on(3, 18, "P")
on(7, 18, "h")
on(10, 18, "W")
on(14, 18, "m")
on(28, 13, "m")
gem((12, 15), (13, 15), (16, 12), (17, 10), (21, 10), (24, 10))

# --- 2. The drop and the garden (x 31-81, ground 38) -----------------------------------------
room(31, 36, 4, 37)            # the drop
room(31, 81, 22, 37)           # the garden, its ceiling row 21
on(40, 38, "c")
on(42, 38, "h")
for x0, x1, y in ((44, 48, 33), (58, 63, 33), (66, 71, 29), (74, 78, 33)):
    L.row(x0, x1, y, "=")      # mushroom caps
L.put(64, 30, "1")             # letter G
L.put(70, 26, "V")             # spores off the green cap
gem((45, 31), (47, 31), (59, 31), (62, 31), (67, 27), (75, 31), (77, 31))
L.at("skitter", 50, 37)
L.at("gloop", 56, 37)
L.at("spitpod", 68, 28)
L.at("skitter", 76, 37, dir="r")
on(46, 38, "m")

# 2a. Secret: a goo pillar hangs from the ceiling into a hidden pocket.
L.fill(50, 51, 22, 33, "#")
room(52, 58, 16, 20)
L.clear(52, 53, 21, 21)        # the way up beside the pillar
goo(51, 16, 51, 33)
on(56, 21, "Q")
on(54, 21, "m")
gem((55, 18), (57, 18))

# --- 3. Goo chimney 1 (x 82-89, from ground 38 up to row 14) ---------------------------------
room(82, 87, 34, 37)           # the way in under its west wall
room(84, 87, 4, 33)            # the chimney
goo(83, 14, 83, 33)
goo(88, 14, 88, 37)
L.at("skitter", 85, 37)
gem((85, 30), (86, 26), (85, 22), (86, 18))

# --- 4. The upper garden (x 88-115, ground 14) ------------------------------------------------
room(88, 115, 4, 13)
on(92, 14, "c")
on(94, 14, "h")
on(97, 14, "W")
L.row(100, 104, 10, "=")
L.put(102, 7, "2")             # letter U
L.at("gloop", 99, 13)
L.at("spitpod", 106, 13)
L.at("skitter", 111, 13)
on(108, 14, "m")
gem((101, 8), (103, 8), (90, 11), (113, 11))

# 4a. Secret: the dry cliff over the shaft; Goo Gun patches up its face.
room(120, 129, 4, 7)           # the shelf (on blocks 120-129, row 8)
on(124, 8, "$")
L.at("deco", 127, 6, kind="42", text="XLII", w=1, h=2)
on(121, 8, "m")

# --- 5. The shaft and the lower canopy (x 116-137, ground 30) ---------------------------------
room(116, 119, 4, 29)
room(116, 137, 20, 29)
on(122, 30, "c")
on(124, 30, "h")
L.at("gloop", 128, 29)
L.at("gloop", 133, 29, dir="r")
gem((117, 17), (118, 22), (126, 26), (131, 26), (135, 26))

# --- 6. Goo chimney 2 (x 138-145, from ground 30 up to row 8) ---------------------------------
room(138, 139, 26, 29)
room(140, 144, 2, 29)
goo(139, 8, 139, 25)
goo(145, 8, 145, 29)
L.at("skitter", 142, 29)
gem((141, 22), (143, 18), (141, 14), (143, 10))

# --- 7. The canopy top and the exit (x 145-178, ground 8) -------------------------------------
room(145, 178, 2, 7)
on(148, 8, "c")
on(150, 8, "T")
on(153, 8, "W")
L.put(160, 4, "3")             # letter N
L.at("gloop", 158, 7)
L.at("spitpod", 165, 7)
L.at("skitter", 170, 7)
on(156, 8, "h")
on(167, 8, "m")
on(175, 8, "X")
gem((152, 5), (162, 5), (168, 5), (172, 5))

L.write(__file__, "44_crash_garden.txt")
