#!/usr/bin/env python3
"""Level 11 bonus - Pinball Mine (SPEC.md 11, "Bonus level").

Rule pinball: the runner is a one-block ball on a mine-themed pinball table.
Jump works the left flipper, fire the right one; either launches the ball
from the plunger (it goes by itself after 45 frames). Five bumpers kick
(500 points), six lanterns light when the ball passes (1000) and with all
six lit the gate at the top opens: roll into it to leave. The drain between
the flippers sends the ball back to the plunger with nothing lost. 90
seconds, 30 gems on the table.

Changes from the spec: the table is 21 x 24 (not 40 x 24) so it is about
one screen wide and the flippers are in view whenever the ball comes down
to them; the camera follows the ball up and down. The ramps are slanted
deflectors in the top corners, the plunger lane is x 18-19 behind the
separator x 17, and everything else is placed to fit: flippers pivot at
(5, 19) and (12, 19), the gate is at (9, 1) between the top lanterns,
and two of the six lanterns hang on the side walls (1, 9) and (16, 9). The
bumpers sit below row 9 so the top of the table is an open lane.
"""

import math

from lib import Level

W, H = 21, 24
L = Level(W, H,
          name="BONUS - PINBALL MINE", episode=2, theme="gold_mines",
          music="bonus_pinball", rules="pinball", timer=90, goal="exit")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, 16, 1, 20)          # the table
L.clear(6, 11, 21, 21)         # under the flippers
L.clear(8, 9, 22, 22)          # the drain
L.clear(17, 17, 1, 5)          # over the separator
L.clear(18, 19, 1, 21)         # the plunger lane

# Under the inlane guides: rock, kept clear of the guide itself (the ball
# would catch on a corner poking through it).
for x in range(1, 6):
    low = 28 + 2 * x                   # the guide's lowest point over block x, in cells
    for y in range(math.ceil(low / 2) + 1, 22):
        L.put(x, y, "#")
for x in range(12, 17):
    low = 62 - 2 * x
    for y in range(math.ceil(low / 2) + 1, 22):
        L.put(x, y, "#")
# The top corners, behind the deflectors.
for x, y in ((1, 1), (2, 1), (1, 2), (19, 1), (18, 1), (19, 2), (17, 1)):
    L.put(x, y, "#")

L.at("pwall", path=[(1, 5), (5, 1)])
L.at("pwall", path=[(20, 5), (16, 1)])
L.at("pwall", path=[(1, 14), (6, 19)])
L.at("pwall", path=[(17, 14), (12, 19)])
L.at("flipper", 5, 19, side="l", len=3)
L.at("flipper", 12, 19, side="r", len=3)
L.at("plunger", 18, 22)
L.at("drain", rect=(6, 21, 11, 22))
L.at("gate", 9, 1)
for x, y in ((3, 4), (6, 2), (12, 2), (15, 4), (1, 9), (16, 9)):
    L.at("lamp", x, y)
for x, y in ((5, 9), (12, 9), (8, 12), (8, 6), (8, 16)):
    L.at("pbumper", x, y)
L.put(9, 1, "X")

gems = [(2, 6), (8, 4), (10, 4), (14, 6), (9, 6), (3, 9), (14, 9), (6, 10), (11, 10), (2, 11),
        (15, 11), (8, 13), (10, 13), (6, 15), (12, 15), (9, 16), (7, 17), (11, 17), (3, 16), (14, 16),
        (18, 6), (18, 10), (18, 14), (18, 18), (9, 8), (2, 3), (15, 2), (6, 5), (12, 5), (9, 19)]
for x, y in gems:
    assert L.get(x, y) == ".", (x, y, L.get(x, y))
    L.put(x, y, "g")

L.write(__file__, "11_bonus_pinball.txt")
