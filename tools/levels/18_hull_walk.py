#!/usr/bin/env python3
"""Level 18 - Hull Walk (SPEC.md 18). Twist: Low Gravity. Prototype: Recoil Cannon.

A walk west to east along the outside of the station's ring, from an airlock
at the west end to an airlock at the east end. The hull is ground 30 (solid
to the bottom); where there is no hull the column is open to the bottom of
the map, and dropping out of it means drifting off into space (back to the
checkpoint). Everything above the hull is starfield.

Low Gravity (`flags=lowgrav`): gravity x 0.6. Jumps go 1.6 x as high
(Dash 11 cells, Rocco 10, Nova 14), the fall is capped at 1.2 cells a frame,
and steering in the air against the take-off runs at 3/4.

Recoil Cannon: every shot kicks the runner 2 blocks back over 6 frames
(Rocco, on the ground, 1); a shot straight down in the air is a fresh jump,
once per jump.

Changes from the spec, so it plays in this engine:
- The hull plates are `@ hullplate` (`@ plate` is Level 9's pressure plate).
- The UFO's hatch is a breakable (`look=ufo`, hp 2) over the hatch hole
  x 60-61 rows 27-29, with the bonus door inside it, as Level 16's pod.
- The scenery (mast, dishes, trusses, solar wings, airlocks, the UFO dome,
  the girders) is `@ deco kind=` drawn over solid tiles; the satellite
  dish's mast is drawn only, so it is no wall.
- The candid camera is the `C` enemy, carried round its loop by the
  drifter.
"""

from lib import Level

W, H = 200, 46
L = Level(W, H,
          name="STAGE 18 - HULL WALK", episode=3, theme="station_hull",
          music="hull_breathing", weapon="recoil_cannon", par=190, flags="lowgrav")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def hull(x0, x1, top=30):
    L.fill(x0, x1, top, H - 1, "#")


# --- 1. Airlock and dish (x 0-40) ----------------------------------------------------------------
hull(0, 50)
L.at("deco", None, None, kind="airlock", rect=(0, 26, 2, 29))
L.fill(18, 24, 27, 29, "#")                       # the dish's base, 3 up
L.at("deco", None, None, kind="dish", rect=(18, 24, 24, 29))
L.fill(36, 36, 25, 29, "#")                       # the antenna mast
L.at("deco", None, None, kind="mast", rect=(36, 21, 36, 29))
put(3, 29, "P")
put(12, 29, "m")
put(21, 26, "W")
put(34, 29, "h")
put(36, 24, "Q")                                  # the duck in its EVA suit, tethered to the mast
gem((15, 26), (16, 24), (18, 23), (21, 22), (24, 23), (26, 24))

# --- 2. Hull gaps and barnacles (x 41-80) -----------------------------------------------------------
# Hull to x 50, open x 51-54, hull x 55-70 with the UFO's dome, open x 71-74, hull x 75-86.
hull(55, 70)
L.fill(58, 63, 27, 29, "#")                       # the buried UFO's dome
L.at("deco", None, None, kind="ufo", rect=(58, 27, 63, 29))
L.clear(60, 61, 27, 29)                          # the hatch hole (the breakable fills it)
L.at("breakable", None, None, rect=(60, 27, 61, 29), by="any", hp=2, look="ufo")
put(60, 27, "B")                                  # the static inside the hatch
hull(75, 86)
put(42, 29, "c")
L.at("space_barnacle", 48, 29, "SB1")
L.at("space_barnacle", 68, 29, "SB2")
L.at("space_barnacle", 78, 29, "SB3")
put(66, 29, "m")
put(80, 29, "h")
gem((51, 26), (52, 25), (53, 25), (54, 26), (71, 26), (72, 25), (73, 25), (74, 26))
# Nova line: 1-block girders on row 20 from the mast top.
for gx in (42, 48, 54, 60, 66, 72, 78):
    L.put(gx, 20, "=")
    L.at("deco", None, None, kind="girder", rect=(gx, 20, gx, 20))
gem((39, 21), (42, 19), (45, 18), (48, 19), (51, 18), (54, 19), (57, 18), (60, 19), (63, 18), (66, 19),
    (72, 19), (78, 19))

# --- 3. Solar wings (x 81-125) ----------------------------------------------------------------------
# Wing panels one row thick at ground 28: A x 87-96, B x 101-110, C x 115-121.
for x0, x1 in ((87, 96), (101, 110), (115, 121)):
    L.fill(x0, x1, 28, 28, "#")
    L.at("deco", None, None, kind="wing", rect=(x0, 28, x1, 28))
put(82, 29, "c")
put(84, 29, "h")
put(88, 27, "W")
L.at("eva_ram", 100, 20, "ER1")
L.at("eva_ram", 118, 20, "ER2")
put(105, 26, "2")                                 # letter U
put(118, 27, "m")
put(104, 18, "C")
L.at("drifter", None, None, "CAM", path=[(104, 18), (112, 22), (106, 26), (100, 22)], speed="1/4")
gem((89, 24), (92, 24), (95, 24), (98, 24), (103, 24), (108, 24), (112, 24), (116, 24), (120, 24), (124, 24))

# --- 4. Truss towers (x 126-160) ----------------------------------------------------------------------
hull(126, 130)
for x0, x1, top in ((131, 133, 27), (136, 138, 24), (141, 146, 21), (147, 149, 18), (152, 154, 22),
                    (157, 160, 26)):
    L.fill(x0, x1, top, H - 1, "#")
    L.at("deco", None, None, kind="truss", rect=(x0, top, x1, H - 1))
put(127, 29, "c")
put(128, 29, "W")
put(147, 17, "T")
put(148, 17, "h")
# Secret 1: the satellite dish on its mast above the top truss.
L.fill(150, 155, 9, 9, "#")
L.at("deco", None, None, kind="dish", rect=(150, 5, 155, 9))
L.at("deco", None, None, kind="mast", rect=(152, 10, 152, 21))
put(152, 8, "1")                                  # letter G
put(154, 8, "$")
gem((132, 26), (137, 23), (142, 20), (145, 20), (153, 21), (154, 21), (158, 25), (159, 25))

# --- 5. Rivet plates and airlock (x 161-199) ------------------------------------------------------------
hull(161, 164)
hull(165, 190, 32)                                # the inner hull, row 31 open
hull(191, 199)
L.fill(165, 190, 30, 30, "#")                     # the plates (they come loose)
for pid, x0, x1 in (("PL1", 165, 171), ("PL2", 172, 178), ("PL3", 179, 185), ("PL4", 186, 190)):
    L.at("hullplate", None, None, pid, x0=x0, x1=x1, y=30, rivets=4)
L.fill(198, 199, 24, 29, "#")                     # the east airlock's wall
L.at("deco", None, None, kind="airlock", rect=(197, 24, 199, 29))
L.at("deco", 194, 26, kind="42", text="AIRLOCK 42")
put(162, 29, "c")
put(163, 29, "h")
L.at("rivet_mites", 175, 29, "RM1", carrier=1)
L.at("rivet_mites", 188, 29, "RM2", carrier=1)
put(176, 27, "3")                                 # letter N
put(184, 29, "m")
put(196, 29, "X")
gem((167, 28), (170, 27), (180, 27), (187, 28), (192, 28), (194, 28))

L.write(__file__, "18_hull_walk.txt")
