#!/usr/bin/env python3
"""Level 6 bonus - Light Trail (SPEC.md 06, "Bonus level").

Rule lighttrail: in the air the runner's feet leave a one-way neon trail
(a block lasts 45 frames, then fades), so a jump draws a bridge you can
walk on; hold down to sink through it. A start ledge, four rest pylons
climbing to the goal ledge, and 40 gems in arcs only a drawn bridge
reaches. Falling off the bottom ends the bonus.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - LIGHT TRAIL", episode=1, theme="maglev_express",
          music="bonus_light_trail", rules="lighttrail", timer=60, goal="exit")

L.ground(0, 5, 20)                         # the start ledge
L.ground(74, 79, 6)                        # the goal ledge
for x, y in ((20, 17), (38, 14), (52, 11), (64, 8)):
    L.fill(x, x + 1, y, y, "#")            # rest pylons
L.put(2, 19, "P")
L.put(77, 5, "X")

# 40 gems: five arcs of eight, each hanging between two rests.
RESTS = [(5, 19), (20, 16), (38, 13), (52, 10), (64, 7), (74, 5)]
gems = []
for (ax, ay), (bx, by) in zip(RESTS, RESTS[1:]):
    n = 8
    for i in range(n):
        u = (i + 1) / (n + 1)
        x = round(ax + (bx - ax) * u)
        y = round(ay + (by - ay) * u - 4 * u * (1 - u) * 3) - 1
        gems.append((x, y))
gems = gems[:40]
assert len(set(gems)) == 40, len(set(gems))
for x, y in gems:
    L.put(x, y, "g")

L.write(__file__, "06_bonus_light_trail.txt")
