#!/usr/bin/env python3
"""Level 33 - Magma Mainframe (SPEC.md 33). Twist: Rising Lava. Prototype:
Heat Sink.

A vertical escape up a data centre inside a volcano, bottom to top: the
lowest server floors, the server floors with Lava Daemons and the
corrupted rack, the three Firewall corridors, the cooling shaft with its Fan
Bots, then the crater floor and a ladder up the airship's mooring mast to
the exit. The lava (`@ fluid kind=rising`) starts below the lowest floor and
rises a cell every 20 frames after 150 frames; each checkpoint's purge stops
it for 150 frames, and it never rises above row 30.

Everything in world_magma.cpp.

Changes from the spec, so it plays in this engine:
- The floor above each rack stops two blocks short of the rack's side (the
  spec has one), so a runner (1.5 blocks wide) can jump onto the rack
  without hitting the floor above with their head.
- The 2-block steps up to g98 and g92 are a rack, a one-way shelf and a
  one-way end of the floor above (the spec's steps were the wrong way
  round for a runner coming along the floor).
- The first Firewall corridor's rack (on g92) is at 9-11 instead of 5-7,
  and g86's left piece starts at 14, so the coolant pipe's nook (x 2-13,
  floor g84, its right half one-way) sits beside the rack's way up instead
  of over it. The nook is entered from g86's left piece with a jump up and
  left.
- The coolant pipe is a ladder up the wall at x 0-1 (the engine has no
  crawlway up a wall), from the nook to floor g80; the bonus entrance B is
  on the nook's floor at (4, 83).
- The cooling shaft's walls (x 2-13 and 46-57) run from row 29 down to the
  g66 ledge; below that the third Firewall corridor is open across the
  map, so its rack (5-7 on g80) can be climbed. Ladders through a ledge
  have a one-way block on their left.
- The g42 ledge is one-way at 42-45, so Turbo's jump from g50 can rise
  through it into the stash notch (46-56, floor g45).
- The third Firewall starts at the corridor's right end (the spec has it
  on the checkpoint).
"""

from lib import Level

W, H = 60, 150
L = Level(W, H, name="STAGE 33 - MAGMA MAINFRAME", episode=5, theme="volcano_datacenter",
          music="theme_driving_hihat", weapon="heat_sink", par=360, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def floor(g, x0, x1, holes=()):
    L.row(x0, x1, g, "#")
    for h0, h1 in holes:
        L.row(h0, h1, g, ".")


def rack(x0, top, g):
    """A server rack standing on floor g, its top at row `top`."""
    L.fill(x0, x0 + 2, top, g - 1, "#")


NOVA = ((24, 26),)

# Rock walls, the bottom.
L.fill(0, 1, 0, H - 1, "#")
L.fill(58, 59, 0, H - 1, "#")
L.fill(2, 57, 147, H - 1, "#")
L.at("fluid", None, None, kind="rising", rect=(0, 149, 59, 149), rate="1/20", delay=150, cap=30)

# --- 1. The lowest floors (122-149) ----------------------------------------------------------------
floor(146, 2, 54)                              # g146; the magma gutter 55-57
L.at("fluid", None, None, kind="gutter", rect=(55, 146, 57, 146))
put(56, 146, "$")
rack(9, 144, 146)                              # the first rack (2 high), the box on it
rack(24, 144, 146)                             # Nova's racks under the holes
rack(52, 143, 146)
floor(140, 2, 49, NOVA)
rack(24, 138, 140)
rack(5, 137, 140)
floor(134, 10, 57, NOVA)
rack(24, 132, 134)
rack(52, 131, 134)
floor(128, 2, 49, NOVA)
rack(24, 126, 128)
rack(5, 125, 128)
floor(122, 10, 57, NOVA)
put(4, 145, "P")
put(10, 143, "W")
put(50, 145, "m")
put(45, 133, "h")
put(25, 131, "G")
put(30, 121, "c")
L.at("purge", 30, 121, "purge1")
L.at("reboot", 20, 121)
gem((14, 145), (18, 145), (36, 145), (44, 145), (53, 142), (54, 142),
    (6, 136), (7, 136), (14, 139), (32, 139), (40, 139), (46, 139),
    (20, 133), (30, 133), (38, 133), (50, 133), (52, 130), (53, 130),
    (6, 124), (7, 124), (12, 127), (30, 127), (36, 127), (44, 127))

# --- 2. The server floors (98-121) -------------------------------------------------------------------
rack(24, 120, 122)
rack(52, 119, 122)
floor(116, 2, 49, NOVA)
rack(24, 114, 116)
rack(5, 113, 116)
floor(110, 10, 57, NOVA)
rack(52, 107, 110)
floor(104, 2, 49)
L.fill(6, 8, 102, 103, "#")                    # 2-block steps (after the Virus): a rack,
L.row(2, 5, 100, "=")                          # a one-way shelf,
floor(98, 10, 57)
L.row(6, 9, 98, "=")                           # and g98's one-way end
L.at("lava_daemon", 30, 115, x2=36)
L.at("lava_daemon", 45, 109, x2=39)
L.at("lava_daemon", 25, 103, x2=19)
L.at("lava_daemon", 48, 115, marshmallow=1)
put(20, 115, "h")
put(40, 109, "W")
L.at("deco", 15, 108, kind="42", art="rack_label", text="42", w=1, h=1)
L.at("virusrack", 40, 103, hp=4, on=60, off=45, rect=(34, 100, 39, 103))
put(12, 103, "m")
put(30, 97, "c")
L.at("purge", 30, 97, "purge2")
put(34, 97, "T")
gem((12, 121), (18, 121), (40, 121), (46, 121), (53, 118), (54, 118),
    (6, 112), (7, 112), (14, 115), (36, 115), (42, 115), (10, 115),
    (16, 109), (20, 109), (32, 109), (48, 109), (53, 106), (54, 106),
    (7, 101), (3, 99), (16, 103), (28, 103), (44, 103), (48, 103))

# --- 3. The Firewall corridors (74-97) ---------------------------------------------------------------
L.fill(51, 53, 96, 97, "#")                    # 2-block steps: a rack, a one-way shelf,
L.row(54, 57, 94, "=")
floor(92, 2, 49)
L.row(50, 53, 92, "=")                         # and g92's one-way end
rack(9, 89, 92)
floor(86, 14, 57, ((18, 19), (38, 39)))
rack(52, 83, 86)
floor(80, 2, 49, ((18, 19), (38, 39)))
rack(5, 77, 80)
floor(74, 10, 57, ((18, 19), (38, 39)))
# Secret 1: the coolant pipe's nook (floor g84) and the pipe up the wall.
L.row(2, 7, 84, "#")
L.row(8, 13, 84, "=")
L.clear(0, 1, 76, 83)
L.col(1, 80, 83, "H")
put(0, 82, "U")
L.at("treasure", 7, 83, score=10000, art="server_ratings")
put(4, 83, "B")
gem((3, 83), (5, 83), (0, 80), (0, 78), (10, 83), (12, 83))
L.at("firewall", 30, 85, x0=14, x1=57, ports=(18, 38))
L.at("firewall", 30, 79, x0=2, x1=49, ports=(18, 38))
L.at("firewall", 50, 73, x0=10, x1=57, ports=(18, 38), dir="l")
put(45, 91, "m")
put(45, 85, "h")
put(25, 79, "W")
put(30, 73, "c")
L.at("purge", 30, 73, "purge3")
gem((52, 95), (56, 93), (20, 91), (30, 91), (40, 91), (14, 91),
    (16, 85), (24, 85), (32, 85), (44, 85), (53, 82), (54, 82),
    (6, 76), (7, 76), (12, 79), (30, 79), (42, 79), (46, 79),
    (12, 73), (16, 73), (24, 73), (34, 73), (42, 73), (52, 73))

# --- 4. The cooling shaft (34-73) --------------------------------------------------------------------
L.fill(2, 13, 29, 66, "#")
L.fill(46, 57, 29, 66, "#")
for g in (66, 58, 50, 42, 34):
    floor(g, 14, 45)


def ladder(x, top, bottom, ledge):
    """A ladder up through a ledge: the ladder's column, one-way to its left
    (a runner on a ladder hangs half a block to the left of its block)."""
    L.col(x, top, bottom, "H")
    if L.get(x - 1, ledge) == "#":
        L.put(x - 1, ledge, "=")


ladder(44, 66, 73, 66)
ladder(15, 58, 65, 58)
ladder(44, 50, 57, 50)
ladder(15, 42, 49, 42)
L.row(42, 45, 42, "=")                         # Turbo jumps up through here
ladder(44, 34, 41, 34)
# The stash notch in the east wall (Turbo's 5-block jump from g50).
L.clear(46, 56, 42, 44)
put(53, 44, "$")
gem((47, 44), (48, 44), (49, 44), (50, 44), (51, 44), (52, 44), (54, 44), (55, 44))
L.at("fan_bot", 13, 62, dir="r")
L.at("fan_bot", 46, 54, dir="l")
L.at("fan_bot", 13, 38, dir="r")
put(25, 57, "h")
put(30, 49, "c")
L.at("purge", 30, 49, "purge4")
put(34, 49, "T")
put(30, 38, "N")
put(35, 41, "m")
put(40, 33, "h")
gem((42, 70), (42, 68), (17, 62), (17, 60), (42, 54), (42, 52), (17, 46), (17, 44), (42, 38), (42, 36))

# --- 5. The crater and the mast (0-33) ---------------------------------------------------------------
floor(28, 2, 57)
ladder(15, 28, 33, 28)
L.col(45, 12, 27, "H")
L.row(46, 57, 12, "#")
put(30, 27, "c")
L.at("purge", 30, 27, "purge5")
put(10, 27, "h")
put(20, 27, "m")
put(56, 27, "Q")
L.at("duck", 56, 27, costume="heat_suit")
put(54, 11, "X")
gem((24, 27), (36, 27), (40, 27), (48, 11), (50, 11), (52, 11))

L.write(__file__, "33_magma_mainframe.txt")
