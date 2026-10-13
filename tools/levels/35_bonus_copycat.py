#!/usr/bin/env python3
"""Level 35 bonus - Copycat (SPEC.md 35). rules=copycat: every enemy gets
the runner's moves each frame (walk left or right, jump); they never
attack, and touching one bumps the runner back two blocks. timer=60,
goal=collect:8: eight Mast Climber dummies walked into the pits under the
holes in their tiers (1000 each).

The runner is on the floor (g22) with two holes to jump; the dummies are on
the one-way tiers above (g16, g10), each with three 2-block holes and two
stubs a dummy only gets past with a jump. Every step the runner takes, the
dummies take too at half speed, so the trick is a walk (and jumps, over the
floor's holes and the stubs) that leaves each dummy over a hole. Falling
through a floor hole ends the bonus.

Everything in world_tower.cpp.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - COPYCAT", episode=5, theme="broadcast_mast", music="theme_heroic_build",
          rules="copycat", timer=60, goal="collect:8")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 22, H - 1, "#")
L.fill(0, 0, 0, 21, "#")
L.fill(W - 1, W - 1, 0, 21, "#")
for hx in (16, 62):
    L.clear(hx, hx + 2, 22, H - 1)               # the floor's holes: jump them

TIERS = {16: (6, 33, 72), 10: (12, 45, 66)}
STUBS = {16: (22, 52), 10: (30, 58)}            # a dummy only gets past these with a jump
DUMMIES = {16: (26, 44, 60, 76), 10: (4, 36, 54, 75)}
for g, holes in TIERS.items():
    L.row(1, W - 2, g, "=")
    for hx in holes:
        L.clear(hx, hx + 1, g, g)
        L.at("pit", None, None, rect=(hx, g, hx + 1, g))
    for x in STUBS[g]:
        L.put(x, g - 1, "#")
    for x in DUMMIES[g]:
        L.at("copycat_dummy", x, g - 1)

put(40, 21, "P")
for x in (4, 8, 12, 20, 24, 28, 32, 36, 44, 48, 52, 56, 66, 70, 74):
    put(x, 21, "g")

L.write(__file__, "35_bonus_copycat.txt")
