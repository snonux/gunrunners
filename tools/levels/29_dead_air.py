#!/usr/bin/env python3
"""Level 29 - Dead Air (SPEC.md 29). Twist: Color Bars.
Prototype: Tri-Tone Blaster. Episode 5 opens.

A run left to right through a black void of static: the white floor, the
colour-bar tower, the ghost-bar maze, the Test Card's arena, and tuning in
at the exit. One signal colour (start red): bars of that colour are solid,
the others ghosts. RGB dials change it (a blaster shot steps it red, green,
blue; a Tri-Tone shot sets its tint). A change flashes the incoming bars
first and then waits until the runner is off the outgoing colour, so the
tower is climbed as "shoot, jump, the change lands mid-air, land on the new
colour". All three Tri-Tone tints fired within 90 frames: black bars (the
test-pattern archive) are solid. Below row 59 is the void.

Everything in world_signal.cpp.

Changes from the spec, so it plays in this engine:
- The dials are at a standing runner's shot height where they are meant to
  be shot from a bar or a floor: s1 (31, 48) over the white island, and on
  the tower one under each step: s2 (46, 48) at its foot, s3 (50, 45),
  s4 (54, 42), s5 (58, 39), s6 (62, 36) (spec: s2 (55, 36), s3 (62, 29),
  out of reach of a shot from the bars). The maze's s7 (79, 33) and
  s8 (91, 33) are shot from below, as specified.
- The maze's `m` is at (84, 39) (spec (86, 39) is inside Nova's tower).
- The arena is `@ cardarena rect= wall=x,y0,y1 exit=x,y0,y1` and tuning in
  `@ tunein x=`.
- Rocco's tunnel comes out by a 4-block hole at 97-100 with a step under
  its right half, so a 3-block jump gets out (spec: a 2-block hole). The way on (x 137 rows 36-41) is shut with the way back
  until the Test Card is down, so the card has to be beaten.
- The arena is lowered so every runner can hit the card: the card hovers
  low at (120, 40) (spec (120, 30), out of reach of Rocco's jump shots),
  the red and blue bars are at g41 (spec g40, a 4-block step from the
  floor) and the green bar at 116-119 g38 (spec 118-121 g37, inside the
  card now), and the white ledge with the treasure at g35 (spec g34), 3
  blocks over the green bar. The card is hit with jump shots from the floor, level shots
  from the red and blue bars, crouched shots from the green bar, and
  blaster shots straight up from under it.
"""

from lib import Level

W, H = 160, 60
L = Level(W, H, name="STAGE 29 - DEAD AIR", episode=5, theme="dead_air", music="theme_glitch",
          weapon="tri_tone_blaster", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def floor(x0, x1, g, bottom=None):
    L.fill(x0, x1, g, g + 3 if bottom is None else bottom, "#")


def bar(color, x0, y0, x1, y1):
    L.at("bar", None, None, color=color, rect=(x0, y0, x1, y1))


L.at("signal", None, None, start="red")

# --- 1. White floor (x 0-44) ----------------------------------------------------------------------
floor(0, 24, 50)
bar("red", 25, 50, 28, 53)
floor(29, 32, 50)
bar("blue", 33, 50, 36, 53)
floor(37, 44, 50)
put(2, 49, "P")
put(6, 49, "W")
put(12, 49, "m")
put(20, 49, "h")
L.at("rgbswitch", 31, 48, "s1", mode="shoot")
put(38, 49, "T")
put(42, 49, "c")
gem((23, 48), (24, 47), (25, 46), (26, 46), (27, 46), (28, 46), (29, 47), (30, 48))  # the arc over the red bar

# --- 2. Bar towers (x 45-72) ----------------------------------------------------------------------
bar("red", 45, 47, 48, 47)
bar("green", 49, 44, 52, 44)
bar("blue", 53, 41, 56, 41)
bar("red", 57, 38, 60, 38)
bar("green", 61, 35, 64, 35)
floor(65, 72, 32, 53)                       # the white tower top
for x, y in ((46, 48), (50, 45), (54, 42), (58, 39), (62, 36)):
    L.at("rgbswitch", x, y, "s%d" % ((x - 46) // 4 + 2), mode="shoot")
for x0, g in ((40, 45), (46, 40), (52, 35), (58, 30)):
    L.row(x0, x0 + 1, g, "#")               # the Turbo stair (white)
put(63, 34, "h")
L.at("ghost_image", 66, 31)
put(70, 31, "W")
put(68, 31, "c")
L.at("deco", 66, 30, kind="42", text="CH 42", art="ch42_sign", w=2, h=1)
gem((40, 44), (41, 44), (44, 42), (46, 39), (47, 39), (50, 37), (52, 34), (53, 34), (58, 29), (59, 29))

# --- 3. Ghost-bar maze (x 73-104) -----------------------------------------------------------------
floor(73, 104, 40, 53)
for x0, c in ((76, "red"), (82, "green"), (88, "blue"), (94, "red")):
    bar(c, x0, 34, x0 + 1, 39)
L.at("rgbswitch", 79, 33, "s7", mode="shoot")
L.at("rgbswitch", 91, 33, "s8", mode="shoot")
L.fill(79, 80, 36, 39, "#")                 # Nova's towers over the green wall
L.fill(85, 86, 36, 39, "#")
# Rocco's tunnel under the maze floor: in by the hole at 74-75, out by the
# hole at 97-100 and the step at 99-100 under it.
L.clear(74, 98, 41, 44)
L.clear(74, 75, 40, 40)
L.clear(97, 100, 40, 41)
for x, y, c in ((80, 36, 1), (87, 35, 1), (93, 37, 1), (99, 36, 1), (82, 43, 0), (88, 43, 0), (94, 43, 0)):
    L.at("static_swarm", x, y, carrier=c)
put(84, 39, "m")
put(92, 36, "U")
put(101, 39, "h")
put(102, 39, "c")
gem((78, 39), (81, 35), (87, 39), (90, 39), (92, 39), (93, 39), (97, 39), (103, 39))

# The test-pattern archive over the maze: up the black step from the tower top.
bar("black", 73, 30, 75, 30)
L.row(78, 87, 28, "#")
bar("black", 88, 28, 89, 28)
L.row(90, 91, 28, "#")
L.row(76, 91, 22, "#")
L.col(91, 23, 27, "#")
put(84, 27, "G")
put(80, 27, "m")
put(88, 25, "B")
gem((79, 27), (81, 27), (82, 27), (83, 27), (85, 27), (86, 27), (87, 27), (90, 27), (79, 24), (82, 24), (85, 24),
    (90, 24))

# --- 4. The Test Card's arena (x 105-136) ---------------------------------------------------------
floor(105, 136, 44)
bar("red", 110, 41, 113, 41)
bar("green", 116, 38, 119, 38)
bar("blue", 126, 41, 129, 41)
L.row(124, 127, 35, "#")                    # the white ledge
L.at("test_card", 120, 40)
L.at("cardarena", None, None, rect=(105, 20, 136, 44), wall=(104, 34, 39), exit=(137, 36, 41))
L.at("rgbswitch", 106, 34, "t1", mode="timed", period=150, warn=22, rect=(105, 20, 136, 44))
L.at("rgbswitch", 135, 34, "t2", mode="timed", period=150, warn=22, phase=75, rect=(105, 20, 136, 44))
put(108, 43, "h")
put(124, 43, "W")
put(131, 43, "m")
L.at("treasure", 126, 34, score=10000, art="maxtv_ident")
gem((111, 40), (112, 40), (117, 37), (118, 37), (127, 40), (128, 40))

# --- 5. Tune in (x 137-159) -----------------------------------------------------------------------
floor(137, 159, 42)
put(140, 41, "c")
put(142, 41, "h")
L.at("ghost_image", 147, 41)
L.at("ghost_image", 152, 41)
put(146, 39, "N")
put(156, 41, "X")
put(158, 41, "Q")
L.at("duck", 158, 41, costume="test_bars")
L.at("tunein", None, None, x=150)
gem((138, 40), (144, 40), (149, 40), (150, 40), (154, 39), (159, 40))

L.header["tip"] = "ONLY BARS OF THE SIGNAL COLOUR ARE SOLID: SHOOT A DIAL TO CHANGE THE COLOUR"  # HINTS: the level's first 12 seconds
L.write(__file__, "29_dead_air.txt")
