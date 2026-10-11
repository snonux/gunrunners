#!/usr/bin/env python3
"""Level 33 bonus - Overclock (SPEC.md 33). rules=overclock: the whole
simulation, the runner included, runs three logic frames for every one;
timer=30 (real seconds), goal=exit.

Three server floors at g22, g16 and g10 joined by racks at alternate ends,
a Firewall on the middle floor whose flames only push back, 30 gems along
the floors, the exit at (77, 9).

Changes from the spec, so it plays in this engine: the Firewall dips at two
ports (x 24 and 48), since its flames can't be jumped under the floor above
otherwise; the floors above each rack stop two blocks short of it.

Everything in world_magma.cpp.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - OVERCLOCK", episode=5, theme="volcano_datacenter", music="theme_driving_hihat",
          rules="overclock", timer=30, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 22, H - 1, "#")              # g22
L.fill(0, 1, 0, 21, "#")
L.fill(78, 79, 0, 21, "#")
L.fill(73, 75, 19, 21, "#")                   # rack up to g16 at the right end
L.row(2, 70, 16, "#")                         # g16
L.fill(4, 6, 13, 15, "#")                     # rack up to g10 at the left end
L.row(9, 77, 10, "#")                         # g10
put(3, 21, "P")
put(77, 9, "X")
L.at("firewall", 40, 15, x0=2, x1=70, ports=(24, 48))
for x in range(8, 70, 6):
    put(x, 21, "g")
for x in range(10, 70, 6):
    put(x, 15, "g")
for x in range(12, 76, 7):
    put(x, 9, "g")

L.write(__file__, "33_bonus_overclock.txt")
