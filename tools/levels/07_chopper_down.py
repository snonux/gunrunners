#!/usr/bin/env python3
"""Level 7 - Chopper Down (SPEC.md 07). Twist: The Hunter. Boss: Black Halo.

A building site by the bay at night. Start in the yard bottom left (ground
38), cross the rooftops (35 -> 29), climb the girders (26 -> 13), cross the
crane walkway (15) and climb the mast to the jib (deck 10), where Black Halo
is fought; the exit drops from the crane cab.

The hunter: the gunship in the backdrop drifts its searchlight spot (radius
3 blocks) toward you; 15 frames in the light locks you, a beep, and 22
frames later three rockets land on shown markers. Covers (the office roof,
the plastic sheets, the awning and every girder) hide you from the light.

Changes from the spec, so the route plays as intended in this engine:
- The girders climb as a staircase, six of them, none above another: a
  girder 6 rows over the one below leaves no room to jump. Nova's stubs
  shrink to one perch (N1) with a gem.
- The hook runs up the gap between G5 and G6 (x128-129), and its latch is
  at (128, 13), at gun height from G5.
- The Hover Bikers keep to their own roofs (R2, R3) rather than jumping gaps.
- Black Halo turns over x162-165 so its tail rotor is in reach of a runner
  shooting up from the deck.
- Ladders climb the left column of their block, so each one stands a block
  clear of the wall beside it: x30 at the office, x52, x68 and x82 between
  the roofs. The street (ground 40) runs on under the girders, with a
  ladder at x94 back up to G1 for runners who fall.
- The mast shaft is 2 blocks wide (x157-158, ladder on x158): runners are
  1.5 blocks wide.
- The last checkpoint sits on the deck at (159, 9) instead of on the ladder
  top, and "BEAM 42" is stamped on G1 by the girder itself.
"""

from lib import Level

W, H = 190, 42
L = Level(W, H,
          name="STAGE 7 - CHOPPER DOWN", episode=1, theme="chopper_down",
          music="theme_rotor", weapon="lock_on_rockets", par=420, flags="")


def gem(*pts):
    for x, y in pts:
        L.put(x, y, "g")


def cover(x0, y0, x1, y1):
    L.at("cover", rect=(x0, y0, x1, y1))


# The hunter: off until x12, then one demonstration salvo on (30, 37).
L.at("hunter", None, None, "halo", zone=(0, 156), r=3, lock=15, salvo=22, rockets=3, damage=1,
     cooldown=60, speed=1, demo=(30, 37), demotrigger=12)

# --- 1. Yard (x0-35) ----------------------------------------------------------------
L.ground(0, 35, 38)
L.row(18, 28, 32, "#")                      # site office roof
L.col(18, 33, 34, "#")                      # walls over the doorways
L.col(28, 33, 34, "#")
L.col(30, 32, 37, "H")                      # ladder to the office roof
L.at("deco", kind="office", rect=(19, 33, 27, 37))
cover(18, 32, 28, 37)
L.on(2, 38, "P")
L.on(10, 38, "h")
L.at("vehicle", 12, 37, kind="helicopter")  # optional: our own chopper (vehicles/README)
L.on(22, 38, "W")
L.on(25, 38, "T")
L.on(27, 38, "m")
L.put(20, 31, "Q")                          # the duck in a hard hat, on the office roof
L.on(34, 38, "c")
gem((6, 36), (8, 36), (14, 36), (32, 35))

# --- 2. Rooftops (x36-92) -----------------------------------------------------------
L.ground(36, 92, 40)                        # the street
L.ground(36, 50, 35)                        # R1
L.ground(54, 66, 33)                        # R2
L.ground(70, 80, 31)                        # R3
L.ground(84, 92, 29)                        # R4
L.col(52, 35, 39, "H")
L.col(68, 33, 39, "H")
L.col(82, 31, 39, "H")
L.at("deco", kind="sheet", rect=(40, 31, 43, 34))
cover(40, 31, 43, 34)
L.at("deco", kind="sheet", rect=(58, 29, 60, 32))
cover(58, 29, 60, 32)
L.row(72, 78, 27, "=")                      # the awning: one-way, shade below
L.at("deco", kind="awning", rect=(72, 27, 78, 30))
cover(72, 27, 78, 30)
L.put(45, 33, "1")                          # letter G
L.at("hover_biker", 58, 32)
L.at("hover_biker", 77, 30)
L.on(60, 33, "h")
# Secret 1: the cement mixer, merch x2 and the N inside.
L.fill(62, 64, 30, 32, "#")
L.at("breakable", rect=(62, 30, 64, 32), hp=4, by="any", look="mixer")
L.put(62, 32, "m")
L.put(64, 32, "m")
L.put(63, 31, "3")
L.on(74, 31, "W")
L.on(86, 29, "c")
L.at("shield_trooper", 90, 28)
L.put(90, 39, "$")
L.row(88, 90, 25, "#")                      # Nova's perch N1
gem((52, 32), (53, 31), (68, 30), (69, 29), (82, 28), (83, 27), (89, 24), (92, 26))

# --- 3. Girder climb (x93-136) ------------------------------------------------------
L.ground(93, 136, 40)
L.col(94, 26, 39, "H")
GIRDERS = [(95, 99, 26), (102, 106, 23), (109, 113, 20), (116, 120, 17), (123, 127, 15), (130, 136, 13)]
for i, (x0, x1, y) in enumerate(GIRDERS):
    L.row(x0, x1, y, "#")
    L.at("deco", kind="girder", rect=(x0, y, x1, y), text="BEAM 42" if i == 0 else None)
    cover(x0, y + 1, x1, y + 3)
L.on(117, 17, "W")
L.put(118, 14, "2")                         # letter U
L.on(120, 17, "h")
L.on(113, 20, "m")
L.at("shield_trooper", 111, 19)
L.at("shield_trooper", 125, 14)
L.at("rappel", zone=(93, 0, 136, 30), max=2, period=150)
# Secret 2: shoot the latch from G5, drop onto the hook, ride it to the
# counterweight.
L.at("platform", 128, 20, "hook", w=2, path=((128, 20), (128, 5), (140, 5)), speed="1/2", mode="once",
     latch="hooklatch")
L.at("switch", 128, 13, "hooklatch", kind="shootable")
L.on(133, 13, "c")
gem((97, 25), (100, 23), (104, 22), (111, 19), (122, 15), (127, 14))

# --- 4. Crane base (x137-159) -------------------------------------------------------
L.ground(137, 158, 15)                      # walkway
L.col(158, 10, 14, "H")                     # the mast ladder in its shaft (x157-158)
L.ground(159, 159, 11)                      # the mast
L.fill(142, 147, 2, 4, "#")                 # counterweight
L.row(148, 155, 4, "#")                     # the spare gunship's deck
L.at("deco", kind="spareship", rect=(148, 0, 155, 3))
L.at("deco", kind="lattice", rect=(140, 5, 156, 9))
L.at("health", 152, 14, full=1)
L.put(144, 1, "C")
L.put(151, 1, "B")
gem((143, 1), (145, 1), (146, 1))

# --- 5. Arena: the jib (x159-189) ---------------------------------------------------
L.ground(159, 189, 10)
L.row(160, 163, 6, "#")                     # the cab roof
L.at("cab", rect=(160, 7, 163, 9))
L.at("deco", kind="radio", x=161, y=9, w=2, h=1)
L.on(159, 10, "c")
L.put(165, 9, "X")                          # drops in after the fight
L.at("boss", None, None, "black_halo", rect=(160, 0, 189, 11), deck=10, exit=(165, 9))

L.write(__file__, "07_chopper_down.txt")
