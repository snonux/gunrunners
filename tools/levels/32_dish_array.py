#!/usr/bin/env python3
"""Level 32 - Dish Array (SPEC.md 32). Twist: Tracking Dishes. Prototype:
Signal Jammer.

A run left to right across the desert: the dunes, the sand sea (Sand
Sharks), up two big dishes onto the mesa, down a ladder to the control
bunker (Scorpion Mechs), then the biggest dish up to the uplink tower. The
big dishes share one 900-frame cycle: hold east 240 frames, turn to
overhead (45), hold 240, turn west (45), hold 240, swing back east (90),
groaning for 30 frames before each turn. Their rims and feed arms are
`mode=script` platforms with a pose each (east;overhead;west), their struts
script layers solid in the poses named in `state=`. The teaching dish D0
turns on its own 180-frame cycle while the runner is within 6 blocks.

Everything in world_dish.cpp.

Changes from the spec, so it plays in this engine:
- The receiver cabin's floor (row 20) is one-way and its west side is open
  (the door is the whole side), so a runner on D2's feed arm can walk
  under it onto the mesa as well as jump up into it.
- Strut 2 (97-99 g31) is a 2-row slab in the air, not a pillar down to
  the ground, so it doesn't wall off the ground in the overhead and west
  poses.
- The array's 10 gems sit on the struts, the ground and the mesa instead
  of on the rims (items don't ride platforms).
- The bonus entrance is at (119, 19) on the cabin floor.
- The sharks start on the sand at row 37.
- Vulture Drones circle about 5 blocks over the ground under them (the
  spec's rows 4-10 are far above the view from the ground), and a Scorpion
  Mech only notices a runner in front of it (or right behind it).
- A Sand Shark breaches when the runner is within a block of it (middle to
  middle), so its bite can land.
- D4's footing under the rim (193-204) is raised to g36, 2 below strut 2,
  so a runner who falls off the rim can climb back (the spec's ground there
  is a pit with no way out).
"""

from lib import Level

W, H = 220, 44
L = Level(W, H, name="STAGE 32 - DISH ARRAY", episode=5, theme="desert_array", music="theme_desert_slide",
          weapon="signal_jammer", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


L.fill(0, W - 1, 38, H - 1, "#")              # ground g38 everywhere (no pits)
L.at("satellite", None, None, period=900)

# --- 1. The dune (0-30) ------------------------------------------------------------------------------
L.fill(7, 11, 36, 37, "#")                     # the survey truck
put(3, 37, "P")
put(9, 35, "W")
L.at("minidish", 14, 37)
put(20, 37, "m")
L.at("cactus", 26, 37, hat=1)
put(28, 37, "h")
L.at("dish", 22, 35, "D0", small=1, radius=6)
L.at("platform", 20, 35, "D0r", w=4, mode="script", path=((20, 35), (21, 33), (22, 35)), dish="D0")
gem((5, 37), (13, 37), (16, 37), (18, 37), (24, 37), (30, 37))

# --- 2. The dune sea (31-80) -------------------------------------------------------------------------
L.at("sand", None, None, rect=(31, 38, 80, 39))
for x0 in (38, 50, 62, 74):                    # rock islands, g36
    L.fill(x0, x0 + 4, 36, 37, "#")
L.at("sand_shark", 45, 37)
L.at("sand_shark", 58, 37)
L.at("sand_shark", 70, 37)
L.at("minidish", 52, 35)
L.at("minidish", 76, 35)
put(64, 35, "m")
put(64, 32, "U")
put(82, 37, "c")
gem((38, 35), (40, 35), (42, 35), (50, 35), (54, 35), (62, 35), (63, 35), (65, 35), (66, 35), (74, 35), (75, 35),
    (78, 35))

# --- 3. The array (81-140) ---------------------------------------------------------------------------
put(88, 37, "h")
L.at("layer", None, None, "D1s1", rect=(93, 35, 96, 37), driver="script", state="east,over", style="strut",
     color="white")
L.at("layer", None, None, "D1s2", rect=(97, 31, 99, 32), driver="script", state="over,west", style="strut",
     color="white")
L.at("platform", 97, 32, "D1r", w=5, mode="script", path=((97, 32), (100, 28), (103, 32)))
L.at("platform", 104, 30, "D2r", w=5, mode="script", path=((104, 30), (107, 26), (110, 30)))
L.at("platform", 108, 22, "D2a", w=5, mode="script", path=((108, 22), (112, 23), (113, 27)))
L.fill(118, 140, 24, H - 1, "#")              # the mesa
L.row(117, 122, 14, "#")                      # the receiver cabin: roof,
L.col(122, 15, 19, "#")                       # east wall,
L.row(117, 122, 20, "=")                      # one-way floor (g20)
L.at("vulture_drone", 100, 8, zone=(84, 116))
L.at("vulture_drone", 130, 6, zone=(118, 140))
put(120, 23, "W")
put(124, 23, "c")
put(134, 23, "m")
L.at("minidish", 136, 23)
put(138, 23, "h")
gem((86, 37), (90, 37), (94, 34), (96, 34), (101, 37), (106, 37), (110, 37), (128, 23), (130, 23), (132, 23))
# Secret 1: the receiver cabin.
put(120, 19, "G")
put(119, 19, "B")
gem((118, 19), (121, 19), (118, 16), (119, 16), (120, 16), (121, 16), (119, 17), (120, 17))

# Secret 2: the hangar in the mesa's face, behind a door a mech's beam breaks.
L.clear(132, 139, 33, 37)
L.at("breakable", None, None, rect=(140, 34, 140, 37), hp=1, by="beam", look="hatch")
L.at("dummy", 139, 35, art="halcyon_target")
put(136, 37, "m")
put(137, 37, "m")
put(134, 37, "$")
L.at("treasure", 133, 37, score=10000, art="halcyon_crate")

# --- 4. The control bunker (141-184) -----------------------------------------------------------------
L.col(141, 24, 37, "H")                       # down the mesa's east face
L.row(155, 170, 32, "#")                      # the bunker's roof (g32)
L.col(155, 33, 34, "#")                       # its walls (doorways rows 35-37)
L.col(170, 33, 34, "#")
put(143, 37, "c")
L.at("scorpion_mech", 146, 37)
L.at("scorpion_mech", 160, 37)
L.at("scorpion_mech", 172, 37)
put(152, 37, "W")
put(164, 37, "T")
put(166, 37, "h")
L.at("vulture_drone", 160, 8, zone=(150, 170), carrier=1)
put(176, 37, "c")
put(180, 37, "h")
L.at("minidish", 182, 37)
gem((148, 37), (150, 37), (158, 37), (162, 37), (168, 37), (178, 37))

# --- 5. The big dish (185-219) -----------------------------------------------------------------------
L.fill(185, 188, 36, 37, "#")                 # D4's fixed struts
L.fill(189, 192, 34, 37, "#")
L.fill(193, 204, 36, 37, "#")                 # the dish's footing (g36): no pit under the rim
L.at("platform", 193, 32, "D4r", w=6, mode="script", path=((193, 32), (196, 29), (199, 20)))
L.fill(205, 212, 18, H - 1, "#")              # the uplink tower
L.at("deco", 194, 28, kind="42", art="dish_number", text="42", w=1, h=1)
put(196, 24, "N")
put(208, 17, "X")
put(211, 17, "Q")
L.at("duck", 211, 17, costume="sunglasses")
gem((186, 35), (187, 35), (190, 33), (191, 33), (206, 17), (207, 17), (209, 17), (210, 17))

L.write(__file__, "32_dish_array.txt")
