#!/usr/bin/env python3
"""Level 16 - Cryo Labs (SPEC.md 16). Twist: Ice Floors. Prototype: Freeze Ray.

Station Zero's cold labs, a run west to east at floor level (ground 34):
the ice corridor, the first lab, the ramps and the puck rink, the pod hall
with its catwalk, the curling lane under the Lab Arms, and the cryo vault
with the exit at the east end.

Ice Floors: `@ ice rect=x0,y,x1,y` marks the top of block row y as ice.
On ice the runner speeds up 1/8 cell a frame each frame up to 1 and slides
to a stop at 1/16 (Rocco 1/10 and 1/20); jumps and landings keep the
speed; matte stops a slide within 2 cells. A kicker (`@ kicker`) crossed at
3/4 cell a frame or more launches a 14-cell rise (Nova 16). Turbo grips.

Freeze Ray: a shot freezes what it hits for 120 frames into a block of ice
you can stand on; a frozen enemy on ice that is shot slides at 2 cells a
frame until it hits a wall (it shatters, killed for its score) or leaves
the ice; whatever it hits takes 4 damage.

Changes from the spec, so it plays in this engine:
- Positions of enemies are the block they stand in (bottom row), so the
  pods sit at y 33 (the spec gives their top row, 31).
- Only Puck Drones and frozen blocks slide on ice; the other enemies keep
  their grip.
- The Rocco tunnel's ladder is at x 133 (a 3-block hole at x 132-134 in the
  lane floor): x 131 is the pod hall's east wall, and a ladder shaft needs
  the block left of the ladder open too.
- Ladder holes in the catwalk deck are 3 blocks wide (x 125-127).
- The vault shelf is a one-way platform on row 30 (ground 30), so the way
  to the exit runs under it; the duck's ice cube is on it at (184, 29).
- The goal vent sits on the goal bumper's face at (86, 31)-(86, 32).
- The marked arena is left out: this engine wakes every enemy on screen.
- A bulkhead at x 90 (rows 22-27) closes the catwalk's west end, so the
  Nova shelves do not lead straight onto it over the pod hall.
- The Lab Arms hang at row 24 (claw bottom), in view under the rail.
- Puck Drones are 1 block tall: only a crouched shot hits them.
- A frozen enemy that cannot slide (off the ice, or shot from above)
  takes the shot's damage instead.
"""

from lib import Level

W, H = 190, 40
L = Level(W, H,
          name="STAGE 16 - CRYO LABS", episode=3, theme="station_cryo",
          music="cryo_bells", weapon="freeze_ray", par=170, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def ice(x0, x1, y=34):
    L.at("ice", None, None, rect=(x0, y, x1, y))


# --- The rooms -----------------------------------------------------------------------------
L.clear(2, 31, 28, 33)      # 1. the ice corridor (ceiling rows 0-27)
L.clear(32, 46, 26, 31)     # 2. the first lab (ceiling 0-25, ground 32)
L.clear(47, 187, 22, 33)    # 3-6. ceiling rows 0-21, ground 34

# --- 1. Ice corridor (x 0-31) --------------------------------------------------------------
put(3, 33, "P")
ice(6, 31)
gem((12, 31), (17, 31), (22, 31), (27, 31))

# --- 2. First lab (x 32-46): ground 32, a 2-block step up from the corridor -------------------
for x in (34, 39, 44):
    L.at("deco", x, 31, kind="glass")
put(36, 31, "W")
put(40, 31, "h")
put(43, 31, "m")
put(45, 31, "c")

# --- 3. Ramps and pucks (x 47-90) -------------------------------------------------------------
ice(47, 64)
L.at("kicker", 64, 33, dir="r", launch=14)
L.clear(65, 67, 34, 36)     # the pit (floor ground 37, not deadly)
L.col(67, 34, 36, "H")
ice(68, 70)
L.put(71, 33, "#")          # bumper
ice(72, 85)
L.put(86, 33, "#")          # goal bumper
L.at("goalvent", 86, 32, w=1, h=2)
put(56, 30, "1")            # letter G
L.at("puck_drone", 75, 33, "PD1")
L.at("puck_drone", 82, 33, "PD2")
put(88, 33, "h")
put(89, 33, "W")
put(90, 33, "c")
# The Nova shelves: one-way, 4 up from the lab, gaps of 2.
L.row(48, 63, 28, "=")
L.row(66, 73, 27, "=")
L.row(76, 86, 28, "=")
gem((50, 27), (53, 27), (56, 27), (59, 27), (62, 27),
    (67, 26), (69, 26), (72, 26),
    (77, 27), (80, 27), (83, 27), (85, 27))
put(70, 26, "$")            # the gem cache the kicker launches to

# --- 4. Pod hall (x 91-136) -------------------------------------------------------------------
# A bulkhead from the ceiling down to the catwalk: the way in is the floor.
L.fill(90, 90, 22, 27, "#")
# The floor is one row thick; the frost tunnel runs under it.
L.clear(91, 131, 35, 37)
L.clear(132, 134, 34, 37)
L.col(133, 34, 37, "H")
L.at("breakable", None, None, rect=(91, 34, 93, 34), hp=1, by="heavy", look="ice")
L.at("frost", None, None, rect=(91, 35, 131, 37), dmg=1, every=27)
L.fill(129, 131, 29, 34, "#")   # the east wall
# The catwalk: two decks, the ladder up through the east one.
L.fill(91, 117, 28, 29, "#")
L.fill(122, 136, 28, 29, "#")
L.clear(125, 127, 28, 29)
L.col(126, 28, 33, "H")
put(92, 33, "h")
for i, x in enumerate((95, 101, 107, 113, 119)):
    keys = dict(carrier=1) if x == 107 else {}
    L.at("sleeper_pod", x, 33, "SP%d" % (i + 1), **keys)
# The catwalk's west part: Turbo, merch, the DO NOT OPEN pod.
put(98, 27, "T")
put(104, 27, "m")
L.at("breakable", None, None, rect=(108, 25, 109, 27), hp=3, look="pod")
put(108, 25, "B")
put(109, 26, "2")           # letter U, inside the pod
gem((93, 27), (95, 27), (100, 27), (102, 27), (106, 27), (111, 27))

# --- 5. Curling lane (x 132-166) --------------------------------------------------------------
ice(141, 166)
L.at("armrail", None, None, x0=146, x1=170, y=20)
put(139, 33, "c")
put(140, 33, "h")
put(141, 33, "W")
L.at("sleeper_pod", 143, 33, "SP6")
L.row(142, 145, 28, "#")    # the camera shelf
L.at("breakable", None, None, rect=(144, 27, 144, 27), hp=2, look="ice")
put(144, 27, "C")
L.at("puck_drone", 150, 33, "PD3")
L.at("puck_drone", 155, 33, "PD4")
L.at("puck_drone", 160, 33, "PD5")
L.at("lab_arm", 150, 24, "LA1", rail="146,153")
L.at("lab_arm", 157, 24, "LA2", rail="154,161")
L.at("lab_arm", 165, 24, "LA3", rail="162,170")
for pid, x, arm in (("PX1", 163, "LA1"), ("PX2", 164, "LA2"), ("PX3", 165, "LA3")):
    L.at("powerbox", x, 33, pid, arm=arm)
put(150, 30, "3")           # letter N
gem((145, 31), (148, 31), (153, 31), (157, 31), (161, 31), (166, 31))

# --- 6. Cryo vault (x 167-189) ----------------------------------------------------------------
L.fill(188, 189, 0, H - 1, "#")
put(171, 33, "c")
put(174, 33, "h")
for x in range(175, 187, 2):
    if x != 177:
        L.at("deco", x, 33, kind="crewpod")
L.at("deco", 178, 33, kind="hostpod")
put(182, 33, "m")
L.row(183, 186, 30, "=")    # the vault shelf
L.at("breakable", None, None, rect=(184, 29, 184, 29), hp=1, look="ice")
put(184, 29, "Q")
L.at("deco", 181, 30, kind="42")
put(187, 33, "X")
gem((168, 31), (170, 31), (172, 31), (176, 30), (179, 30), (183, 28), (185, 28), (186, 31))

L.write(__file__, "16_cryo_labs.txt")
