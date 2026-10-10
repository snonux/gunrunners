#!/usr/bin/env python3
"""Extra level - Deep Dive. The submarine level.

The harbour's reef, left to right under the water: four reef walls stand
up out of the sea and the way on is under each of them, and the passage
under the second is grown shut with coral that only torpedoes break
(`by=vehicle`). Sea mines, piranhas, jellyfish and an anglerfish in the
deep chamber. The submarine waits at the left dock (`bot=1`, `drop=` at
the right dock); a runner can swim it too, surfacing between the walls to
breathe.
"""

from lib import Level

L = Level(200, 52, name="EXTRA - DEEP DIVE", theme="deep_sea", music="theme_deep_dive",
          par=360, flags="")

# The docks and the sea bed.
L.ground(0, 11, 12)
L.ground(176, 199, 12)
L.col(0, 0, 11, "#")
L.col(199, 0, 11, "#")
L.ground(12, 175, 48)
L.at("sea", rect=(12, 12, 175, 47))
L.on(4, 12, "P")
L.at("vehicle", 12, 13, kind="submarine", bot=1, drop=(172, 13))

# The reef walls, each standing out of the water; the way is under them.
for x0, bottom in ((30, 38), (70, 36), (110, 40), (150, 37)):
    L.fill(x0, x0 + 3, 4, bottom, "#")

# Coral grown across the passage under the second wall.
L.fill(70, 73, 37, 47, "#")
L.at("breakable", rect=(70, 37, 73, 47), hp=5, by="vehicle", look="rock")

# Rocks on the bed and a ledge or two.
for x0, x1, top in ((40, 47, 44), (56, 60, 42), (84, 92, 43), (98, 103, 45), (136, 143, 44), (160, 166, 45)):
    L.fill(x0, x1, top, 47, "#")
L.fill(120, 128, 22, 24, "#")

# What lives down here.
for x, y in ((50, 30), (90, 38), (131, 42), (142, 28)):
    L.at("sea_mine", x, y)
for x, y in ((45, 20), (95, 24), (135, 18)):
    L.at("piranha", x, y)
for x, y in ((62, 24), (104, 36), (164, 28)):
    L.at("jellyfish", x, y)
L.at("angler", 118, 46)

# Gems in the nooks; a health box on the far side.
for x, y in ((36, 44), (52, 46), (66, 46), (78, 44), (94, 41), (116, 44), (124, 20), (146, 46), (156, 42)):
    L.put(x, y, "g")
L.on(180, 12, "h")
L.on(184, 12, "c")
L.on(194, 12, "X")

L.write(__file__, "extra_deep_dive.txt")
