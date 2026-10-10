#!/usr/bin/env python3
"""Level 45 - Hive Gullets (docs/DEEP_SPACE.md). Twist: Gullet Tubes. Prototype: Bile Blaster.

Episode 7, DEEP SPACE, inside the hive on the planet Vurr. A hub with wings
joined by tubes: the throat (start, west), the tube up into the great hub,
the hub's floor mouth (a valve on its tube picks the branch) down to the
east wing, the breathing mouth at the east wing's end up to the crown
(the exit chamber, top right).

`@ tube` is a Gullet Tube: `path=` from its mouth block to its exit block
(blocks; wall mouths are two blocks tall, the block given and the one
above; floor mouths two wide, the block given and the one right of it),
`in=` the side its mouth opens to, `out=` the side its exit opens to.
`alt=`/`altout=` with `valve=x,y` is a second branch the valve (shoot it)
switches to. `breath=1` mouths only open while the hive breathes in (40
frames of every 90). `look=pore` is a small unmarked pore.

Secrets: the hub's valve (in a niche in its east wall) sends the floor
mouth's tube up to the vault (the gem cache and the 42), with a tube back
to the hub; a small pore low in the throat's west wall leads down to a
pocket with the rubber duck, and a tube back up.
"""

from lib import Level

W, H = 160, 58
L = Level(W, H,
          name="STAGE 45 - HIVE GULLETS", episode=7, theme="hive_gullets",
          music="theme_hive_gullets", weapon="bile_blaster", par=300, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def tube(tid, path, inn, out, **keys):
    L.at("tube", None, None, tid, path=path, in_=inn, out=out, **keys)


# --- 1. The throat (start, x 1-34): ground 46, a two-block step at x 20 ---------------------
room(1, 34, 38, 45)
L.ground(20, 34, 44, 45)
on(4, 46, "P")
on(8, 46, "h")
on(14, 46, "m")
on(30, 44, "m")
gem((10, 42), (12, 41), (16, 42), (24, 40), (27, 40))
L.at("pore", 20, 45, dir="l", count=3)    # mites out of the step's face
# The first mouth: the throat's east wall swallows you up into the hub.
tube("throat", [(35, 43), (37, 43), (37, 33), (44, 33)], "l", "r")

# 1a. Secret: a small pore low in the west wall, down to the duck's pocket,
# and a tube back up through the throat's floor.
room(2, 12, 49, 53)
on(8, 54, "Q")
on(5, 54, "m")
gem((4, 51), (6, 51), (10, 51), (11, 51))
tube("pore", [(0, 45), (0, 53), (1, 53)], "r", "r", look="pore")
tube("pocket", [(13, 53), (14, 53), (14, 48), (10, 48), (10, 46)], "l", "u", look="pore")

# --- 2. The hub (x 45-104, rows 18-33, ground 34) -------------------------------------------
room(45, 104, 18, 33)
on(48, 34, "c")
on(52, 34, "W")
on(57, 34, "h")
# Shelves of cartilage up the hub's west half, the letter G at the top.
for x0, x1, y in ((58, 61, 32), (63, 66, 30), (68, 71, 28), (73, 76, 26)):
    L.row(x0, x1, y, "=")
L.put(74, 23, "1")
gem((59, 30), (64, 28), (69, 26), (75, 24))
L.at("drone_warden", 74, 24)
# A green dripping pore in the ceiling, the Virus under it.
L.at("drip", 66, 18)
L.put(66, 22, "V")
# A raised block with a Polyp on its face.
L.ground(84, 86, 32, 33)
L.at("polyp", 82, 33, dir="l")
on(85, 32, "m")
on(100, 34, "W")
gem((88, 31), (91, 31), (101, 30))
# The floor mouth: its tube runs to the east wing, or (the valve in the
# niche in the east wall turned) up to the vault.
L.clear(105, 105, 32, 32)
tube("hub", [(96, 34), (96, 37), (105, 37), (105, 32), (110, 32), (110, 44), (114, 44)], "u", "r",
     alt=[(96, 34), (96, 37), (105, 37), (105, 32), (105, 26), (108, 26)], altout="r", valve=(105, 32))
# The vault's way back out lands on a shelf high in the hub.
L.row(100, 104, 22, "=")

# 2a. Secret: the vault (x 109-118, ground 27).
room(109, 118, 22, 26)
on(112, 27, "$")
on(116, 27, "m")
L.at("deco", 114, 25, kind="42", text="XLII", w=1, h=2)
gem((110, 24), (111, 23), (117, 24))
tube("vault", [(119, 26), (121, 26), (121, 21), (105, 21)], "l", "l")

# --- 3. The east wing (x 115-156, ground 45, steps at 128 and 141) -----------------------------
room(115, 156, 37, 44)
L.ground(128, 156, 43, 44)
L.ground(141, 156, 41, 42)
on(118, 45, "c")
on(121, 45, "h")
L.put(124, 41, "2")
L.at("pore", 128, 44, dir="l", count=3)
on(133, 43, "T")
L.at("drone_warden", 134, 38)
L.at("polyp", 139, 42, dir="l")
on(146, 41, "m")
gem((119, 42), (126, 40), (131, 40), (137, 39), (144, 38), (150, 38))
# The breathing mouth at the far end, up to the crown.
tube("crown", [(157, 40), (158, 40), (158, 11), (157, 11)], "l", "l", breath=1)

# --- 4. The crown (x 112-156, ground 14, a ledge from 141 at ground 12) ------------------------
room(112, 156, 6, 13)
L.ground(141, 156, 12, 13)
on(147, 12, "c")
L.at("hive_mite", 150, 11)
L.at("hive_mite", 152, 11)
on(136, 14, "h")
on(130, 14, "m")
L.put(126, 10, "3")
gem((138, 10), (133, 10), (122, 10), (119, 10))
on(115, 14, "X")

L.write(__file__, "45_hive_gullets.txt")
