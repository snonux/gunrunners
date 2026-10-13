#!/usr/bin/env python3
"""Level 39 bonus - Rewind (SPEC.md 39). rules=rewind: the game keeps the
last 45 frames; a death (spikes, a crusher, a fall) plays a 20-frame VHS
rewind and puts the runner, the enemies, the platforms, the crushers and
the gems back as they were then (the oldest moment kept with the runner on
their feet); unlimited rewinds; the timer runs on. timer=120, goal=exit.

Changes from the spec, so it plays in this engine: the spike strips stand
on the floor (2 blocks wide, every 6 blocks between the crushers); the
tripwire lasers cost a heart (they don't kill); the crushers slam from row
13 to the floor.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - REWIND", episode=6, theme="gallery", music="theme_synth_pulse",
          rules="rewind", timer=120, goal="exit")

L.fill(0, W - 1, 0, 1, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.ground(1, 65, 22)
L.ground(75, W - 2, 22)
L.put(2, 21, "P")
for x in (8, 14, 26, 40):
    L.row(x, x + 1, 21, "^")
for x in (20, 34, 48):
    L.at("crusher", x, 13, w=2, to=21, period=30, tell=8)
    L.fill(x - 1, x + 2, 12, 12, "#")             # the press's housing
L.at("lasergrid", rect="53,17,63,21", dmg=1, every=30)
L.at("platform", 66, 22, "pl1", w=3, mode="pingpong", path="66,22;72,22", speed="1/2")
L.put(78, 21, "X")
n = 0
for x in range(4, 66):
    if L.get(x, 20) == ".":
        L.put(x, 20 if x % 2 == 0 else 19, "g")
        n += 1
    if n >= 46:
        break
for x in (67, 69, 71, 73):
    L.put(x, 19, "g")
    n += 1
assert n == 50, n
L.write(__file__, "39_bonus_rewind.txt")
