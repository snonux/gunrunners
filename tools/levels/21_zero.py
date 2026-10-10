#!/usr/bin/env python3
"""Level 21 - ZERO (SPEC.md 21). Twist: Live Rewiring. Prototype: Phase Rifle.

A run west to east through ZERO's server cathedral (x 0-149: the corridor is
rows 24-35 on ground 36, the rack mass above it) into the core room, the
boss arena (x 152-187). The level script moves the bulkheads (`@ layer
BK_* driver=script style=zero`) between five states (`@ shift`), each
previewed on the monitors 45 frames ahead: S1 when the runner passes x 20,
L1 at the first checkpoint, L2a at the second, L2b when the kill switch K1
is shot, L3 at the third.

Changes from the spec, so it plays in this engine (the view is 11 blocks
tall, and everything in the arena has to fit in it with the floor):
- The arena is open from row 24 (not 4) to row 39. ZERO's eye is 6 x 6
  blocks at x 167-172 rows 28-33 (centre (170, 31)), its eight racks orbit
  at 9 cells (not 7 blocks), and ALT1 and ALT2 ride rails at row 29 either
  side of the ring (x 152-162 and x 178-187).
- DOOR1 reaches the ceiling (rows 24-35), so it has to be shot open.
- The floor pocket's grating (secret 2) breaks to three landings on it
  (by=stomp; a shot can't reach the floor), and glints when a phase shot
  passes over it.
- The mezzanine is x 99-104 (not 97): x 97-98 is the drop into the lower
  tier for the Rocco line (the step's top is too close under the
  mezzanine to walk in). W is at (99, 29), U at (101, 29), K1 hangs from the
  rack tops over x 102 rows 27-28.
- The rack tops x 100-108 have the ceiling over them cut back to row 22, so
  Nova can stand on them.
- The full refill is `@ health full=1`.
"""

from lib import Level

W, H = 190, 44
L = Level(W, H,
          name="STAGE 21 - ZERO", episode=3, theme="station_servers",
          music="organ_theme", weapon="phase_rifle", par=240, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def bulkhead(lid, x0, y0, x1, y1, shut):
    L.fill(x0, x1, y0, y1, "#" if shut else ".")
    L.at("layer", None, None, lid, rect=(x0, y0, x1, y1), driver="script", style="zero", solid=1 if shut else 0)


# --- The shell: rack mass, the corridor, the arena ----------------------------------------------------
L.fill(0, W - 1, 0, H - 1, "#")
L.clear(2, 149, 24, 35)                           # the corridor, on ground 36
L.clear(150, 151, 33, 35)                         # the arena door (shuts behind you)
L.clear(152, 187, 24, 39)                         # the arena
L.clear(152, 187, 41, 42)                         # under the floor: the live grille
for m in range(8, 149, 16):
    L.at("monitor", m, 24)
L.at("monitor", 156, 24)
L.at("monitor", 184, 24)

# --- 1. The rack corridor (x 2-48) ---------------------------------------------------------------------
put(3, 35, "P")
bulkhead("BK_S1a", 34, 30, 35, 35, True)          # opens in S1
bulkhead("BK_S1b", 12, 30, 13, 35, False)         # closes behind you in S1
L.at("shift", None, None, "S1", trigger="x:20", open="BK_S1a", close="BK_S1b", slide=60)
put(20, 35, "h")
put(30, 35, "m")
put(46, 35, "W")
put(48, 35, "c")
gem((8, 35), (16, 35), (26, 35), (38, 35), (42, 35), (44, 32))
# Secret 1: RACK 42, the rack that never moves, on the ledges over the corridor.
L.fill(22, 23, 33, 33, "=")                       # the first ledge (ground 33)
L.fill(24, 29, 30, 30, "#")                       # the second (ground 30)
L.fill(24, 29, 26, 26, "#")                       # RACK 42's roof
L.fill(29, 29, 27, 29, "#")                       # its back
L.at("fakewall", None, None, rect=(24, 27, 24, 29))  # its front: walk through it
L.at("deco", None, None, kind="rack42", rect=(24, 26, 29, 29))
L.at("deco", 25, 26, kind="42")
put(25, 27, "B")
put(28, 29, "1")                                  # letter G
gem((26, 27), (27, 27), (28, 27), (26, 28), (27, 28), (28, 28), (26, 29), (27, 29))
L.at("terminal", 27, 29, text="ZERO LOG 0042: INSTRUCTED TO LOSE")

# --- 2. Layout 1, turrets (x 49-85) --------------------------------------------------------------------
L.fill(56, 58, 33, 35, "#")                       # rack blocks
L.fill(70, 72, 33, 35, "#")
put(57, 32, "Q")                                  # the duck, in a tiny headset
L.at("lattice_turret", 56, 24, "LT1", rail=(50, 66))
L.at("repair_swarm", 60, 30, "RS1")
L.at("repair_swarm", 66, 30, "RS2")
L.at("breakable", None, None, "DOOR1", rect=(77, 24, 78, 35), by="any", hp=6, look="rackdoor", rebuild=1)
bulkhead("BK_L1", 82, 24, 83, 32, False)          # lowers in L1: the east arch
L.at("shift", None, None, "L1", trigger="x:48", close="BK_L1")
# Secret 2: a floor grating over a pocket.
L.clear(63, 66, 37, 38)
L.at("breakable", None, None, rect=(63, 36, 66, 36), by="stomp", hp=3, look="grate")
put(64, 38, "$")
put(65, 38, "m")
put(75, 35, "h")
put(80, 35, "m")
put(86, 35, "c")
gem((52, 35), (62, 35), (67, 35), (71, 32), (74, 35), (84, 35))

# --- 3. Layout 2, the Echo maze (x 87-113) -------------------------------------------------------------
L.fill(93, 96, 33, 35, "#")                       # the first rack step
L.fill(105, 108, 33, 35, "#")                     # the second
L.fill(99, 104, 30, 31, "#")                      # the mezzanine
L.fill(100, 108, 26, 26, "#")                     # rack tops (the Nova line)
L.clear(100, 108, 22, 23)                         # headroom over them
L.at("lasergrid", None, None, rect=(97, 32, 104, 35), dmg=1, every=6)
bulkhead("BK_L2c", 104, 27, 104, 29, False)       # shuts the mezzanine's east end in L2a
bulkhead("BK_L2d", 92, 24, 92, 32, False)         # shuts behind it in L2b
L.at("shift", None, None, "L2a", trigger="x:86", close="BK_L2c")
L.at("shift", None, None, "L2b", trigger="switch:K1", open="BK_L2c", close="BK_L2d")
L.at("switch", 102, 27, "K1", kind="shootable")
L.at("echopad", 90, 35, "E1")
L.at("echopad", 110, 35, "E2", wake="K1")
put(99, 29, "W")
put(101, 29, "2")                                 # letter U
put(112, 35, "h")
put(114, 35, "c")
gem((89, 35), (95, 32), (100, 29), (103, 25), (107, 25), (111, 35))

# --- 4. Layout 3, the approach (x 115-149) -------------------------------------------------------------
bulkhead("BK_L3a", 126, 24, 127, 30, False)       # half-height baffles, down in L3
bulkhead("BK_L3b", 138, 24, 139, 30, False)
L.at("shift", None, None, "L3", trigger="x:114", close="BK_L3a,BK_L3b")
put(120, 35, "h")
put(124, 35, "W")
L.at("repair_swarm", 128, 30, "RS3")
L.at("lattice_turret", 132, 24, "LT2", rail=(125, 140))
put(130, 35, "3")                                 # letter N
L.at("terminal", 135, 35, text="ZERO PUBLIC TERMINAL - HELLO RUNNER", code=1)
put(142, 35, "T")
L.at("health", 146, 35, full=1)
put(148, 35, "c")
gem((118, 35), (122, 35), (128, 35), (134, 35), (140, 35), (144, 35))

# --- 5. The arena (x 152-187) --------------------------------------------------------------------------
L.at("boss", 167, 30, "ZERO", w=6, h=6, arena=(152, 24, 187, 39), door=(150, 33, 151, 35),
     wall=(188, 24, 189, 42), turrets="ALT1,ALT2")
for k in range(12):
    L.at("layer", None, None, "SEG%d" % (k + 1), rect=(152 + 3 * k, 40, 154 + 3 * k, 40), driver="script",
         style="zero", solid=1)
L.at("fluid", None, None, rect=(152, 42, 187, 42), kind="lava")
L.at("lattice_turret", 157, 29, "ALT1", rail=(152, 162))
L.at("lattice_turret", 183, 29, "ALT2", rail=(178, 187))
put(184, 39, "X")

L.write(__file__, "21_zero.txt")
