#!/usr/bin/env python3
"""Level 21 bonus - Wireframe (SPEC.md 21).

rules=wireframe: ZERO's debug view. Every `#` inside the frame is drawn as
a green wireframe and is passable; the pink `@ hitbox rect=` outlines are
the only solid ground (with the frame's floor). Normal gravity and jumps.
timer=90, goal=collect:40.

Three screens, x 1-26, 27-52, 53-78: each a wall mass the runner falls
straight through, with a staircase of hitbox platforms 4-5 blocks wide, 3
blocks up from one to the next and never more than 2 blocks apart. 48 gems,
two over every platform and the rest down by the floor, all inside what
used to be solid wall.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - WIREFRAME", episode=3, theme="station_servers",
          music="bonus_wireframe", par=90, flags="",
          rules="wireframe", timer=90, goal="collect:40")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, W - 2, 1, H - 2)

# The wall masses (x0, x1, y0, y1): they look like a level, but none of it
# is there.
WALLS = [
    (3, 9, 12, 22), (14, 20, 3, 11), (21, 25, 13, 22), (10, 13, 18, 22),
    (28, 33, 15, 22), (36, 44, 4, 12), (45, 51, 14, 22), (34, 39, 18, 22),
    (54, 60, 3, 10), (61, 66, 14, 22), (70, 77, 6, 13), (55, 59, 17, 22), (71, 77, 18, 22),
]
for x0, x1, y0, y1 in WALLS:
    L.fill(x0, x1, y0, y1, "#")

gems = []


def platform(x0, x1, row):
    """A hitbox platform (its top row `row`) with two gems over it."""
    L.at("hitbox", None, None, rect=(x0, row, x1, row))
    L.fill(x0, x1, row, row, "#")
    gems.append((x0 + 1, row - 1))
    gems.append((x1 - 1, row - 2))


# Each screen: four platforms up to the right, then two back to the left
# over the top. The way back is offset so nothing hangs 6 blocks over the
# edge a jump leaves from (a runner's head would hit it), and nothing hangs
# low over the floor (a runner is 5 cells tall).
for s in (1, 27, 53):
    for x0, x1, row in ((s + 2, s + 6, 20), (s + 8, s + 12, 17), (s + 14, s + 18, 14),
                        (s + 20, s + 24, 11), (s + 14, s + 17, 8), (s + 8, s + 12, 5)):
        platform(x0, x1, row)
# Down by the floor, in the low walls (never under a bottom platform).
gems += [(10, 22), (14, 22), (21, 22), (25, 22), (27, 22), (36, 22), (42, 22), (48, 22),
         (53, 22), (62, 22), (68, 22), (76, 22)]

L.put(1, 22, "P")
for x, y in gems:
    L.put(x, y, "g")

L.write(__file__, "21_bonus_wireframe.txt")
