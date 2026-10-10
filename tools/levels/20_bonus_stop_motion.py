#!/usr/bin/env python3
"""Level 20 bonus - Stop Motion (SPEC.md 20).

rules=stop_motion: the world (enemies, shots, hazards, pulse rings) moves one
frame only on frames when the runner moves (any direction held, mid-jump,
mid-fall); standing still freezes everything. The timer counts real time.
timer=90, goal=exit.

- x 0-26: a slow pulse ring going out from a little core over the walkway;
  its arcs above and below are solid, the sides are open, so the walkway
  threads through them (close under the core the arcs still catch you).
- x 27-52: four turrets whose shots hang in a lattice.
- x 53-79: Isotope Imps frozen mid-stride on narrow ledges over a spike pit,
  the exit at (77, 20).

Changes from the spec, so it plays in this engine:
- The pulse ring is `@ pulse` with speed=1/4 (a quarter block a frame),
  gaps= for its open sides and reach=13 blocks, so it stays in its section.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - STOP MOTION", episode=3, theme="station_reactor",
          music="bonus_stop_motion", par=90, flags="",
          rules="stop_motion", timer=90, goal="exit")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


L.fill(0, W - 1, 0, 1, "#")
L.fill(0, W - 1, 22, 23, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")

# --- 1. The frozen ring (x 1-26) ---------------------------------------------------------------------
L.fill(1, 26, 14, 21, "#")                       # the walkway's bed: it runs at row 13
put(2, 13, "P")
L.at("pulse", 13, 9, period=40, damage=1, speed="1/4", reach=13, gaps="120-240,300-60")
L.at("deco", None, None, kind="core", rect=(12, 8, 14, 10))
L.fill(5, 8, 9, 9, "=")                          # ledges in the open sides
L.fill(18, 21, 9, 9, "=")
for x in (4, 6, 8, 18, 20, 22, 25):
    put(x, 13, "g")
for x in (6, 19):
    put(x, 8, "g")

# --- 2. The lattice (x 27-52) ------------------------------------------------------------------------
L.fill(27, 30, 16, 21, "#")                      # steps down to the floor (row 21)
L.fill(31, 34, 18, 21, "#")
for tx, ty in ((35, 11), (40, 9), (45, 13), (50, 9)):
    L.fill(tx - 1, tx + 1, 2, ty - 1, "#")       # stubs from the ceiling
    L.put(tx, ty, "t")
for x, y in ((29, 15), (33, 17), (37, 20), (41, 20), (44, 20), (50, 20), (52, 20), (42, 17), (48, 17)):
    put(x, y, "g")

# --- 3. The imps on their ledges (x 53-79) -----------------------------------------------------------
L.fill(56, 73, 21, 21, "^")                      # the spike pit
L.fill(56, 59, 18, 18, "=")
L.fill(63, 66, 16, 16, "=")
L.fill(70, 73, 18, 18, "=")
L.at("isotope_imp", 65, 15, "II1")
L.at("isotope_imp", 72, 17, "II2", hat=1)
for x, y in ((54, 20), (57, 17), (59, 17), (64, 15), (71, 17), (75, 20)):
    put(x, y, "g")
put(77, 20, "X")

L.write(__file__, "20_bonus_stop_motion.txt")
