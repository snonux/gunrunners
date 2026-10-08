#!/usr/bin/env python3
"""Level 1 - Rooftop Run (SPEC.md 01). Twist: Beat Signs.

A left-to-right rooftop run: start low on the left (ground 30), climb to
ground 16 by the ladder wall, climb the sign tower to ground 6 and finish on
the far right across the giant GUN sign. Neon signs are layers that are only
solid while lit on the 120 BPM music clock.
"""

from lib import Level

L = Level(190, 34,
          name="STAGE 1 - ROOFTOP RUN", episode=1, theme="neon_rooftops",
          music="theme_synthwave", weapon="pulse_pistol", par=300, flags="")


def sign(sid, x0, y0, x1, y1, beats="1,2,3", color=None):
    L.at("layer", None, None, sid, rect=(x0, y0, x1, y1), driver="beat", beats=beats, style="sign", color=color)


def letter(lid, rects, beat, color):
    # The GUN letters are always solid; only their lighting runs on the beat.
    for i, (x0, y0, x1, y1) in enumerate(rects):
        L.at("layer", None, None, "%s%d" % (lid, i), rect=(x0, y0, x1, y1), driver="lit", beats=beat,
             style="sign", color=color)


# 1. Start and steps (x0-25).
L.ground(0, 14, 30)
L.ground(15, 18, 28)
L.ground(19, 25, 25)
L.put(1, 29, "*")
L.on(3, 30, "P")
L.on(9, 30, "m")
L.at("chrome_cop", 12, 29)
L.at("roof_turret", 22, 24)
for x in (6, 7, 8):
    L.put(x, 27, "g")

# 2. First sign over the dip (x26-29).
L.ground(26, 29, 26)
sign("s0", 26, 25, 29, 25, color="pink")
L.put(27, 24, "g")
L.put(28, 24, "g")

# 3. Side roof (x30-40).
L.ground(30, 40, 25)
L.row(33, 37, 22, "=")
L.put(35, 20, "V")
L.on(31, 25, "h")

# 4. Sign row over the spike pit (x41-59).
L.ground(41, 51, 33)
L.row(42, 51, 32, "^")
L.ground(52, 59, 31)
L.row(52, 55, 30, "^")
L.col(41, 25, 32, "H")
L.col(59, 25, 30, "H")
sign("s1", 42, 25, 45, 25, color="cyan")
sign("s2", 48, 25, 51, 25, color="pink")
sign("s3", 54, 25, 57, 25, color="yellow")
L.put(49, 24, "W")
L.put(57, 30, "m")
L.put(58, 30, "m")
for x in (44, 50, 56):
    L.put(x, 22, "g")

# 5. Roof and ladder wall (x60-70).
L.ground(60, 70, 25)
L.col(70, 16, 24, "H")
L.on(63, 25, "c")
L.at("chrome_cop", 67, 24)
L.put(66, 21, "1")
L.at("billboard", rect=(68, 18, 74, 24), text="NEON CITY COLA")

# 6. Deck (x71-84), with secret 1: a crawlspace behind the billboard.
L.ground(71, 84, 16)
L.clear(71, 78, 23, 24)
L.put(78, 24, "2")
L.put(76, 24, "$")
for x in (72, 73, 74, 75, 77):
    L.put(x, 24, "g")
L.put(78, 15, "W")
L.put(82, 15, "h")
L.put(73, 15, "m")
L.at("chrome_cop", 75, 15)

# 7. Hang bar over the long pit (x85-100).
L.ground(85, 100, 33)
L.row(85, 99, 32, "^")
L.row(83, 101, 12, "-")
L.col(100, 16, 32, "H")
L.at("hover_lens", 89, 8)
L.at("hover_lens", 95, 8)
for x in (88, 91, 94, 97):
    L.put(x, 14, "g")

# 8. Card and force field (x101-124).
L.ground(101, 124, 16)
L.fill(110, 118, 0, 11, "#")
L.col(114, 12, 15, "D")
L.put(103, 15, "c")
L.put(107, 15, "k")
L.put(109, 15, "h")
L.put(111, 15, "m")
L.at("roof_turret", 112, 15)
L.at("hover_lens", 106, 8)
L.put(119, 15, "W")
L.at("chrome_cop", 121, 15)
L.put(124, 15, "T")

# 9. Sign tower (x125-135): pairs A (beats 1,2) and B (beats 3,4).
L.ground(125, 135, 16)
sign("a1", 126, 14, 128, 14, beats="1,2", color="cyan")
sign("b1", 130, 12, 132, 12, beats="3,4", color="pink")
sign("a2", 126, 10, 128, 10, beats="1,2", color="cyan")
sign("b2", 130, 8, 132, 8, beats="3,4", color="pink")
sign("a3", 126, 6, 128, 6, beats="1,2", color="cyan")
L.row(134, 135, 12, "#")
L.row(133, 135, 8, "=")
L.put(134, 7, "3")
L.fill(121, 123, 3, 4, "#")
L.put(122, 2, "C")
L.at("deco", kind=42, x=121, y=4, w=3, h=1, text="TANK 42")
L.at("deco", kind="text", x=133, y=9, w=3, h=2, text="DUKE'S BURGERS - CLOSED SINCE 1993")

# 10. Top roof (x136-148).
L.ground(136, 148, 6)
L.put(138, 5, "c")
L.put(145, 5, "h")
L.at("chrome_cop", 146, 5)

# 11. The GUN sign (x149-174) over a lower roof.
L.ground(149, 174, 14)
L.col(174, 6, 13, "H")
letter("g", [(151, 6, 156, 6), (151, 7, 151, 9), (151, 10, 156, 10), (156, 8, 156, 9), (154, 8, 155, 8)],
       "1", "pink")
letter("u", [(159, 6, 160, 9), (163, 6, 164, 9), (159, 10, 164, 10)], "2", "cyan")
letter("n", [(167, 6, 168, 10), (171, 6, 172, 10), (169, 7, 169, 8), (170, 8, 170, 9)], "3", "yellow")
L.put(161, 7, "B")
L.put(161, 13, "Q")
for x in (153, 155, 159, 164, 168, 171):
    L.put(x, 5, "g")

# 12. Exit roof (x175-189).
L.ground(175, 189, 6)
L.on(185, 6, "X")
L.put(188, 5, "*")

# Easter egg: hold up at the start and a tiny UFO crosses the sunset.
L.at("deco", kind="ufo_flyby", x=0, y=26, w=7, h=4)

L.write(__file__, "01_rooftop_run.txt")
