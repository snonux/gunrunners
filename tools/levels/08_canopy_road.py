#!/usr/bin/env python3
"""Level 8 - Canopy Road (SPEC.md 08). Twist: Swing Vines. Prototype: Boomerang.

A jungle crossed left to right through three layers: a riverbank start
(bottom left), the mid canopy (plank floors at row 22) carrying the route,
the top canopy (row 12) for Nova and the Turbo run, and the forest floor
(row 41) as the safety net. The exit is the temple gate, bottom right.

Vines: grab one by jumping into its lower three blocks; hold the way it
swings to pump it higher (5 degrees a half-swing, up to 60), and jump near
the end of a swing of 45 degrees or more to launch (14 cells up, 2 cells a
frame that way until you land). Rope bridges snap under Rocco, after you
leave them, or at a Bridge Cutter's third chop. Shoot the ropes over the
ravine and the logs drop across it.

Changes from the spec, so the route plays as intended in this engine:
- The stream's far bank starts at x26, not x23: a vine anchored at x18
  swinging 60 degrees would put the runner inside a bank at x23. The bed
  runs x14-25.
- The mid and top canopy floors are one-row plank floors (`=`), the low
  canopy (x112-147) and the Rocco pocket are solid ground, and M6 is a
  plank floor over the pocket.
- The hollow tree's gap is x95 rows 28-33 (walk out at floor level); the
  roots room (x96-99) has walls and a ladder (x98) up through the floor
  hole; the bonus door is at (96, 37).
- A ladder at x105 (rows 22-40) gets runners who fall east of the hollow
  tree back up to M3.
- Logs hang 3 blocks under pulleys at row 6; the cage hangs from (170, 12)
  with its top at row 15, over L2's landing, and the vine VR hangs from
  (165, 14), 6 blocks long, so it clears the hanging logs.
- The carved 42 is on the trunk under M1 (42, 27), and the swing hint sits
  on a sign by the start.
"""

from lib import Level

W, H = 210, 44
L = Level(W, H,
          name="STAGE 8 - CANOPY ROAD", episode=2, theme="jungle_canopy",
          music="theme_marimba", weapon="boomerang", par=360, flags="")


def gem(*pts):
    for x, y in pts:
        L.put(x, y, "g")


def vine(vid, x, y, length, amp):
    L.at("vine", x, y, vid, len=length, amp=amp)


def trunk(x0, y0, x1, y1, hollow=False):
    L.at("deco", kind="trunk", rect=(x0, y0, x1, y1), text="hollow" if hollow else None)


# --- 1. Stream (x0-40) ----------------------------------------------------------------
L.ground(0, 13, 38)                          # left bank
L.ground(14, 25, 40)                         # stream bed
L.at("water", rect=(14, 38, 25, 39))
L.ground(26, 40, 34)                         # far bank
L.row(0, 3, 31, "=")                         # branch left of the start
L.row(24, 27, 31, "=")                       # branch over the far bank
L.col(40, 22, 33, "H")                       # trunk ladder up to M1
L.on(3, 38, "P")
L.on(10, 38, "h")
L.at("deco", 4, 33, kind="text", text="PUSH WITH THE SWING, JUMP AT THE TOP", w=8, h=1)
vine("V1", 18, 27, 8, 30)
L.on(25, 31, "W")
L.put(2, 30, "Q")                            # the duck, in a pith helmet
L.on(30, 34, "m")
gem((22, 29), (24, 27), (26, 26), (28, 26), (30, 27), (32, 29))

# --- 2. Vine chains (x41-115) ----------------------------------------------------------
L.ground(41, 111, 41)                        # forest floor
L.row(55, 60, 40, "^")
L.row(88, 93, 40, "^")
L.row(41, 50, 22, "=")                       # M1
L.row(74, 82, 22, "=")                       # M2
L.row(106, 115, 22, "=")                     # M3
L.col(51, 22, 40, "H")
L.col(83, 22, 40, "H")
L.col(105, 22, 40, "H")
trunk(41, 23, 43, 40)
trunk(78, 23, 79, 40)
trunk(110, 23, 111, 31)
L.at("deco", 42, 27, kind="42")
L.row(57, 59, 11, "=")
L.row(79, 81, 16, "=")
L.row(109, 111, 16, "=")
L.row(76, 78, 18, "=")
L.on(43, 22, "c")
L.on(46, 22, "h")
for i, x in enumerate((55, 62, 69)):
    vine("VA%d" % (i + 1), x, 12, 7, 20)
for i, x in enumerate((87, 94, 101)):
    vine("VB%d" % (i + 1), x, 12, 7, 20)
L.at("howler", 58, 10, look="dash")
L.at("howler", 80, 15)
L.at("howler", 110, 15, carrier=1)
L.at("viper", 77, 17)
L.on(78, 22, "W")
L.on(108, 22, "c")
L.on(111, 22, "h")
gem((53, 19), (57, 17), (60, 19), (64, 17), (67, 19), (71, 17), (85, 19), (89, 17), (92, 19), (96, 17),
    (99, 19), (103, 17), (62, 39), (66, 39), (70, 39), (74, 39))

# --- 2a. Hollow tree (secret) --------------------------------------------------------
L.fill(95, 100, 26, 40, "#")
L.clear(96, 99, 28, 33)                      # the hollow
L.clear(95, 95, 28, 33)                      # the gap in its west side
L.clear(96, 99, 35, 39)                      # the roots room
L.clear(97, 99, 34, 34)                      # floor hole
L.col(98, 34, 39, "H")
trunk(95, 26, 100, 40, hollow=True)
vine("SV", 86, 23, 4, 20)                    # the vine "to nowhere"
L.row(85, 87, 23, "=")                       # its branch stub
L.put(97, 33, "1")                           # letter G
L.put(96, 37, "B")
gem((96, 33), (99, 33), (98, 31), (99, 39), (90, 25), (92, 26), (94, 28))

# --- 2b. Top canopy (x70-143) --------------------------------------------------------
L.row(79, 80, 18, "=")                       # Nova's steps up from M2
L.row(75, 76, 14, "=")
L.row(70, 80, 12, "=")                       # TL1
L.row(90, 112, 12, "=")                      # TL2
L.row(119, 140, 12, "=")                     # TL3
L.row(106, 110, 8, "=")
L.row(126, 127, 9, "=")                      # the tallest tree's branches
L.row(129, 130, 6, "=")
L.row(127, 129, 4, "=")                      # the nest
trunk(128, 5, 129, 21)
L.at("deco", kind="nest", rect=(126, 3, 130, 4))
vine("VT", 84, 2, 14, 20)
vine("VU", 116, 4, 6, 20)
L.on(96, 12, "T")
L.at("viper", 108, 7)
L.put(120, 9, "2")                           # letter U
L.on(124, 12, "m")
L.put(128, 3, "C")
gem((72, 11), (74, 11), (77, 11), (86, 9), (88, 9), (91, 11), (93, 11), (99, 11), (102, 11), (105, 11),
    (114, 7), (116, 6), (118, 7), (122, 11), (126, 11), (127, 8), (130, 5), (132, 11), (136, 11), (139, 11))

# --- 3. Rope bridges (x116-155) ------------------------------------------------------
L.ground(112, 147, 32)                       # low canopy
L.ground(148, 159, 30)                       # the Rocco pocket
L.row(128, 133, 22, "=")                     # M4
L.row(144, 149, 22, "=")                     # M5
L.row(156, 159, 22, "=")                     # M6
L.col(128, 22, 31, "H")
L.col(144, 22, 31, "H")
L.col(158, 22, 29, "H")
L.at("bridge", None, None, "B1", rect=(116, 22, 127, 22), cut=3)
L.at("bridge", None, None, "B2", rect=(134, 22, 143, 22), cut=3)
L.at("bridge", None, None, "WB", rect=(150, 22, 155, 22), snap=8, after=30)
L.at("cutter", 128, 21, bridge="B1")
L.at("cutter", 145, 21, bridge="B2")
L.row(130, 132, 18, "=")
L.row(146, 148, 18, "=")
L.at("viper", 131, 17)
L.at("viper", 147, 17)
L.row(137, 139, 28, "=")
L.at("howler", 138, 27)
L.on(146, 22, "c")
L.on(148, 22, "h")
for x in (150, 152, 154):
    L.on(x, 30, "m")
gem((118, 20), (121, 20), (124, 20), (136, 20), (139, 20), (142, 20), (118, 31), (124, 31), (132, 31),
    (140, 31))

# --- 4. Ravine (x156-181) ----------------------------------------------------------------
L.ground(160, 173, 41)
L.row(161, 172, 40, "^")
L.ground(167, 167, 22)                       # rock pillar
L.put(167, 40, "#")
L.ground(174, 181, 22)                       # M7
L.col(160, 23, 40, "H")
L.at("log", 163, 6, "L1", len=7, hang=3, tie=(158, 21), lands=(160, 166, 22))
L.at("log", 171, 6, "L2", len=6, hang=3, tie=(175, 21), lands=(168, 173, 22))
L.at("cage", 170, 15, "C1", gems=6, rope=(170, 12))
vine("VR", 165, 14, 6, 20)
L.on(157, 22, "W")
L.on(177, 22, "c")
L.on(179, 22, "h")

# --- 5. Descent (x182-209) ----------------------------------------------------------------
L.ground(182, 200, 41)
L.row(183, 199, 40, "^")
L.col(200, 34, 40, "H")
L.ground(201, 209, 34)                       # the temple-gate ledge
vine("VE1", 185, 16, 6, 30)
vine("VE2", 191, 20, 6, 30)
vine("VE3", 197, 24, 6, 30)
L.at("deco", kind="gate", rect=(203, 26, 209, 33))
L.put(194, 25, "3")                          # letter N
L.on(206, 34, "X")
gem((183, 21), (186, 24), (189, 26), (192, 29), (195, 31), (198, 33), (201, 32), (203, 32), (182, 20),
    (188, 22))

L.write(__file__, "08_canopy_road.txt")
