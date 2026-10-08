#!/usr/bin/env python3
"""Level 2 bonus - Free Fall (SPEC.md 02, "Bonus level").

Rule freefall: no ground until the net at the bottom. You fall a cell a
frame (half that holding up, two holding down) and steer a cell a frame;
gondolas and window-cleaner ropes bounce you back up and stall the fall.
Seven obstacle bands, offset so a straight drop always hits something, and
50 gems in curving columns between them. 30 seconds; land in the net.
"""

import math

from lib import Level

W, H = 24, 80
L = Level(W, H,
          name="BONUS - FREE FALL", episode=1, theme="glass_canyon",
          music="bonus_free_fall", rules="freefall", timer=30, goal="exit")

L.on(12, 2, "P")

# (gondolas as x0 of a 3-wide car, ropes as x) per band; rows 10, 20, .., 70.
BANDS = [
    ([3, 15], [11]),
    ([8, 19], [3, 14]),
    ([0, 12], [7, 20]),
    ([5, 17], [11]),
    ([9, 20], [2, 15]),
    ([2, 14], [8, 21]),
    ([6, 17], [12]),
]
covered = set()
for i, (cars, ropes) in enumerate(BANDS):
    row = 10 * (i + 1)
    for j, x in enumerate(cars):
        L.at("platform", x, row, "b%d%d" % (i, j), w=3)
        covered.update(range(x, x + 3))
    for x in ropes:
        L.at("rope", x, row - 3, h=6)
        covered.add(x)
# A runner is 1.5 blocks wide: every place to fall through meets something.
for x in range(W - 1):
    assert x in covered or x + 1 in covered, x

# The net.
L.row(0, W - 1, 78, "=")
L.on(12, 78, "X")
L.at("deco", kind="text", x=8, y=76, w=8, h=1, text="SAFETY NET")

# 50 gems in curving columns between the bands.
n = 0
for i in range(7):
    top = 10 * i + 3
    for k in range(7 if i < 6 else 8):
        y = top + k
        x = int(round(11.5 + 8.5 * math.sin((y + i * 3) * 0.35)))
        if L.get(x, y) == ".":
            L.put(x, y, "g")
            n += 1
assert n == 50, n

L.write(__file__, "02_bonus_free_fall.txt")
