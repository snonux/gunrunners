#!/usr/bin/env python3
"""Level 12 bonus - The Floor Is Lava (SPEC.md 12, "Bonus level").

Rule floor_lava: every floor is lava. Landing on an enemy's head bounces
the runner 10 cells up (14 with jump held); touching the lava puts them
back on the last head they bounced on, unhurt. Enemies are harmless.
90 seconds, reach the exit.

One long cavern over a lava lake (x 4-75, surface row 20): the start ledge
(x 1-3, ground 18), 14 Magma Toads leaping straight up out of the lava on
fixed 45-frame cycles (x 6-71, arcs 4-8 blocks high), 6 Ember Wisps hanging
at fixed spots as fallback heads, and the exit ledge (x 76-78, ground 14).
40 gems along the bounce line.

Changes from the spec: the toads run from x 6 to 71 (every 5 blocks), so
the last one sits under the exit ledge's lip.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - THE FLOOR IS LAVA", episode=2, theme="basalt_magma",
          music="bonus_floor_lava", rules="floor_lava", timer=90, goal="exit")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, 78, 1, 22)
L.ground(1, 3, 18, 23)         # the start ledge
L.ground(76, 78, 14, 23)       # the exit ledge
L.at("fluid", rect=(4, 20, 75, 22), kind="lava")
L.on(2, 18, "P")
L.on(78, 14, "X")

ARCS = (5, 6, 4, 7, 5, 8, 6, 4, 7, 5, 6, 8, 5, 7)
for i, arc in enumerate(ARCS):
    L.at("toad", 6 + 5 * i, 21, cycle=1, arc=arc, phase=(i * 11) % 45)
for x, y in ((14, 11), (26, 10), (38, 11), (50, 10), (62, 11), (72, 9)):
    L.at("wisp", x, y, fixed=1)

# Gems on the bounce line: over each toad's peak, and between them.
gems = []
for i, arc in enumerate(ARCS):
    top = 20 - arc
    gems.append((6 + 5 * i + 1, max(2, top - 4)))
    gems.append((6 + 5 * i + 3, max(2, top - 5)))
gems += [(4, 14), (5, 12), (74, 8), (75, 9), (76, 10), (77, 11), (9, 6), (40, 5), (60, 5), (70, 4), (30, 6), (20, 5), (50, 4)]
gems = list(dict.fromkeys(gems))
assert len(gems) == 40, len(gems)
for x, y in gems:
    assert L.get(x, y) == ".", (x, y, L.get(x, y))
    L.put(x, y, "g")

L.write(__file__, "12_bonus_floor_lava.txt")
