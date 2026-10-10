#!/usr/bin/env python3
"""Level 22 - Dry Gulch (SPEC.md 22). Twist: Fuses. Prototype: Six-Shooter.

The town runs left to right on ground 22 from the start at the west edge; at
x 136 a mine shaft drops to a tunnel on ground 34 that runs back under the
town (the well drops into it at x 86) and east to a second shaft, which
climbs to the water tower and the stagecoach exit at the east edge.

Fuses (`@ fuse`) burn both ways from where they are lit, a block every two
frames, to their TNT barrels (`@ barrel`): the boulder in section 1, the
fuse yard's three in section 3 (the drawbridge, the rock, the balcony), the
hidden vault fuse of Secret 1 and the Rocco line's well.

Changes from the spec, so it plays in this engine:
- Buildings, poles, the wire, the winch, the steeple and the water tower's
  legs are background `@ deco`s; their floors, roofs and counters are tiles.
- The rock over the boulder in section 1 is a cliff from the top of the map
  down to row 16, so the boulder is the only way on.
- The Tumble Mine spawner stands on the tunnel floor (y 33).
- A mesa (x 140-174, rows 4-21) stands between the two shafts, so the way
  east is through the mine.
- The letter U is at (113, 26), beside the escape ladder over the spikes
  (an item can't share the ladder's tile).
- The fuses the bot lights carry `bot=1 stand=x` (where it lights them from).
"""

from lib import Level

W, H = 200, 38
L = Level(W, H,
          name="STAGE 22 - DRY GULCH", episode=4, theme="western_gulch",
          music="theme_western", weapon="six_shooter", par=330, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def deco(kind, rect=None, x=None, y=None, text=None):
    L.at("deco", x, y, kind=kind, rect=rect, text=text)


# --- Ground, walls ----------------------------------------------------------------------------------
L.ground(0, W - 1, 22)
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")

# --- 1. Main street (x 0-43) --------------------------------------------------------------------------
deco("sheriff", rect=(6, 15, 13, 21), text="SHERIFF")
deco("post", x=15, y=21)
L.at("deco", 8, 18, kind="42", text="BOUNTY: 42 GEMS")
# The cliff and its boulder (5 tall, cannot be jumped).
L.fill(29, 33, 0, 16, "#")
L.fill(30, 32, 17, 21, "#")
L.at("breakable", None, None, "BOULDER", rect=(30, 17, 32, 21), hp=1, by="explosion", look="rock")
L.at("fuse", None, None, "f1", path=[(18, 21), (26, 21)], to="b1", speed=1, bot=1, stand=14)
L.at("barrel", 27, 21, "b1", radius=3, damage=8, hurt=3, flash=12)
put(3, 21, "P")
put(10, 21, "W")
put(38, 21, "h")
gem((5, 20), (7, 20), (16, 19), (20, 19), (23, 19), (35, 20), (40, 20), (41, 19))

# --- 2. Saloon, church, bank (x 44-101) -------------------------------------------------------------
put(42, 21, "c")
deco("facade", rect=(44, 12, 62, 21), text="SALOON")
L.fill(45, 45, 17, 21, "H")          # ladder to the upper floor
L.fill(46, 61, 17, 17, "=")          # upper floor
L.fill(54, 58, 20, 21, "#")          # bar counter
L.fill(59, 61, 19, 19, "=")          # shelf
L.fill(44, 62, 13, 13, "=")          # roof
L.at("piano", 46, 20)
L.at("window_bandit", 47, 18, "WB1")
put(48, 16, "W")
put(58, 16, "T")
put(60, 18, "V")
L.at("vskin", 60, 18, skin="tonic_bottle", bob=0)
L.at("prize", 52, 15, kind="gold_revolver", score=10000)
put(50, 21, "m")
put(63, 21, "h")
# Church: its roof, the steeple and its bell (Secret 1: shoot it from 66).
deco("facade", rect=(64, 13, 68, 21), text="CHURCH")
deco("steeple", rect=(65, 6, 67, 12))
L.fill(64, 68, 13, 13, "=")
L.at("metal", 66, 8, out="r")
L.at("bell", 66, 8)
# Bank and its vault under the street.
deco("facade", rect=(70, 13, 82, 21), text="BANK")
L.fill(70, 82, 13, 13, "=")
L.at("window_bandit", 71, 19, "WB2")
L.clear(72, 80, 23, 27)
L.at("breakable", None, None, "HATCH", rect=(74, 22, 75, 22), hp=1, by="explosion", look="hatch")
L.fill(80, 80, 22, 27, "H")
L.at("fuse", None, None, "fh", path=[(69, 8), (70, 8), (70, 24), (74, 24)], to="b_vault", hidden=1)
L.at("barrel", 75, 24, "b_vault", radius=3, damage=8, hurt=3, flash=12)
L.at("metal", 78, 23, out="d", reveal=1)
put(73, 27, "m")
put(77, 27, "m")
put(76, 27, "1")
put(78, 25, "B")
# The dry creek under the footbridge (the plank at 84-85 is missing), and
# the Rocco line's well down into the mine.
L.clear(84, 90, 22, 24)
L.fill(86, 90, 22, 22, "=")
L.clear(85, 87, 26, 28)
L.at("breakable", None, None, "WELL", rect=(85, 25, 87, 25), hp=1, by="explosion", look="rock")
L.at("fuse", None, None, "fw", path=[(85, 24), (86, 24)], to="b_well")
L.at("barrel", 87, 24, "b_well", radius=3, damage=8, hurt=3, flash=8)
L.at("duelist", 94, 21, "D1")
L.at("deco", 91, 18, kind="42", text="BOUNTY: 42 GEMS")
# Street gems, roof gems (the Nova line).
gem((44, 21), (47, 21), (52, 21), (60, 21), (65, 20), (67, 20), (72, 21), (76, 21), (82, 21), (92, 20),
    (97, 20), (99, 20))
gem((46, 12), (50, 12), (54, 12), (58, 12), (62, 12), (66, 12), (72, 12), (76, 12), (80, 12), (82, 11))

# --- 3. Fuse yard (x 102-139) -----------------------------------------------------------------------
put(100, 21, "c")
L.clear(112, 117, 22, 27)
L.fill(113, 117, 27, 27, "^")
L.fill(112, 112, 22, 27, "H")
deco("pole", rect=(106, 12, 106, 21))
deco("pole", rect=(122, 12, 122, 21))
deco("wire", rect=(106, 12, 129, 12))
deco("winch", rect=(119, 16, 119, 21))
L.fill(118, 118, 16, 21, "#")       # the drawbridge, standing
L.at("drawbridge", None, None, "DB", hinge=(118, 21), len=6, falls="l", barrel="b_winch")
L.fill(126, 127, 17, 21, "#")
L.at("breakable", None, None, "ROCK", rect=(126, 17, 127, 21), hp=1, by="explosion", look="rock")
deco("facade", rect=(128, 12, 134, 21), text="ASSAY")
L.fill(128, 133, 16, 16, "#")
L.at("breakable", None, None, "BALCONY", rect=(128, 16, 133, 16), hp=1, by="explosion", look="crack")
L.at("window_bandit", 132, 15, "WB3")
put(108, 21, "W")
L.at("fuse", None, None, "fp", path=[(102, 21), (106, 21), (106, 12), (122, 12), (122, 21), (123, 21)],
     to="b_rock", speed=1, bot=1, stand=99)
L.at("fuse", None, None, "fa", path=[(119, 12), (119, 14)], to="b_winch", **{"from": "fp"})
L.at("fuse", None, None, "fc", path=[(122, 12), (129, 12), (129, 14)], to="b_balc", **{"from": "fp"})
L.at("barrel", 119, 15, "b_winch", radius=3, damage=8, hurt=3, flash=12)
L.at("barrel", 124, 21, "b_rock", radius=3, damage=8, hurt=3, flash=12)
L.at("barrel", 129, 15, "b_balc", radius=3, damage=8, hurt=3, flash=12)
put(133, 21, "h")
put(113, 26, "2")  # by the escape ladder, over the spikes
gem((103, 20), (105, 20), (108, 19), (110, 20), (120, 19), (125, 15), (130, 20), (135, 20))

# --- 4. The mine (x 84-178) ---------------------------------------------------------------------------
# A mesa between the two shafts: the only way east is through the mine.
L.fill(140, 174, 4, 21, "#")
deco("headframe", rect=(135, 17, 139, 21))
L.clear(136, 138, 22, 28)
L.clear(84, 178, 29, 33)
L.fill(137, 137, 22, 33, "H")
for x in (146, 156, 166):
    L.clear(x, x + 1, 34, 35)
L.at("wind", rect=(100, 29, 175, 33), push="1/4", dir="l", period=90, on=45)
put(140, 33, "c")
put(150, 33, "W")
put(152, 33, "h")
put(160, 33, "m")
put(157, 35, "3")
L.at("spawner", kind="tumble_mine", x=174, y=33, every=75, max=3)
gem((146, 35), (147, 35), (156, 35), (166, 35), (167, 35), (143, 33), (154, 33), (162, 33), (171, 33),
    (120, 33))

# --- 5. The water tower (x 176-199) ------------------------------------------------------------------
deco("headframe", rect=(175, 17, 179, 21))
L.clear(176, 178, 22, 28)
L.fill(177, 177, 22, 33, "H")
L.fill(184, 188, 11, 15, "#")
deco("legs", rect=(184, 16, 188, 21))
L.fill(189, 189, 11, 21, "H")
L.at("metal", None, None, rect=(184, 11, 188, 15), out="d")
deco("stagecoach", rect=(194, 18, 198, 21))
put(181, 21, "h")
L.at("duelist", 192, 21, "D2")
L.at("deco", 190, 18, kind="42", text="BOUNTY: 42 GEMS")
put(186, 10, "Q")
put(197, 21, "X")
gem((180, 20), (183, 20), (185, 10), (187, 10), (191, 20), (195, 16))

L.write(__file__, "22_dry_gulch.txt")
