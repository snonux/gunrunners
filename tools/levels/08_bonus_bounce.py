#!/usr/bin/env python3
"""Level 8 bonus - Bounce House (SPEC.md 08, "Bonus level").

Rule bounce: every surface is a trampoline and the runner never stands.
Each landing bounces back to the last height: 2 cells higher with jump
held, 3 lower with down held, 1 lower with neither (4 to 18 cells). Walls
bounce you back. 60 seconds, reach the exit.

Screen 1 (x0-26): the floor at ground 22 under a slab at row 12 with a gap
at x12-14 (gems up top). Screen 2 (x27-53): ledges at ground 18, 14 and
10, each 8 cells up: build the bounce first. Screen 3 (x54-79): leaf pads
from ledge 10 to the exit ledge (ground 5), and a second column of pads up
out of the pit. 45 gems in bounce arcs.

Changes from the spec: the "ceiling gap" is the slab's gap over the first
floor, and screen 3's pads come in two columns (from ledge 10, and up out
of the pit) so a missed pad is not the end of the run.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - BOUNCE HOUSE", episode=2, theme="jungle_canopy",
          music="bonus_bounce", rules="bounce", timer=60, goal="exit")

L.row(0, W - 1, 0, "#")                      # ceiling
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")
# Screen 1
L.ground(1, 26, 22)
L.row(1, 11, 12, "#")                        # the slab, with its gap at x12-14
L.row(15, 24, 12, "#")
# Screen 2
L.ground(27, 35, 18)
L.ground(36, 44, 14)
L.ground(45, 53, 10)
# Screen 3
L.ground(54, 78, 22)                         # the pit
L.row(58, 60, 8, "=")                        # from ledge 10
L.row(65, 67, 7, "=")
L.row(60, 62, 18, "=")                       # up out of the pit
L.row(66, 68, 14, "=")
L.row(70, 72, 10, "=")
L.ground(73, 78, 5, 5)                       # the exit ledge
L.on(3, 22, "P")
L.on(77, 5, "X")
L.at("deco", kind="text", x=4, y=15, w=8, h=1, text="HOLD JUMP TO GO HIGHER")


def arc(x0, ground, n, top):
    """Gems on a bounce's rising side: n columns from x0, peaking `top` blocks up."""
    pts = []
    for i in range(n):
        u = (i + 0.5) / n
        rise = max(1, round(top * 4 * u * (1 - u)))
        pts.append((x0 + i, ground - 1 - rise))
    return pts


gems = []
gems += arc(5, 22, 6, 4)                      # floor, low bounces
gems += arc(13, 13, 1, 2) + [(12, 10), (14, 10), (13, 7)]   # up through the gap
gems += arc(4, 12, 5, 3)                      # on the slab
gems += arc(18, 12, 4, 3)
gems += arc(28, 18, 6, 3)                     # the ledges
gems += arc(37, 14, 6, 3)
gems += arc(46, 10, 6, 3)
gems += [(56, 6), (61, 5), (63, 4), (69, 4), (71, 3)]   # pads to the exit ledge
gems += [(61, 16), (67, 12), (71, 8)]          # the pit column
gems = list(dict.fromkeys(gems))
assert len(gems) == 45, len(gems)
for x, y in gems:
    assert L.get(x, y) == ".", (x, y, L.get(x, y))
    L.put(x, y, "g")

L.write(__file__, "08_bonus_bounce.txt")
