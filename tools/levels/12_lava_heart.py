#!/usr/bin/env python3
"""Level 12 - Lava Heart (SPEC.md 12). Twist: Sinking Stones. Prototype: Serpent Spear.

Horizontal and descending, left to right: the practice pool (start, top
left), the stone chains over the lava lake, the serpent shaft (drop in,
climb out on spears), the crab ferries across the lava river and the
rising bridge to the exit (bottom right).

Basalt stones (`@ platform mode=sink`) sink under a runner (Rocco faster),
a crab or a toad, and rise back when free; they stop a block under the
lava. Lava costs 2 hearts and pops you back to the last solid ground
(the practice pool 1; Rocco's shelf burns a heart every 20 frames and
holds you). Ferries are sinking stones with a path and dock=1: they cross
15 frames after you board, sinking with their load, and go back empty a
while after you get off. The bridge segments come up out of the lava
when you stand on the last island. Serpent Spears stick in walls as
footholds a jump higher than where you threw them.

Changes from the spec, so the route plays as intended in this engine:
- The lake is two rows deep over a basalt floor (ground 26): Rocco's
  shelf is its west part (x 50-79, wade=1) and the rest burns (x 80-89).
  The shelf ends at a drop (x 80-81) into Rocco's tunnel, ground 30,
  which runs under the lake to the shaft.
- The lavafall on the shaft's west side starts below the tunnel's mouth
  (rows 31-45), so the tunnel opens into the shaft.
- The serpent heads (`@ serpent`) are props; the camera sits in the river
  head's eye (160, 30).
- The bridge segments are `@ platform mode=rise trigger=169..170`, each
  10 frames after the one before; they wait at row 46 under the lava.
- Two more notches sit at the corridor's mouth (107, 26 and 23): from
  them a spear thrown at the top of a jump starts the climb past the
  corridor's ceiling to the forge (Rocco's short jump needs both).
- The marshmallow is `@ marshmallow`; the forge hearth is the lava pool
  with hearth=1.
"""

from lib import Level

W, H = 200, 50
L = Level(W, H,
          name="STAGE 12 - LAVA HEART", episode=2, theme="basalt_magma",
          music="theme_deep_drums", weapon="serpent_spear", par=360, flags="")

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


def stone(x, top, w=2, **keys):
    L.at("platform", x, top, w=w, mode="sink", **keys)


# --- 1. The practice pool (x 0-40, ground 12) ----------------------------------------------
room(1, 40, 3, 11)
room(10, 29, 12, 15)           # the pool's pit
L.at("fluid", rect=(10, 14, 29, 15), kind="lava", shallow=1)
for x in (11, 15, 19, 23, 27):
    stone(x, 12)
on(2, 12, "P")
on(6, 12, "h")
L.put(20, 8, "1")              # letter G over the pool
on(36, 12, "W")
on(38, 12, "m")
gem((11, 10), (12, 10), (15, 10), (16, 10), (19, 10), (23, 10), (27, 10), (28, 10))

# --- 2. The stone chains (x 41-95) --------------------------------------------------------
room(41, 45, 5, 14)            # ground 15
room(46, 49, 5, 17)            # ground 18
room(50, 89, 5, 25)            # over the lake (its floor is ground 26)
room(90, 97, 5, 20)            # the landing, ground 21, and the shaft's lip
L.fill(90, 95, 21, 49, "#")
L.at("fluid", rect=(50, 24, 81, 25), kind="lava", wade=1)   # Rocco's shelf
L.at("fluid", rect=(82, 24, 89, 25), kind="lava")
room(80, 81, 26, 29)           # the shelf's end drops into Rocco's tunnel
room(80, 97, 27, 29)           # the tunnel, ground 30
for x in range(50, 90, 5):
    stone(x, 21)
L.at("deco", 44, 13, kind="42", text="XLII", w=1, h=2)
L.at("wisp", 52, 17, carrier=1)
for x in (58, 68, 78):
    L.at("toad", x, 25)
L.at("wisp", 75, 16)
on(92, 21, "c")
on(94, 21, "h")
on(78, 26, "m")                # on the shelf, under the lava
gem((43, 13), (47, 16), (51, 19), (56, 19), (61, 19), (66, 19), (71, 19), (76, 19), (81, 19), (86, 19),
    (91, 18), (93, 18))

# --- 3. The serpent shaft (x 96-125) -------------------------------------------------------
room(96, 107, 10, 45)          # the shaft (floor ground 46)
L.at("fluid", rect=(96, 31, 97, 45), kind="lava")   # the lavafall
L.ground(98, 107, 46)
for y in (42, 38, 34, 26, 23):
    L.put(107, y, "=")         # notches in the east wall (the last two at the corridor's mouth)
room(108, 125, 23, 25)         # the exit corridor, ground 26 (its ceiling slab is row 22)
L.at("serpent", 100, 12, dir="r")
L.at("serpent", 106, 12, dir="l")
on(100, 46, "W")
L.at("respawn", 100, 45, frames=150)
on(104, 46, "T")
on(106, 46, "h")
on(112, 26, "c")
on(121, 26, "W")
on(124, 26, "h")
L.at("wisp", 120, 24)
gem((106, 43), (106, 39), (106, 35), (106, 31), (106, 28), (104, 20), (102, 24), (100, 28), (102, 32), (104, 36))

# 3a. The old forge (secret 1): in at the sill from the shaft, the chimney up to the bonus.
room(108, 120, 11, 15)         # ground 16
L.at("fluid", rect=(119, 16, 120, 16), kind="lava", shallow=1, hearth=1)
L.clear(119, 120, 16, 16)
room(109, 115, 1, 4)           # the chimney top, ground 5
L.col(112, 5, 15, "H")
L.clear(111, 111, 5, 10)       # the chimney (a ladder shaft is two blocks wide)
L.put(115, 15, "2")            # letter U
on(113, 16, "m")
L.at("marshmallow", 117, 15)
L.put(111, 2, "B")
L.put(113, 4, "Q")
gem((109, 14), (110, 14), (114, 13), (116, 13), (118, 14), (110, 3))

# 3b. The ash vent (secret 2): shoot its cracked floor from the corridor.
room(116, 121, 18, 21)         # ground 22
L.at("breakable", rect=(118, 22, 119, 22), hp=4, by="any")
on(118, 22, "$")
on(120, 22, "m")

# --- 4. The crab ferries (x 126-170) ---------------------------------------------------------
room(126, 170, 18, 40)
room(138, 165, 41, 49)         # the river's bed
L.at("fluid", rect=(138, 41, 165, 49), kind="lava")
L.ground(126, 129, 30)
L.ground(130, 133, 34)
L.ground(134, 137, 36)         # I1
L.ground(150, 153, 36)         # I2
L.ground(166, 170, 36)         # I3
L.at("platform", 138, 36, "F1", w=4, mode="sink", path=[(138, 36), (146, 36)], speed="1/2", dock=1)
L.at("platform", 154, 36, "F2", w=4, mode="sink", path=[(154, 36), (162, 36)], speed="1/2", dock=1)
L.at("crab", 140, 35)
L.at("crab", 156, 35, hat=1)
L.at("serpent", 160, 30, dir="l")
L.put(160, 30, "C")
L.put(158, 32, "3")            # letter N over the river
L.at("wisp", 145, 30)
on(151, 36, "c")
on(153, 36, "h")
on(168, 36, "c")
on(170, 36, "h")
gem((127, 28), (131, 32), (135, 34), (140, 32), (143, 32), (146, 32), (155, 32), (162, 32), (165, 32), (167, 34))

# --- 5. The rising bridge and the exit (x 171-199) -------------------------------------------
room(171, 198, 18, 43)
room(171, 186, 44, 49)
L.at("fluid", rect=(171, 44, 186, 49), kind="lava")
for i, x in enumerate((171, 175, 179, 183)):
    L.at("platform", x, 36, "BB%d" % (i + 1), w=4, mode="rise", start=46, trigger="169..170", delay=10 * i)
L.ground(187, 198, 40)         # the exit ledge
L.at("wisp", 180, 30)
on(192, 40, "m")
on(196, 40, "X")
gem((172, 34), (176, 34), (180, 34), (184, 34), (189, 38), (194, 38))

L.write(__file__, "12_lava_heart.txt")
