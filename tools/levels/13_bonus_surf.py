#!/usr/bin/env python3
"""Level 13 bonus - Boulder Surfing (SPEC.md 13, "Bonus level").

Rule boulder_surf: the runner rides a 6 x 6 block boulder down a canyon.
Left and right speed its roll up or down (1/16 cell a frame each frame, 2
at most); pushing, the runner leans back on top, letting be, the runner
finds the middle again. More than 2 blocks off the top centre is a fall:
back to the start of the screen, nothing lost. Jump leaves the boulder,
which rolls on. 60 seconds, reach the exit.

The canyon floor is ground 20 with dips (ground 22-23) and ramps (ground
17-18), 20 Cultists walking at the boulder to be crushed (100 each), 30
gems at the riding runner's head height, and at the end a gap the boulder
falls into: jump off it onto the exit ledge (ground 16) and walk to the
gate.

Changes from the spec, so it plays in this engine:
- Dips are 7 blocks wide or more: the boulder rides over anything
  narrower than itself, so a 2-block dip would not show under it. That
  leaves room for 3 dips and 4 ramps in 80 blocks, not two of each a
  screen.
- The exit gate is on a ledge past a 10-block gap (x 62-71), with the
  gate at x 77: the boulder cannot climb the ledge, the runner jumps.
- A fall restarts at the screen starts x 1, 20, 40, and 52 for the gap,
  so there is a run-up for the jump.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - BOULDER SURFING", episode=2, theme="collapsing_temple",
          music="bonus_boulder_surf", rules="boulder_surf", timer=60, goal="exit")

GAP = range(62, 72)
LEDGE = 72

# The floor's height under each block column (None: the gap).
g = {x: 20 for x in range(1, W - 1)}
for x, h in {9: 21, 10: 22, 11: 22, 12: 22, 13: 22, 14: 22, 15: 21,       # dip A
             17: 19, 18: 18, 19: 18, 20: 19,                                # ramp A
             23: 21, 24: 22, 25: 22, 26: 22, 27: 22, 28: 22, 29: 21,      # dip B
             31: 19, 32: 18, 33: 17, 34: 18, 35: 19,                       # ramp B, the kicker
             40: 19, 41: 18, 42: 18, 43: 19,                                # ramp C
             44: 21, 45: 22, 46: 23, 47: 23, 48: 23, 49: 22, 50: 21}.items():  # dip C, deep
    g[x] = h
for x in GAP:
    g[x] = None
for x in range(LEDGE, W - 1):
    g[x] = 16

L.fill(0, W - 1, 0, H - 1, "#")
for x in range(1, W - 1):
    L.clear(x, x, 1, (g[x] - 1) if g[x] is not None else H - 1)

L.at("boulder", 1, 14, "SB", size=6, exit=LEDGE, restart="1,20,40,52")
L.put(3, 13, "P")
L.on(77, 16, "X")

# Cultists walking at the boulder.
for i in range(20):
    x = 8 + (i * 51) // 19
    if g.get(x) is None or g.get(x + 1) != g[x]:
        x += 1
    L.at("cultist", x, g[x] - 1)

# Gems where the riding runner's head goes: over the boulder centred at x.
def head_row(x):
    floors = [g[k] for k in range(x - 3, x + 3) if g.get(k) is not None]
    feet = min(floors) * 2 - 12 - 1   # cells
    return (feet - 3) // 2

gems = [(x, head_row(x)) for x in range(5, 61, 2)] + [(66, 11), (69, 12)]
assert len(gems) == 30, len(gems)
for x, y in gems:
    assert L.get(x, y) == ".", (x, y, L.get(x, y))
    L.put(x, y, "g")

L.write(__file__, "13_bonus_surf.txt")
