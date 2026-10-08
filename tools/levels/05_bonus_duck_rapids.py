#!/usr/bin/env python3
"""Level 5 bonus - Duck Rapids (SPEC.md 05, "Bonus level").

Rule autorun: the runner rides a rubber duck down a sludge river. The duck
paddles right half a cell a frame and never stops; only jump (6 cells for
everyone) and duck (crouch) work, and anything it runs into bumps it back
four blocks. The river drops two rows at three small falls (x20, x40, x60);
floating logs to jump, low pipes to duck under, 50 gems in arcs over the
logs, and the finish at x78.

Deviations: the runner keeps their size (crouched they are 4 cells tall,
standing 5), so the pipes leave 4 cells of clearance over the river, and
the river runs at rows 13/15/17/19 so the falls fit the 24-row map,
and the last pipe hangs at x68 (not x70) so there is room to jump the log
at x72 after ducking under it.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - DUCK RAPIDS", episode=1, theme="sludge_line",
          music="bonus_duck_rapids", rules="autorun", timer=40, goal="exit")

L.fill(0, W - 1, 0, 2, "#")                  # ceiling
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")

SEGMENTS = [(1, 19, 13), (20, 39, 15), (40, 59, 17), (60, 78, 19)]  # x0, x1, surface row


def surface(x):
    for x0, x1, s in SEGMENTS:
        if x0 <= x <= x1:
            return s
    return SEGMENTS[-1][2]


for x0, x1, s in SEGMENTS:
    L.ground(x0, x1, s + 1)                  # the river bed
    L.at("fluid", None, None, kind="sludge", rect=(x0, s, x1, s))

LOGS = (8, 15, 27, 34, 47, 55, 66, 72)
PIPES = (12, 31, 44, 52, 68)
for x in LOGS:
    L.put(x, surface(x), "#")
    L.at("deco", kind="log", x=x, y=surface(x), w=1, h=1)
for x in PIPES:
    L.fill(x, x, 3, surface(x) - 2, "#")     # hangs from the roof: duck under it
    L.at("deco", kind="lowpipe", rect=(x, 3, x, surface(x) - 2))

L.put(2, 12, "P")
L.put(78, 19, "X")

gems = []
for x in LOGS:
    s = surface(x)
    gems += [(x - 2, s - 1), (x - 1, s - 2), (x, s - 3), (x + 1, s - 3), (x + 2, s - 2), (x + 3, s - 1)]
gems += [(76, 18), (77, 18)]
assert len(gems) == 50, len(gems)
for x, y in gems:
    L.put(x, y, "g")

L.write(__file__, "05_bonus_duck_rapids.txt")
