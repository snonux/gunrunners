#!/usr/bin/env python3
"""Level 28 - Toon Town Throwdown (SPEC.md 28). Twist: Cartoon Physics.
Prototype: Portable Hole. Boss: Buddy Boom.

A run east from the cliff over the canyon, through the painted tunnels and
the air-run gaps on ground 20, down the steps to the big top (ground 36),
where Buddy Boom waits. Walk off a ledge holding the way you go and you
keep running on air; let go or turn and the "uh-oh" sign comes up, then
the fall. Painted tunnels are passages (with a shadow) or solid paint
(a bonk). Every air-run gap has a floor under it.

Everything in world_toon.cpp.

Changes from the spec, so it plays in this engine:
- The tunnel walls WA, WB and WC go up from row 4, not 14: three or four
  rows of wall are jumped over, and then the tunnels would not matter.
- The inkwell (the bonus entrance) stands on a one-way desk at row 28 in
  the studio.
- The ink under the arena is rows 37-38 with a floor at row 39 under it
  (the map ends at row 39).
- Buddy Boom's line is drawn 10 blocks long near the runner, at row 26 or
  22 in turn.
- The canyon sign stands on the plateau at (19, 19).
"""

from lib import Level

W, H = 220, 40
L = Level(W, H,
          name="STAGE 28 - TOON TOWN THROWDOWN", episode=4, theme="rubber_hose_toon",
          music="theme_toon_jazz", weapon="portable_hole", par=360, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


# --- 1. Cliff and canyon (x 0-48) -----------------------------------------------------------------
L.fill(0, 20, 20, 21, "#")                  # the plateau's slab
L.fill(0, 1, 22, 29, "#")                   # the studio's west wall
L.col(17, 22, 29, "#")                      # its east wall (the real tunnel goes through)
L.ground(0, 42, 30)                         # the studio's floor and the safe ledge
L.ground(43, 44, 27)                        # the steps up out of the canyon
L.ground(45, 46, 24)
L.ground(47, 48, 21)
put(3, 19, "P")
L.at("sign", 19, 19)
L.at("tunnel", 17, 27, real=1)
L.at("secret", None, None, "studio", rect=(2, 22, 16, 29))
L.row(10, 14, 28, "=")                      # the drawing desk
put(12, 27, "B")                            # the inkwell
put(8, 29, "G")
put(14, 29, "m")
put(30, 29, "W")
put(40, 29, "h")
gem(*[(x, 18) for x in (23, 26, 29, 32, 35, 38, 41, 44, 46, 48)])   # the air-run's arc

# --- 2. Painted tunnels (x 49-100) ----------------------------------------------------------------
L.ground(49, 100, 20)
for x0 in (64, 78, 92):
    L.fill(x0, x0 + 1, 4, 19, "#")          # walls WA, WB, WC
L.at("tunnel", 64, 17, real=1)
L.fill(76, 77, 17, 19, "#")                 # the crate in front of WB
L.at("tunnel", 78, 17, real=0)
L.at("tunnel", 78, 14, real=1)
L.at("tunnel", 92, 17, real=1)
put(50, 19, "c")
put(60, 19, "W")
for i, x in enumerate((70, 74, 86, 90)):
    L.at("grinning_bomb", x, 19)
put(82, 19, "h")
put(96, 19, "m")
gem((55, 18), (58, 18), (68, 18), (72, 18), (77, 16), (83, 17), (88, 18), (98, 18))

# --- 3. Air-runs (x 101-152) ----------------------------------------------------------------------
L.ground(101, 103, 20)
L.fill(104, 107, 20, 21, "#")               # the ledge before gap A
L.fill(118, 123, 20, 21, "#")               # the pillar between the gaps
L.ground(138, 152, 20)
L.ground(104, 137, 34)                      # the basement road
for x0, y in ((132, 31), (135, 28), (132, 25), (135, 22)):
    L.row(x0, x0 + 2, y, "=")               # the stairs up out of gap B
L.row(140, 150, 12, "=")                    # the high ledge
L.at("deco", 107, 19, kind="text", text="v", w=1, h=1)
L.at("deco", 118, 19, kind="text", text="v", w=1, h=1)
L.at("deco", 123, 19, kind="text", text="v", w=1, h=1)
put(101, 19, "c")
L.at("rubber_goon", 120, 19, "RG1")
L.at("rubber_goon", 142, 19, "RG2")
L.at("rubber_goon", 118, 33, "RG3")
L.at("grinning_bomb", 112, 33, "GB5")
L.at("grinning_bomb", 126, 33, "GB6")
L.at("spawner", None, None, kind="piano_bird", trigger=105)
L.at("spawner", None, None, kind="piano_bird", trigger=122)
put(110, 33, "h")
put(130, 33, "W")
put(128, 33, "m")
put(145, 11, "U")
put(148, 11, "m")
put(146, 19, "h")
gem(*[(x, 18) for x in (109, 111, 113, 115)])                         # gap A's air-run
gem(*[(x, 18) for x in (125, 127, 129, 131, 133, 135, 137)])          # gap B's
gem((120, 18))
gem((105, 33), (108, 33), (115, 33), (121, 33), (123, 33), (136, 27))  # the basement
gem((141, 11), (142, 11), (143, 11), (144, 11), (146, 11), (147, 11), (149, 11), (150, 11))  # the high ledge

# --- 4. Big top door (x 151-171) ------------------------------------------------------------------
L.ground(153, 155, 24)
L.ground(156, 158, 28)
L.ground(159, 161, 32)
L.ground(162, 171, 36)
L.at("tunnel", 154, 25, real=0)             # painted rock: the programmer art is behind it
put(151, 19, "c")
put(157, 27, "N")
L.at("deco", 166, 30, kind="42", text="SHOW No. 42", w=4, h=1)
put(165, 35, "T")
L.at("fullheal", 168, 35)
put(170, 35, "c")

# --- 5. The big top (x 172-219) -------------------------------------------------------------------
L.fill(172, 213, 0, 7, "#")                 # the tent roof
L.col(172, 8, 32, "#")                      # the west wall; its door is rows 33-35
L.col(213, 8, 35, "#")                      # the east wall
L.row(172, 212, 36, "#")                    # the floor: eight segments of 5 blocks
L.row(173, 212, 39, "#")                    # under the ink
L.fill(214, 219, 0, 29, "#")                # behind the dressing room
L.ground(213, 219, 36)                      # its floor
L.row(178, 184, 30, "=")
L.row(201, 207, 30, "=")
L.row(189, 196, 24, "=")
L.at("fluid", None, None, kind="pond", rect=(173, 37, 212, 38))
L.at("buddy", 192, 35, arena=(172, 8, 213, 36), door=(172, 33, 172, 35), floor=(173, 5, 8, 36),
     platforms=(178, 184, 201, 207, 30), drops=[(180, 35), (205, 35)], turbo=(192, 23))
L.at("secret", None, None, "dressing", rect=(214, 30, 219, 35))
put(175, 35, "W")
put(209, 35, "X")
L.at("prize", 216, 35, kind="buddy_sketch", score=25000)
put(218, 35, "Q")

L.write(__file__, "28_toon_town_throwdown.txt")
