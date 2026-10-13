#!/usr/bin/env python3
"""Level 38 bonus - Breakout (SPEC.md 38). rules=breakout: the runner is the
paddle on the floor (walks only, no jump, a cell and a half a frame); fire
launches the ball-shot 45 degrees up when none is out; it bounces off the
walls, the ceiling, the bricks and the runner (30, 45 or 60 degrees from
where it hits); one past the runner is lost, and after three the bonus
ends. Bricks are letters spelling THE SHOW MUST GO ON over and over; red
ink takes two hits; 20 bricks drop a gem. timer=90, goal=collect:60 (bricks).

Changes from the spec, so it plays in this engine: the 64 bricks are four
rows of 16 (rows 2, 4, 6, 8, x 4-35) with a row of air between, so the ball
gets in among them.
"""

from lib import Level

W, H = 40, 24
L = Level(W, H, name="BONUS - BREAKOUT", episode=6, theme="writers_room", music="theme_typewriter_clarinet",
          rules="breakout", timer=90, goal="collect:60")

L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.ground(1, W - 2, 22)
L.put(19, 21, "P")
LETTERS = "THESHOWMUSTGOON"
n = 0
for row in (2, 4, 6, 8):
    for i in range(16):
        x = 4 + 2 * i
        L.at("brick", x, row, ch=LETTERS[n % len(LETTERS)], red=1 if n % 5 == 2 else None,
             gem=1 if n % 16 in (1, 4, 7, 10, 13) else None)
        n += 1

L.write(__file__, "38_bonus_breakout.txt")
