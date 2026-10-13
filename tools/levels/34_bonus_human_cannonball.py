#!/usr/bin/env python3
"""Level 34 bonus - Human Cannonball (SPEC.md 34). rules=cannonball: the
runner sits in a cannon at (2, 20); up and down set the angle (30, 45 or 60
degrees), jump or fire launches. In flight gravity pulls and left and right
steer a little; landing anywhere but a net reloads the cannon. timer=60,
goal=score:10000.

Three target nets at row 20: x 28-33 (1000), 48-51 (2500) and 69-70
(5000); 20 gems along three of the arcs.

Changes from the spec, so it plays in this engine: the launch speed is 5
cells a frame (the spec's 4 cannot reach the 5000 net at all).

Everything in world_fleet.cpp.
"""

import math

from lib import Level

W, H = 80, 24
L = Level(W, H, name="BONUS - HUMAN CANNONBALL", episode=5, theme="sunset_airships", music="theme_waltz",
          rules="cannonball", timer=60, goal="score:10000")


def put(x, y, c):
    if L.get(x, y) == ".":
        L.put(x, y, c)
        return True
    return False


L.fill(0, W - 1, 22, H - 1, "#")
L.fill(0, 0, 0, 21, "#")
L.fill(W - 1, W - 1, 0, 21, "#")
put(2, 21, "P")
L.at("cannon", 2, 20)
L.at("net", 28, 20, x1=33, score=1000)
L.at("net", 48, 20, x1=51, score=2500)
L.at("net", 69, 20, x1=70, score=5000)


def arc(angle, steer, count):
    """Gems along a flight (cells, as world_fleet.cpp flies it)."""
    x, y = 4.0, 41.0
    vx = 5.0 * math.cos(math.radians(angle))
    vy = -5.0 * math.sin(math.radians(angle))
    pts = []
    while True:
        vy += 0.25
        vx += 0.125 * steer
        x += vx
        y += vy
        if y > 41:
            break
        pts.append((int((x + 1.5) // 2), int((y - 2) // 2)))
    step = max(1, len(pts) // (count + 1))
    placed = 0
    for bx, by in pts[step::step]:
        if placed < count and 0 < bx < W - 1 and 0 <= by < 20 and put(bx, by, "g"):
            placed += 1
    return placed


n = arc(30, -1, 6) + arc(45, 0, 7) + arc(45, 1, 7)
assert n == 20, n

L.write(__file__, "34_bonus_human_cannonball.txt")
