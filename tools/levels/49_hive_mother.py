#!/usr/bin/env python3
"""Level 49 - The Hive Mother (docs/DEEP_SPACE.md). Boss. Prototype: Star Seed.

Episode 7, DEEP SPACE, the last level: down through the hive's root tunnels
to the egg chamber at its heart, where the Hive Mother sits on her throne.
`@ hive_mother x y arena= door=` is her: her left block on the throne, the
arena inside its walls, the door that shuts behind you. The fight runs in
three phases (world_mother.cpp): her crown glows (shoot it; jump-shoot it
while she bows to lay an egg, or from the upper ledges), she breathes in
(shoot the sacs on her flanks while she pulls you in), and she charges
(get up on a ledge, then shoot her back while she is dazed against the
wall). Spore Nurses heal her while they live. The Hive Remembers: goo on
the chamber's west wall reaches her crown, and two Swap Crystals up high
swap you out of her charge (both optional; the ledges do the same). The
exit is the hatch in her throne, open once she falls.

Shape: the root tunnel with the Star Seed and the first eggs; a goo wall
up to the gallery; the gallery east, stepped ledges up to a secret cache;
a drop into the antechamber (checkpoint, hearts); the arena door.

Secrets: the cache high over the gallery (gems, merch, the 42, the rubber
duck). The Virus: the sickly green Egg Guard on the gallery.
"""

from lib import Level

W, H = 150, 36
L = Level(W, H,
          name="STAGE 49 - THE HIVE MOTHER", episode=7, theme="hive_mother",
          music="theme_hive_mother", weapon="star_seed", par=360, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


G = 30   # the root tunnel's and the arena's floor
UP = 25  # the gallery's floor

# --- A: the root tunnel (x 1-39) ---------------------------------------------------------------
L.clear(1, 39, 18, G - 1)
on(3, G, "P")
on(7, G, "h")
on(10, G, "W")                  # the Star Seed
L.at("egg_guard", 18, G - 1)
L.at("spore_nurse", 23, G - 6)
L.row(25, 27, G - 3, "=")       # a resin shelf: the letter G on it
on(26, G - 3, "1")
on(30, G, "c")
L.at("egg_guard", 35, G - 1)
gem((12, G - 3), (21, G - 4), (29, G - 2), (33, G - 4))

# --- B: the goo wall up to the gallery (x 40) ------------------------------------------------
L.clear(40, 95, 14, UP - 1)
L.at("goo", rect=(40, UP, 40, G - 1))
gem((38, UP - 2), (38, G - 4))

# --- C: the gallery (x 41-95) ----------------------------------------------------------------
L.at("egg_guard", 50, UP - 1)
on(55, UP, "2")                 # letter U
on(46, UP, "h")
L.at("spore_nurse", 62, UP - 7)
# Secret: ledges up to the cache under the gallery's roof.
L.row(58, 60, UP - 3, "=")
L.row(62, 64, UP - 6, "=")
L.row(66, 73, UP - 9, "=")
on(67, UP - 9, "m")
on(70, UP - 9, "Q")
L.at("deco", 72, UP - 10, kind="42", text="XLII", w=1, h=2)
gem((68, UP - 10), (69, UP - 11), (71, UP - 10), (63, UP - 8))
L.at("egg_guard", 76, UP - 1, carrier=1)  # the Virus
L.at("egg_guard", 86, UP - 1)
on(80, UP, "m")
gem((52, UP - 3), (78, UP - 4), (90, UP - 3))

# --- D: the antechamber (x 96-108) -----------------------------------------------------------
L.clear(96, 108, 18, G - 1)
on(98, G, "3")                  # letter N
on(101, G, "c")
on(104, G, "h")
on(106, G, "h")
gem((99, G - 4), (103, G - 3))

# --- E: the egg chamber (x 110-148) ----------------------------------------------------------
L.clear(110, 148, 12, G - 1)
L.clear(109, 109, G - 3, G - 1)  # the door
# Low ledges three blocks up: over her when she charges, and level with
# her crown when she bows. Higher ledges for jump shots at her crown.
for x0 in (112, 119, 126, 133):
    L.row(x0, x0 + 3, G - 3, "=")
for x0 in (116, 129, 137):
    L.row(x0, x0 + 2, G - 6, "=")
on(127, G - 3, "h")
# The Hive Remembers: goo on the chamber's west wall to climb to her crown,
# and two Swap Crystals up high to swap out of the way of her charge.
L.at("goo", rect=(109, 14, 109, G - 4))
L.at("crystal", 115, G - 9)
L.at("crystal", 135, G - 9)
gem((113, G - 5), (120, G - 5), (134, G - 5), (117, G - 8), (130, G - 8))
L.at("hive_mother", 141, G - 1, arena=(110, 12, 148, G - 1), door=(109, G - 3, 109, G - 1))
on(144, G, "X")                 # the hatch in her throne

L.write(__file__, "49_hive_mother.txt")
