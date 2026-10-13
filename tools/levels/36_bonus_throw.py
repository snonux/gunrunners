#!/usr/bin/env python3
"""Level 36 bonus - Throw Everything (SPEC.md 36). rules=throw_all: fire
next to anything loose (crates, the practice dummies, the loose pieces in
the walls) lifts it overhead; fire again throws it six blocks ahead, where
it lands on top of whatever is there as a solid block. No weapon.
timer=90, goal=exit.

Changes from the spec, so it plays in this engine: the ledges climb in a
zigzag, each 5 blocks over the last, so a stack of three throws at the end
of one leaves a short hop to the next (every runner makes it): A (ground
17, x 2-13) from a stack on the floor at x 14 (thrown from the right: a stack built from
under A walls the thrower in), B (ground 12, x 15-37) from
a stack on A at x 13, C (ground 7, x 2-14, the exit) from a stack on B at
x 15. Loose wall pieces sit beside each ledge; a throw that lands within a
block of a taller stack lands on it (stack assist). `@ step` lines are the
bot's stacks.
"""

from lib import Level

W, H = 40, 24
L = Level(W, H, name="BONUS - THROW EVERYTHING", episode=6, theme="prop_warehouse", music="theme_lonely_piano",
          rules="throw_all", timer=90, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 1, 0, H - 1, "#")
L.fill(W - 2, W - 1, 0, H - 1, "#")
L.ground(2, W - 3, 22)
L.row(2, 13, 17, "#")                            # A
L.row(15, 37, 12, "#")                           # B
L.row(2, 14, 7, "#")                             # C
# Loose wall pieces beside the floor and each ledge.
for y0, x in ((19, 1), (14, 1), (19, 38), (9, 38)):
    L.at("loose", None, None, rect=(x, y0, x, y0 + 2))
# Loose crates.
crates = [(x, 21) for x in range(24, 37, 2)] + [(x, 20) for x in range(24, 37, 4)]
crates += [(x, 11) for x in range(22, 37, 2)] + [(30, 10), (9, 16), (11, 16)]
crates += [(x, 6) for x in range(8, 15, 2)]
for x, y in crates:
    L.at("loose", None, None, rect=(x, y, x, y))
put(4, 21, "P")
put(3, 6, "X")
for x, y in ((10, 21), (12, 21), (22, 21), (26, 11), (30, 11), (34, 11)):
    L.at("practice_dummy", x, y)
gems = [(x, 19) for x in range(4, 13, 2)] + [(x, 14) for x in range(3, 12, 2)] + [(x, 9) for x in range(17, 36, 2)]
gems += [(x, 3) for x in range(4, 14, 2)] + [(x, 16) for x in range(20, 36, 2)] + [(x, 4) for x in (16, 18, 20)]
gems += [(16, 17), (14, 15), (16, 10), (6, 2), (8, 2)]
for x, y in gems:
    put(x, y, "g")
for x, y, col in ((20, 21, 14), (7, 16, 13), (21, 11, 15)):
    L.at("step", x, y, act="stack", arg=col, n=3)

L.write(__file__, "36_bonus_throw.txt")
