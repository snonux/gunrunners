#!/usr/bin/env python3
"""Level 20 - Reactor Core (SPEC.md 20). Twist: Core Pulse. Prototype: Deflector Bracer.

A descent: six floors stacked top to bottom, each spanning x 2-87, walked in
alternating directions and joined by ladders at alternating ends, from the
start at the top-left to the exit lift on the core floor at the bottom-left.
Every 150 frames a ring races out from the core's base (45, 98) and costs
two hearts to a runner it passes outside a lead booth (`@ shield rect=`).
Three valves on the core floor stop the pulses and open the lift cage.

Changes from the spec, so it plays in this engine:
- A booth's roof is one block row of lead right over its rect (no overhang),
  so the booths next to the Nova pipe leave room to jump into it.
- The Nova lead pipe runs x 43-58 (not 59): the x 60 booth's roof is beside it;
  its roof leaves the end blocks open so a runner can climb out onto a booth.
- F2's ladder down is at x 13 (hole x 12-14), not x 11: the storeroom's
  cracked lead wall stands at x 9-10 and a hole under it would swallow it.
- F3's coolant pipe ledge is x 76-82 at ground 46 (not x 76-86 ground 47),
  so runners walk under it to the ladder hole; a lead crate at x 74 is the
  step up to it.
- The crane catwalk runs x 40-51 so it meets the unlit ladder at x 52.
- The lift cage has a lead roof (row 100) over x 4-8; its bars are x 8.
"""

from lib import Level

W, H = 90, 110
L = Level(W, H,
          name="STAGE 20 - REACTOR CORE", episode=3, theme="station_reactor",
          music="core_drone", weapon="deflector_bracer", par=210, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def booth(x0, y0, x1, y1):
    """A lead booth: safe inside the rect; its roof is the row above."""
    L.fill(x0, x1, y0 - 1, y0 - 1, "#")
    L.at("shield", rect=(x0, y0, x1, y1))


def ladder(x, y0, y1):
    """A ladder down through a floor slab: the hole is 3 blocks wide."""
    L.clear(x - 1, x + 1, y0, y0 + 1)
    L.fill(x, x, y0, y1, "H")


def wire(wid, pts):
    L.at("wire", None, None, wid, path=pts)


# --- The shell: solid all round, six floors ----------------------------------------------------------
L.fill(0, W - 1, 0, H - 1, "#")
FLOORS = [(4, 11), (14, 30), (33, 49), (52, 68), (71, 87), (90, 105)]
for y0, y1 in FLOORS:
    L.clear(2, 87, y0, y1)
ladder(86, 12, 30)
ladder(13, 31, 49)
ladder(86, 50, 68)
ladder(3, 69, 87)
ladder(86, 88, 105)

L.at("pulse", 45, 98, period=150, damage=2)
L.at("deco", None, None, kind="core", rect=(40, 52, 49, 105))

# --- 1. F1, the shielded room (x 2 -> 85) --------------------------------------------------------------
put(3, 11, "P")
L.at("shield", rect=(2, 4, 60, 11))
for wx in (12, 28, 44):
    L.at("deco", None, None, kind="window", rect=(wx, 6, wx + 5, 8))
L.fill(6, 6, 11, 11, "#")                         # the first lead wall, knee high
put(8, 11, "W")
put(30, 11, "h")
put(50, 11, "m")
L.at("deco", 60, 9, kind="42")                    # the dosimeter, reading 42
L.fill(61, 61, 4, 8, "#")                         # the room's end wall (its door rows 9-11)
booth(72, 8, 74, 11)                              # the practice booth
gem((16, 11), (22, 11), (38, 11), (56, 11), (66, 11), (80, 11))

# --- 2. F2, shield to shield (x 85 -> 2) ---------------------------------------------------------------
put(85, 30, "c")
put(70, 30, "h")
for bx in (78, 60, 40, 18):
    booth(bx, 27, bx + 2, 30)
put(61, 30, "W")                                  # in the second booth
put(40, 29, "1")                                  # letter G, in the third
# The Nova lead pipe, shielded along its length.
L.fill(43, 58, 27, 27, "=")
L.fill(44, 57, 23, 23, "#")                       # its roof (open over the ends, to climb out)
L.at("shield", rect=(43, 24, 58, 26))
wire("W_CS1", [(44, 30), (58, 30)])
L.at("conduit_spark", 44, 30, "CS1", wire="W_CS1")
wire("W_CS2", [(22, 30), (36, 30), (36, 16), (22, 16), (22, 30)])
L.at("conduit_spark", 36, 22, "CS2", wire="W_CS2")
gem((82, 30), (74, 30), (66, 30), (54, 30), (48, 30), (30, 30), (26, 30), (14, 30))
# Secret 1: the sealed storeroom behind a cracked lead wall.
L.fill(2, 10, 24, 24, "#")                        # its roof
L.fill(9, 10, 25, 30, "#")                        # the lead wall
L.at("breakable", rect=(9, 28, 10, 30), by="any", hp=4, look="crack")
L.at("shield", rect=(2, 25, 8, 30))
put(5, 30, "3")                                   # letter N
gem((4, 30), (6, 30), (7, 30), (8, 30), (6, 27), (7, 27))
put(2, 28, "B")                                   # the crack in its back wall

# --- 3. F3, open halls (x 13 -> 85) ----------------------------------------------------------------------
put(15, 49, "c")
for bx in (22, 46, 71):
    booth(bx, 46, bx + 2, 49)
booth(17, 46, 19, 49)                             # the first booth, by the ladder
put(23, 49, "T")                                  # behind the second lead wall
L.at("shield_drone", 35, 44, "SD1", guard="II1,II2")
L.at("isotope_imp", 33, 49, "II1")
L.at("isotope_imp", 38, 49, "II2")
put(47, 49, "h")
L.at("shield_drone", 60, 44, "SD2", guard="II3,II4,II10")
L.at("isotope_imp", 58, 49, "II3")
L.at("isotope_imp", 63, 49, "II4")
L.at("isotope_imp", 66, 49, "II10")
put(75, 49, "m")
L.fill(74, 74, 49, 49, "#")                       # a lead crate: the step up
L.fill(76, 82, 46, 46, "=")                       # the coolant pipe ledge
L.at("deco", None, None, kind="pipe", rect=(76, 46, 82, 46))
put(80, 45, "Q")                                  # the duck, in a tiny radiation suit
gem((28, 49), (32, 47), (41, 49), (52, 49), (56, 47), (68, 49), (77, 45), (81, 45))

# --- 4. F4, the core spiral, upper (x 85 -> 3) ---------------------------------------------------------
put(85, 68, "c")
put(80, 68, "W")
put(65, 68, "m")
L.at("deco", None, None, kind="pipe", rect=(30, 52, 60, 52))
L.at("drip", 45, 53, period=20)
wire("W_CS3", [(70, 68), (50, 68), (50, 58), (70, 58), (70, 68)])
L.at("conduit_spark", 60, 68, "CS3", wire="W_CS3")
L.at("isotope_imp", 30, 68, "II5")
gem((76, 68), (56, 68), (38, 68), (24, 68), (14, 68), (8, 68))

# --- 5. F5, the core spiral, lower (x 3 -> 85) ---------------------------------------------------------
put(10, 87, "h")
L.at("isotope_imp", 40, 87, "II6", hat=1)
L.at("isotope_imp", 60, 87, "II7")
put(50, 87, "W")
put(60, 85, "2")                                  # letter U
wire("W_CS4", [(20, 87), (34, 87)])
L.at("conduit_spark", 27, 87, "CS4", wire="W_CS4")
gem((16, 87), (36, 87), (46, 87), (66, 87), (72, 87), (80, 87))
# Secret 2: the crane catwalk over the core, up a ladder seen only in a flash.
L.fill(40, 51, 76, 76, "=")
L.at("deco", None, None, kind="crane", rect=(38, 72, 53, 76))
L.fill(52, 52, 76, 84, "H")
L.at("unlit", rect=(52, 76, 52, 84))
put(45, 75, "C")
put(48, 75, "$")

# --- 6. F6, the core floor (x 85 -> 6) -----------------------------------------------------------------
put(85, 105, "c")
put(80, 105, "h")
for bx in (64, 40, 16):
    booth(bx, 102, bx + 2, 105)
L.at("valve", 72, 105, "V1")
L.at("valve", 52, 105, "V2")
L.at("valve", 28, 105, "V3")
L.at("shield_drone", 50, 99, "SD3", guard="II8,II9")
L.at("isotope_imp", 48, 105, "II8")
L.at("isotope_imp", 56, 105, "II9")
put(35, 105, "m")
L.at("console", 20, 104)
L.fill(4, 8, 100, 100, "#")                       # the lift cage's roof
L.at("liftcage", rect=(4, 101, 8, 105))
put(6, 105, "X")
gem((76, 105), (60, 105), (44, 105), (24, 105))

L.write(__file__, "20_reactor_core.txt")
