#!/usr/bin/env python3
"""Extra level - Motor Pool. The vehicle showcase.

The Gunrunners' depot, left to right, one vehicle per stretch: the tank
shells two rock walls (`by=vehicle`) and crushes the cops, the hoverbike
floats over a field of spikes, the helicopter lifts over the cliff and
down the canyon to its pad, the mech jet-jumps onto the ledge and stomps
through the cracked floor into the tunnel under the blast wall, and the
space ship drifts across the bottomless hangar to the exit. Every vehicle
is `bot=1` with a `drop=` where the autopilot climbs out (the ship takes
it all the way to the exit).
"""

from lib import Level

L = Level(334, 42, name="EXTRA - MOTOR POOL", theme="motor_pool", music="theme_motor_pool",
          par=420, flags="")

# A: the start and the tank (x0-64), ground 34.
L.ground(0, 64, 34)
L.col(0, 0, 33, "#")
L.on(3, 34, "P")
L.at("vehicle", 7, 33, kind="tank", bot=1, drop=(60, 33))
for x in (20, 36, 53):
    L.at("chrome_cop", x, 33)
L.fill(28, 29, 27, 33, "#")
L.at("breakable", rect=(28, 27, 29, 33), hp=4, by="vehicle", look="rock")
L.fill(46, 47, 26, 33, "#")
L.at("breakable", rect=(46, 26, 47, 33), hp=6, by="vehicle", look="rock")
for x in (32, 34, 50, 52):
    L.put(x, 31, "g")
L.on(62, 34, "c")

# B: the hoverbike over the spike field (x64-130).
L.ground(65, 89, 34)
L.ground(90, 130, 32)
L.row(72, 89, 33, "^")
L.row(90, 118, 31, "^")
L.at("vehicle", 66, 33, kind="hoverbike", bot=1, drop=(126, 31))
for x, y in ((80, 27), (100, 24), (112, 25)):
    L.put(x, y, "f")
for x in range(76, 116, 6):
    L.put(x, 29 if x < 90 else 27, "g")
L.on(128, 32, "c")

# C: the helicopter over the cliff, down the canyon to the pad (x130-200).
L.ground(131, 139, 32)
L.fill(140, 147, 8, 41, "#")
L.ground(148, 185, 39)
L.row(148, 185, 38, "^")
L.ground(186, 200, 30)
L.at("vehicle", 132, 31, kind="helicopter", bot=1, drop=(194, 29))
for x, y in ((156, 14), (166, 22), (176, 16)):
    L.put(x, y, "f")
for x in (152, 160, 168, 176):
    L.put(x, 26, "g")
L.on(198, 30, "c")

# D: the mech (x200-262): the ledge, the cracked floor, the tunnel.
L.ground(201, 226, 30)
L.ground(212, 226, 24)
L.ground(227, 250, 37)
L.fill(227, 240, 30, 31, "#")
L.at("breakable", rect=(227, 30, 240, 31), hp=1, by="vehicle", look="rock")
L.fill(241, 246, 0, 31, "#")
L.ground(251, 262, 31)
L.at("vehicle", 202, 29, kind="mech", bot=1, drop=(257, 30))
L.at("chrome_cop", 220, 23)
L.at("chrome_cop", 234, 36)
for x in (230, 236, 246):
    L.put(x, 35, "g")
L.on(260, 31, "c")

# E: the space ship across the bottomless hangar to the exit (x262-334).
L.ground(263, 271, 31)
L.at("vehicle", 264, 30, kind="spaceship", bot=1)
for x0, y0, x1, y1 in ((282, 20, 285, 24), (296, 28, 300, 31), (306, 12, 309, 17)):
    L.fill(x0, x1, y0, y1, "#")
for x, y in ((290, 22), (302, 18), (312, 24)):
    L.put(x, y, "f")
L.ground(318, 333, 26)
L.col(333, 0, 25, "#")
L.on(326, 26, "X")
for x in (288, 294, 304, 314):
    L.put(x, 20, "g")

L.write(__file__, "extra_motor_pool.txt")
