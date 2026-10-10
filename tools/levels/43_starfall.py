#!/usr/bin/env python3
"""Level 43 - Starfall (docs/DEEP_SPACE.md). Twist: flying the ship. No prototype: the ship's guns.

Episode 7, DEEP SPACE, the first level: you start in the cockpit of the old
courier ship (`@ vehicle kind=spaceship pilot=1`) and fly it the whole way,
left to right, over the planet Vurr, then down through its clouds to the
landing pad and the exit. `flags=space`: the map's top and bottom are walls
for the ship, and there is no climbing out until it lands; a wreck is a
death, and you respawn in a new ship at the last beacon you flew through.

Asteroids (`@ rock x y size=1..3 v=vx,vy`, v in sixteenths of a cell a
frame) drift through it. Shots push them along and chip at them: a big
rock splits in two middle ones, those split into small ones, and the small
ones crumble, so you clear your own lane. Rock Leeches ride the big rocks
and spit; Void Rays sweep across in waves, light their fins and dive.

Shape: 1. open space, a few slow rocks to learn the ship; 2. the belt,
rocks with Rock Leeches on them, then the rock river, a long tunnel
between two walls of rock with asteroids streaming down it at you; 3. Void
Ray formations around a zig-zag through the rock; 4. down into Vurr's
clouds (`@ clouds x`) to the landing pad. Each part ends in a wall of rock
with a gap and a beacon floating in the gap (the checkpoints).

Secrets: a hollow asteroid at the top of the belt, its crust broken only by
the ship's guns (`by=vehicle`), full of gems; a derelict probe tucked under
a rock in part 3, the duck (in a space helmet) floating in its window and
SN-42 on its side. The Virus is a green comet up out of the way in part 3.
"""

from lib import Level

W, H = 300, 30
L = Level(W, H,
          name="STAGE 43 - STARFALL", episode=7, theme="starfall",
          music="theme_starfall", par=240, flags="space")


def blob(cx, cy, rx, ry, c="#"):
    """A roundish lump of rock, rx by ry blocks either side of its middle."""
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            dx, dy = (x - cx) / (rx + 0.5), (y - cy) / (ry + 0.5)
            if dx * dx + dy * dy <= 1.0:
                L.put(x, y, c)


def gate(x, top):
    """A wall of rock across the whole map at x..x+2, open for four rows
    from `top`, with a beacon floating in the middle of the gap (the ship
    can't get through without touching it)."""
    bottom = top + 3
    for y in range(H):
        if top <= y <= bottom:
            continue
        d = min(abs(y - top), abs(y - bottom))
        w = 1 if d <= 1 else 2
        L.fill(x - w + 1, x + w + 1, y, y, "#")
    L.put(x + 1, top + 2, "c")


def wall(x0, x1, rows, top):
    """A ragged wall of rock along the top (top=True) or bottom of the map,
    `rows` deep give or take a block."""
    for x in range(x0, x1 + 1):
        d = rows + (1 if (x * 7) % 5 == 0 else 0) - (1 if (x * 3) % 7 == 0 else 0)
        if top:
            L.fill(x, x, 0, d - 1, "#")
        else:
            L.fill(x, x, H - d, H - 1, "#")


def rock(x, y, size, vx, vy):
    L.at("rock", x, y, size=size, v=(vx, vy))


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


# --- 1. Open space (x 0-51): learn the ship ------------------------------------------------
L.put(5, 14, "P")
L.at("vehicle", 4, 14, kind="spaceship", pilot=1, bot=1)
blob(20, 5, 3, 2)
blob(27, 23, 4, 3)
blob(38, 9, 2, 2)
rock(22, 13, 1, -2, 0)
rock(31, 16, 2, -2, 1)
rock(40, 13, 2, -2, -1)
rock(46, 18, 1, -1, 1)
gem((12, 14), (14, 14), (16, 14), (30, 13), (32, 12), (34, 11))
L.put(26, 15, "1")              # letter G
blob(44, 23, 2, 1)
L.on(44, 22, "h")               # a health box on the rock
gate(52, 12)

# --- 2a. The belt (x 56-129): rocks everywhere, Rock Leeches on the big ones ----------------
for cx, cy, rx, ry in ((60, 6, 3, 2), (64, 24, 3, 3), (70, 13, 2, 2), (77, 26, 3, 2),
                       (82, 18, 2, 2), (88, 9, 2, 2), (93, 22, 3, 2), (100, 13, 2, 2),
                       (106, 25, 3, 3), (112, 9, 2, 2), (117, 18, 2, 2), (124, 6, 2, 2),
                       (125, 25, 2, 2)):
    blob(cx, cy, rx, ry)
for x, y, size, v in ((58, 13, 3, (-2, 1)), (66, 18, 2, (-3, -1)), (74, 8, 1, (-2, 2)),
                      (76, 14, 3, (-2, -1)), (85, 13, 2, (-3, 1)), (90, 15, 3, (-2, 0)),
                      (96, 7, 1, (-1, 2)), (104, 16, 3, (-2, 1)), (110, 14, 2, (-3, -1)),
                      (115, 11, 1, (-2, 1)), (120, 14, 3, (-2, -1)), (122, 20, 2, (-1, -2))):
    rock(x, y, size, *v)
for x, y in ((59, 12), (77, 13), (91, 14), (105, 15), (121, 13)):
    L.at("rock_leech", x, y)       # on the big rocks
gem((56, 14), (67, 12), (74, 17), (86, 16), (96, 12), (110, 18), (118, 13))
L.put(78, 23, "2")              # letter U, tucked over the low rock
# Secret: the hollow asteroid, high in the belt. Its west face is a crust
# only the ship's guns break; inside, a cave of gems.
blob(101, 3, 7, 3)
L.clear(97, 104, 2, 4)
L.fill(94, 96, 1, 5, "#")
L.at("breakable", rect=(94, 2, 96, 4), hp=3, by="vehicle", look="rock")
gem((98, 3), (100, 3), (102, 3), (99, 2), (101, 2), (103, 4))
L.on(104, 5, "m")               # and a merch box in the back

# --- 2b. The rock river (x 130-171): a tunnel with asteroids streaming down it -------------
wall(130, 171, 8, True)
wall(130, 171, 9, False)
for x, y, size, vy in ((136, 10, 2, 1), (142, 15, 3, -1), (148, 11, 2, 2), (153, 16, 2, -1),
                       (158, 10, 3, 1), (163, 14, 2, -2), (167, 17, 1, 1), (169, 10, 2, 0)):
    rock(x, y, size, -4, vy)
L.at("rock_leech", 143, 14)
L.at("rock_leech", 159, 9)
gem((134, 14), (145, 13), (156, 13), (166, 13))
gate(172, 11)

# --- 3. Void Rays and the zig-zag (x 176-241) ---------------------------------------------
for cx, cy, rx, ry in ((178, 25, 2, 2), (192, 4, 3, 2)):
    blob(cx, cy, rx, ry)
# The zig-zag: a wall from the top, then one from the bottom.
L.fill(204, 207, 0, 17, "#")
L.fill(218, 221, 12, H - 1, "#")
blob(219, 11, 2, 1)
for x, y in ((188, 8), (190, 10), (188, 12)):        # a V
    L.at("void_ray", x, y)
for x, y in ((210, 18), (212, 20), (214, 22), (216, 24)):  # a line, high to low
    L.at("void_ray", x, y)
for x, y in ((228, 5), (230, 7), (228, 9)):
    L.at("void_ray", x, y)
for x, y, size, v in ((184, 15, 3, (-2, 0)), (198, 9, 2, (-2, 1)), (212, 6, 2, (-1, 1)),
                      (226, 18, 3, (-1, -1)), (234, 12, 2, (-2, 1))):
    rock(x, y, size, *v)
L.at("rock_leech", 185, 14)
L.at("rock_leech", 227, 17)
gem((180, 13), (196, 14), (210, 23), (213, 25), (225, 6), (236, 15))
L.put(200, 3, "3")              # letter N, up by the zig-zag's wall
# Secret: the derelict probe, in a pocket under a ledge of rock, open to
# the east. The duck floats in its window.
L.fill(183, 196, 20, 20, "#")
L.fill(183, 183, 21, 28, "#")
L.fill(183, 196, 29, 29, "#")
L.at("probe", 185, 23, w=5, h=3)
L.put(187, 24, "Q")
gem((192, 24), (193, 26))
L.at("virus", 214, 2, skin="comet")
blob(238, 25, 2, 1)
L.on(238, 24, "h")
gate(242, 15)

# --- 4. Down through Vurr's clouds to the landing pad (x 246-299) --------------------------
L.at("clouds", 246, 0)
# Vurr's surface: spires and a mesa with the pad on it.
for x in range(246, W):
    top = 27
    if 254 <= x <= 257:
        top = 19 + abs(x * 2 - 511) // 2   # a spire
    elif 266 <= x <= 268:
        top = 21
    elif 276 <= x <= 278:
        top = 18 + (x - 276)               # another, leaning
    elif 284 <= x <= 296:
        top = 22                           # the mesa
    L.fill(x, x, top, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")
L.at("landingpad", rect=(285, 22, 295, 22))
for x, y, size, v in ((252, 10, 2, (-2, 1)), (262, 14, 3, (-1, -1)), (272, 9, 1, (-2, 1))):
    rock(x, y, size, *v)
L.at("void_ray", 264, 7)
L.at("void_ray", 266, 9)
L.at("void_ray", 280, 11)
gem((250, 18), (260, 16), (271, 18), (281, 15))
L.on(287, 22, "T")              # Turbo, on the pad
L.on(295, 22, "m")
L.on(292, 22, "X")

L.write(__file__, "43_starfall.txt")
