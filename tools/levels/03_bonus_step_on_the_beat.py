#!/usr/bin/env python3
"""Level 3 bonus - Step on the Beat (SPEC.md 03, "Bonus level").

Rule beatstep: inputs are queued and the last one runs on the next beat. A
step is 2 blocks over the beat, a jump starts on the beat; nothing moves in
between. A dance-floor corridor with three 2-block gaps, a staircase of
2-block steps up to the top floor, a row of cardboard Bouncers that step
toward you on every beat, and the exit. 40 gems, one set per jump arc.
"""

from lib import Level

W, H = 64, 20
L = Level(W, H,
          name="BONUS - STEP ON THE BEAT", episode=1, theme="club_laserdisc",
          music="bonus_step_on_the_beat", rules="beatstep", timer=90, goal="exit")

L.fill(0, W - 1, 0, 2, "#")                  # ceiling
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")
L.ground(1, 19, 17)
GAPS = (6, 11, 16)
for g in GAPS:
    L.clear(g, g + 1, 17, H - 1)
# The staircase: 2-block steps up to ground 11.
L.ground(20, 23, 15)
L.ground(24, 27, 13)
L.ground(28, W - 2, 11)
L.on(2, 17, "P")
L.on(61, 11, "X")
L.at("deco", kind="dancefloor", rect=(1, 17, 19, 17))
L.at("deco", kind="dancefloor", rect=(28, 11, 62, 11))
for x in (36, 40, 44):
    L.at("cardboard_bouncer", x, 10)
L.at("deco", kind="text", x=48, y=5, w=10, h=1, text="STEP ON THE BEAT")


def arc(cx, ground, n=5):
    """Gems along a jump arc centred on block cx over ground row `ground`."""
    rise = [1, 2, 3, 2, 1, 1, 2, 3, 2, 1]
    placed = 0
    for i in range(n):
        x = cx - n // 2 + i
        y = ground - 1 - rise[i]
        if L.get(x, y) == ".":
            L.put(x, y, "g")
            placed += 1
    return placed


n = 0
for g in GAPS:
    n += arc(g + 1, 17)            # over each gap
n += arc(21, 15) + arc(25, 13) + arc(29, 11)   # up the stairs
n += arc(37, 11, 10)              # over the cardboard crowd
assert n == 40, n

L.write(__file__, "03_bonus_step_on_the_beat.txt")
