#!/usr/bin/env python3
"""Level 25 bonus - Dandelion Drift (SPEC.md 25). rules=glide: the runner
hangs from a seed head drifting right; Up and Down steer, gusts lift or
drop it, thorns (`^` and solid blocks) and dragonflies bounce it back 4
blocks. Through the gap in the fence to the exit at x 78.

Three screens: the lawn (two updraft columns between bramble stems), the
pond (dragonflies hovering up and down, a downdraft) and the fence (two
picket rows, the last one open only at rows 8-12). 30 gems on the gust
lines.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - DANDELION DRIFT", episode=4, theme="macro_garden",
          music="theme_string_quartet", rules="glide", timer=90, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


L.row(0, W - 1, H - 1, "#")                # the soil
put(1, 10, "P")

# --- The lawn (x 0-26) -----------------------------------------------------------------------------
L.fill(0, 26, 21, 22, "^")                  # grass burrs
L.col(11, 13, 20, "^")                      # a bramble stem
L.col(21, 0, 9, "^")                        # a bramble hanging from a branch
L.at("wind", rect=(6, 4, 8, 20), dir="u", push=1)
L.at("wind", rect=(15, 4, 17, 20), dir="u", push=1)
gem((4, 12), (6, 10), (7, 8), (8, 6), (10, 6), (13, 9), (15, 12), (16, 10), (17, 8), (19, 12))

# --- The pond (x 27-53) ----------------------------------------------------------------------------
L.fill(27, 53, 21, 22, "#")                 # the water (the seed head can't touch it)
L.at("dragonfly", 32, 10, range=4, speed="1/4")
L.at("dragonfly", 40, 6, range=3, speed="1/3")
L.at("dragonfly", 47, 14, range=4, speed="1/4")
L.at("wind", rect=(36, 2, 38, 12), dir="d", push=1)
L.col(44, 0, 3, "^")                        # reeds' tips over the pond's far side
gem((28, 15), (30, 16), (34, 16), (36, 14), (38, 15), (42, 12), (43, 9), (45, 7), (50, 8), (52, 10))

# --- The fence (x 54-79) ---------------------------------------------------------------------------
L.fill(54, W - 1, 21, 22, "^")              # nettles
L.col(60, 0, 5, "#")                        # the first pickets, a wide gap at rows 6-14
L.col(60, 15, 20, "#")
L.col(70, 0, 7, "#")                        # the last pickets, the gap at rows 8-12
L.col(70, 13, 20, "#")
L.at("wind", rect=(64, 12, 66, 20), dir="u", push=1)
put(78, 10, "X")
gem((56, 12), (58, 10), (60, 8), (60, 11), (62, 10), (65, 13), (66, 11), (68, 10), (70, 9), (73, 10))

L.write(__file__, "25_bonus_dandelion_drift.txt")
