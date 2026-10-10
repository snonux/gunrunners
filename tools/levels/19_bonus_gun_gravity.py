#!/usr/bin/env python3
"""Level 19 bonus - Gun Gravity (SPEC.md 19).

rules=gun_gravity: down is wherever the runner's last shot went (the four
straight ways; walking and jumping work along the current floor, the fall is
the usual one). One closed room with five floating blocks; goal=collect:30
gathers 30 of the gems on the four walls and the blocks' faces.

Changes from the spec, so it plays in this engine:
- The bonus has no prototype: the blaster's shots turn gravity.
"""

from lib import Level

W, H = 48, 24
L = Level(W, H,
          name="BONUS - GUN GRAVITY", episode=3, theme="station_gravlab",
          music="bonus_gun_gravity", par=60, flags="",
          rules="gun_gravity", timer=60, goal="collect:30")

L.fill(0, W - 1, 0, 0, "#")
L.fill(0, W - 1, H - 1, H - 1, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
BLOCKS = [(10, 6), (24, 12), (36, 5), (14, 17), (34, 18)]
for bx, by in BLOCKS:
    L.fill(bx, bx + 2, by, by + 1, "#")
L.put(4, 22, "P")

gems = []
gems += [(x, 22) for x in (9, 20, 28, 40)]           # the floor
gems += [(x, 1) for x in (6, 17, 29, 43)]            # the ceiling
gems += [(1, y) for y in (5, 11, 17)]                # the left wall
gems += [(46, y) for y in (4, 10, 16, 21)]           # the right wall
for bx, by in BLOCKS:                                # every block: over, under and one end
    gems += [(bx + 1, by - 1), (bx + 1, by + 2), (bx + 3, by)]
gems.append((24, 3))
for x, y in gems:
    assert L.get(x, y) == ".", (x, y)
    L.put(x, y, "g")
assert len(gems) == 31, len(gems)

L.write(__file__, "19_bonus_gun_gravity.txt")
