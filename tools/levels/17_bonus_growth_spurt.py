#!/usr/bin/env python3
"""Level 17 bonus - Growth Spurt (SPEC.md 17).

rules=grow: the runner has five sizes, x1.0 to x2.0 in quarter steps.
Every gem is a size up, every shot a size down. A bigger runner jumps
higher (the jump arc scales with size), at x1.5 or more walks through
cracked blocks (`by=heavy`) and shrugs off small Globs, and only at x1.0
fits a tunnel 3 blocks high (taller than that, it stops at the mouth).

Left to right: a gem trail to a wall of cracked blocks (grow, then
smash), a low tunnel (shoot yourself small), six small Globs and a stair
of gems to the exit.

Changes from the spec, so it plays in this engine:
- Only the runner's height grows (the hitbox stays 3 cells wide).
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - GROWTH SPURT", episode=3, theme="station_greenhouse",
          music="bonus_growth_spurt", weapon="hedge_trimmer", par=60, flags="",
          rules="grow", timer=90, goal="exit")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, 78, 1, 20)          # ground 21, ceiling row 0


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


put(3, 20, "P")
# x 0-26: the gem trail, then a wall of cracked blocks floor to ceiling.
for x in (7, 10, 13, 16):
    put(x, 20, "g")
put(19, 18, "g")
L.at("breakable", None, None, rect=(23, 1, 25, 20), by="heavy", hp=8, look="rock")
# x 27-52: the low tunnel, rows 18-20 (3 blocks high).
L.fill(32, 50, 1, 17, "#")
for x in (36, 41, 46):
    put(x, 20, "g")            # a shot costs a size, a gem in there buys it back
# x 53-79: six small Globs, the gem stair and the exit.
for i, x in enumerate((55, 58, 61, 64, 67, 70)):
    L.at("glob_small", x, 20, "GS%d" % (i + 1))
L.row(57, 59, 18, "=")
L.row(61, 63, 15, "=")
L.row(65, 67, 12, "=")
L.row(69, 71, 9, "=")
for x, y in ((58, 17), (62, 14), (66, 11), (70, 8), (73, 20), (75, 20)):
    put(x, y, "g")
put(77, 20, "X")

L.write(__file__, "17_bonus_growth_spurt.txt")
