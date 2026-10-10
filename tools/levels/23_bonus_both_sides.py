#!/usr/bin/env python3
"""Level 23 bonus - Both Sides (SPEC.md 23).

rules=split_mirror: two runners on one input. The top one is in the manor
(rows 0-10), the bottom one in its reflection (rows 13-23) with left and
right swapped; rows 11-12 are the divider. Both have to stand on their exits
at once: the top exit (56, 9), the bottom exit (3, 22). goal=exit, timer=90.

Three rooms per half (x 0-19, 20-39, 40-59) whose walls differ: the top
manor has pits through its floor, a raised dais and doorways under hanging
walls; the reflection has a low ceiling, a step and a knee-high wall. A
runner who falls through a pit returns to its half's last floor (`@ twin`).
The bottom runner starts six blocks short of the mirror spot, so it reaches
its exit first and waits there, pressed against the outer wall, for the top
one. 20 gems, ten per half.
"""

from lib import Level

W, H = 60, 24
L = Level(W, H,
          name="BONUS - BOTH SIDES", episode=4, theme="haunted_manor",
          music="theme_theremin", par=90, flags="",
          rules="split_mirror", timer=90, goal="exit")


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y)
        L.put(x, y, "g")


# Shell: ceilings, floors, the divider, the outer walls.
L.fill(0, W - 1, 0, 0, "#")
L.fill(0, W - 1, 10, 12, "#")       # top floor + divider
L.fill(0, W - 1, 13, 13, "#")       # bottom ceiling
L.fill(0, W - 1, 23, 23, "#")       # bottom floor
L.fill(0, 1, 0, H - 1, "#")
L.fill(W - 2, W - 1, 0, H - 1, "#")

# --- Top: the manor (the runner goes east) ---------------------------------------------------------
# Room A: a pit through the floor.
L.clear(8, 9, 10, 12)
gem((5, 9), (8, 7), (14, 8))
# Hanging walls between the rooms, doorways under them.
L.fill(19, 20, 1, 6, "#")
L.fill(39, 40, 1, 6, "#")
# Room B: a dais two rows up, a shelf over the doorway.
L.fill(26, 33, 8, 9, "#")
L.fill(23, 25, 6, 6, "=")
gem((24, 5), (28, 7), (31, 7), (36, 9))
# Room C: two pits.
L.clear(46, 47, 10, 12)
L.clear(51, 52, 10, 12)
gem((44, 9), (46, 7), (54, 9))
L.put(3, 9, "P")
L.put(56, 9, "X")

# --- Bottom: the reflection (the runner goes west) -------------------------------------------------
# Room C' (x 40-57): a low ceiling.
L.fill(44, 50, 14, 19, "#")
gem((42, 21), (47, 22))
L.fill(39, 40, 14, 18, "#")
# Room B' (x 21-38): a step and a shelf.
L.fill(28, 30, 22, 22, "#")
L.fill(25, 27, 19, 19, "=")
gem((22, 21), (26, 18), (29, 20), (33, 22))
L.fill(19, 20, 14, 18, "#")
# Room A' (x 2-18): a knee-high wall.
L.fill(10, 10, 21, 22, "#")
gem((6, 22), (8, 19), (13, 21), (16, 22))
L.put(3, 22, "X")

L.at("twin", 50, 22, top=(56, 9), bottom=(3, 22), divider=11)
L.at("manordeco", kind="chandelier", rect=(12, 2, 14, 3))
L.at("manordeco", kind="chandelier", rect=(45, 15, 47, 16))

L.write(__file__, "23_bonus_both_sides.txt")
