#!/usr/bin/env python3
"""Level 48 - Bounder Plains (docs/DEEP_SPACE.md). Twist: Bounders. Prototype: Tamer's Whip.

Episode 7, DEEP SPACE, west to east across the thorny violet plains of Vurr
under its two moons. Thorn grass (`^`) hurts on foot; a Bounder runs over
it. `@ bounder x y` is a Bounder standing in its pen: walk up and press USE
(or land on its back) to ride it; hold jump for a hop twice as high as
yours; USE or down + jump gets off. It won't fit a low tunnel or climb a
ladder, and one left behind trots back to its pen. The Tamer's Whip's crack
dazes aliens and calls any Bounder within 16 blocks to you. `bot=1` and
`drop=` are for the autopilot: where it climbs off.

Shape: on foot to the first pen (the whip on the way); on a Bounder over
the thorn fields, a mesa, a gully and a rock wall, Thorn Hogs and Sky
Gulpers on the way; off it at a low tunnel and on foot through; a second
pen past the tunnel and a second ride over high ledges and gullies; off at
the foot of the last cliff and up its ladder to the exit.

Secrets: a high ledge only a Bounder's hop reaches (gems, merch, the 42);
a crawlway under the tunnel only reachable on foot (the rubber duck). The
Virus: the green thornbush up on the first mesa.
"""

from lib import Level

W, H = 240, 36
G = 28  # the plains' ground row
L = Level(W, H,
          name="STAGE 48 - BOUNDER PLAINS", episode=7, theme="bounder_plains",
          music="theme_bounder_plains", weapon="tamers_whip", par=240, flags="")

L.ground(0, W - 1, G)
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def thorns(x0, x1, g=G):
    L.row(x0, x1, g, "^")


def mesa(x0, x1, top):
    L.ground(x0, x1, top)


# --- A: on foot to the first pen (x 1-35) ----------------------------------------------------
on(3, G, "P")
on(8, G, "h")
on(12, G, "W")                  # the Tamer's Whip
L.at("thorn_hog", 18, G - 1)
L.row(14, 16, G - 3, "=")        # a log to jump up on: the letter G on it
on(15, G - 3, "1")
on(22, G, "c")
gem((10, G - 3), (20, G - 4), (24, G - 3))
# Pen 1: Bounder A.
L.at("bounder", 28, G - 1, bot=1, drop=(114, G - 1))
on(32, G, "m")

# --- B: the first ride (x 36-119) -------------------------------------------------------------
thorns(36, 49)
gem((40, G - 5), (45, G - 7))
# The mesa: five blocks up, a hop for a Bounder. The Virus up on it.
mesa(50, 58, G - 5)
L.at("thornbush", 56, G - 6, carrier=1)
gem((52, G - 8))
thorns(59, 71)
L.at("thorn_hog", 63, G - 1)
L.at("thorn_hog", 69, G - 1)
L.at("sky_gulper", 66, G - 7)
gem((61, G - 6), (67, G - 10))
# Secret: the ledge only a Bounder's hop reaches (eight blocks up).
LEDGE = G - 8
L.fill(72, 78, LEDGE, LEDGE + 1, "#")
on(73, LEDGE, "m")
L.at("deco", 75, LEDGE - 1, kind="42", text="XLII", w=1, h=2)
gem((74, LEDGE - 2), (76, LEDGE - 2), (77, LEDGE - 1))
# The gully: four blocks down, thorns at the bottom.
L.clear(82, 92, G, G + 3)
thorns(82, 92, G + 4)
L.at("thorn_hog", 87, G + 3)
gem((87, G - 3))
# A rock wall, six blocks high.
mesa(98, 100, G - 6)
gem((99, G - 9))
on(104, G, "c")
thorns(106, 110)
L.at("thorn_hog", 108, G - 1)
L.at("sky_gulper", 103, G - 8)
on(116, G, "h")

# --- C: on foot through the low tunnel (x 120-140) --------------------------------------------
# Three blocks high: room for a runner, none for a Bounder.
L.fill(120, 140, G - 12, G - 4, "#")
gem((123, G - 2), (127, G - 2), (135, G - 2))
on(125, G, "2")                 # letter U
L.at("thorn_hog", 137, G - 1)
# Secret: a crawlway under the tunnel floor, down a hole and a ladder just
# outside its mouth (a Bounder steps over the hole; it won't go down).
L.clear(117, 134, G + 1, G + 3)
L.clear(117, 118, G, G)
L.col(118, G, G + 3, "H")
on(132, G + 4, "Q")
on(133, G + 4, "m")
gem((124, G + 2), (128, G + 2))

# --- D: the second ride (x 141-238) -----------------------------------------------------------
on(143, G, "c")
L.at("bounder", 146, G - 1, bot=1, drop=(220, G - 1))
thorns(151, 165)
L.at("thorn_hog", 158, G - 1)
L.at("sky_gulper", 160, G - 8)
gem((155, G - 6), (163, G - 8))
# High ground: six blocks up.
HI = G - 6
mesa(166, 185, HI)
L.at("thorn_hog", 175, HI - 1)
on(180, HI, "3")                # letter N
gem((170, HI - 4), (184, HI - 6))
# A deep gully, thorns in it.
L.clear(186, 196, HI, G + 1)
thorns(186, 196, G + 2)
gem((191, HI - 2))
MID = G - 4
mesa(197, 210, MID)
thorns(200, 206, MID)
L.at("sky_gulper", 203, MID - 7)
gem((198, MID - 5), (208, MID - 5))
on(214, G, "m")
on(218, G, "h")
# The last cliff, twelve blocks: up its ladder on foot to the exit.
TOP = G - 12
mesa(226, W - 2, TOP)
L.col(225, TOP, G - 1, "H")
on(230, TOP, "g")
on(235, TOP, "X")

L.write(__file__, "48_bounder_plains.txt")
