#!/usr/bin/env python3
"""Level 31 bonus - Bungee (SPEC.md 31). rules=bungee: the runner is tied to
the lighthouse post at (40, 12) by an elastic rope (rest 6 blocks, 14 at
most). On the ground, holding a direction stretches it; letting go flings
the runner toward and past the post, up to 4 cells a frame. In the air they
swing on it like a pendulum (left and right pump a little) and can fire.
timer=60, goal=score:5000: 30 storm rings, 200 points each. The lightning
glows but never strikes.

The rope reaches 14 blocks from the post, so the play area is the circle
round it: the ground (row 22) from about 28 to 52, the lighthouse drawn
behind the post (not solid, so flings fly through it), and rock ledges at
row 18 out at the sides. The rings sit on the arcs the flings fly.

Everything in world_storm.cpp.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - BUNGEE", episode=5, theme="storm_cliffs", music="theme_rock_storm",
          rules="bungee", timer=60, goal="score:5000")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 22, H - 1, "#")             # the low ledges (g22) and the rock under them
L.at("lighthouse", None, None, rect=(37, 12, 43, 21))  # drawn behind: the post is on its top
L.fill(20, 25, 18, 21, "#")                  # the high ledges (g18)
L.fill(55, 60, 18, 21, "#")
L.at("post", 40, 12)
put(34, 21, "P")

# Ring middles (blocks), on the flings' arcs: points a fling passes, found by
# flinging from where the last one landed, alternating the stretch.
RINGS = ((34, 16), (38, 14), (42, 12), (46, 11), (50, 10), (53, 10), (53, 12), (50, 14), (47, 16), (39, 18),
         (44, 18), (42, 17), (40, 16), (32, 18), (36, 17), (38, 16), (43, 14), (46, 14), (48, 14), (51, 16),
         (49, 18), (33, 12), (29, 11), (27, 11), (30, 14), (49, 12), (51, 12), (52, 14), (35, 13), (28, 13))
for x, y in RINGS:
    L.at("ring", x, y)

L.write(__file__, "31_bonus_bungee.txt")
