#!/usr/bin/env python3
"""Level 34 - Zeppelin Relay (SPEC.md 34). Twist: The Drifting Fleet.
Prototype: Grapple Gun.

A run left to right across a fleet of airships over a sea of clouds: the
steady flagship, the first fleet (A, B, C), the mines and parrots (D, E,
F2, G), the grapple chain over the Looter, then H, I and the signal ship S
whose bridge is the exit. `@ ship` decks move as one platform (bob and
drift on sines, a parrot on the balloon sinks it); what stands on a deck
rides it. Rows 36 and down are the clouds: a fall.

Everything in world_fleet.cpp.

Changes from the spec, so it plays in this engine:
- Ships that never move (F, C, D, E, H and S) have their decks in the map
  (`tiles=1`); the others are moving platforms. Hulls and balloons are
  scenery, except C's hull, which is built round its bomb bay.
- C keeps still (the spec has it bob and drift) so its bomb bay can be built
  from blocks: a one-way grating floor (86-92, row 29), and a ladder at x
  92 up to the deck whose top (92, 25) is a hatch that opens once you stand
  in the bay. The bay's ring is under the grating at (89, 30).
- The killzone starts at row 38 (the spec has 34), so hanging from the bay
  ring is above the clouds.
- The Looter sits lower and shorter (176-184, deck g26, no balloon), so
  the swings from the chain's rings pass over it and it is no stepping stone
  to H (gap 8, three rows up). The captain's log is on its stern at (183,
  25).
- Parrots perch on top of their balloons (row g-9); the green carrier
  perches on E's rigging at (133, 17) and hops along E's deck (x 124-142)
  once you are there.
- The crates beside the flagship's W box are left out.
- Gems: 6 on each deck (above the deck on the moving ships), 6 along the
  flagship ring's swing, 8 in the bomb bay.
"""

from lib import Level

W, H = 240, 40
L = Level(W, H, name="STAGE 34 - ZEPPELIN RELAY", episode=5, theme="sunset_airships", music="theme_waltz",
          weapon="grapple_gun", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def ship(sid, name, x0, x1, g, tiles=False, **keys):
    if tiles:
        L.row(x0, x1, g, "=")
    L.at("ship", x0, g, sid, name=name, x1=x1, tiles=1 if tiles else None, **keys)


def deck_gems(x0, x1, g, skip=(), lift=1):
    xs = [x for x in range(x0 + 2, x1 - 1, max(1, (x1 - x0 - 3) // 6)) if x not in skip][:6]
    gem(*[(x, g - lift) for x in xs])


# --- The fleet ----------------------------------------------------------------------------------------
ship("F", "CHROME COP", 0, 40, 24, tiles=True)
ship("A", "HOVER LENS", 44, 58, 25, bob="1/120", drift="1/300")
ship("B", "ROOF TURRET", 62, 78, 23, sink=2)
ship("C", "GLASS CRAWLER", 82, 96, 25, tiles=True)
ship("D", "SQUEEGEE DRONE", 100, 116, 24, tiles=True)
ship("E", "BOUNCER", 120, 146, 22, tiles=True)
ship("F2", "DISCO DRONE", 149, 160, 23, drift="1/300", sink=1, phase=150)
ship("G", "GLOW RAVER", 163, 172, 22)
ship("L", "LOOTER", 176, 184, 26, looter=1, balloon=0)
ship("H", "NIGHT STALKER", 193, 206, 23, tiles=True)
ship("I", "GRID LEECH", 209, 222, 22, bob="1/120", drift="1/300", phase=60)
ship("S", "VALVE KEEPER", 225, 239, 21, tiles=True)
L.at("killzone", None, None, rect=(0, 38, 239, 39))

# --- 1. The flagship (0-40) ----------------------------------------------------------------------------
put(3, 23, "P")
put(10, 23, "h")
put(19, 23, "W")
put(30, 23, "m")
put(34, 23, "T")
L.at("ring", 24, 14)
gem((6, 23), (14, 23), (22, 23), (26, 23), (37, 23), (39, 23))
gem((20, 19), (21, 20), (23, 21), (25, 21), (27, 20), (28, 19))   # the ring's swing

# --- 2. The first gaps (44-96) ----------------------------------------------------------------------
put(52, 21, "G")
L.at("spawn", None, None, kind="sky_pirate", trigger=50)
deck_gems(44, 58, 25, skip=(52,), lift=2)
L.at("ring", 60, 15)
put(64, 22, "c")
put(70, 22, "h")
L.at("peck_parrot", 70, 14)
deck_gems(62, 78, 23, skip=(64, 70), lift=2)
L.at("ring", 80, 15)
L.at("spawn", None, None, kind="sky_pirate", trigger=90)
deck_gems(82, 96, 25)
# C's hull and its bomb bay.
L.fill(82, 96, 26, 29, "#")
L.clear(86, 92, 26, 28)
L.row(86, 92, 29, "=")
L.col(92, 26, 28, "H")
L.at("bay", None, None, rect=(86, 26, 92, 28), hatch=(92, 25), drop=(100, 24))
L.at("ring", 89, 30)
put(86, 28, "B")
put(88, 28, "N")
put(90, 28, "m")
gem((86, 26), (87, 26), (88, 26), (89, 26), (90, 26), (91, 26), (87, 27), (89, 27))

# --- 3. Mines and parrots (100-172) -----------------------------------------------------------------
put(104, 23, "W")
put(114, 23, "c")
deck_gems(100, 116, 24, skip=(104, 114))
L.at("spawn", None, None, kind="balloon_mine", rect=(100, 36, 160, 36), every=90, max=2)
put(125, 21, "h")
put(128, 18, "U")
L.at("peck_parrot", 133, 17, carrier=1, rail=(124, 22, 142, 22))
put(140, 21, "m")
deck_gems(120, 146, 22, skip=(125, 140))
L.at("peck_parrot", 154, 14)
deck_gems(149, 160, 23, lift=2)
put(165, 21, "W")
put(166, 21, "h")
put(170, 21, "c")
gem((164, 21), (167, 21), (168, 21), (169, 21))

# --- 4. The grapple chain (173-192) -----------------------------------------------------------------
L.at("ring", 177, 14)
L.at("ring", 183, 13)
L.at("ring", 189, 14)
L.at("chain", None, None, **{"from": (172, 22)}, to=193)
L.at("treasure", 183, 25, art="captains_log", score=10000)

# --- 5. The signal ship (193-239) -------------------------------------------------------------------
put(196, 22, "c")
put(200, 22, "h")
deck_gems(193, 206, 23, skip=(196, 200))
L.at("spawn", None, None, kind="sky_pirate", trigger=205)
put(215, 21, "m")
deck_gems(209, 222, 22, skip=(215,), lift=2)
L.at("deco", 228, 25, kind="42", art="hull_number", text="42", w=1, h=1)
put(230, 20, "Q")
L.at("duck", 230, 20, costume="aviator_goggles")
put(236, 20, "X")
gem((227, 20), (229, 20), (232, 20), (234, 20))

L.write(__file__, "34_zeppelin_relay.txt")
