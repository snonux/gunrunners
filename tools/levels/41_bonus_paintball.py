#!/usr/bin/env python3
"""Level 41 bonus - Paintball (SPEC.md 41). rules=paint: every floor
block top the runner stands on, and every wall block they touch, turns
their colour; shots paint the first block they hit. `goal=paint:80`:
80 % of the floor tops (counted, shown on the HUD) wins. No enemies; the
audience chants at 25 / 50 / 75 %. timer=60.

An arena floor (ground 18, x 1-58) between walls, four floating one-way
platforms (rows 14 and 10), 2-block steps up to them on both sides and a
spring pad in the middle of the floor. Everything is in world_sweeps.cpp.

Changes from the spec, so it plays in this engine:
- The bounce pad is `@ springpad x y w=` (its top the floor row y): a
  runner landing on it is launched 18 cells up, onto the row-10
  platforms.
- A floor top counts when the block over it is open; blocks under the
  steps and walls only show paint.
"""

from lib import Level

W, H = 60, 20
L = Level(W, H, name="BONUS - PAINTBALL", episode=6, theme="sweeps_week", music="theme_big_band",
          rules="paint", timer=60, goal="paint:80")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.fill(1, W - 2, 18, 19, "#")                 # the arena floor
# The 2-block steps up to the platforms.
L.fill(4, 5, 16, 17, "#")
L.fill(54, 55, 16, 17, "#")
L.row(14, 15, 12, "=")
L.row(44, 45, 12, "=")
# The floating platforms.
L.row(6, 13, 14, "=")
L.row(46, 53, 14, "=")
L.row(16, 23, 10, "=")
L.row(36, 43, 10, "=")
L.at("springpad", 29, 18, w=2)
put(2, 17, "P")
n = 0
for x, y in ((8, 17), (12, 17), (20, 17), (25, 17), (34, 17), (39, 17), (47, 17), (51, 17),
             (7, 13), (10, 13), (13, 13), (47, 13), (50, 13), (53, 13),
             (17, 9), (20, 9), (23, 9), (36, 9), (39, 9), (42, 9)):
    put(x, y, "g")
    n += 1
assert n == 20, n
L.write(__file__, "41_bonus_paintball.txt")
