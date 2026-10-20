#!/usr/bin/env python3
"""Level 26 - Kaiju Tonight (SPEC.md 26). Twist: You Are the Monster.
Prototype: Atomic Breath.

A stomp east along the model city's street (ground 30): the suburbs, the
downtown office blocks and the canal (crossed on tower T1, toppled into a
bridge), the power plant with its two power lines and Maser Trucks, the
airport under the Toy Jets (the control tower falls toward the runner), and
the harbor arena, where Gorgazilla waits on the piers and the exit is a
model ocean liner beyond the bay (crossed on the harbor tower). Under the
street from x 84 to 150 runs the subway (rows 32-36), reached by standing
on the station roof until it gives way; the studio storeroom at its east
end holds U, a merch box, the rubber duck and the TV set (the bonus door).

Everything in world_city.cpp; `@ building` blocks are solid until broken.

Changes from the spec, so it plays in this engine:
- Tank squads spawn on the open street spots of their rects (office blocks
  and the Halcyon stand in SQ1's); SQ2's rect is x 77-87, clear of the
  station.
- The subway train is left out (a one-way platform on the subway floor
  would lift a runner walking under it); the subway is walked.
- The manhole is a ladder at x 150 (rows 30-36) with a one-way hatch at
  x 149 beside it.
- The TV set B stands on the storeroom floor at (148, 36).
- Merch `m` at the airport is at (187, 29) (184 is inside a hangar).
- The liner is x 213-219 so the east pier (208-212) has room for
  Gorgazilla, the drop at (211, 29) and the Turbo crate at (210, 29).
- The arena door is rows 20-29 at x 191 (rows 27-29 alone could be jumped).
- Maser Trucks stand on the street (row 29).
"""

from lib import Level

W, H = 220, 40
L = Level(W, H,
          name="STAGE 26 - KAIJU TONIGHT", episode=4, theme="model_city",
          music="theme_monster_brass", weapon="atomic_breath", par=360, flags="")

G = 30  # the street


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def building(bid, x0, y0, x1, y1, kind, **keys):
    L.at("building", None, None, bid, rect=(x0, y0, x1, y1), kind=kind, **keys)


# --- The ground ------------------------------------------------------------------------------------
L.ground(0, 199, G)
L.ground(208, W - 1, G)
L.ground(200, 207, 37)                      # the bay's floor
L.clear(70, 75, G, 34)                      # the canal (floor 35)
L.clear(84, 150, 32, 36)                    # the subway under the street (floor 37)

# --- 1. Suburbs (x 0-40) ---------------------------------------------------------------------------
put(3, 29, "P")
for i, x in enumerate((8, 11, 14, 17, 26, 33)):
    building("house%d" % (i + 1), x, 29, x, 29, "house")
building("diner", 20, 29, 23, 29, "house", rating=250)
building("watertower", 29, 27, 31, 29, "block")
put(30, 26, "W")
put(36, 29, "m")
put(38, 29, "h")
gem((5, 28), (10, 27), (13, 27), (16, 27), (24, 28), (35, 27))

# --- 2. Downtown (x 41-100) ------------------------------------------------------------------------
for i, x in enumerate((44, 52, 80, 96)):
    building("office%d" % (i + 1), x, 26, x + 2, 29, "block")
building("halcyon", 58, 22, 59, 29, "prize")
building("t1", 67, 22, 68, 29, "tower", fall="r")
L.at("fluid", None, None, "canal", rect=(70, 33, 75, 34), kind="pond")
building("station", 88, 27, 94, 29, "roof", under=(89, 30, 93, 31))
put(45, 25, "G")
L.at("squad", None, None, "sq1", kind="model_tank", count=4, rect=(50, 29, 62, 29))
L.at("squad", None, None, "sq2", kind="model_tank", count=4, rect=(77, 29, 87, 29))
L.at("deco", 50, 29, kind="42", w=3, h=1)
put(78, 29, "h")
gem((44, 25), (46, 25), (52, 25), (53, 25), (54, 25), (80, 25), (82, 25), (96, 25), (97, 25), (98, 25))

# --- 3. Power plant (x 101-150) --------------------------------------------------------------------
L.at("sector", None, None, "A", rect=(101, 0, 125, 39))
L.at("sector", None, None, "B", rect=(126, 0, 150, 39))
L.at("powerline", None, None, "L1", path=[(104, 25), (124, 25)], sector="A")
L.at("powerline", None, None, "L2", path=[(126, 25), (148, 25)], sector="B")
building("cooling1", 118, 26, 119, 29, "block")
building("cooling2", 132, 26, 133, 29, "block")
put(100, 29, "c")
put(102, 29, "W")
L.at("maser_truck", 112, 29)
L.at("maser_truck", 138, 29)
put(120, 29, "T")
put(130, 29, "h")
gem((106, 28), (108, 28), (110, 27), (116, 27), (122, 27), (128, 28), (135, 27), (144, 28))

# --- 3s. Subway (x 84-150, rows 32-36) -------------------------------------------------------------
L.row(149, 149, G, "=")                     # the manhole's hatch
L.clear(149, 149, 31, 31)
L.col(150, G, 36, "H")                      # the manhole ladder
put(139, 36, "Q")
put(146, 36, "U")
put(144, 36, "m")
put(148, 36, "B")
gem(*[(x, 35) for x in range(92, 138, 5)])

# --- 4. Airport (x 151-190) ------------------------------------------------------------------------
building("hangar1", 175, 27, 178, 29, "block")
building("hangar2", 182, 27, 185, 29, "block")
building("control", 170, 22, 171, 29, "tower", fall="l")
put(150, 29, "c")
put(152, 29, "W")
put(176, 25, "N")
put(160, 29, "h")
put(187, 29, "m")
for t in (158, 172, 184):
    L.at("formation", None, None, "jets%d" % t, kind="toy_jet", count=3, trigger=t, row=24)
gem((156, 28), (166, 27), (175, 26), (177, 26), (178, 26), (182, 26), (184, 26), (185, 26))

# --- 5. Harbor (x 191-219) -------------------------------------------------------------------------
put(190, 29, "c")
L.at("fullheal", 193, 29)
L.at("photo", 194, 27)
put(196, 29, "W")
building("harbor", 197, 22, 198, 29, "tower", fall="r")
L.at("fluid", None, None, "bay", rect=(200, 31, 207, 36), kind="pond")
L.fill(213, W - 1, 27, 29, "#")             # the liner
put(216, 26, "X")
L.at("lizard", 209, 29, arena=(191, 10, 219, 36), door=(191, 20, 191, 29), piers=(191, 199, 208, 212),
     drops=[(195, 29), (211, 29)], turbo=(210, 29), dish=(214, 26))

L.header["tip"] = "YOU ARE THE MONSTER: STOMP THROUGH THE CITY, WALK INTO BUILDINGS TO BREAK THEM"  # HINTS: the level's first 12 seconds
L.write(__file__, "26_kaiju_tonight.txt")
