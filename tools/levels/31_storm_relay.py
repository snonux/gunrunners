#!/usr/bin/env python3
"""Level 31 - Storm Relay (SPEC.md 31). Twist: Lightning. Prototype: Tesla Rod.

A climb up a sea cliff in a thunderstorm: the beach, the metal cliff stairs
and a dead lift on the right, back left through the sea caves, up a ladder
to three switchbacks, then right into the lighthouse and up its stairs to
the lamp room and the relay dish. Each `@ lightning` zone strikes its
highest metal every `period` frames after a 22-frame glow, and for 4 frames
anything touching the struck metal is hurt. A strike powers a dead machine:
the lift, the generator that opens the smugglers' hatch, the lamp. A stuck
Tesla rod takes its zone's next strike, arcs to the enemies around it and
powers what it is stuck in.

Everything in world_storm.cpp.

Changes from the spec, so it plays in this engine:
- The lift is a `wait=rider` lift: dead until a strike powers it, then it
  takes a rider up and parks, and a runner below calls it back down (the
  spec's lift stays at the top, which would strand a runner who fell).
- The lift's housing on its mast is not solid (a runner on the lift can
  stand clear of it).
- The switchbacks' ledge ends over each step are one-way (`=`) so a runner
  can jump up through them: ledge 2's right end (57-62) over the step from
  ledge 1, ledge 3's left end (20-25) over the step from ledge 2.
- The lighthouse stair flights are one-way metal grating, so the flights
  above don't take the headroom of the ones below. Flight 1 is 66-83 and its
  landing 84-90 (g26); flight 2 is 82-69 and flight 3 68-77.
- The smugglers' room is entered and left by the hatch: the generator opens
  32-33 rows 68-69 and turns column 33 into the ladder down (no second
  ladder at x 44).
- The ladder up from the caves runs beside the cave's west wall (x 29 is
  open, a runner is a block and a half wide) and comes out through ledge 1
  beside a one-way block.
- The Nova line's knobs are 64-65 g79, 68-69 g75 and 72-73 g72; the cliff's
  rock face is at x 76.
"""

from lib import Level

W, H = 100, 100
L = Level(W, H, name="STAGE 31 - STORM RELAY", episode=5, theme="storm_cliffs", music="theme_rock_storm",
          weapon="tesla_rod", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def metal(x0, y0, x1, y1, ident=None):
    if ident:
        L.at("metal", None, None, ident, rect=(x0, y0, x1, y1))
    else:
        L.at("metal", None, None, rect=(x0, y0, x1, y1))


# --- The rock ---------------------------------------------------------------------------------------
L.fill(0, 40, 92, H - 1, "#")                 # the beach (the sea below is backdrop)
L.fill(41, W - 1, 36, H - 1, "#")             # the cliff
L.clear(41, 75, 70, 91)                       # the cliff path's air
L.clear(30, 75, 52, 67)                       # the sea caves
L.clear(41, 66, 36, 49)                       # the switchbacks' air
L.fill(28, 46, 68, 77, "#")                   # the rock under the caves (the smugglers' room)
L.fill(14, 40, 50, 69, "#")                   # the caves' west end, under ledge 1
L.clear(29, 40, 52, 67)
L.clear(41, 64, 0, 35)                        # sky over the switchbacks and the lighthouse door
L.clear(92, W - 1, 0, 35)                     # sky east of the lighthouse

# --- 1. The beach (x 0-40) --------------------------------------------------------------------------
L.row(6, 12, 87, "#")                         # the fisherman's hut's roof
put(3, 91, "P")
put(8, 91, "m")
put(9, 91, "Q")
L.at("duck", 9, 91, costume="sou_wester")
put(12, 91, "W")
put(30, 91, "h")
L.at("frygull", 20, 80)
metal(36, 85, 36, 91)                         # the antenna: Z1's only metal
L.at("deco", 38, 89, kind="42", art="buoy", text="42", w=1, h=1)
gem((21, 91), (23, 91), (25, 91), (27, 91), (29, 91), (31, 91))
L.at("lightning", None, None, "z1", zone=(0, 70, 40, 99), period=90, damage=2, phase=20)

# --- 2. The cliff path (x 41-75) --------------------------------------------------------------------
L.fill(41, 46, 89, 91, "#")
L.fill(47, 52, 86, 91, "#")
L.fill(53, 58, 83, 91, "#")
L.fill(62, 75, 83, 91, "#")                   # the ground east of the lift
L.fill(59, 61, 84, 91, "#")                   # under the lift
metal(41, 89, 46, 91, "stairs")
metal(47, 86, 52, 91, "stairs")
metal(53, 83, 58, 91, "stairs")
L.at("platform", 59, 83, "lift1", w=3, mode="pingpong", path=((59, 83), (59, 68)), speed="1/2", wait="rider")
L.at("machine", None, None, "lift1m", kind="lift", lift="lift1", rect=(62, 80, 62, 82))
L.row(64, 65, 79, "#")                        # Nova's knobs
L.row(68, 69, 75, "#")
L.row(72, 73, 72, "#")
L.at("spawn", None, None, rect=(41, 70, 75, 92), kind="storm_gull", count=3, max=2)
put(52, 80, "1")
gem((43, 88), (45, 88), (49, 85), (51, 85), (55, 82), (57, 82), (66, 82), (70, 82))
L.at("lightning", None, None, "z2", zone=(41, 69, 75, 99), period=105, damage=2, phase=0)

# --- 3. The sea caves (x 30-75, going left) ---------------------------------------------------------
L.clear(59, 61, 68, 69)                       # the lift's shaft through the ledge
L.clear(71, 75, 68, 69)                       # the ledge ends at 70
put(64, 67, "W")
put(70, 67, "c")
L.col(30, 50, 67, "H")                        # the ladder up to ledge 1
put(44, 67, "h")
put(35, 67, "m")
L.at("kite_drone", 40, 58)
L.at("kite_drone", 55, 58)
L.at("machine", None, None, "gen1", kind="generator", rect=(34, 66, 35, 67), opens=(32, 68, 33, 69), ladder=33)
for x in (38, 47, 52, 66):                    # the arches
    L.row(x, x + 1, 52, "#")
gem((37, 67), (41, 67), (48, 67), (50, 67), (53, 67), (56, 67), (66, 64), (68, 64))
L.at("lightning", None, None, "z3", zone=(14, 51, 75, 68), period=90, damage=2, phase=45)

# Secret 1: the smugglers' room under the caves, through the generator's hatch.
L.clear(30, 44, 70, 75)
L.col(33, 70, 75, "H")
put(40, 75, "3")
put(36, 75, "$")
put(42, 73, "B")
gem((34, 75), (35, 75), (37, 75), (38, 75), (39, 75), (41, 75), (34, 72), (36, 72), (38, 72), (40, 72))

# --- 4. The switchbacks (x 14-66) -------------------------------------------------------------------
L.fill(14, 62, 50, 51, "#")                   # ledge 1 (g50, going right)
L.col(30, 50, 51, "H")
L.put(29, 50, "=")                            # beside the ladder's top: walk over it, climb through it
L.put(29, 51, ".")
L.fill(63, 66, 47, 51, "#")                   # the step to ledge 2
L.fill(20, 56, 44, 45, "#")                   # ledge 2 (g44, going left)
L.row(57, 62, 44, "=")
L.fill(16, 19, 41, 45, "#")                   # the step to ledge 3
L.fill(26, 62, 38, 39, "#")                   # ledge 3 (g38, going right)
L.row(20, 25, 38, "=")
L.fill(63, 64, 36, 37, "#")                   # the step to the lighthouse door
metal(44, 44, 52, 44, "grate")                # the wet grate: Z4's target
metal(30, 33, 30, 37)                         # Z5's antennas
metal(50, 33, 50, 37)
put(18, 49, "c")
put(40, 49, "h")
L.at("foghorn", 50, 49, dir="l")
put(60, 49, "m")
L.at("foghorn", 25, 43, dir="r")
put(30, 43, "T")
put(40, 43, "W")
put(46, 40, "2")
put(24, 37, "c")
put(50, 37, "h")
L.at("foghorn", 58, 37, dir="l", carrier=1)
gem((22, 49), (26, 49), (34, 49), (46, 49), (54, 43), (50, 43), (36, 43), (22, 43), (32, 37), (38, 37), (44, 37),
    (56, 37))
L.at("lightning", None, None, "z4", zone=(14, 41, 66, 50), period=75, damage=3, phase=30)
L.at("lightning", None, None, "z5", zone=(14, 30, 66, 40), period=90, damage=2, phase=60)

# --- 5. The lighthouse (x 63-91) --------------------------------------------------------------------
L.col(64, 6, 30, "#")                         # the west wall (the door is rows 31-35)
L.col(91, 6, 34, "#")
L.fill(64, 91, 4, 5, "#")                     # the roof
L.clear(65, 90, 6, 34)
L.fill(65, 91, 35, 35, "#")                   # the stone floor (g35)
metal(82, 2, 82, 3, "roofrod")
for k in range(9):                            # flight 1: right, 66-83 (g34 up to g26)
    L.row(66 + 2 * k, 67 + 2 * k, 34 - k, "=")
    metal(66 + 2 * k, 34 - k, 67 + 2 * k, 34 - k, "flight1")
L.row(84, 90, 26, "#")                        # the keeper's landing (wood)
for k in range(8):                            # flight 2: left, 82-69 (g25 up to g18)
    L.row(82 - 2 * k, 83 - 2 * k, 25 - k, "=")
    metal(82 - 2 * k, 25 - k, 83 - 2 * k, 25 - k, "flight2")
L.row(65, 67, 17, "#")                        # the west landing (wood)
for k in range(5):                            # flight 3: right, 68-77 (g16 up to g12)
    L.row(68 + 2 * k, 69 + 2 * k, 16 - k, "=")
    metal(68 + 2 * k, 16 - k, 69 + 2 * k, 16 - k, "flight3")
L.fill(78, 90, 12, 12, "#")                   # the lamp room's floor
L.at("machine", None, None, "lamp", kind="lamp", rect=(81, 8, 83, 11))
put(72, 34, "c")
put(87, 25, "h")
L.at("treasure", 88, 25, score=10000, art="keepers_log")
put(80, 11, "m")
put(86, 11, "X")
gem((70, 31), (78, 27), (74, 20), (68, 17), (74, 12), (88, 11))
L.at("lightning", None, None, "z6", zone=(63, 0, 99, 37), period=60, damage=2, cycle="flight3,flight2,flight1",
     phase=10)

L.header["tip"] = "LIGHTNING STRIKES THE HIGHEST METAL AFTER IT GLOWS: STEP OFF THE METAL WHEN IT GLOWS"  # HINTS: the level's first 12 seconds
L.write(__file__, "31_storm_relay.txt")
