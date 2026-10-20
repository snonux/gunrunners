#!/usr/bin/env python3
"""Level 35 - The Transmitter (SPEC.md 35). Twist: The Turning Tower.
Prototype: Satellite Swarm. Boss: the Halcyon Titan.

A climb up a lattice mast whose x wraps at 64 (`flags=wrapx`): the base
ring, ladders and ledges, the dish walkway, the Feedback Nodes' narrow
ledges, and the top deck where a cargo airship delivers the Halcyon Titan.
After it falls, the beam on the deck is the exit. The world turns the
tower round the runner so the runner stays in the middle of the map;
everything is in world_tower.cpp.

Changes from the spec, so it plays in this engine:
- The tower is drawn flat with its sides falling away into shadow, and a
  faint lattice behind for the back of the tower (no true cylinder).
- The crawl space is not a separate sub-map: it is a sealed room under the
  base ring (rows 105-111, map height 112). The door on the dish walkway
  at (36, 65) and the door back inside it (21, 109) are `@ door`s; press up
  on one to come out at the other.
- Every ledge is one-way (`=`), so a runner can jump up through a ledge
  from below and a fall always lands on a ledge (or the base ring).
- The Relay Drones do no contact damage (they only shoot): their orbits
  cross the ledges.
- `@ route` lists spots up the spiral for the bot, less than half a lap
  apart, so its planner (which knows no wrap) always has a near target.
- Gems: base 8, section 2: 10, dish level 10, nodes 8, crawl space 10.
"""

from lib import Level

W, H = 64, 112
L = Level(W, H, name="STAGE 35 - THE TRANSMITTER", episode=5, theme="broadcast_mast", music="theme_heroic_build",
          weapon="satellite_swarm", par=420, flags="wrapx")


def wx(x):
    return x % W


def put(x, y, c):
    x = wx(x)
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def ledge(x0, x1, g, c="="):
    """A ledge from x0 to x1 (wrapping past 63), surface row g."""
    n = (x1 - x0) % W
    for i in range(n + 1):
        L.put(wx(x0 + i), g, c)


def ladder(x, top, bottom):
    """A ladder from the ledge at row `top` down to the row above the floor
    it stands on; the block to its left must be clear or one-way."""
    L.col(x, top, bottom, "H")
    assert L.get(x - 1, top) in ".=", (x, top, L.get(x - 1, top))


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


# --- The tower's base and the crawl space under it ------------------------------------------------------
L.fill(0, W - 1, 102, H - 1, "#")
L.clear(20, 43, 106, 109)                       # the crawl space
L.at("deco", 0, 100, kind="42", art="mast_plate", text="MAST 42", w=1, h=1)

# --- 1. Base ring (96-104) ----------------------------------------------------------------------------
put(2, 101, "P")
put(10, 101, "m")
put(20, 101, "h")
ledge(38, 47, 99)
put(40, 98, "W")
gem((6, 101), (14, 101), (26, 101), (30, 101), (34, 101), (52, 101), (56, 101), (60, 101))

# --- 2. Ladders and ledges (78-98) --------------------------------------------------------------------
ledge(47, 60, 88)
ledge(61, 3, 85)
ledge(4, 16, 82)
ledge(17, 26, 79)
ladder(47, 88, 98)
ledge(44, 45, 95)                               # Nova's knobs past ladder 47
ledge(42, 43, 91)
L.at("spawn", 44, 98, kind="mast_climber", ladder=47)
L.at("mast_climber", 10, 81)
put(55, 87, "m")
put(8, 81, "h")
put(10, 79, "2")                                 # U
put(20, 78, "c")
gem((50, 87), (52, 87), (58, 87), (62, 84), (1, 84), (6, 81), (12, 81), (14, 81), (18, 78), (23, 78))

# --- 3. Dish level (54-77) ----------------------------------------------------------------------------
ledge(26, 50, 66)
ledge(51, 6, 63)
ledge(7, 21, 60)
ledge(16, 40, 54)
ladder(26, 66, 78)
ladder(21, 54, 59)
L.at("relay_drone", 0, 58)
L.at("relay_drone", 32, 58)
L.at("relay_drone", 16, 62)
put(30, 65, "W")
put(45, 65, "h")
L.at("door", 36, 65, "crawl", to="back")
put(2, 62, "m")
put(24, 53, "c")
put(36, 53, "m")
gem((28, 65), (33, 65), (40, 65), (48, 65), (54, 62), (58, 62), (10, 59), (14, 59), (30, 53), (39, 53))

# --- The crawl space (under the base) -----------------------------------------------------------------
L.at("door", 21, 109, "back", to="crawl")
put(26, 109, "$")
put(30, 109, "1")                                # G
put(40, 107, "B")
gem((23, 109), (24, 109), (28, 109), (29, 109), (32, 109), (33, 109), (34, 109), (35, 109), (36, 109), (37, 109))

# --- 4. Feedback Nodes (22-53) ------------------------------------------------------------------------
ledge(41, 46, 51)
ledge(47, 52, 48)
ledge(53, 58, 45)
ledge(59, 2, 42)
ledge(3, 8, 39)
ledge(8, 20, 30)
ledge(21, 26, 27)
ledge(27, 32, 24)
ledge(33, 44, 22)
ladder(8, 30, 38)
ledge(4, 5, 35)                                 # Nova's knobs past ladder 8
ledge(6, 7, 31)
put(44, 50, "h")
L.at("feedback_node", 50, 47)
L.at("feedback_node", 62, 41)
L.at("feedback_node", 24, 26)
put(56, 42, "3")                                 # N
put(6, 38, "c")
put(14, 29, "W")
put(30, 23, "h")
put(40, 21, "c")
gem((42, 50), (48, 47), (55, 44), (58, 44), (60, 41), (1, 41), (4, 38), (36, 21))

# --- 5. Top deck and the Halcyon Titan (0-21) ----------------------------------------------------------
L.row(0, W - 1, 8, "#")
L.clear(57, 58, 8, 8)                            # the hatch down to the lattice beam
L.put(43, 8, "=")
L.put(59, 8, "=")
ladder(44, 8, 21)
ledge(56, 60, 14)
ladder(60, 8, 13)
put(44, 7, "c")
put(40, 7, "h")
put(48, 7, "h")
put(56, 7, "h")
put(50, 7, "T")
L.at("titan", 12, 7)
L.at("treasure", 58, 13, art="titan_brochure", score=10000)
put(56, 13, "Q")
L.at("duck", 56, 13, costume="hard_hat")
L.at("flag", 32, 1)

# --- 6. Ride the signal ------------------------------------------------------------------------------
put(32, 7, "X")

# The bot's way up the spiral.
L.at("route", None, None, path=[
    (20, 102), (34, 102), (40, 99), (46, 99),
    (52, 88), (59, 88), (63, 85), (2, 85), (8, 82), (15, 82), (21, 79), (25, 79),
    (30, 66), (40, 66), (49, 66), (55, 63), (61, 63), (3, 63), (10, 60), (20, 60),
    (24, 54), (32, 54), (39, 54), (44, 51), (48, 48), (56, 45), (59, 42), (1, 42), (5, 39), (7, 39),
    (12, 30), (19, 30), (22, 27), (30, 24), (36, 22), (43, 22), (46, 8)])

L.header["tip"] = "THE TOWER TURNS AROUND YOU: CLIMB THE LADDERS AND LEDGES TO THE TOP DECK"  # HINTS: the level's first 12 seconds
L.write(__file__, "35_the_transmitter.txt")
