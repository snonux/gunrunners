#!/usr/bin/env python3
"""Level 6 - Maglev Express (SPEC.md 06). Twist: Clearance.

Nine train cars, rear (left) to engine (right), on a roof at row 12. The
map never moves: the backdrop scrolls (trainscroll) and gantries sweep over
the train from the front. Crouch under the low ones, jump the tall ones,
take a hatch into a car for the tall4 ones and for the tunnel's mouth and
rings. The exit is on the station platform that slides in as the train
brakes past x244.

Deviations from the spec, all for footing:
- the right-hand hatch of each interior car is x0+23..x0+24 with its
  ladder on x0+24 (the right column of the hole, as in every shaft);
- car 5's checkpoint is at (125, 11) and car 6's at (155, 11), off the
  hatch holes at x122-123 and x152-153;
- the health box on the engine is at (248, 11), outside the cab;
- the dining car's floor panel is pried up (stand on it and hold down)
  rather than shot, since nobody can shoot at their own feet;
- seats are single solid blocks with a seat drawn over them.
"""

from lib import Level

W, H = 280, 24
L = Level(W, H,
          name="STAGE 6 - MAGLEV EXPRESS", episode=1, theme="maglev_express",
          music="theme_arpeggio", weapon="arc_caster", par=300)

INTERIOR = (2, 4, 5, 6, 8)


def car(k):
    x0 = 30 * (k - 1)
    return x0, x0 + 26


def gems(*pts):
    for x, y in pts:
        L.put(x, y, "g")


L.at("trainscroll", speeds=(6, 12, 24))

for k in range(1, 10):
    x0, x1 = car(k)
    L.fill(x0, x1, 12, 20, "#")          # roof 12-13, body 14-18, floor 19-20
    if k in INTERIOR:
        L.clear(x0 + 1, x1 - 1, 14, 18)
        L.at("deco", kind="carinterior", rect=(x0 + 1, 14, x1 - 1, 18))
        for hx in (x0 + 2, x0 + 23):     # roof hatches with their ladders
            L.clear(hx, hx + 1, 12, 13)
            L.col(hx + 1, 12, 18, "H")

# Gangways joining cars 4-5 and 5-6 inside, over the gaps.
for gx in (117, 147):
    L.fill(gx, gx + 2, 15, 19, "#")
    L.clear(gx - 1, gx + 3, 16, 18)
    L.at("deco", kind="carinterior", rect=(gx - 1, 16, gx + 3, 18))

# --- 1. Rear cars (0-56): gantries only ----------------------------------------
L.put(2, 11, "P")
L.put(20, 11, "h")
for x in (36, 47):                       # luggage in car 2
    L.fill(x, x + 1, 17, 18, "#")
    L.at("deco", kind="crate", rect=(x, 17, x + 1, 18))
L.put(43, 18, "W")
L.put(52, 18, "$")
L.put(35, 18, "m")
gems((8, 11), (12, 11), (16, 11), (24, 11), (38, 11), (46, 11))
for kind, x in (("low", 6), ("tall", 18), ("low", 32), ("tall4", 40), ("tall", 50)):
    L.at("gantry", kind=kind, trigger=x, speed=3, warn=30)

# --- 2. Middle cars (60-116) -----------------------------------------------------
for x in (94, 98, 102, 106, 110):        # car 4's seats
    L.put(x, 18, "#")
    L.at("deco", kind="seat", x=x, y=18, w=1, h=1)
L.at("deco", kind="hiscore", x=102, y=15, w=4, h=2)
L.at("track_hopper", 72, 11)
L.at("track_hopper", 98, 11)
L.at("track_hopper", 110, 11)
L.put(75, 11, "h")
L.put(104, 18, "W")
L.put(96, 18, "m")
gems((62, 11), (68, 11), (80, 11), (84, 11), (100, 11), (106, 11))
for kind, x in (("tall", 64), ("low", 78), ("tall4", 96), ("low", 108)):
    L.at("gantry", kind=kind, trigger=x, speed=3, warn=30)

# --- 3. Tunnel (117-149) -----------------------------------------------------------
L.at("tunnel", rect=(118, 0, 149, 23), ring=90)
for x in (125, 132):                     # car 5's seats; the sleeper has 132
    L.put(x, 18, "#")
    L.at("deco", kind="seat", x=x, y=18, w=1, h=1)
L.row(136, 140, 16, "=")                 # the luggage rack
L.at("deco", kind="sleeper", rect=(131, 16, 133, 17))
L.at("deco", kind=42, x=130, y=16, w=1, h=1)
L.put(125, 11, "c")
L.put(128, 18, "h")
L.put(138, 15, "Q")
L.put(141, 18, "m")
gems((123, 18), (129, 18), (135, 18), (139, 18))
L.at("rail_drone", 125, 6)
L.at("rail_drone", 140, 6)
# The passing train: a gap in the tunnel roof and a second train's roof
# three rows above ours, with the camera and the bonus patch on it.
L.at("platform", None, None, "passer", x=150, y=9, w=12, mode="once", path=[(150, 9), (110, 9)], speed="1/2")
L.at("candid_camera", 156, 8, ride="passer")
L.at("bonuspatch", on="passer", dx=2)
L.at("passgap", 125, 11, frames=160, platform="passer")

# --- 4. Last three cars (150-236) -------------------------------------------------
x0, x1 = car(6)
L.fill(x0, x1, 19, 22, "#")              # the dining car sits on a service deck
L.clear(152, 174, 20, 21)                # the crawlspace
L.at("breakable", None, None, rect=(157, 19, 158, 19), hp=1, by="pry", look="panel")
L.put(155, 11, "c")
L.put(165, 18, "h")
L.put(172, 18, "T")
L.put(170, 21, "1")                      # letter G
gems((153, 21), (156, 21), (160, 21), (163, 21), (166, 21), (173, 21))
L.at("rail_drone", 165, 6, carrier=1)
L.at("rail_drone", 215, 6, carrier=1)
L.at("track_hopper", 195, 11)
L.put(186, 11, "W")
for x in (215, 220, 230):                # car 8's seats
    L.put(x, 18, "#")
    L.at("deco", kind="seat", x=x, y=18, w=1, h=1)
L.put(225, 18, "2")                      # letter U
L.put(228, 18, "m")
for x in (178, 208, 238):
    L.at("decoupler", x, 11)
gems((184, 11), (190, 11), (198, 11), (202, 11), (216, 11), (228, 11))
for kind, x in (("low", 160), ("tall", 190), ("low", 220)):
    L.at("gantry", kind=kind, trigger=x, speed=3, warn=30)

# --- 5. Engine and station (240-279) -----------------------------------------------
L.fill(250, 262, 9, 11, "#")             # the cab
L.put(244, 11, "c")
L.put(248, 11, "h")
L.put(256, 8, "3")                       # letter N
L.fill(270, 279, 14, 23, "#")
L.at("layer", None, None, "station", rect=(270, 14, 279, 23), tile="#", driver="script", style="station", solid=0)
L.at("arrival", 244, 11, layer="station")
L.put(276, 13, "X")

L.write(__file__, "06_maglev_express.txt")
