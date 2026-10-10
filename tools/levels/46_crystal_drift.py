#!/usr/bin/env python3
"""Level 46 - Crystal Drift (docs/DEEP_SPACE.md). Twist: Swap Crystals. Prototype: Swap Rifle.

Episode 7, DEEP SPACE, high in the planet Vurr's pale sky: islands of violet
and teal crystal floating over a sea of cloud, left to right, island to
island. Fall between them and you are gone.

`@ crystal x y` is a Swap Crystal hovering in block (x, y): shoot it and you
and it trade places in a flash, so the crystal ends up where you stood (and
the way back needs another). The gaps between the islands are too wide to
jump; each has a crystal on the far side. `path=x,y;...` makes one drift
back and forth (`speed=` frames a cell) until it is first swapped, so the
shot has to be timed for when it is in range; high ones hover over a
crystal slab (one-way, shots go through it) and are shot from below.
`cracked=1` is the Virus's green cracked crystal; `hidden=1` only shows in a
pool's reflection (`@ pool rect=`, a mirror-still sheet on the floor).

Shape: 1. the first gap (learn the swap) and the first Blinker; 2. the Swap
Rifle, a drifting crystal, a Shard Golem's island, a Prism Bat over the
next gap; 3. the cliff: a crystal over a slab against its face takes you up;
Blinkers and a Prism Bat on the long island beyond; 4. a crystal drifting up
and down across the last gap, the second golem and the exit.

Secrets: the pool on the second island shows a crystal hovering where
nothing is (the hidden one) beside a little island with the 42 and a gem
cache; a chain of three crystals up from the fifth island leads to an
island above the clouds with the rubber duck. The Virus: the cracked crystal
hovering over the golem's island.
"""

from lib import Level

W, H = 220, 40
L = Level(W, H,
          name="STAGE 46 - CRYSTAL DRIFT", episode=7, theme="crystal_drift",
          music="theme_crystal_drift", weapon="swap_rifle", par=270, flags="")


def isle(x0, x1, top, depth=6):
    """A floating island: flat on top (row `top`), tapering underneath."""
    for d in range(depth):
        cut = 0 if d < 2 else (d - 1) * max(1, (x1 - x0) // (depth * 2 + 2))
        if x0 + cut > x1 - cut:
            break
        L.fill(x0 + cut, x1 - cut, top + d, top + d, "#")


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def crystal(x, y, **keys):
    L.at("crystal", x, y, **keys)


# --- 1. The first gap (x 0-58) ------------------------------------------------------------
isle(0, 22, 26, 8)
on(4, 26, "P")
on(10, 26, "h")
on(16, 26, "m")
gem((8, 24), (12, 23), (18, 24))
# Gap x 23-30: too wide to jump; the crystal on the far side.
isle(31, 58, 26, 8)
crystal(31, 25)
on(38, 26, "c")
gem((36, 23), (41, 23))
L.at("blinker", 52, 25)
# The letter G on a slab, a crystal over it: shoot up from under it.
L.row(42, 46, 19, "=")
crystal(44, 18)
L.put(42, 18, "1")
gem((45, 17))
# Secret: the pool. It shows a crystal hovering where nothing is, beside a
# little island with the 42 and a gem cache.
L.at("pool", rect=(46, 26, 54, 26))
crystal(49, 19, hidden=1)
isle(50, 55, 20, 2)
L.at("deco", 54, 19, kind="42", text="XLII", w=1, h=2)
gem((51, 19), (52, 18), (53, 19))
on(52, 20, "m")

# --- 2. The Swap Rifle, the drift, the golem's island (x 59-115) --------------------------
isle(59, 76, 23, 7)
L.fill(59, 60, 24, 25, "#")     # the step up from the second island
on(62, 23, "c")
on(66, 23, "W")
on(72, 23, "h")
gem((64, 21), (69, 20), (74, 21))
# Gap x 77-84; on the far island a crystal drifts back and forth: shoot it
# while it is close.
isle(85, 106, 23, 7)
crystal(85, 22, path=[(91, 22)], speed=4)
L.at("shard_golem", 98, 22)
on(90, 23, "m")
gem((89, 20), (94, 20), (101, 19))
# The Virus: the cracked crystal hovering high over the golem's island.
crystal(100, 15, cracked=1)
# Gap x 107-114: a Prism Bat flutters over it; a crystal across it.
L.at("prism_bat", 111, 19)
isle(115, 142, 23, 8)
crystal(115, 22)
on(120, 23, "c")
on(124, 23, "W")
on(130, 23, "T")
on(136, 23, "h")
L.put(127, 19, "2")             # letter U
gem((122, 20), (133, 20), (139, 21))
# Secret: up the chain of crystals to the island above the clouds.
L.row(124, 128, 16, "=")
crystal(126, 15)
L.row(128, 132, 9, "=")
crystal(130, 8)
isle(132, 140, 3, 2)
crystal(131, 2)
on(136, 3, "Q")
on(138, 3, "m")
gem((133, 1), (134, 2), (135, 1), (139, 2))

# --- 3. The cliff, and the long island (x 143-188) ----------------------------------------
isle(143, 186, 23, 8)
L.fill(152, 155, 15, 22, "#")   # the cliff: too tall to climb
L.row(148, 151, 15, "=")        # a slab against its face, the crystal over it
crystal(149, 14)
gem((147, 21), (150, 13), (154, 13))
on(160, 23, "c")
L.at("blinker", 166, 22)
L.at("blinker", 178, 22)
L.at("prism_bat", 172, 18)
L.put(170, 19, "3")             # letter N
on(164, 23, "h")
on(182, 23, "m")
gem((162, 20), (168, 20), (175, 20), (184, 21))

# --- 4. The last gap, the second golem and the exit (x 187-219) ---------------------------
# Gap x 187-194; the crystal drifts up and down on the far side: shoot it
# when it comes down level with you.
isle(195, 219, 23, 8)
crystal(195, 22, path=[(195, 16)], speed=3)
L.at("shard_golem", 205, 22)
on(200, 23, "h")
gem((201, 20), (208, 19), (212, 20))
on(215, 23, "X")

L.write(__file__, "46_crystal_drift.txt")
