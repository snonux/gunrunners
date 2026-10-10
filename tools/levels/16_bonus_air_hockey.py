#!/usr/bin/env python3
"""Level 16 bonus - Air Hockey (SPEC.md 16, "Bonus level").

Rule zero_friction (world_cryo.cpp): nothing slows down on any floor.
Walking adds 1/8 cell a frame up to 1, and only pushing the other way
brakes; the runner stops dead against a wall, a puck bounces off it at full
speed. The pucks are frozen Puck Drones that never thaw: a shot sends one
sliding at 2 cells a frame, a puck that hits a puck stops and the other
goes on. 90 seconds, 6 goals (`goal=goals:6`); a scored puck comes back to
the face-off spot.

Two goalies (`goalie=1` Puck Drones) guard the goals: a puck that hits one
comes straight back, shots bounce off them, and every 48 frames each hops
6 cells for 18 frames (out of step with the other), which lets a puck
under. Time the shot.

The rink: floor row 20 wall to wall, goals as openings in the side walls
(x 0-1 and x 58-59, rows 14-19), 6 pucks in a row at x 24-35, the runner
at (6, 19), 24 gems on rows 10-16 above the ice.

Changes from the spec, so it plays in this engine:
- The goal is counted in goals (`goal=goals:6`), not score.
- The two bumpers hang overhead at (20, 16) and (40, 16): on the floor they
  would trap every puck between them for good.
- The goalies are new: with no friction every kicked puck would score.
- A scored puck comes back at the face-off spot (30, 19), or the nearest
  free spot beside it.
"""

from lib import Level

W, H = 60, 24
L = Level(W, H,
          name="BONUS - AIR HOCKEY", episode=3, theme="station_cryo",
          music="bonus_air_hockey", weapon="freeze_ray", rules="zero_friction", timer=90, goal="goals:6")

L.fill(0, W - 1, 0, 0, "#")
L.fill(0, W - 1, 20, H - 1, "#")
L.fill(0, 1, 0, 13, "#")
L.fill(W - 2, W - 1, 0, 13, "#")
L.at("ice", None, None, rect=(0, 20, W - 1, 20))
L.at("hockeygoal", None, None, rect=(0, 14, 1, 19))
L.at("hockeygoal", None, None, rect=(W - 2, 14, W - 1, 19))
L.at("puckspawn", 30, 19)
L.put(20, 16, "#")
L.put(40, 16, "#")

L.put(6, 19, "P")
L.put(54, 19, "X")
for i, x in enumerate(range(24, 36, 2)):
    L.at("puck_drone", x, 19, "PK%d" % (i + 1))
# The goalies: a hop every 48 frames lets a puck under, out of step.
L.at("puck_drone", 3, 19, "GL", goalie=1, phase=0)
L.at("puck_drone", 55, 19, "GR", goalie=1, phase=24)

GEMS = ([(x, 10) for x in range(8, 52, 6)] + [(x, 13) for x in range(11, 52, 6)] +
        [(x, 16) for x in (6, 12, 16, 24, 28, 32, 36, 44, 48)])
assert len(GEMS) == 24, len(GEMS)
for x, y in GEMS:
    L.put(x, y, "g")

L.write(__file__, "16_bonus_air_hockey.txt")
