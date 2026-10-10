#!/usr/bin/env python3
"""Level 15 bonus - Asteroid Belt (SPEC.md 15, "Bonus level").

Rule recoil_only (world_drift.cpp): no gravity, no ground, no jump. Every
shot pushes the runner half a cell a frame the other way (8 directions, 2
cells a frame at most); drag takes 1/32 of the speed a frame. Touching an
asteroid, or the belt's edge, bounces the runner back at half speed, no
damage. 60 seconds, collect 40 gems; the return beacon ends it early.

The runner starts drifting at (4, 12); 12 asteroids (`@ asteroid w= h=
path= speed=1/4`, 2 x 2 to 5 x 4 blocks) tumble on slow loops across x
12-73; 40 gems in three clusters (x 15-23, x 36-43, x 58-70), each in the
lee of an asteroid; the beacon is at (76, 12).

Changes from the spec, so it plays in this engine:
- The belt is drawn in Starfall's open space (theme=starfall), not the
  hangar's.
- Every loop keeps 3 blocks clear of the belt's edge and of the gems, so a
  rock never pins the runner (2.5 blocks tall) against the edge or sits on
  a gem for good.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - ASTEROID BELT", episode=3, theme="starfall",
          music="bonus_asteroid_belt", rules="recoil_only", timer=60, goal="collect:40")

for x in range(W):
    L.put(x, 0, "#")
    L.put(x, H - 1, "#")
for y in range(H):
    L.put(0, y, "#")
    L.put(W - 1, y, "#")

L.put(4, 12, "P")
L.put(76, 12, "X")

# Asteroids: (x, y, w, h, path); the first point is where it starts.
ROCKS = [
    (3, 3, [(12, 4), (12, 7)]),
    (4, 3, [(16, 14), (19, 16), (16, 17)]),
    (2, 2, [(21, 5), (24, 8)]),
    (5, 4, [(27, 10), (27, 13)]),
    (3, 2, [(33, 3), (37, 4)]),
    (4, 3, [(40, 15), (44, 16)]),
    (3, 3, [(45, 5), (45, 9)]),
    (2, 2, [(51, 12), (54, 10), (54, 14)]),
    (5, 3, [(57, 3), (60, 5)]),
    (3, 4, [(62, 14), (65, 16)]),
    (2, 2, [(68, 7), (70, 10)]),
    (3, 2, [(71, 18), (71, 15)]),
]
for i, (w, h, path) in enumerate(ROCKS):
    x, y = path[0]
    L.at("asteroid", x, y, "A%d" % (i + 1), w=w, h=h,
         path=";".join("%d,%d" % p for p in path), speed="1/4")

GEMS = (
    # Cluster 1 (x 15-23): behind the first three rocks.
    [(x, 11) for x in (15, 17, 19, 21, 23)] + [(x, 12) for x in (16, 18, 20, 22)] +
    [(x, 2) for x in (16, 18, 20, 22)] +
    # Cluster 2 (x 36-43): in the lee of the big rock and the drifting pair.
    [(x, 8) for x in (36, 38, 40, 42)] + [(x, 10) for x in (36, 38, 40, 42)] +
    [(x, 12) for x in (37, 39, 41, 43)] + [(41, 20), (43, 20)] +
    # Cluster 3 (x 58-70): round the last rocks, short of the beacon.
    [(x, 9) for x in (60, 62, 64, 66)] + [(x, 11) for x in (58, 60, 62, 64)] +
    [(x, 2) for x in (66, 68, 70)] + [(68, 13), (70, 13)]
)
assert len(GEMS) == 40, len(GEMS)
for x, y in GEMS:
    L.put(x, y, "g")

L.write(__file__, "15_bonus_asteroid_belt.txt")
