#!/usr/bin/env python3
"""Level 36 - Prop Warehouse (SPEC.md 36). Twist: Magnet Cranes.
Prototype: Magnet Gun.

A hub that reads left to right: the loading dock, the shelf aisles, the
curtained prop maze with the upper walkway over it, the crane yard over a
foam pit, and the freight lifts up the right edge to the dressing-room
landing. Everything is in world_warehouse.cpp.

Changes from the spec, so it plays in this engine:
- Aisles (section 2): crane cr2 starts over MC3 on shelf board 48 and
  carries it right onto MC5 (top 52); MC4 waits on the floor and is pushed
  along it with the Magnet Gun until it stops against the stack at x 61
  (top 54). The same two-step stair as the spec, but every runner can
  build it (board 48 is out of reach for all but Nova).
- Crane yard (section 4): the lip ends in a one-way block at x 99 and the
  islands are pillars down to the foam, so the pit is three basins; each
  has a ladder up (x 100 back to the lip, x 118 up to island I2, x 128 up
  to the far lip), set where the ride crates' ends leave a one-block gap
  (ladders need an open block on their left). The ride crates run 101-107,
  112-116 and 122-126.
- Ride cranes: the lever wakes the crane; then stepping on rings the bell,
  the crate rides to the far end, holds 120 frames and comes back.
- The EP. 1 BLOOPERS crate is a 4 x 4 block breakable at the walkway's
  left end (x 65-68, rows 36-39) holding the bonus entrance and U.
- The mezzanine's crane cr7 runs x 76-95 from x 86.
- `@ step` lines are the bot's way through: where to stand and what to do.
"""

from lib import Level

W, H = 150, 64
L = Level(W, H, name="STAGE 36 - PROP WAREHOUSE", episode=6, theme="prop_warehouse", music="theme_lonely_piano",
          weapon="magnet_gun", par=420)


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def ladder(x, top, bottom):
    """A ladder from the ledge row `top` down to `bottom`; the block to its
    left must never be solid."""
    L.col(x, top, bottom, "H")
    for y in range(top, bottom + 1):
        assert L.get(x - 1, y) in ".=", (x, y, L.get(x - 1, y))


def gems(*pts):
    for x, y in pts:
        put(x, y, "g")


def crate(cid, x, y):
    """A metal crate, 2 x 2 blocks with (x, y) its top-left block."""
    for dy in (0, 1):
        for dx in (0, 1):
            assert L.get(x + dx, y + dy) == ".", (cid, x + dx, y + dy)
    L.at("crate", x, y, cid, metal=1)


# The building: roof, outer walls.
L.fill(0, W - 1, 0, 1, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")

# --- 1. Loading dock (0-31) --------------------------------------------------------------------
L.ground(1, 17, 56)
L.ground(18, 31, 51)
put(3, 55, "P")
put(6, 55, "W")
crate("MC1", 9, 54)
L.at("crane", None, None, "cr1", x0=4, x1=17, row=42, start=9, lever="L1")
L.at("switch", 13, 55, "L1", kind="lever")
put(28, 50, "h")
put(30, 50, "c")
put(24, 50, "m")
gems((12, 49), (13, 48), (14, 47), (15, 47), (16, 48), (17, 49))

# --- 2. First aisles (32-64) --------------------------------------------------------------------
L.ground(32, 64, 56)
for g in (52, 48, 44):
    L.row(34, 56, g, "=")
# MC5: a fixed wooden crate (solid blocks).
L.fill(63, 64, 54, 55, "#")
L.ground(65, 99, 50)
L.at("forklift", 44, 55, dir="l", lane=(33, 60))
L.at("forklift", 58, 55, dir="l", lane=(33, 60))
crate("MC3", 50, 46)
crate("MC4", 40, 54)
L.at("crane", None, None, "cr2", x0=38, x1=64, row=40, start=50, lever="L2")
L.at("switch", 35, 55, "L2", kind="lever")
put(48, 55, "m")
put(60, 55, "h")
put(46, 43, "1")                                 # G
put(57, 43, "Q")
L.at("duck", 57, 43, costume="hard_hat_vest")
L.at("deco", 33, 55, kind="42", art="crate_42", text="CRATE 42", w=1, h=1)
gems((36, 51), (40, 51), (44, 51), (38, 47), (42, 47), (54, 47), (40, 43), (50, 43))

# --- 3. Prop maze (65-99) and the upper walkway --------------------------------------------------
L.ground(77, 84, 47)
L.fill(65, 94, 40, 41, "#")                      # the walkway slab (ground 40)
L.put(95, 40, "=")
ladder(96, 40, 49)
L.at("stagehand", 78, 46)
L.at("stagehand", 86, 49)
L.at("mimic", 70, 49)
L.at("mimic", 82, 46)
L.at("mimic", 90, 49, carrier=1)
put(80, 46, "h")
put(88, 49, "m")
put(74, 49, "W")
L.at("turbocrate", 67, 49)
put(66, 49, "c")
gems((68, 46), (72, 49), (76, 48), (78, 44), (80, 44), (83, 44), (86, 46), (92, 49), (93, 46), (94, 49))
# 3a. Upper walkway: the BLOOPERS crate, the mezzanine.
L.fill(65, 68, 36, 39, "#")
L.at("breakable", None, None, rect=(65, 36, 68, 39), hp=3, look="crate")
L.put(65, 37, "B")
L.put(68, 39, "2")                               # U, inside the crate
L.fill(66, 79, 32, 33, "#")                      # the mezzanine (ground 32)
for i, x in enumerate(range(84, 94, 2)):
    crate("MC%d" % (9 + i), x, 38)
crate("MC14", 88, 36)
L.at("crane", None, None, "cr7", x0=76, x1=95, row=24, start=86, lever="L7")
L.at("switch", 93, 39, "L7", kind="lever")
for x in range(68, 80, 2):
    put(x, 31, "m")
gems((71, 39), (73, 39), (75, 39), (77, 39), (79, 39), (81, 39), (83, 37), (69, 31), (71, 31), (73, 31))

# --- 4. Crane yard (100-128) ---------------------------------------------------------------------
L.clear(99, 128, 50, 61)
L.ground(99, 128, 62)
L.put(99, 50, "=")                               # the lip's edge
L.fill(109, 111, 50, 61, "#")                    # island I1
L.fill(119, 121, 50, 61, "#")                    # island I2
ladder(100, 50, 61)
ladder(118, 50, 61)
ladder(128, 50, 61)
L.at("foam", None, None, rect=(99, 60, 128, 61))
put(95, 49, "c")
put(97, 49, "T")
L.at("switch", 98, 49, "L3", kind="lever")
L.at("crane", None, None, "cr3", x0=101, x1=109, row=36, start=101, end=107, deck=50, ride=1, lever="L3")
L.at("switch", 110, 49, "L4", kind="lever")
L.at("crane", None, None, "cr4", x0=112, x1=118, row=36, start=112, end=116, deck=50, ride=1, lever="L4")
L.at("switch", 120, 49, "L5", kind="lever")
L.at("crane", None, None, "cr5", x0=122, x1=128, row=36, start=122, end=126, deck=50, ride=1, lever="L5")
put(111, 49, "h")
put(114, 61, "3")                                # N, on the foam
gems((102, 46), (104, 45), (106, 46), (108, 47), (113, 46), (115, 45), (117, 46),
     (123, 46), (125, 45), (127, 46), (105, 61), (124, 61))

# --- 5. Freight lifts (129-149) -------------------------------------------------------------------
L.ground(129, 148, 50)
L.clear(144, 147, 50, 50)                        # FL1's recess
L.fill(133, 143, 32, 33, "#")                    # mid landing
L.fill(133, 148, 14, 15, "#")                    # top landing
put(131, 49, "c")
put(130, 49, "W")
L.at("crane", None, None, "cr6", x0=129, x1=142, row=38, start=136, lever="L6")
L.at("switch", 133, 49, "L6", kind="lever")
L.at("forklift", 138, 49, dir="l", lane=(129, 142))
L.at("forklift", 141, 49, dir="l", lane=(129, 142))
put(140, 49, "h")
L.at("platform", 144, 50, "FL1", w=4, path=[(144, 50), (144, 32)], speed=1, mode="pingpong", wait="rider")
L.at("stagehand", 136, 31)
L.at("mimic", 134, 31)
put(139, 31, "h")
L.at("platform", 129, 32, "FL2", w=4, path=[(129, 32), (129, 14)], speed=1, mode="pingpong", wait="rider")
put(146, 13, "X")
gems((134, 13), (136, 13), (138, 13), (140, 13), (142, 13), (144, 13))

# Props of levels 1-35 on the shelf backdrop.
for n in range(1, 36):
    L.at("deco", 30 + 3 * n, 46 if n % 2 else 38, kind="prop", level=n)

# The bot's way through.
for x, y, act, arg, n in [
        (13, 55, "lever", "L1", 0), (13, 55, "wait", "cr1", 0),
        (35, 55, "lever", "L2", 0), (35, 55, "wait", "cr2", 0),
        (36, 55, "push", "MC4", 61),
        (98, 49, "lever", "L3", 0), (110, 49, "lever", "L4", 0), (120, 49, "lever", "L5", 0)]:
    L.at("step", x, y, act=act, arg=arg, n=n)

L.write(__file__, "36_prop_warehouse.txt")
