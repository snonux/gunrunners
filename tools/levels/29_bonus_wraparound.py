#!/usr/bin/env python3
"""Level 29 bonus - Wraparound (SPEC.md 29). rules=wrap_xy: one screen
whose edges join. Leaving at x < 0 enters at x = 39 and the other way;
falling below row 23 enters at row 0 with the fall kept (and capped), so
falling forever is safe. timer=60, goal=collect:30.

The way up is the way down: there is nothing to climb. The floor's gap
(x 18-21) drops the runner through the bottom onto the ceiling slab at the
top (x 8-31, g2), and from there they fall down through the two ledges
(g6, g12) to the floor again. 30 gems, 10 of them only reached by
wrapping: 4 on the ceiling slab, 3 in the pocket at x 0-2 under the g12
ledge (walled off on the inside, reached from x 39 on the floor) and 3 in
the pocket at x 37-39 on g12 (walled off on the inside from the top down,
reached from x 0 on the g12 ledge). 3 carrier=0 Static Swarms drift at the
runner across the edges too; a touch puts the runner back at (20, 11).

Changes from the spec, so it plays in this engine:
- The pockets are walled off (x 3 under the g12 ledge, x 35 from the top
  to g12) instead of guarded by spikes: a runner jumps spikes, and in a
  bonus with no hearts to lose spikes guard nothing.
- The swarms drift a cell every third frame here (spec: half a cell a
  frame), so a runner can outrun them.

Everything in world_signal.cpp.
"""

from lib import Level

W, H = 40, 24
L = Level(W, H, name="BONUS - WRAPAROUND", episode=5, theme="dead_air", music="theme_glitch",
          rules="wrap_xy", timer=60, goal="collect:30")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


L.row(8, 31, 2, "#")                       # the ceiling slab
L.row(2, 15, 6, "#")                       # the g6 ledges
L.row(24, 32, 6, "#")
L.row(0, 13, 12, "#")                      # the g12 ledges (the left one goes on at 36-39)
L.row(17, 34, 12, "#")
L.row(36, 39, 12, "#")
L.col(35, 0, 12, "#")                      # pocket at 37-39: walled off on the inside
L.col(3, 13, 17, "#")                      # pocket at 0-2: walled off on the inside
L.row(0, 17, 18, "#")                      # the floor, with the gap at 18-21
L.row(22, 39, 18, "#")

put(20, 11, "P")
# Only by wrapping.
gem((11, 1), (15, 1), (24, 1), (28, 1))   # on the ceiling slab
gem((0, 17), (1, 17), (2, 17))             # the floor pocket
gem((37, 11), (38, 11), (39, 11))          # the ledge pocket
# The rest.
gem((22, 11), (26, 11), (30, 11), (6, 11), (10, 11))
gem((4, 5), (8, 5), (12, 5), (26, 5), (30, 5))
gem((6, 17), (10, 17), (14, 17), (25, 17), (29, 17), (33, 17))
gem((19, 21), (20, 21), (19, 8), (20, 8))

L.at("static_swarm", 4, 21, carrier=0)
L.at("static_swarm", 30, 21, carrier=0)
L.at("static_swarm", 20, 4, carrier=0)

L.write(__file__, "29_bonus_wraparound.txt")
