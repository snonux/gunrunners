#!/usr/bin/env python3
"""Level 4 - Blackout (SPEC.md 04). Twist: Power Cuts.

A street at ground 26 runs left to right through four dark sectors. Sector A
climbs a fire escape to its breaker, sector B's breaker is down in a
two-level garage (out by the lift it powers), sector C's sits past a dark
spike corridor with a Grid Leech crawling along the catwalk cable toward
it, and sector D's is on the last roof next to the exit, which stays dead
until D is lit.

Deviations from the spec, all for footing:
- the side ladders reach one row above their roof (x164 rows 20-25, x177
  rows 18-25), and the two in the gaps between D2/D3 and D3/D4 stand next
  to the roof they climb (x189, x198) rather than mid-gap (x188, x197);
- the shaft ladder runs from the street row (x57 rows 26-38) so you can
  step off it at the top, as in the club's laser room;
- the lift's floor hatch is a door: `@ door hB 66 32 w=3 h=2 open=bB`;
- the billboard is its own dark sector with no breaker, so only a flare
  lights it.
"""

from lib import Level

W, H = 210, 40
G = 26  # the street
L = Level(W, H,
          name="STAGE 4 - BLACKOUT", episode=1, theme="blackout",
          music="theme_heartbeat", weapon="flare_gun", par=420, flags="dark")

# Everything below the street is solid; above it is night air.
L.ground(0, W - 1, G)
L.row(0, W - 1, 0, "#")


def gems(*pts):
    for x, y in pts:
        L.put(x, y, "g")


# --- Sectors (declared first: breakers name them) ------------------------------
for sid, x0, x1 in (("A", 15, 55), ("B", 56, 105), ("C", 106, 155), ("D", 156, 209)):
    L.at("dark", None, None, sid, rect=(x0, 0, x1, 39))
L.at("dark", None, None, "billboard", rect=(80, 15, 86, 19))

# --- 1. Last lit block (0-14) ------------------------------------------------------
L.at("light", 8, 18, r=6)
L.put(2, 25, "P")
L.put(8, 25, "W")
L.put(12, 25, "*")
L.put(5, 25, "m")

# --- 2. First dark alley (15-31) ---------------------------------------------------
gems(*[(x, 24) for x in range(16, 31, 2)])
L.at("deco", kind="graffiti", x=22, y=19, w=5, h=3)
L.at("deco", kind=42, x=20, y=24, w=1, h=1, text="42")
L.at("looter", 28, 25)

# --- 3. Sector A fire escape (32-55) -------------------------------------------------
for x0, x1, y in ((33, 38, 23), (41, 46, 20), (33, 38, 17), (41, 46, 14), (33, 38, 11), (41, 46, 8)):
    L.row(x0, x1, y, "=")
L.put(37, 22, "h")
L.put(36, 10, "m")
L.at("night_stalker", 43, 13)
L.at("breaker", 45, 7, "bA", sector="A")
L.put(42, 7, "Q")
L.at("light", 42, 7, r=1)                    # the duck's miner's headlamp
L.fill(50, 58, 9, 21, "#")                   # overhang (roof R1 on top)
L.put(49, 25, "c")

# --- 4. Sector B street (56-105), manhole, stash -------------------------------------
L.clear(56, 57, G, 38)                       # the manhole shaft
L.at("deco", kind="interior", rect=(56, 27, 57, 38))
L.col(57, G, 38, "H")
L.clear(49, 54, 30, 32)                      # the stash
L.clear(55, 55, 30, 32)                      # its opening to the shaft
L.at("deco", kind="interior", rect=(49, 30, 55, 32))
L.at("stash", None, None, rect=(49, 30, 54, 32), manhole=(56, 26))
L.put(53, 32, "1")                           # letter G
L.put(52, 32, "$")
L.put(56, 36, "B")
L.at("deco", kind="text", rect=(80, 15, 86, 19), text="MAX'S DINER")
L.put(83, 17, "C")
L.at("looter", 90, 25)
L.fill(94, 107, 9, 21, "#")                  # building (roof R5 on top)
# Nova's roof line: R2-R4, slabs over the street.
L.row(62, 67, 5, "#")
L.row(71, 80, 7, "#")
L.row(84, 90, 3, "#")

# --- 5. Garage (60-102) ------------------------------------------------------------
L.clear(60, 102, 28, 31)                     # G1, ground 32
L.clear(60, 102, 34, 37)                     # G2, ground 38
L.clear(60, 62, G, 27)                       # the way in
L.clear(98, 101, 32, 33)                     # G1 down to G2
L.clear(66, 68, G, 38)                       # the lift shaft
L.at("deco", kind="interior", rect=(60, 28, 102, 31))
L.at("deco", kind="interior", rect=(60, 34, 102, 37))
L.at("deco", kind="interior", rect=(66, 27, 68, 38))
L.at("deco", kind="interior", rect=(98, 32, 101, 33))
L.put(63, 31, "W")
L.put(80, 31, "h")
L.put(95, 31, "m")
gems(*[(x, 30) for x in (72, 77, 82, 86, 90, 94)])
for x, y in ((75, 31), (90, 31), (85, 37), (72, 37)):
    L.at("night_stalker", x, y)
L.put(99, 37, "c")
L.put(70, 37, "h")
L.at("breaker", 64, 37, "bB", sector="B")
L.at("door", 66, 32, "hB", w=3, h=2, open="bB")
L.at("platform", 66, 26, "liftB", w=3, path=((66, 26), (66, 38)), mode="pingpong", speed=1, powered="bB")

# --- 6. Sector C (106-155) ------------------------------------------------------------
L.put(108, 25, "c")
L.put(112, 25, "W")
L.at("looter", 115, 25)
for x0 in (120, 127, 134):
    L.row(x0, x0 + 2, 25, "^")
    L.at("spikes", None, None, rect=(x0, 25, x0 + 2, 25), hidden="dark")
L.row(108, 150, 16, "=")                     # the catwalk
L.col(110, 16, 25, "H")
L.col(150, 16, 25, "H")
L.at("night_stalker", 125, 15)
L.put(130, 15, "h")
L.put(130, 13, "2")                          # letter U
L.put(146, 15, "m")
gems((115, 14), (120, 14), (136, 14), (141, 14))
L.at("deco", kind="cat", x=141, y=24, w=1, h=1)
L.at("breaker", 152, 25, "bC", sector="C")
L.at("cable", None, None, "cC", path=((108, 15), (148, 15), (151, 16), (151, 24), (152, 25)), breaker="bC")
L.fill(153, 157, 9, 21, "#")                 # overhang

# --- 7. Sector D rooftops (156-209) ------------------------------------------------------
L.at("light", 160, 19, r=4)
L.put(158, 25, "c")
L.put(160, 25, "T")
L.put(162, 25, "W")
for x0, x1, top in ((165, 175, 21), (178, 186, 19), (190, 196, 21), (199, 207, 19)):
    L.fill(x0, x1, top, G - 1, "#")
L.col(164, 20, 25, "H")
L.col(177, 18, 25, "H")
L.col(189, 20, 25, "H")
L.col(198, 18, 25, "H")
L.put(172, 20, "h")
L.at("night_stalker", 183, 18)
L.at("night_stalker", 192, 20)
L.put(194, 20, "3")                          # letter N
gems((168, 17), (172, 17), (181, 17), (185, 17), (201, 17), (204, 17))
L.at("breaker", 203, 18, "bD", sector="D")
L.put(206, 18, "X")

# --- Shutters (after their breakers) ------------------------------------------------------
L.at("door", 54, 22, "dA", h=4, open="bA")
L.at("door", 104, 22, "dB", h=4, open="bB")
L.at("door", 154, 22, "dC", h=4, open="bC", opentime=240)

L.write(__file__, "04_blackout.txt")
