#!/usr/bin/env python3
"""Level 11 - Idol Mines (SPEC.md 11). Twist: Mine Carts. Prototype: Blasting Caps.

Winding, left to right through lantern-lit gold mines: the practice track
(start, top left), the downhill ride through the bat tunnels and under the
cursed veins, the rock maze on foot, the trestle race over the chasm with
the Cart Bandits riding alongside, and the mine elevator up to the exit.

Mine carts ride rails (`@ rail`, a polyline lying on the ground rows it
names). Jump into a cart to ride it; a still one sets off the way you face.
Jump hops it (4 cells, 12 frames); jump again in the air bails out; down
ducks. Lanterns stand before every gap and dead end; a bumper stops a cart
(too fast and you crash: thrown forward, no damage). Shoot a lever to send
the next cart down the other branch. Blasting Caps are lobbed, bounce twice
and roll (fast along a rail), then blow up after 30 frames: they break the
cracked rock (`look=rock`, by=explosion) and hurt you if you're too close.

Changes from the spec, so the route plays as intended in this engine:
- Carts are `@ cart` on a rail (not platform mode=cart) and the junction
  levers are `@ lever` (`switch` is level 7's latch); both are shot.
- The J1 lever stands on the dock so it can be shot before the ride; its
  loop-the-loop branch is a real loop (x 60-71, rows 15-24).
- The first gap's pit has a ladder: a cart that drops in is lost (it docks
  again), and you climb out and walk on to the station.
- The bat clouds fly over flat track, centred 5 rows above it: the lower
  peak catches a standing rider and misses a ducking one.
- The maze's steps have headroom over their take-off spots; the corridor
  under CW2 widens at its east end for the climb onto the CW3 step.
- The trestle's gaps are x 185-189 and 193-197 over the chasm's lower
  track R5 (x 185-203), so a cart that falls lands on R5.
- The trestle deck is one-way timber (`=`) you can walk; its spur hole
  (x 164-166) drops you to the strongbox too.
- J2 stands at the west end of the station (151, 27).
- The trestle station has no roof (it would close the walk in from x 150).
- Tunnel bats patrol two stretches of the Rocco tunnel (4 bats each).
- The maze's last room (x 145-150) is ground 28, level with the trestle
  station (the spec's step up to 27 had no headroom under the office).
- CW4 stands on the landing (x 205) rather than at the trestle's end, so
  a rider who stops at the bumper has room to back off from the blast.
- Ladder shafts are two blocks wide (ladders snap to the left half of their
  block): x 147-148, x 160-161, and x 206-207 from the chasm to the landing.
"""

import math

from lib import Level

W, H = 220, 44
L = Level(W, H,
          name="STAGE 11 - IDOL MINES", episode=2, theme="gold_mines",
          music="theme_wood_blocks", weapon="blasting_caps", par=360, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def rock(x0, y0, x1, y1, **keys):
    """A cracked rock wall that only a cap blast breaks."""
    L.fill(x0, x1, y0, y1, "#")
    L.at("breakable", rect=(x0, y0, x1, y1), hp=1, by="explosion", look="rock", **keys)


# --- 1. Practice track (x1-34, ground 14) ------------------------------------------------
room(1, 34, 4, 13)
L.row(4, 6, 11, "=")           # the timber ledge
on(2, 14, "P")
L.at("rail", None, None, "R0", path=[(6, 14), (28, 14)])
L.at("bumper", 5, 13)
L.at("bumper", 29, 13)
L.at("cart", 7, 13, "CT0", rail="R0")
L.at("dayssign", 9, 8, w=4, h=2)
L.put(5, 10, "Q")              # the duck in a miner's helmet
on(31, 14, "W")
on(33, 14, "m")
gem((12, 12), (16, 12), (20, 12), (24, 12), (4, 10))

# --- 2. The first ride (x35-110) ----------------------------------------------------------
room(35, 110, 4, 23)
L.ground(35, 44, 14)           # the dock


def slope_row(x):
    """The ground under the downhill rail (44,14)-(60,24), right edge of block x."""
    y = 28 + (2 * x + 2 - 89) * 0.625
    return math.ceil(y / 2)


for x in range(45, 60):
    L.ground(x, x, max(14, slope_row(x)))
L.ground(60, 110, 24)
room(80, 84, 24, 29)           # the pit under the gap, with a ladder out
L.col(84, 24, 29, "H")
L.fill(94, 102, 4, 19, "#")    # the vein tunnel's low ceiling
L.at("rail", None, None, "R1", path=[(35, 14), (44, 14), (60, 24), (103, 24)], gaps="80..84")
L.at("rail", None, None, "LOOP",
     path=[(62, 24), (66, 24), (69, 23), (71, 20), (70, 17), (67, 15), (64, 15), (61, 17), (60, 20), (62, 23),
           (65, 24), (67, 24)], branch="J1:1", reset=1)
L.at("bumper", 34, 13)
L.at("bumper", 104, 23)
L.at("cart", 36, 13, "CT1", rail="R1", paint42=1)
L.at("lever", 41, 13, "J1")
L.at("batcloud", 62, 19, count=6, patrol="60..78", amp=2)
L.at("batcloud", 86, 19, count=5, patrol="85..93", amp=2)
L.at("lantern", 79, 23)
L.at("lantern", 85, 23)
L.at("vein", rect=(94, 18, 102, 19), carrier=1)
on(106, 24, "c")
on(108, 24, "h")
gem((47, 14), (49, 15), (51, 16), (53, 18), (55, 19), (57, 20), (66, 21), (74, 22), (90, 22), (98, 22))

# --- 3. The rock maze, on foot (x111-150) ---------------------------------------------------
room(111, 118, 20, 23)         # ground 24, the trapdoor at x116-117
rock(119, 20, 119, 23)         # CW1
room(120, 128, 21, 25)         # ground 26
rock(127, 26, 128, 26)         # CW2, the cracked floor
room(127, 135, 27, 29)         # the lower corridor, floor 30
room(133, 135, 25, 26)         # headroom for the step up at its east end
L.fill(136, 136, 28, 29, "#")  # the step
rock(136, 25, 136, 27)         # CW3 above it
room(137, 144, 25, 27)         # ground 28
room(136, 144, 20, 23)         # 3a. the foreman's office, floor 24 (secret 2)
rock(139, 24, 140, 24)         # CW5, its cracked floor
L.at("rubble", 141, 27, breakable=3)  # CW5 is the 4th breakable
room(145, 150, 23, 27)         # ground 28 (level with the trestle station)
# Rocco's way: a trapdoor down a shaft to the tunnel and a ladder back up.
L.at("trapdoor", 116, 24, w=2)
room(116, 117, 25, 36)
room(116, 148, 37, 39)         # the tunnel, ground 40
room(147, 148, 28, 36)         # the ladder shaft, its left column open to the top
L.col(148, 28, 39, "H")
on(113, 24, "W")
L.at("respawn", 113, 23, frames=150)
on(122, 26, "c")
on(126, 26, "m")
L.put(131, 28, "2")            # letter U
L.at("mole", 124, 25)
L.at("mole", 132, 29)
L.at("mole", 140, 27)
on(142, 28, "h")
L.at("batcloud", 122, 38, count=4, patrol="118..130", amp=1)
L.at("batcloud", 138, 38, count=4, patrol="134..146", amp=1)
on(138, 24, "$")
on(143, 24, "m")
gem((112, 22), (115, 22), (121, 24), (124, 23), (129, 28), (134, 28), (138, 26), (146, 25), (149, 25),
    (120, 38), (130, 38), (140, 38))
gem((137, 21), (139, 21), (140, 21), (141, 22), (142, 21), (144, 22))

# --- 4. The trestle race (x151-200) ----------------------------------------------------------
room(151, 200, 16, 27)         # station ground 28 x151-161, then the trestle
L.row(162, 199, 28, "=")       # the deck
L.row(164, 166, 28, ".")       # the spur's hole
L.row(185, 189, 28, ".")       # the two gaps
L.row(193, 197, 28, ".")
room(162, 181, 29, 39)         # under the deck, ground 40
L.fill(173, 173, 29, 34, "#")  # the strongbox: its wall and roof
L.row(173, 181, 34, "#")
room(184, 207, 29, 41)         # the chasm, ground 42
room(160, 160, 28, 39)         # the ladder shaft up to the station
L.col(161, 28, 39, "H")
L.at("rail", None, None, "R4", path=[(158, 28), (199, 28)], gaps="185..189;193..197")
L.at("rail", None, None, "SPUR", path=[(163, 28), (167, 40), (173, 40)], branch="J2:1")
L.at("rail", None, None, "R5", path=[(185, 42), (203, 42)])
L.at("rail", None, None, "R6", path=[(165, 22), (199, 22)])
L.at("bumper", 157, 27)
L.at("bumper", 200, 27)
L.at("bumper", 174, 39)
L.at("bumper", 184, 41)
L.at("bumper", 204, 41)
L.at("lever", 151, 27, "J2")
L.at("cart", 159, 27, "CT4", rail="R4")
L.at("cart", 186, 41, "CT5", rail="R5")
L.at("cartbandit", 166, 21, rail="R6")
L.at("cartbandit", 186, 21, rail="R6")
for x in (184, 190, 192, 198):
    L.at("lantern", x, 27)
on(152, 28, "h")
on(153, 28, "W")
on(154, 28, "c")
on(156, 28, "T")
L.put(187, 24, "3")            # letter N, over the first gap
L.put(178, 17, "C")            # the candid camera on a lantern hook
# The strongbox (secret 1): J2 to state 1 before the cart passes x163.
L.put(176, 39, "1")            # letter G
on(178, 40, "$")
on(175, 40, "m")
L.put(180, 37, "B")
gem((174, 36), (175, 36), (176, 36), (177, 36), (178, 36), (179, 36), (177, 38), (179, 39), (174, 38),
    (180, 39), (181, 39), (181, 36))
gem((170, 26), (174, 26), (178, 26), (182, 26), (187, 26), (188, 26), (195, 26), (196, 26), (186, 38),
    (192, 40), (198, 40), (170, 38), (166, 38), (163, 38))

# --- 5. The landing and the elevator (x201-219) ---------------------------------------------------
room(201, 207, 23, 27)         # the landing, ground 28
rock(205, 23, 205, 27)         # CW4, between the landing and the elevator
L.col(200, 28, 41, "H")
L.col(207, 28, 41, "H")
L.put(206, 28, ".")            # (a ladder shaft is two blocks wide)
room(208, 211, 2, 28)          # the elevator shaft
room(212, 218, 2, 5)           # the exit ledge, ground 6
L.at("platform", 208, 28, "EL", w=4, path=[(208, 28), (208, 6)], speed="1", mode="pingpong", wait="rider")
L.at("mole", 201, 41)
on(204, 28, "c")
on(203, 28, "h")
on(216, 6, "X")
on(214, 6, "m")
gem((209, 22), (210, 19), (209, 16), (210, 13), (209, 10), (210, 7))

L.write(__file__, "11_idol_mines.txt")
