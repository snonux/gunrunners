#!/usr/bin/env python3
"""Level 24 bonus - Turntable (SPEC.md 24).

rules=turntable: a microwave interior that makes one full turn in 60 s
(0.4 degrees a frame) about its centre while gravity keeps pointing down the
screen; the runner and the loose gems slide along whatever is "down".
timer=60, goal=collect:24.

Walls of 1 block; a shelf 4 blocks long on each wall, 6 blocks from a
corner, set round the room like a pinwheel (the spec leaves which corner
open); the turntable plate in the middle (x 8-15, row 16). Each wall has a
grille at x/y 11-12 (`@ grille`) that lets gems out but not the runner.
24 gems: on the two flat shelves, on the plate and along the floor, the
ones left of the floor's grille sliding over it in the first quarter turn.

Change from the spec: the file is 36 x 36 with the 24 x 24 room in the
middle (blocks 6-29), so the room's corners stay inside the map (and the
camera's bounds) all the way round. Coordinates below are the room's own.
"""

from lib import Level

W, H = 24, 24
O = 6  # the room's offset in the map
L = Level(W + 2 * O, H + 2 * O,
          name="BONUS - TURNTABLE", episode=4, theme="tv_kitchen",
          music="theme_accordion", par=60, flags="",
          rules="turntable", timer=60, goal="collect:24")


def fill(x0, x1, y0, y1, c):
    L.fill(x0 + O, x1 + O, y0 + O, y1 + O, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x + O, y + O) == ".", (x, y)
        L.put(x + O, y + O, "g")


fill(0, W - 1, 0, 0, "#")
fill(0, W - 1, H - 1, H - 1, "#")
fill(0, 0, 0, H - 1, "#")
fill(W - 1, W - 1, 0, H - 1, "#")
for rect in ((11, 0, 12, 0), (11, H - 1, 12, H - 1), (0, 11, 0, 12), (W - 1, 11, W - 1, 12)):
    x0, y0, x1, y1 = rect
    L.at("grille", rect=(x0 + O, y0 + O, x1 + O, y1 + O))
fill(1, 4, 6, 6, "#")                  # left wall's shelf
fill(17, 17, 1, 4, "#")                # ceiling's shelf
fill(19, 22, 17, 17, "#")              # right wall's shelf
fill(6, 6, 19, 22, "#")                # floor's shelf
fill(8, 15, 16, 16, "#")               # the turntable plate
L.put(12 + O, 22 + O, "P")
gem((1, 5), (2, 5), (3, 5), (4, 5))
gem((19, 16), (20, 16), (21, 16), (22, 16))
gem(*[(x, 15) for x in range(8, 16)])
gem((2, 22), (3, 22), (4, 22), (8, 22), (16, 22), (18, 22), (20, 22), (21, 22))

L.write(__file__, "24_bonus_turntable.txt")
