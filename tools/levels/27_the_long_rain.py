#!/usr/bin/env python3
"""Level 27 - The Long Rain (SPEC.md 27). Twist: Shadows. Prototype: Silencer.

A walk east along the wet street (ground 38) from the rainy alley down main
street, through the jazz club (dark, its lamps and the stage spotlight) and
the newspaper office (the press lift up to the mezzanine, the window out),
then up the fire escape in the alley to the hotel roof (ground 20), where
the exit is a red neon EXIT sign. Hidden in `@ shadow` and `@ dark` rects
out of the lamps' light; seen in a Flatfoot's flashlight or the spotlight
for 8 frames, the whistle blows and Goons come out of the nearest `@ door`.

Everything in world_noir.cpp.

Changes from the spec, so it plays in this engine:
- F4 patrols x 182-186 (the chimney stands at 180-181, in the spec's path).
- The fire escapes over main street are in shadow (rows 26-33, x 44-89).
- C1 sits on a two-block sign ledge (one-way) at (48-49, 34).
- The club's and the office's roof is solid rows 22-27; the walls between
  street, club and office have 4-block doorways (rows 34-37).
- The projection booth is a ledge (top rows 28-29, floor row 33, x 123-130)
  open to the west: ladders are only climbed up, so the way back down after
  the bonus is a drop off its west end (its floor is 10 cells over the club
  floor, out of reach of any jump but a Turbo one). The safe's ladder is
  x 127 rows 33-37 with a one-way hatch at (126, 33) when the painting
  opens (ladders through a floor need the block to their left).
- The jukebox, the lipstick note and the painting are `@ jukebox`, `@ note`
  and `@ switch painting` lines; the office alarm's switch carries its
  shutter (x 165, rows 35-37).
"""

from lib import Level

W, H = 190, 44
L = Level(W, H,
          name="STAGE 27 - THE LONG RAIN", episode=4, theme="noir_city",
          music="theme_noir_trumpet", weapon="silencer", par=420, flags="")

G = 38  # the street


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def shadow(x0, y0, x1, y1):
    L.at("shadow", None, None, rect=(x0, y0, x1, y1))


# --- The ground and the buildings ------------------------------------------------------------------
L.ground(0, W - 1, G)
L.fill(91, 165, 22, 27, "#")                # the club's and the office's roof
L.col(91, 28, 33, "#")                      # the club's front wall over its doorway
L.col(131, 28, 33, "#")                     # the wall between the club and the office
L.fill(123, 130, 28, 29, "#")               # the projection booth: its top
L.row(123, 130, 33, "#")                    # its floor (the safe's ladder comes up through it)
L.col(165, 28, 28, "#")                     # the office's east wall: the window (29-31) is open,
L.col(165, 32, 34, "#")                     # the shutter (35-37) is set by the alarm switch
L.fill(176, W - 1, 20, G - 1, "#")          # the hotel
L.fill(180, 181, 17, 19, "#")               # its chimney
L.at("rain", None, None, rect=(0, 0, 90, 37))
L.at("rain", None, None, rect=(91, 0, 165, 21))
L.at("rain", None, None, rect=(166, 0, 189, 37))

# --- 1. Rainy alley (x 0-30) ----------------------------------------------------------------------
put(3, 37, "P")
shadow(12, 34, 15, 37)                      # the doorway
L.at("door", 7, 37)                         # the side door
put(16, 34, "G")
put(22, 37, "W")                            # in the trash can
put(26, 37, "h")
L.at("flatfoot", 30, 37, "F0", path=[(30, 37), (7, 37)], once=1, trigger=10)
gem((5, 36), (9, 36), (13, 36), (19, 36))

# --- 2. Main street (x 31-90) ---------------------------------------------------------------------
put(32, 37, "c")
for x0 in (36, 50, 64, 78):
    shadow(x0, 34, x0 + 6, 37)              # the awnings
    L.at("deco", x0, 33, kind="awning", w=7, h=1)
for x0 in (44, 58, 72, 86):
    L.row(x0, x0 + 3, 34, "=")              # fire escapes
for x0 in (50, 64, 78):
    L.row(x0, x0 + 5, 30, "=")
shadow(44, 26, 89, 33)                      # the fire escapes are in shadow
L.row(48, 49, 34, "=")                      # C1's sign ledge
for x in (40, 60, 86):
    L.at("door", x, 37)
L.at("flatfoot", 44, 37, "F1", path=[(44, 37), (62, 37)])
L.at("flatfoot", 66, 37, "F2", path=[(66, 37), (88, 37)])
L.at("crow", 48, 33)
L.at("crow", 75, 33)
put(54, 37, "h")
put(66, 37, "m")
gem((38, 36), (41, 36), (51, 36), (56, 36), (65, 36), (69, 36), (79, 36), (83, 36))   # in the shadows
gem((45, 33), (59, 33), (73, 33), (87, 33), (52, 29), (54, 29), (67, 29), (81, 29))   # on the fire escapes

# --- 3. Jazz club (x 91-130) ----------------------------------------------------------------------
L.at("dark", None, None, rect=(92, 28, 129, 37))
for x in (100, 106, 126):
    L.at("light", x, 35, r=2)
L.at("spotlight", 117, 28, sweep=(108, 126), period=120, cone=(20, 10), floor=37)
L.at("door", 110, 37)
put(91, 37, "c")
put(93, 37, "W")
L.at("note", 96, 34)
L.at("jukebox", 99, 37)
put(104, 34, "V")
L.at("vskin", 104, 34, skin="smoke_cloud", path=[(98, 34), (110, 34)], speed="1/4")
put(117, 33, "U")
put(120, 37, "h")
L.at("switch", 127, 34, "painting", kind="shootable", states=2, ladder=(127, 33, 37))
put(125, 32, "m")
put(126, 32, "N")
put(129, 30, "B")
gem((95, 36), (98, 36), (101, 36), (103, 36), (107, 36), (109, 36), (113, 36), (122, 36))

# --- 4. Newspaper office (x 131-165) --------------------------------------------------------------
L.at("platform", 140, 36, "press", w=3, path=[(140, 36), (140, 34)], speed="1/4", mode="pingpong")
L.row(144, 164, 32, "=")                    # the mezzanine
for x0 in (138, 148, 156):
    shadow(x0, 34, x0 + 3, 37)              # the blinds
L.at("door", 150, 37)
put(131, 37, "c")
put(133, 37, "W")
put(134, 37, "T")
put(146, 37, "h")
L.at("flatfoot", 136, 37, "F3", path=[(136, 37), (160, 37)])
L.at("prize", 152, 31, kind="red_evidence", score=2000)
L.at("deco", 145, 33, kind="42", text="42 WITNESSES, NO SUSPECTS", w=4, h=1)
L.at("switch", 162, 34, "alarm", kind="shootable", states=2, shutter=(165, 35, 37))
put(155, 31, "m")
gem((145, 31), (147, 31), (149, 31), (151, 31), (154, 31), (157, 31), (160, 31), (163, 31))

# --- 5. Fire escape and roof (x 166-189) ----------------------------------------------------------
for y, x0 in ((36, 167), (34, 172), (32, 167), (29, 172), (26, 167), (23, 172)):
    L.row(x0, x0 + 3, y, "=")
put(168, 31, "c")
shadow(176, 17, 179, 19)                    # the water tank
L.at("door", 182, 19)
L.at("flatfoot", 182, 19, "F4", path=[(182, 19), (186, 19)])
L.at("crow", 181, 16)
put(177, 19, "h")
put(185, 19, "m")
put(189, 19, "Q")
put(187, 19, "X")
gem((170, 35), (174, 33), (168, 25), (173, 22), (178, 18), (183, 18))

L.write(__file__, "27_the_long_rain.txt")
