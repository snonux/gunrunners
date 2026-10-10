#!/usr/bin/env python3
"""Level 14 bonus - Golden Touch (SPEC.md 14, "Bonus level").

Rule golden_touch: everything the runner touches (stands on, bumps or
shoots) turns gold. Marked blocks (`@ goldfield rect=`, every solid block
in the rect) gild with their neighbours; moving platforms freeze where they
are; the Temple Cats turn into solid gold statues (2 x 2 blocks) to stand
on; a gold door (`@ golddoor`) opens as the runner comes near, unless it
was touched or shot first: then it never opens. With 80 % of the marked
blocks gold the exit gate (`@ goldgate`) opens, unless the runner touched
it first: then it stays shut until the timer runs out (nothing is lost).
90 seconds, 30 gems.

Screen 1 (x 0-26): the floor, and three platforms sliding along their
rows (19, 16, 13); where they freeze makes a stair up to the ledge at
ground 10 over the wall. Screen 2 (x 27-53): drop into the hall past the
first door; four cats on the floor and four on the step at ground 19 turn
into statues, and one on the step is the way up to the ledge at ground 14
and through the second door. Screen 3 (x 54-79): down the steps to the field and the
gate at x 75.

Changes from the spec, so it plays in this engine:
- The ledge is at ground 10, not 8, and the platforms slide sideways at
  fixed heights: every runner can climb 3 blocks a jump.
- The marked blocks are the floors, ledges and steps along the route
  (their top two rows and the faces you bump), 180-odd, not 400: the
  runner cannot reach the rest. The field stops 2 blocks short of the
  gate, so painting it never touches the gate.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - GOLDEN TOUCH", episode=2, theme="gold_sanctum",
          music="bonus_golden_touch", rules="golden_touch", timer=90, goal="paint:80")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, W - 2, 1, 21)              # the hall; floor rows 22-23


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def field(x0, y0, x1, y1):
    L.at("goldfield", rect=(x0, y0, x1, y1))


# --- Screen 1 (x 0-26) -------------------------------------------------------------------
L.put(2, 21, "P")
L.fill(19, 27, 10, 11, "#")           # the ledge, ground 10
L.fill(26, 27, 12, 21, "#")           # its wall, down to the floor
L.at("platform", 5, 19, "P1", w=3, mode="pingpong", path=((5, 19), (9, 19)), speed="1/4")
L.at("platform", 13, 16, "P2", w=3, mode="pingpong", path=((13, 16), (9, 16)), speed="1/4")
L.at("platform", 13, 13, "P3", w=3, mode="pingpong", path=((13, 13), (17, 13)), speed="1/4")
field(1, 22, 25, 23)
field(19, 10, 27, 11)
field(26, 12, 26, 21)
gem((4, 20), (8, 18), (11, 18), (10, 15), (13, 15), (15, 12), (17, 12), (21, 9), (24, 9), (14, 20))

# --- Screen 2 (x 27-53) ------------------------------------------------------------------
L.at("golddoor", rect=(31, 18, 31, 21))
L.fill(44, 49, 19, 21, "#")           # the step, ground 19
L.fill(50, 56, 14, 21, "#")           # the ledge, ground 14
L.at("golddoor", rect=(55, 10, 55, 13))
for x in (34, 37, 40, 42):
    L.at("templecat", x, 21)
for x in (44, 46, 47, 49):
    L.at("templecat", x, 18)
field(28, 22, 43, 23)
field(44, 19, 49, 20)
field(44, 21, 44, 21)
field(50, 14, 56, 15)
field(50, 16, 50, 21)
gem((29, 20), (33, 20), (36, 19), (39, 20), (42, 19), (45, 17), (48, 17), (51, 12), (53, 12), (35, 14))

# --- Screen 3 (x 54-79) ------------------------------------------------------------------
L.fill(57, 60, 17, 21, "#")           # steps down to the field (and back up)
L.fill(61, 64, 19, 21, "#")
L.fill(68, 69, 20, 21, "#")           # a hump in the field
L.at("goldgate", rect=(75, 18, 75, 21))
L.put(77, 21, "X")
field(57, 17, 60, 18)
field(61, 19, 64, 20)
field(65, 22, 72, 23)
field(68, 20, 69, 21)
gem((58, 13), (60, 15), (63, 17), (65, 17), (67, 20), (70, 19), (72, 20), (62, 15), (59, 15), (71, 17))

L.write(__file__, "14_bonus_golden.txt")
