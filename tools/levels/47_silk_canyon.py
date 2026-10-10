#!/usr/bin/env python3
"""Level 47 - Silk Canyon (docs/DEEP_SPACE.md). Twist: Silk Lines. Prototype: Silk Shooter.

Episode 7, DEEP SPACE, down a deep amber canyon on the planet Vurr strung
with giant webs. Top to bottom: from the rim, line to line across the
canyon, alcove to alcove down its walls. The canyon has no floor: fall and
you are gone.

`@ silk x y to=x,y` is a Silk Line between two blocks (through the top
middle of each; hanging from a point in block row y you stand on a floor at
row y + 3). Jump into one (or step off a ledge under it) and you hang from
it and slide down it, faster and faster, and drop off the low end; jump off
any time and you keep its speed. Every line here starts at an alcove's
mouth two rows over its floor (step off and you catch it) and ends three
rows over the next alcove's floor. `spin=1`: it is spun as you come near.
`spider=1`: a Loom Spider walks its top and cuts it under you: shoot the
spider first.

Shape: the rim (start) and seven rides down: to the first right alcove,
back left, past the first spider to the right, onto the rock pillar hanging
mid-canyon, left again, the line the spider spins for you, and down past
the second spider to the exit.

Secrets: the spider's lair, a cave in the east wall over a ledge no line
goes to: stand on the pillar's east edge and fire the Silk Shooter down at
the ledge, then ride your own line (gem cocoon, the 42, the rubber duck); a
line from the ledge goes on down. The Virus: the green cocoon at the back of
the second alcove.
"""

from lib import Level

W, H = 100, 78
L = Level(W, H,
          name="STAGE 47 - SILK CANYON", episode=7, theme="silk_canyon",
          music="theme_silk_canyon", weapon="silk_shooter", par=200, flags="")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(0, W - 1, 0, 9)      # the sky over the rim
L.clear(30, 69, 0, H - 1)    # the canyon, open to the bottom


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def left_alcove(floor, x0=23):
    """An alcove cut into the west wall: open x0-29, rows floor-4..floor-1."""
    L.clear(x0, 29, floor - 4, floor - 1)


def right_alcove(floor, x1=76):
    """An alcove cut into the east wall: open 70-x1, rows floor-4..floor-1."""
    L.clear(70, x1, floor - 4, floor - 1)


def line(x0, y0, x1, y1, **keys):
    L.at("silk", x0, y0, to=(x1, y1), **keys)


def down_right(floor_a, floor_b, **keys):
    """A line from a west alcove's mouth (floor floor_a) to the east alcove
    at floor_b."""
    line(29, floor_a - 2, 72, floor_b - 3, **keys)


def down_left(floor_a, floor_b, **keys):
    line(70, floor_a - 2, 27, floor_b - 3, **keys)


# --- The rim (start) ---------------------------------------------------------------------
on(4, 10, "P")
on(9, 10, "h")
on(14, 10, "m")
on(20, 10, "c")
gem((11, 8), (17, 8), (24, 8))
# Ride 1: off the rim's edge, across to the first east alcove.
line(29, 8, 72, 15)
R1 = 18
right_alcove(R1)
on(74, R1, "1")                 # letter G
on(76, R1, "h")
L.at("cocoon_pod", 73, R1 - 3)  # its Dropling drops as you pass under
gem((50, 12), (56, 13))         # along the line: picked up as you ride

# Ride 2: back west, to the second alcove (the Silk Shooter, a checkpoint).
L2 = 26
down_left(R1, L2)
left_alcove(L2, 21)
on(24, L2, "W")
on(27, L2, "c")
L.at("cocoon", 21, L2 - 2, virus=1)   # the Virus, at the back
gem((44, 20), (38, 21))

# Ride 3: east again; a Loom Spider walks the top of this one: shoot it
# from the alcove's mouth before you go.
R3 = 34
down_right(L2, R3, spider=1)
right_alcove(R3)
on(74, R3, "h")
on(76, R3, "m")
gem((52, 28), (60, 29))

# Ride 4: onto the rock pillar hanging mid-canyon (webs hold it up).
P = 40                          # the pillar's top row
L.fill(46, 53, P, P + 4, "#")
line(70, R3 - 2, 50, P - 3)
on(48, P, "2")                  # letter U
on(51, P, "T")                  # a Turbo box
on(52, P, "c")
gem((58, 35), (62, 34))

# Ride 5: off the pillar's west edge, to the west alcove below.
L4 = 46
line(46, P - 2, 27, L4 - 3)
left_alcove(L4)
on(25, L4, "h")
on(23, L4, "m")
gem((38, 41), (34, 42))

# Ride 6: the line the spider spins as you come (wait for it), and the
# spider then: shoot it.
R5 = 56
down_right(L4, R5, spin=1, spider=1)
right_alcove(R5)
on(73, R5, "c")
on(75, R5, "h")
L.at("cocoon_pod", 74, R5 - 3)
gem((44, 46), (56, 49))

# Ride 7: back west to the last west alcove.
L6 = 64
down_left(R5, L6)
left_alcove(L6, 20)
on(25, L6, "3")                 # letter N
on(22, L6, "c")
on(20, L6, "h")
L.at("cocoon_pod", 26, L6 - 3)
gem((48, 58), (40, 59))

# Ride 8: down past the second spider to the exit alcove.
R7 = 72
down_right(L6, R7, spider=1)
right_alcove(R7, 82)
on(74, R7, "m")
on(80, R7, "X")
gem((50, 67), (60, 68))

# --- Secret: the spider's lair -----------------------------------------------------------
# A ledge out of the east wall that no line goes to, a cave behind it.
# From the pillar's east edge the Silk Shooter's shot (down ahead, 45
# degrees) hits the ledge's top: ride your own line down onto it.
LEDGE = 49
L.fill(61, 69, LEDGE, LEDGE, "#")
L.clear(70, 78, LEDGE - 4, LEDGE - 1)
L.at("cocoon", 77, LEDGE - 2, gems=8)
L.at("deco", 75, LEDGE - 1, kind="42", text="XLII", w=1, h=2)
on(72, LEDGE, "Q")
on(73, LEDGE, "m")
gem((71, 46), (74, 46))
# The way on: off the ledge's west edge, down to the last west alcove.
line(61, LEDGE - 2, 27, L6 - 3)

L.write(__file__, "47_silk_canyon.txt")
