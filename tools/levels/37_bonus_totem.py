#!/usr/bin/env python3
"""Level 37 bonus - Totem (SPEC.md 37). rules=totem: the three runners are
one stack (Rocco at the bottom, Dash, Nova on top) that walks at Rocco's
speed and can't jump. Jump throws the top runner 13 cells up and 4 blocks
on, and the player plays them from then on; holding down for 8 frames
hands control back (to Rocco's group first). Walking the stack into a
runner on its floor, or under one standing just above its top, stacks
them again. Heavy plates (weight 4) only go down under the whole stack.
timer=90, goal=exit.

Changes from the spec, so it plays in this engine: each "tower" is a
block 2 wide and 4 tall, 7 blocks above the floor, with a 3-block tunnel
under it that only a single runner fits through; the stack can't pass, so
Nova and Dash are thrown over it (onto its top, then off the far side)
while Rocco walks through the tunnel and picks them up on the heavy plate
behind it, which opens the gate. `@ tower` lines are the bot's script.
"""

from lib import Level

W, H = 64, 24
L = Level(W, H, name="BONUS - TOTEM", episode=6, theme="wardrobe", music="theme_jazz_bass",
          rules="totem", timer=90, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, 0, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
L.ground(1, W - 2, 22)
put(3, 21, "P")
put(61, 21, "X")
gems = []
for i, tx in enumerate((12, 30, 48)):
    L.fill(tx, tx + 1, 15, 18, "#")              # the tower, the tunnel under it
    L.at("switch", tx + 5, 21, "PL%d" % (i + 1), kind="plate", weight=4, latch=1, target="G%d" % (i + 1), w=2)
    L.at("shutter", None, None, "G%d" % (i + 1), rect=(tx + 9, 1, tx + 9, 21))
    L.at("tower", tx, 21, plate=tx + 5)
    gems += [(tx, 14), (tx + 1, 14), (tx, 13), (tx + 1, 13)]       # on the tower top
    gems += [(tx + 3, 19), (tx + 4, 17), (tx + 6, 19), (tx + 7, 17)]  # off its far side
    gems += [(tx - 6, 19), (tx - 4, 19), (tx - 2, 19)]             # the way up to it
    gems += [(tx + 5, 16), (tx + 6, 16), (tx + 8, 19)]              # over the plate
for x in range(4, 10, 2):
    gems.append((x, 20))
for x, y in gems:
    put(x, y, "g")

L.write(__file__, "37_bonus_totem.txt")
