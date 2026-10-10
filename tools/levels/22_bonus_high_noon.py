#!/usr/bin/env python3
"""Level 22 bonus - High Noon (SPEC.md 22).

rules=onehit: every hit kills, the runner's included, but a runner who is
shot only starts the duel over. One street on ground 12: the runner's mark
(24, 11), the duelist's (36, 11) (the spec's 20 and 40 are a screen apart,
and shots die at the screen's edge), the bell tower x 29-31 with its bell at
(30, 3). Twenty duels: the bell rings, then again after 22 frames (duels
1-10) or 15-30 (11-20), and the duelist draws 8 frames after the second
ring. A shot before the second ring is a false start. Each win drops a gem;
after the twentieth the exit opens at (57, 11). goal=exit, timer=120 (the
spec's 90 s leaves no room to spare for twenty duels).

The runner holds the Six-Shooter for the whole bonus (`@ highnoon`).
"""

from lib import Level

W, H = 60, 16
L = Level(W, H,
          name="BONUS - HIGH NOON", episode=4, theme="western_gulch",
          music="theme_western", weapon="six_shooter", par=90, flags="",
          rules="onehit", timer=120, goal="exit")

L.ground(0, W - 1, 12)
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.at("deco", kind="facade", rect=(4, 5, 14, 11), text="SALOON")
L.at("deco", kind="steeple", rect=(29, 2, 31, 11))
L.at("deco", kind="facade", rect=(46, 6, 54, 11), text="BANK")
L.at("bell", 30, 3)
L.at("highnoon", duelist=36, duels=20)
L.put(24, 11, "P")
L.put(57, 11, "X")

L.write(__file__, "22_bonus_high_noon.txt")
