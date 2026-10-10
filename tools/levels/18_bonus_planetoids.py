#!/usr/bin/env python3
"""Level 18 bonus - Planetoids (SPEC.md 18).

rules=radial_gravity: each `@ planetoid x y r=` pulls the runner toward its
centre within r + 4 blocks. Walking follows the surface all the way round,
a jump launches along the surface normal as high as a normal jump, and
between the fields the runner drifts in a straight line (lost for 8
seconds, it is beamed back to the last planetoid). goal=collect:5 counts
the alien's five ship parts (`@ part x y`), each on its own planetoid.

Changes from the spec, so it plays in this engine:
- The parts sit 2.5 cells off the surface (where a runner's middle passes).
- No exit: the fifth part ends the level.
"""

import math

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - PLANETOIDS", episode=3, theme="station_hull",
          music="bonus_planetoids", weapon="recoil_cannon", par=60, flags="",
          rules="radial_gravity", timer=120, goal="collect:5")

PLANETS = [(8, 12, 3), (22, 6, 2), (30, 17, 4), (46, 9, 3), (60, 16, 2), (72, 8, 3)]
for x, y, r in PLANETS:
    L.at("planetoid", x, y, r=r)


def around(x, y, r, deg, off):
    """The block `off` cells out from planetoid (x, y, r)'s surface at angle deg (0 up, clockwise)."""
    cx, cy = x * 2 + 1, y * 2 + 1
    a = math.radians(deg)
    px = cx + math.sin(a) * (r * 2 + off)
    py = cy - math.cos(a) * (r * 2 + off)
    return int(round((px - 1) / 2)), int(round((py - 1) / 2))


# The runner starts on top of the first planetoid.
sx, sy = around(*PLANETS[0], 0, 1)
L.put(sx, sy, "P")

# The ship's five parts: on the far sides of the other five.
for (x, y, r), deg in zip(PLANETS[1:], (90, 200, 300, 160, 45)):
    px, py = around(x, y, r, deg, 2.5)
    L.at("part", px, py)

# 20 gems in rings round the fields.
taken = set()
for (x, y, r), degs in zip(PLANETS, ((60, 120, 180, 240), (0, 120, 240), (30, 90, 150, 210, 270),
                                     (0, 90, 180, 270), (60, 300), (120, 240))):
    for d in degs:
        gx, gy = around(x, y, r, d, 5)
        if 0 <= gx < W and 0 <= gy < H and (gx, gy) not in taken and L.get(gx, gy) == ".":
            taken.add((gx, gy))
            L.put(gx, gy, "g")

L.write(__file__, "18_bonus_planetoids.txt")
