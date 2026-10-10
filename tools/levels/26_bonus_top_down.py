#!/usr/bin/env python3
"""Level 26 bonus - Top-Down Rampage (SPEC.md 26). rules=topdown: the camera
looks straight down on a grid city; the runner walks eight ways, every model
building breaks when walked into, and three Model Tank squads drive the
streets firing shells. Damage ratings (houses 200, blocks 500) count toward
goal=score:20000 within 60 seconds.

The grid: streets 3 blocks wide every 6 blocks, 30 city blocks of 3 x 3,
each with a 2 x 2 office block and three 1 x 1 houses (120 buildings) and
two park squares; every third park square holds a gem (20).
"""

from lib import Level

W, H = 64, 24
L = Level(W, H, name="BONUS - TOP-DOWN RAMPAGE", episode=4, theme="model_city",
          music="theme_monster_brass", rules="topdown", timer=60, goal="score:20000")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.row(0, W - 1, 0, "#")
L.row(0, W - 1, H - 1, "#")
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")

put(2, 9, "P")
n = 0
gems = 0
square = 0
for by in (4, 10, 16):
    for bx in range(4, 59, 6):
        L.at("building", None, None, "b%d" % n, rect=(bx, by, bx + 1, by + 1), kind="block", rating=500)
        n += 1
        for dx, dy in ((2, 0), (0, 2), (2, 2)):
            L.at("building", None, None, "b%d" % n, rect=(bx + dx, by + dy, bx + dx, by + dy), kind="house", rating=200)
            n += 1
        # The park squares: (bx + 1, by + 2) and (bx + 2, by + 1).
        for px, py in ((bx + 1, by + 2), (bx + 2, by + 1)):
            if square % 3 == 0:
                put(px, py, "g")
                gems += 1
            square += 1
assert n == 120 and gems == 20, (n, gems)

# Three squads of four on the east-west streets.
for i, row in enumerate((2, 14, 20)):
    L.at("squad", None, None, "sq%d" % (i + 1), kind="model_tank", count=4, rect=(16, row, 60, row))

L.write(__file__, "26_bonus_top_down.txt")
