#!/usr/bin/env python3
"""Level 40 bonus - Trigger Happy (SPEC.md 40). rules=jumpfire: jump and
fire are one button; on the ground a press jumps and fires the blaster
forward at the same time, in the air a press only fires. 24 `@ target`
discs (1 HP, 300 each) hang on the scaffold at jump height (a block
over the head, which even Rocco's jump reaches), 8 a storey;
`goal=collect:24` counts the discs shot. timer=60.

A 3-storey scaffold: floors ground 22, 15 and 8 (x 2-37), joined by
ladders at alternate ends (x 36 up to storey 2, x 3 up to storey 3). The
upper floors are one-way planks with a 2-block gap a jump crosses.

Changes from the spec, so it plays in this engine:
- Shots fly straight ahead, so they can't break the floor the runner
  stands on: the breakable tiles are 2-block scaffold boards standing on
  each floor at shot height; a careless press (its ground shot) breaks the
  board in front instead of the disc above it.
- The storeys are joined by ladders (a jump can't climb 7 blocks); a press
  on a ladder jumps off it.
"""

from lib import Level

W, H = 40, 24
L = Level(W, H, name="BONUS - TRIGGER HAPPY", episode=6, theme="fly_tower", music="theme_arena_rock",
          rules="jumpfire", timer=60, goal="collect:24")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def ladder(x, top, bottom):
    L.col(x, top, bottom, "H")
    for y in range(top, bottom + 1):
        assert L.get(x - 1, y) in ".=^", (x, y, L.get(x - 1, y))


L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.fill(1, W - 2, 22, 23, "#")                 # storey 1
L.row(2, 37, 15, "=")                         # storey 2
L.row(16, 17, 15, ".")
L.row(2, 37, 8, "=")                          # storey 3
L.row(20, 21, 8, ".")
ladder(36, 15, 21)
ladder(3, 8, 14)
put(3, 21, "P")
# The boards (shot height on each floor).
for x, ground in ((12, 22), (24, 15), (28, 8)):
    L.fill(x, x, ground - 2, ground - 1, "#")
    L.at("breakable", rect=(x, ground - 2, x, ground - 1), hp=1, by="any")
# The discs: 8 a storey, a block wide and two tall at jump height.
for xs, ground in (((6, 10, 14, 18, 22, 26, 30, 34), 22), ((33, 29, 25, 21, 17, 13, 9, 5), 15),
                   ((6, 10, 14, 18, 22, 26, 30, 34), 8)):
    for x in xs:
        L.at("target", x, ground - 5)
n = 0
for xs, row in (((5, 7, 9, 15, 17, 20, 24, 28, 31, 33), 21), ((34, 31, 27, 22, 20, 13, 11, 8, 6, 5), 14),
                ((5, 8, 11, 13, 16, 19, 23, 26, 31, 35), 7)):
    for x in xs:
        put(x, row, "g")
        n += 1
assert n == 30, n
L.write(__file__, "40_bonus_trigger.txt")
