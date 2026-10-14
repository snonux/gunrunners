#!/usr/bin/env python3
"""Level 38 bonus - Breakout (SPEC.md 38). rules=breakout: the runner is the
paddle on the floor (walks only, no jump, a cell and a half a frame); fire
launches the ball-shot 45 degrees up when none is out; it bounces off the
walls, the ceiling, the bricks and the runner (30, 45 or 60 degrees from
where it hits); one past the runner is lost, and after three the bonus
ends. Bricks are letters spelling THE SHOW MUST GO ON over and over; red
ink takes two hits; 20 bricks drop a gem. timer=90, goal=collect:60 (bricks).

Changes from the spec, so it plays in this engine: the screen shows 20 x
11 blocks, so the field is 20 x 12 blocks (the spec's 40 x 24 left the
bricks off screen): 45 bricks in five rows of 9 (rows 2-6, x 1-18) under
the ceiling and the HUD, the runner on the floor (ground 11), goal=collect:40; 15
bricks drop a gem.
"""

from lib import Level

W, H = 20, 12
L = Level(W, H, name="BONUS - BREAKOUT", episode=6, theme="writers_room", music="theme_typewriter_clarinet",
          rules="breakout", timer=90, goal="collect:40")

L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.ground(1, W - 2, 11)
L.put(9, 10, "P")
LETTERS = "THESHOWMUSTGOON"
n = 0
for row in range(2, 7):
    for i in range(9):
        L.at("brick", 1 + 2 * i, row, ch=LETTERS[n % len(LETTERS)], red=1 if n % 5 == 2 else None,
             gem=1 if n % 3 == 1 else None)
        n += 1

L.write(__file__, "38_bonus_breakout.txt")
