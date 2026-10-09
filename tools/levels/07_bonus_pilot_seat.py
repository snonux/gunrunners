#!/usr/bin/env python3
"""Level 7 bonus - Pilot Seat (SPEC.md 07, "Bonus level").

Rule flight: you fly Black Halo (8-way, 2 cells a frame, no gravity) over a
cardboard skyline. Fire is the chaingun; down + fire lobs a rocket. 30
cardboard runners pop up on the roofs, one every 60 frames, and 10
cardboard trucks drive the street; a gem for every 2000 points, and 20000
points clears it.

Change from the spec: the map is 26 rows, not 24. The cardboard roofs are
one-way flats at rows 20-22 and the trucks drive the street under them
(rows 23-24, on ground 25), so rockets fall through the roofs onto them.
"""

from lib import Level

W, H = 80, 26
L = Level(W, H,
          name="BONUS - PILOT SEAT", episode=1, theme="chopper_down",
          music="bonus_pilot_seat", rules="flight", timer=90, goal="score:20000")

L.row(0, W - 1, 25, "#")                   # the street
L.put(4, 6, "P")
ROOFS = [(2, 9, 21), (12, 19, 20), (22, 27, 22), (30, 38, 20), (41, 47, 21), (50, 57, 22),
         (60, 66, 20), (69, 77, 21)]
for x0, x1, g in ROOFS:
    L.row(x0, x1, g, "=")
    for x in (x0 + 1, x1 - 1):
        L.at("popup", x, g - 1)
L.at("street", 0, 24)

L.write(__file__, "07_bonus_pilot_seat.txt")
