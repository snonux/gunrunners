#!/usr/bin/env python3
"""Level 42 bonus - Blooper Reel (SPEC.md 42). rules=blooper: every 150
frames (10 s) the rules switch to another bonus level's rule set, picked
at random without repeats, behind a 15-frame "BLOOPER!" card and a VHS
warp; the switch waits (up to 8 frames) while the runner is inside solid
ground. timer=120, goal=exit.

One long gauntlet that works under every rule: the floor (ground 22) with
two shallow dips, steps of 2 blocks up to platforms at ground 18 and 14
and back down, and the exit at the far right. Everything is in
world_finale.cpp.

Changes from the spec, so it plays in this engine:
- The reel holds the rule sets that can be switched on and off in the
  middle of a run here: AIR JUMP (Cloud Nine), ECHO ROOM (sonar), LIGHT
  TRAIL, STOP MOTION, HEATSTROKE (left and right swap) and TRIGGER HAPPY
  (jump and fire are one button). The other bonus rules need their own
  levels (a pinball table, a boulder, a bubble, a cannon...).
- With no Throw Everything, Breakout or Trigger Happy targets on the reel
  for long, the loose crates, letter bricks and targets are left out; the
  60 gems run along the route instead.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - BLOOPER REEL", episode=6, theme="main_stage", music="theme_medley",
          rules="blooper", timer=120, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.fill(1, W - 2, 22, 23, "#")              # the floor
# Two shallow dips (one block).
L.clear(14, 17, 22, 22)
L.clear(58, 61, 22, 22)
# Steps of 2 blocks up to ground 18, a one-way platform at ground 14, and
# back down.
L.fill(22, 25, 20, 21, "#")                # ground 20
L.fill(26, 31, 18, 21, "#")                # ground 18
L.fill(32, 35, 16, 21, "#")                # ground 16
L.fill(36, 43, 18, 21, "#")                # ground 18 under the platform
L.row(36, 43, 14, "=")                     # the one-way platform at ground 14
L.fill(44, 49, 18, 21, "#")                # ground 18
L.fill(50, 53, 20, 21, "#")                # ground 20
put(2, 21, "P")
put(78, 21, "X")
pts = [(x, 21) for x in range(4, 14)]
pts += [(x, 22) for x in range(14, 18)]
pts += [(x, 21) for x in range(18, 22)]
pts += [(x, 19) for x in range(22, 26)]
pts += [(x, 17) for x in range(26, 32)]
pts += [(x, 15) for x in range(32, 36)]
pts += [(x, 13) for x in range(36, 44)]
pts += [(x, 17) for x in range(44, 50)]
pts += [(x, 19) for x in range(50, 54)]
pts += [(x, 21) for x in range(54, 58)]
pts += [(x, 22) for x in range(58, 62)]
pts += [(x, 21) for x in range(62, 64)]
for x, y in pts:
    put(x, y, "g")
assert len(pts) == 60, len(pts)
L.write(__file__, "42_bonus_bloopers.txt")
