#!/usr/bin/env python3
"""Level 24 - Too Many Cooks (SPEC.md 24). Twist: Conveyors. Prototype:
Pepper Grinder.

A horizontal climb through a TV kitchen: the cutting board (ground 40) and
belt B1, the sushi bar's belts B2-B4 (bell1 reverses them; B4 runs west as
fast as a runner walks until then), the sink, the dishwasher's racks, the
stovetop with belt B6 and the hanging pots, the spice rack up to the top of
the fridge (ground 11) and the oven light.

`@ belt` rows are solid in the map (the order rail's row is a hang bar);
world_kitchen.cpp moves what stands on them. `@ flap` columns are left
empty here and closed at load.

Changes from the spec, so it plays in this engine:
- The pantry's west side is closed by a cabinet wall at x 7 (rows 41-43),
  or it would be open to the toaster counter; the toaster (top ground 41)
  is the step from the counter back up onto the board.
- The pantry ladder is at x 9, not 8: a ladder shaft needs the block left
  of the ladder open (x 7 is the cabinet), and the hatch beside it at
  (8, 40) is a one-way `=` (a ladder is only climbed up, so from the board
  the hatch is shut).
- Rack R4 rides x 108-112 (its right end 4 blocks on stops at the
  stovetop's edge instead of running into it).
- The counter under the spice rack (x 147-156) is ground 34 like the
  stovetop, so the first shelf (29) is a 3-row step from the crate.
- `bot=1 stand=X` marks the bell the bot rings on its way (bell1, from B3);
  `bot=bonus` the one it rings for the pantry (bell0).
"""

from lib import Level

W, H = 180, 50
L = Level(W, H,
          name="STAGE 24 - TOO MANY COOKS", episode=4, theme="tv_kitchen",
          music="theme_accordion", weapon="pepper_grinder", par=360, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def belt(bid, x0, y, x1, speed, d, station=None, **k):
    L.at("belt", None, None, bid, rect=(x0, y, x1, y), speed=speed, dir=d, station=station, **k)


# --- 1. Cutting board (x 0-41) ---------------------------------------------------------------------
L.fill(0, 7, 44, H - 1, "#")                # the counter under the toaster
L.fill(2, 5, 41, 43, "#")                   # the toaster (top ground 41)
L.at("toaster", rect=(2, 41, 5, 43), lever=(6, 42))
L.fill(7, 7, 41, 43, "#")                   # the cabinet's side
L.fill(8, 20, 40, 40, "#")                  # the cutting board
L.fill(8, 20, 44, H - 1, "#")               # the pantry floor
L.clear(8, 8, 41, 43)
L.fill(9, 9, 40, 43, "H")                   # the pantry ladder, up through the board
L.put(8, 40, "=")                           # the hatch beside it: one-way, up
L.at("flap", 20, 41, h=3)
L.fill(21, 41, 44, H - 1, "#")              # B1's counter and the static counter
belt("B1", 21, 44, 38, "1/4", "r", "s1", loop=1)
L.at("switch", id="bell0", x=36, y=42, kind="shootable", states=2, station="s1", bot="bonus", stand=32)
put(10, 39, "P")
put(9, 37, "G")
put(34, 41, "U")
put(26, 43, "W")
put(12, 43, "m")
put(16, 43, "N")
put(18, 41, "B")
put(40, 43, "h")
gem((22, 43), (24, 43), (28, 43), (30, 43), (32, 43), (34, 43), (36, 43), (38, 43))

# --- 2. Sushi bar (x 42-84) ------------------------------------------------------------------------
L.fill(42, 84, 44, H - 1, "#")
L.clear(50, 84, 45, 47)                     # the sauce line under the bar (floor 48)
L.at("breakable", rect=(50, 44, 51, 44), hp=1, by="any", look="grate")
L.fill(53, 64, 42, 43, "#")                 # under B3
L.fill(65, 80, 40, 43, "#")                 # under B4
L.clear(81, 84, 40, 47)                     # the sink (floor 48)
L.fill(84, 84, 40, 47, "H")                 # its ladder
belt("B2", 42, 44, 52, "1/4", "l", "s2", loop=1)
belt("B3", 53, 42, 64, "1/2", "r", "s2", loop=1)
belt("B4", 65, 40, 80, "1", "l", "s2")
L.at("sauce", rect=(58, 41, 60, 41))
L.at("sink", rect=(81, 45, 84, 47))
L.fill(64, 84, 33, 33, "-")                 # the order rail
L.at("belt", None, None, "rail", rect=(64, 33, 84, 33), speed="1/2", dir="r", hang=1)
L.at("switch", id="bell1", x=64, y=40, kind="shootable", states=2, station="s2", bot=1, stand=60)
L.at("spawner", kind="meatball", x=78, y=36, every=90, max=3, carrier=4)
L.at("deco", 70, 32, kind="42", text="TABLE 42")
put(41, 43, "c")
put(60, 47, "m")
put(83, 47, "h")
gem((44, 43), (47, 43), (55, 41), (57, 41), (62, 41), (67, 39), (70, 39), (73, 39), (76, 39), (79, 39))
gem((53, 47), (57, 47), (63, 47), (67, 47), (71, 47), (75, 47))
L.at("meatball", 66, 47, "MB1", size="big")
L.at("meatball", 74, 47, "MB2", size="big")

# --- 3. Dishwasher (x 85-116) ----------------------------------------------------------------------
L.fill(85, 90, 40, H - 1, "#")              # the door, open (top ground 40)
L.fill(91, 116, 44, H - 1, "#")             # the tub
L.at("platform", 92, 42, "R1", w=5, path=((92, 42), (99, 42)), speed="1/4", mode="pingpong")
L.at("platform", 99, 40, "R2", w=5, path=((99, 40), (106, 40)), speed="1/4", mode="pingpong")
L.at("platform", 104, 38, "R3", w=5, path=((104, 38), (110, 38)), speed="1/4", mode="pingpong")
L.at("platform", 108, 36, "R4", w=5, path=((108, 36), (112, 36)), speed="1/4", mode="pingpong")
L.at("deco", kind="basket", rect=(95, 41, 103, 43))
put(86, 39, "c")
put(88, 39, "W")
put(93, 43, "h")
put(110, 43, "m")
L.at("egg_timer", 97, 43, "E1")
L.at("egg_timer", 101, 43, "E2")
gem((95, 43), (99, 43), (103, 43), (96, 40), (102, 37), (107, 35), (111, 33), (114, 34))

# --- 4. Stovetop (x 117-156) -----------------------------------------------------------------------
L.fill(117, 156, 34, H - 1, "#")
belt("B6", 125, 34, 140, "1/2", "l", "s3")
for x0 in (126, 132, 138):
    L.fill(x0, x0 + 2, 29, 29, "=")         # hanging pots' rims
L.fill(121, 124, 30, 30, "=")               # the pot on the stove
L.at("deco", kind="pot", rect=(121, 30, 124, 33))
L.fill(143, 146, 32, 33, "#")               # the crate
for y, x0 in ((29, 147), (26, 151), (23, 147), (20, 151), (17, 147), (14, 151)):
    L.fill(x0, x0 + 4, y, y, "=")           # the spice rack's shelves
L.at("switch", id="bell2", x=124, y=32, kind="shootable", states=2, station="s3")
put(117, 33, "c")
put(119, 33, "T")
put(144, 31, "h")
put(146, 31, "W")
put(150, 28, "m")
L.at("cleaver_bot", 133, 28, "CB1")
L.at("cleaver_bot", 149, 16, "CB2")
L.at("egg_timer", 136, 33, "E3")
L.at("egg_timer", 139, 33, "E4")
L.at("prize", 154, 13, kind="golden_whisk", score=10000)
gem((122, 28), (127, 27), (130, 30), (135, 30), (139, 27), (141, 31), (128, 33), (131, 33))
gem((148, 27), (153, 24), (148, 21), (154, 18), (147, 15), (152, 12))

# --- 5. Fridge top (x 157-179) ---------------------------------------------------------------------
L.fill(157, W - 1, 11, H - 1, "#")
L.at("deco", kind="oven", rect=(174, 7, 178, 10))
L.at("script", None, None, "special", x=168, y=10)
put(160, 10, "c")
put(164, 10, "h")
put(176, 10, "X")
put(178, 10, "Q")

L.write(__file__, "24_too_many_cooks.txt")
