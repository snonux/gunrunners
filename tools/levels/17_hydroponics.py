#!/usr/bin/env python3
"""Level 17 - Hydroponics (SPEC.md 17). Twist: Grow Lamps. Prototype: Hedge Trimmer.

Five greenhouse domes stacked bottom to top, linked by lifts at
alternating ends: the route zigzags up from the potting shed at the bottom
left (D1 east, D2 west, D3 east, D4 west on two tiers, D5 east) to the exit
on a staircase grown under the top dome's glass.

Grow Lamps: `@ switch` (shootable) lights the `@ lamp`s wired to it
(`timer=0` stays lit until shot again, `timer=T` goes out after T frames,
flickering for the last 22). While all of a `@ plant`'s lamps are lit it
grows from its root outward over 45 frames (bridge and stairs solid, leaf
and flower one-way, ladder a climbable creeper); once one goes out it
withers tip-first over 30 frames. A plant with several lamps (the top
dome's stairs) locks them on once it grows.

Hedge Trimmer: held, a spinning cone 2 blocks ahead of the runner and 1.5
tall, a cut every 3 frames (1 damage, 4 to thorn walls and seed pods, a
unit of ammo); it shreds spore clouds and enemy shots. Aimed up or down it
fires a plain shot (for switches overhead).

Changes from the spec, so it plays in this engine:
- Positions of enemies are the block they stand in (bottom row): the
  ceiling Spore Puffers hang at y 50, floor enemies stand on ground - 1.
- The trench's escape ladder is at x 29 (a ladder needs the block left of
  it open; x 27 is the shed's floor), rows 76-78.
- Thorn walls fill their dome from floor to ceiling (a 3-block wall is a
  jump for most runners).
- Planter ledges are one-way, so the floor runs on under them.
- The Rocco line's corridor is 3 blocks wide (x 105-107) between walls at
  x 103-104 and x 108-109, rows 22-27, with its deck hole 3 wide; two wall
  Snapjaws bite into it. The lower floor runs on under the walls.
- D4's lower tier is open from x 52 (the Nova shaft comes up at x 54-59)
  to x 117; the upper deck spans x 2-117.
- The trench water is `@ water` (wading at half speed, not deadly).
- The Glob lab is not a marked arena: this engine wakes every enemy on
  screen.
- The seed pod is a breakable (`by=trimmer look=seedpod`) with the candid
  camera inside it.
"""

from lib import Level

W, H = 120, 80
L = Level(W, H,
          name="STAGE 17 - HYDROPONICS", episode=3, theme="station_greenhouse",
          music="greenhouse_pulse", weapon="hedge_trimmer", par=210, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def thorns(x, y0, y1):
    L.at("breakable", None, None, rect=(x, y0, x, y1), by="any", hp=12, look="thorn")


# --- The domes ------------------------------------------------------------------------------
L.clear(2, 27, 64, 75)        # D1 west: ground 76
L.clear(28, 35, 64, 78)       # the irrigation trench (floor row 79)
L.clear(36, 117, 64, 74)      # D1 east: ground 75
L.clear(2, 117, 49, 60)       # D2: ground 61
L.clear(2, 117, 34, 45)       # D3: ground 46
L.clear(52, 117, 24, 30)      # D4 lower tier: ground 31
L.clear(2, 117, 19, 21)       # D4 upper: the deck, ground 22
L.clear(2, 117, 1, 15)        # D5: ground 16, glass row 0

# Lifts: a pit in the floor below, a hole through the slab above.
L.clear(114, 116, 75, 75)
L.clear(114, 116, 61, 63)
L.at("platform", 114, 75, "L1", w=3, mode="pingpong", path=[(114, 75), (114, 61)], speed=1, wait=30)
L.clear(2, 4, 61, 61)
L.clear(2, 4, 46, 48)
L.at("platform", 2, 61, "L2", w=3, mode="pingpong", path=[(2, 61), (2, 46)], speed=1, wait=30)
L.clear(114, 116, 46, 46)
L.clear(114, 116, 31, 33)
L.at("platform", 114, 46, "L3", w=3, mode="pingpong", path=[(114, 46), (114, 31)], speed=1, wait=30)

# --- 1. D1 potting shed and the first lamp (x 2-35) --------------------------------------------
put(4, 75, "P")
put(10, 75, "W")
L.at("deco", 6, 74, kind="42", text="LOT 42")
L.at("switch", 14, 73, "SWB", kind="shootable")
L.at("lamp", 12, 70, "LPB", switch="SWB", timer=0, basil=1)
L.at("switch", 25, 73, "SW1", kind="shootable")
L.at("lamp", 31, 66, "LP1", switch="SW1", timer=0)
L.at("plant", None, None, "PL1", lamp="LP1", kind="bridge", rect=(28, 75, 35, 75), root=(28, 75))
L.at("water", None, None, rect=(28, 76, 35, 78))
L.col(29, 76, 78, "H")
put(31, 78, "Q")
gem((8, 75), (18, 75), (22, 75))

# --- 2. D1 thorn walls (x 36-117) -------------------------------------------------------------
put(40, 74, "h")
put(45, 73, "1")              # letter G
thorns(50, 64, 74)
L.at("snapjaw", 64, 74, "SJ1")
thorns(80, 64, 74)
put(90, 74, "m")
gem((44, 74), (56, 74), (60, 74), (70, 74), (75, 74), (86, 74), (96, 74), (104, 74))

# --- 3. D2 spore dome (113 -> 5) ---------------------------------------------------------------
put(111, 60, "c")
put(100, 60, "h")
thorns(88, 49, 60)
thorns(56, 49, 60)
L.at("spore_puffer", 96, 50, "SPF1", carrier=1, face="ceiling")
L.at("spore_puffer", 64, 50, "SPF3", carrier=1, face="ceiling")
L.row(79, 83, 58, "=")        # planter ledge
L.at("spore_puffer", 80, 57, "SPF2", carrier=1)
put(40, 60, "m")
put(30, 60, "W")
# Secret 2: the planter ledges up to the seed pod on its wall vine.
L.row(24, 26, 58, "=")
L.row(18, 23, 55, "=")
L.fill(20, 21, 52, 53, "#")
L.at("breakable", None, None, rect=(20, 52, 21, 53), by="trimmer", hp=4, look="seedpod")
L.put(20, 53, "C")            # inside the pod
put(22, 54, "$")
gem((106, 60), (94, 60), (84, 60), (74, 60), (68, 60), (50, 60), (36, 60), (12, 60))

# --- Nova line: leaf ledges up a shaft (x 54-59) from D2 to D4 ------------------------------------
L.row(54, 56, 57, "=")
L.row(57, 59, 53, "=")
L.row(54, 56, 49, "=")
L.clear(54, 59, 47, 48)
L.row(54, 59, 46, "=")        # over the hole in D3's floor
L.row(57, 59, 42, "=")
L.row(54, 56, 38, "=")
L.row(57, 59, 34, "=")
L.clear(54, 59, 32, 33)
L.row(54, 59, 31, "=")        # over the hole in D4's floor
gem((55, 56), (58, 52), (55, 48), (58, 41), (55, 37), (58, 33),
    (54, 56), (59, 52), (56, 48), (59, 41), (56, 37), (57, 33))

# --- 4. D3 Glob tanks (5 -> 113) ---------------------------------------------------------------
put(6, 45, "c")
L.at("glob", 30, 45, "GB1")
put(50, 45, "h")
L.at("glob", 62, 45, "GB2")
put(70, 43, "2")              # letter U
L.at("glob", 85, 45, "GB3")
put(100, 45, "m")
put(104, 45, "W")
gem((14, 45), (22, 45), (38, 45), (44, 45), (66, 45), (76, 45), (80, 45), (92, 45), (96, 45), (108, 45))

# --- 5. D4 timed lamps (113 -> 4) --------------------------------------------------------------
L.fill(2, 117, 22, 23, "#")   # the deck
put(111, 30, "c")
put(90, 30, "h")
L.at("switch", 98, 24, "SW4", kind="shootable")
L.at("lamp", 61, 24, "TL1", switch="SW4", timer=150)
L.clear(57, 59, 22, 23)       # the creeper's hole in the deck
L.at("plant", None, None, "VN1", lamp="TL1", kind="ladder", rect=(58, 22, 58, 30), root=(58, 30))
L.at("snapjaw", 80, 30, "SJ2")
# Rocco line: the Snapjaw corridor, a ladder up through the deck.
L.fill(103, 104, 24, 27, "#")
L.fill(108, 109, 24, 27, "#")
L.clear(105, 107, 22, 23)
L.col(106, 22, 30, "H")
L.at("snapjaw", 103, 25, "SJ3", face="left")
L.at("snapjaw", 108, 24, "SJ4", face="right")
# The deck.
put(20, 21, "W")
put(36, 21, "m")
L.at("switch", 30, 19, "SW5", kind="shootable")
L.at("lamp", 6, 19, "TL2", switch="SW5", timer=120)
L.clear(3, 5, 16, 18)         # the creeper's hole in D5's floor
L.at("plant", None, None, "VN2", lamp="TL2", kind="ladder", rect=(4, 16, 4, 21), root=(4, 21))
gem((100, 30), (84, 30), (70, 30), (64, 30), (44, 21), (12, 21))

# --- 6. D5 top dome (5 -> 117) ------------------------------------------------------------------
put(8, 15, "c")
put(12, 15, "T")
put(40, 15, "h")
L.at("glob", 30, 15, "GB5")
L.at("glob", 50, 15, "GB4")
for lid, sid, lx, sx in (("LPa", "SWa", 20, 24), ("LPb", "SWb", 40, 44), ("LPc", "SWc", 60, 64),
                         ("LPd", "SWd", 78, 68)):
    L.at("switch", sx, 14, sid, kind="shootable")
    L.at("lamp", lx, 2, lid, switch=sid, timer=150)
L.at("plant", None, None, "STAIRS", lamp="LPa,LPb,LPc,LPd", kind="stairs",
     rect=(72, 13, 75, 13), rect2=(77, 10, 80, 10), rect3=(82, 7, 85, 7), rect4=(87, 4, 110, 4), root=(72, 13))
# Secret 1: the giant flower in the corner, its bloom holding N and the bonus.
L.at("switch", 3, 14, "SW6", kind="shootable")
L.at("lamp", 2, 1, "LP6", switch="SW6", timer=0)
L.at("plant", None, None, "FLOWER", lamp="LP6", kind="flower",
     rect=(11, 13, 12, 13), rect2=(8, 10, 9, 10), rect3=(8, 7, 11, 7), root=(12, 13))
put(9, 4, "B")
put(8, 6, "3")                # letter N
put(106, 3, "X")
gem((74, 12), (79, 9), (84, 6), (89, 3), (92, 3), (95, 3), (98, 3), (101, 3), (104, 3), (108, 3))

L.write(__file__, "17_hydroponics.txt")
