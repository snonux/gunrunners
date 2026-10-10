#!/usr/bin/env python3
"""Level 28 bonus - Inkwell (SPEC.md 28). rules=ink: hold fire to draw.
The pen tip starts under the runner's leading foot and follows their aim
a cell a frame (Up or Down held: 45 degrees up or down), leaving a solid
one-way line; the inkwell holds 160 cells of line (the meter), and old
lines fade after 600 frames. Jump, then draw at the top of the jump while
running on, and you land on your own line: a step up. No enemies.
timer=90, goal=collect:20.

A blank page: the start ledge (x 0-5, ground 21), 20 gems in clusters up
to row 2 across x 10-76, three ink refills (+80). Falling off the bottom
puts the runner back at the start with what they collected.

Everything in world_toon.cpp (InkState).
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - INKWELL", episode=4, theme="rubber_hose_toon",
          music="theme_toon_jazz", rules="ink", timer=90, goal="collect:20")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


L.row(0, 5, 21, "#")
put(2, 20, "P")
gem((10, 19), (12, 19), (14, 18))
gem((20, 15), (22, 14), (24, 13))
gem((30, 9), (32, 8))
gem((40, 5), (42, 4), (44, 3))
gem((50, 8), (52, 7))
gem((58, 2), (60, 2), (62, 3))
gem((68, 12), (70, 13))
gem((74, 6), (76, 5))
for x, y in ((30, 18), (50, 10), (70, 16)):
    L.at("refill", x, y)

L.write(__file__, "28_bonus_inkwell.txt")
