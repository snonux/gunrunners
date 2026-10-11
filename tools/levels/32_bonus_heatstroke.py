#!/usr/bin/env python3
"""Level 32 bonus - Heatstroke (SPEC.md 32). rules=swap_lr: every 150
frames left and right swap (the screen wobbles and a swap sign blinks for
the 22 frames before). timer=60, goal=exit.

Dunes at g20, three fixed dishes (rims at g17, g14, g17) and a mast with
the exit at (77, 13). Two harmless Vulture Drones drop scrap that knocks
the runner back 2 blocks. 20 gems on the rims.

Changes from the spec, so it plays in this engine: the rims are one-way
dish rims with a strut beside the middle dish (6 up from the dunes is too
high) and the third rim runs on to the mast.

Everything in world_dish.cpp.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - HEATSTROKE", episode=5, theme="desert_array", music="theme_desert_slide",
          rules="swap_lr", timer=60, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 20, H - 1, "#")              # the dunes (g20)
L.row(16, 24, 17, "=")                        # dish 1's rim (g17)
L.row(30, 33, 17, "=")                        # dish 2's strut (g17)
L.row(36, 44, 14, "=")                        # dish 2's rim (g14)
L.row(56, 70, 17, "=")                        # dish 3's rim (g17) on to the mast
L.fill(72, 79, 14, 19, "#")                   # the mast (g14)
put(2, 19, "P")
put(77, 13, "X")
L.at("vulture_drone", 30, 4, zone=(10, 40))
L.at("vulture_drone", 60, 5, zone=(45, 70))
for x in (17, 19, 21, 23, 37, 39, 41, 43, 44, 36, 57, 59, 61, 63, 65, 67, 69, 31, 33, 74):
    put(x, 16 if x < 25 or (x >= 56 and x <= 70) or x in (31, 33) else 13, "g")

L.write(__file__, "32_bonus_heatstroke.txt")
