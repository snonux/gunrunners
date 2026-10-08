#!/usr/bin/env python3
"""Level 4 bonus - Echo Room (SPEC.md 04, "Bonus level").

Rule sonar: nothing draws but the runner. Every shot sends out a ring at 2
cells a frame that outlines each solid block it passes for 15 frames (spikes
ping red), and gems chime as you come within 4 blocks. A cave of ledges
zigzags up from the floor (ground 21) to the exit: ground 18, 15, 12, 9 and
6, 3-block steps with gaps of 2 (the top ledge starts at x17, so the jumps
up onto ground 9 and 6 take off clear of it), and two spike troughs, one on
the floor and one on the top ledge. 30 gems along the ledges.

Deviation: the exit stands on the top ledge at (57, 5) (the spec's (57, 3)
would float two blocks above ground 6).
"""

from lib import Level

W, H = 60, 24
L = Level(W, H,
          name="BONUS - ECHO ROOM", episode=1, theme="blackout",
          music="bonus_echo_room", rules="sonar", timer=90, goal="exit")

L.fill(0, W - 1, 0, 0, "#")                  # ceiling
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")
L.ground(1, W - 2, 21)                       # the floor

# Trough 1 in the floor: three blocks wide, spikes at the bottom.
L.clear(20, 22, 21, 22)
L.row(20, 22, 22, "^")

# The ledges, alternating ends, each 3 blocks above the last.
LEDGES = [(46, 58, 18), (30, 43, 15), (15, 27, 12), (2, 14, 9)]
for x0, x1, y in LEDGES:
    L.row(x0, x1, y, "#")
L.fill(17, 58, 6, 7, "#")                    # the top ledge, two rows thick
# Trough 2 on the top ledge.
L.clear(32, 34, 6, 6)
L.row(32, 34, 7, "^")

L.put(2, 20, "P")
L.put(57, 5, "X")

gems = []
gems += [(x, 19) for x in (6, 10, 14, 18, 26, 30, 34, 38)]         # the floor
gems += [(x, 16) for x in (49, 53, 57)]                           # ground 18
gems += [(x, 13) for x in (32, 36, 40)]                           # ground 15
gems += [(x, 10) for x in (16, 20, 24)]                           # ground 12
gems += [(x, 7) for x in (3, 6, 9)]                               # ground 9
gems += [(x, 4) for x in (19, 22, 25, 29, 38, 42, 46, 50, 54, 56)]  # ground 6
assert len(gems) == 30, len(gems)
for x, y in gems:
    L.put(x, y, "g")

L.write(__file__, "04_bonus_echo_room.txt")
