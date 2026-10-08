#!/usr/bin/env python3
"""Level 2 - Glass Canyon (SPEC.md 02). Twist: Pulley Gondolas.

A tower climbed bottom to top: start at the bottom left, exit on the roof
helipad. Pairs of window-cleaning gondolas hang on one cable each: stand on
one and it sinks while its partner rises. A glass spine with a ladder runs
up the middle; its hatches only open once you have stood on a slab, so it
brings you back after a fall but never skips ahead.

Ledges marked L are solid; window frames (w) are one-way.
"""

from lib import Level

W, H = 60, 150
L = Level(W, H,
          name="STAGE 2 - GLASS CANYON", episode=1, theme="glass_canyon",
          music="theme_airy", weapon="spark_disc", par=360, flags="")

SPINE = 29          # the ladder column
TUBE = (27, 30)     # glass tube walls; the inside (x28-29) fits a runner
SLABS = {"C1": 124, "C2": 97, "C3": 66, "C4": 37, "roof": 7}


def ledge(x0, x1, y):
    L.row(x0, x1, y, "#")


def frame(x0, x1, y):
    L.row(x0, x1, y, "=")


def ladder(x, y0, y1):
    L.col(x, y0, y1, "H")


def pair(pid, ax, bx, home, drop=2, **keys):
    """A pulley pair: gondolas 3 blocks wide, top surface on row `home`."""
    L.at("platform", ax, home, pid + "a", mode="pulley", w=3, pair=pid + "b", drop=drop, slack=8,
         rehome=keys.pop("rehome", 30), **keys)
    L.at("platform", bx, home, pid + "b", mode="pulley", w=3)


def hole(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def gems(*pts):
    for x, y in pts:
        L.put(x, y, "g")


# --- Shell: outer steel frame, ground, slabs, the spine --------------------
L.col(0, 0, H - 1, "#")
L.col(W - 1, 0, H - 1, "#")
L.ground(1, W - 2, 146)
for name, row in SLABS.items():
    L.row(1, W - 2, row, "#")
# One-way where the climb arrives from below.
L.row(1, 14, SLABS["C1"], "=")
L.row(35, 49, SLABS["C2"], "=")
L.row(49, 58, SLABS["C4"], "=")
L.row(17, 26, SLABS["roof"], "=")
# The tube: glass walls with doorways just above every slab.
for x in TUBE:
    L.col(x, 8, 145, "#")
for top in (143, 121, 94, 63, 34):
    for x in TUBE:
        L.col(x, top, top + 2, ".")
ladder(SPINE, 8, 145)
for name in ("C1", "C2", "C3", "C4"):
    row = SLABS[name]
    L.put(28, row, "#")
    L.put(SPINE, row, "#")
    L.at("hatch", 28, row, tile="=")
    L.at("hatch", SPINE, row)

# --- 1. Ground floor (left), rows 146-124 ----------------------------------
L.on(3, 146, "P")
L.put(2, 145, "*")
hole(5, 10, 146, 148)                 # g1's shafts
pair("g1", 5, 8, 146)
frame(22, 25, 142)                    # n1: Nova's shortcut
ledge(11, 18, 141)                    # L1
L.on(15, 141, "W")
frame(21, 25, 138)                    # w1
L.on(23, 138, "h")
frame(14, 19, 135)                    # w2
pair("g2", 11, 8, 135)
ledge(1, 7, 130)                      # L2
L.on(2, 130, "1")                     # letter G
frame(9, 13, 127)                     # w3
gems((13, 143), (19, 139), (20, 136), (5, 128))

# --- 2. Staircase (right), rows 124-97 --------------------------------------
L.on(18, 124, "c")
hole(37, 42, 124, 124)                # g3 sits in the slab
pair("g3", 37, 40, 124)
ledge(43, 50, 119)                    # L3
L.on(47, 119, "W")
frame(53, 57, 116)                    # w4
L.on(55, 116, "h")
ladder(56, 108, 115)
frame(49, 55, 108)                    # w5
pair("g4", 46, 43, 108)
ledge(35, 42, 103)                    # L4
frame(44, 48, 100)                    # w6
L.at("glass_crawler", 58, 117)
L.at("glass_crawler", 58, 104)
L.at("glass_crawler", 31, 110)
L.at("deco", kind=42, x=51, y=111, w=3, h=2, text="uptime: 42 days")
gems((41, 120), (51, 117), (52, 114), (45, 104), (38, 100))

# --- 3. Squeegees (left), rows 97-66 -----------------------------------------
L.on(53, 97, "c")
L.on(6, 97, "T")
hole(19, 24, 97, 97)                  # g5 sits in the slab
pair("g5", 22, 19, 97)
ledge(11, 18, 93)                     # L5
L.on(16, 93, "W")
L.on(14, 93, "h")
frame(3, 8, 91)                       # w7
ladder(4, 86, 90)
frame(1, 8, 86)                       # w8, floor 20
L.put(4, 86, "H")
pair("g6", 9, 12, 86)
ledge(15, 22, 82)                     # L6
frame(23, 26, 80)                     # w9
ladder(24, 74, 79)
frame(17, 25, 74)                     # w10
L.put(24, 74, "H")
pair("g7", 14, 11, 74)
ledge(3, 10, 70)                      # L7
frame(12, 15, 68)                     # w11
L.row(3, 16, SLABS["C3"], "=")
L.at("squeegee_drone", None, None, "sq1", rect=(2, 90, 26, 91))
L.at("squeegee_drone", None, None, "sq2", rect=(2, 79, 26, 80), start="r")
L.at("glass_crawler", 26, 88)
L.at("glass_crawler", 1, 83, carrier=1)
gems((10, 92), (6, 89), (12, 84), (20, 78), (8, 68))

# --- 4. Cop ride (right), rows 66-37 -----------------------------------------
L.on(20, 66, "c")
hole(40, 42, 66, 66)
hole(45, 47, 66, 66)
pair("g8", 45, 40, 56, drop=10, brake=1, start=66, rehome=60)
ledge(36, 39, 46)                     # the cop balcony
L.at("chrome_cop", 41, 45)
L.at("spawner", 38, 45, enemy="chrome_cop", onto="g8b")
ledge(48, 56, 46)                     # L8
L.on(54, 46, "W")
L.on(52, 46, "h")
frame(57, 58, 43)                     # w12
frame(50, 54, 40)                     # w13
# The Halcyon showroom on floor 30, behind a cracked window.
L.row(48, 58, 52, "#")
L.row(48, 58, 57, "#")
L.col(48, 53, 56, "#")
L.at("breakable", rect=(48, 53, 48, 56), hp=2, by="any")
ladder(56, 46, 56)
for x in (50, 52, 54, 55):
    L.on(x, 57, "m")
L.on(57, 57, "2")                     # letter U
L.on(51, 57, "$")
L.at("glass_crawler", 58, 60)
gems((44, 62), (52, 44), (57, 41))

# --- 5. Sniper floors (left), rows 37-7 --------------------------------------
L.on(57, 37, "c")
hole(19, 24, 37, 37)                  # g9 sits in the slab
pair("g9", 22, 19, 37)
ledge(11, 18, 32)                     # L9
frame(3, 9, 29)                       # w14
L.on(6, 29, "h")
ladder(5, 21, 28)
frame(1, 9, 21)                       # w15, floor 42
L.put(5, 21, "H")
L.on(3, 21, "3")                      # letter N
L.at("deco", kind="reflection", rect=(1, 18, 9, 20))  # easter egg: it waves back
pair("g10", 10, 13, 21)
ledge(16, 23, 16)                     # L10
frame(24, 26, 13)                     # w16
L.on(25, 13, "Q")
frame(18, 21, 10)                     # w17
ledge(23, 26, 24)                     # the sniper balcony
L.at("penthouse_sniper", 25, 23)
ledge(1, 4, 11)                       # the penthouse balcony
L.at("penthouse_sniper", 2, 10, dir="r")
# Window-cleaning carts: solid, and they cut the snipers' lines.
for x, y in ((13, 30), (3, 27), (19, 14)):
    L.fill(x, x + 1, y, y + 1, "#")
L.at("squeegee_drone", None, None, "sq3", rect=(5, 26, 26, 27))
gems((12, 30), (4, 26), (14, 18), (22, 11))

# --- 6. Roof ----------------------------------------------------------------
ladder(2, 1, 6)
L.put(2, 0, "C")
L.on(44, 7, "X")
L.put(5, 4, "B")                       # before the exit on the way in
L.on(16, 7, "m")
L.at("deco", kind="text", x=35, y=5, w=16, h=2, text="HALCYON HELIPAD")

L.write(__file__, "02_glass_canyon.txt")
