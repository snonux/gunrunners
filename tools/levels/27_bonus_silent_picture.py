#!/usr/bin/env python3
"""Level 27 bonus - Silent Picture (SPEC.md 27). rules=silent_film: no sound
at all, the logic runs at 1.5x (every other frame runs twice), banana peels
slide the runner 4 blocks, a landing from more than 4 blocks is a 15-frame
pratfall, shot extras lose their hats, and a title card holds the picture
for 30 frames at each room. timer=60 (real seconds), goal=exit at (67, 17).

Three rooms of 23 blocks on ground 18: the bakery (counters, peels), main
street running down four steps with a runaway pram (`@ pram`) rolling down
them every 90 frames, and the rooftop with the ladder gag (`@ laddergag`):
the gap is 9 blocks, too wide to jump; climb the ladder and it tips over
into a bridge with the runner on it. Three Flatfoot extras chase the
runner and fall over when they turn round. 20 gems.

Everything in world_noir.cpp (FilmState).
"""

from lib import Level

W, H = 70, 20
L = Level(W, H, name="BONUS - SILENT PICTURE", episode=4, theme="noir_city",
          music="theme_noir_trumpet", rules="silent_film", timer=60, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


L.ground(0, W - 1, 18)
L.col(0, 0, 17, "#")
L.row(0, W - 1, 0, "#")

# --- The bakery (x 0-22) ---------------------------------------------------------------------------
L.at("room", 0, 0, text="THE BAKERY")
put(2, 17, "P")
L.row(5, 9, 14, "=")                        # counters
L.row(13, 17, 13, "=")
L.at("peel", 8, 17)
L.at("peel", 16, 17)
L.at("flatfoot", 12, 17)
for x, top in ((20, 17), (21, 16), (22, 15)):
    L.fill(x, x, top, 17, "#")              # the steps up to the street
gem((3, 16), (6, 13), (8, 13), (14, 12), (16, 12), (11, 16), (19, 15))

# --- Main street, running down four steps (x 23-45) ------------------------------------------------
L.at("room", 23, 0, text="MEANWHILE, ON MAIN STREET...")
for x0, top in ((23, 14), (27, 15), (31, 16), (35, 17)):
    L.fill(x0, x0 + 3, top, 17, "#")
L.at("pram", 23, 13, to=45, every=90)
L.at("peel", 33, 15)
L.at("flatfoot", 41, 17)
gem((24, 12), (26, 12), (29, 13), (32, 14), (37, 15), (40, 16), (44, 16))

# --- The rooftop and the ladder gag (x 46-69) -------------------------------------------------------
L.at("room", 46, 0, text="AND UP ON THE ROOFS...")
L.clear(54, 62, 18, 19)                     # the gap
L.at("laddergag", 52, 17, h=8, len=10)
L.at("peel", 47, 17)
L.at("flatfoot", 64, 17)
put(67, 17, "X")
gem((49, 16), (52, 13), (61, 16), (63, 16), (65, 16), (66, 16))  # none past the exit

L.write(__file__, "27_bonus_silent_picture.txt")
