#!/usr/bin/env python3
"""Level 30 bonus - Bubble Up (SPEC.md 30). rules=bubble: the runner rides
a bubble up a vertical coral maze, half a cell a frame; a tap of jump sinks
it two cells, left and right move it half a cell a frame. Touching coral
spikes pops it: back at the last bubble ring passed (rows 60, 40, 20),
nothing lost. timer=75, goal=exit, start (12, 77), exit (12, 1).

Ten coral shelves, each with a gap to steer to before the spikes hanging
under it; 24 gems in eight pockets in the side walls, each a lip just a
runner high, so the bubble has to be held level (tapping jump) to slip in;
one more gem under the exit.

The file is 24 x 80 (the 80 x 24 bonus cap on its side). Everything in
world_trench.cpp.
"""

from lib import Level

W, H = 24, 80
L = Level(W, H, name="BONUS - BUBBLE UP", episode=5, theme="deep_sea", music="theme_submerged",
          rules="bubble", timer=75, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, H - 1, "#")
L.clear(3, 20, 1, 77)                       # the shaft (coral walls at 0-2 and 21-23)

# The side pockets: a lip, three rows open, two blocks deep.
POCKETS = ((1, 73), (21, 58), (1, 51), (21, 44), (1, 30), (21, 23), (1, 9), (21, 2))


def by_mouth(x, y):
    # No spikes hang on a pocket's lip row within 4 blocks of its mouth: a
    # bubble held level at the lip has to slip in without touching them.
    return any(y == r and (x <= 6 if px == 1 else x >= 17) for px, r in POCKETS)


# The shelves: (row, gap x0, gap x1); spikes hang under them away from the gap.
SHELVES = ((72, 4, 7), (65, 15, 18), (57, 9, 12), (50, 4, 7), (43, 15, 18), (36, 8, 11), (29, 14, 17),
           (22, 4, 7), (15, 15, 18), (8, 9, 12))
for row, g0, g1 in SHELVES:
    for x in range(3, 21):
        if g0 <= x <= g1:
            continue
        L.put(x, row, "#")
        if (x < g0 - 2 or x > g1 + 2) and L.get(x, row + 1) == "." and not by_mouth(x, row + 1):
            L.put(x, row + 1, "v")

# Coral spike clusters in the open water.
for x, y in ((8, 68), (9, 68), (12, 46), (13, 46), (16, 33), (17, 33), (7, 18), (8, 18)):
    put(x, y, "^")

# The pockets' gems: two at the back, one by the mouth.
for x, r in POCKETS:
    L.clear(x, x + 1, r + 1, r + 3)
    inner = x if x == 1 else x + 1
    outer = x + 1 if x == 1 else x
    put(inner, r + 2, "g")
    put(outer, r + 2, "g")
    put(outer, r + 3, "g")
put(12, 4, "g")

for row in (60, 40, 20):
    L.at("ring", 12, row)
put(12, 77, "P")
put(12, 1, "X")

L.write(__file__, "30_bonus_bubble_up.txt")
