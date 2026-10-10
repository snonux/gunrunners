#!/usr/bin/env python3
"""Level 10 bonus - Negative Space (SPEC.md 10, "Bonus level").

Rule negative: every 75 frames the sun and the moon swap, and so do the
sun blocks (white marble) and the moon blocks (night blue): whichever is
there goes, whichever was gone comes back. For the last 15 frames before a
swap the blocks about to come shimmer and the sun/moon icon at the top of
the screen turns. A block never comes back on top of the runner: it waits
until you are out of it. 90 seconds to reach the exit.

- x1-26: a hall of walls, sun and moon in turn: each is only gone half
  the time.
- x27-53: marble columns over a shallow pit, the gaps between them floored
  in one phase or the other (the pit has a ledge to climb back out).
- x54-78: a climb to the exit, every other ledge a sun or moon ledge, each
  2 blocks above the last.

Changes from the spec: only the sun and moon blocks swap (everything else
in the frame stays put), so the level is built from those blocks; the
gems are all visible in both phases.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - NEGATIVE SPACE", episode=2, theme="marble_observatory",
          music="bonus_negative", rules="negative", timer=90, goal="exit")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(1, W - 2, 1, 21)       # one big hall, ground 22


def phase(lid, x0, y0, x1, y1, moon):
    """A block of sun (or moon) tiles: solid for 75 frames, gone for 75."""
    L.at("layer", None, None, lid, rect=(x0, y0, x1, y1), driver="timer", on=75, off=75,
         phase=75 if moon else 0, style="moon" if moon else "sun")


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


L.on(2, 22, "P")

# --- x1-26: walls in turn ----------------------------------------------------------------
phase("WA", 7, 17, 7, 21, False)
phase("WB", 12, 17, 12, 21, True)
phase("WC", 17, 17, 17, 21, False)
phase("WD", 22, 17, 22, 21, True)
L.fill(25, 26, 20, 21, "#")    # a step up to the columns
gem((4, 21), (5, 21), (9, 21), (10, 21), (14, 21), (15, 21), (19, 21), (20, 21), (24, 20), (26, 19))

# --- x27-53: columns, and the gaps between them --------------------------------------------
for x0, x1 in ((27, 28), (33, 34), (39, 40), (45, 46), (51, 53)):
    L.fill(x0, x1, 18, 21, "#")
for i, (x0, x1) in enumerate(((29, 32), (35, 38), (41, 44), (47, 50))):
    phase("G%d" % i, x0, 18, x1, 18, i % 2 == 1)
    L.row(x0 + 1, x0 + 2, 20, "=")  # a ledge to climb back out of the pit
    gem((x0 + 1, 17), (x0 + 2, 16), (x0 + 3, 17))
gem((28, 17), (34, 17), (40, 17), (46, 17))

# --- x54-78: the climb ----------------------------------------------------------------------
L.fill(54, 55, 20, 21, "#")    # back up to the columns from the floor
ledges = [  # x0, x1, row, kind (None: always there)
    (55, 57, 16, "sun"),
    (58, 60, 14, None),
    (61, 63, 12, "moon"),
    (64, 66, 10, None),
    (67, 69, 8, "sun"),
    (70, 72, 6, None),
]
for i, (x0, x1, row, kind) in enumerate(ledges):
    if kind:
        phase("C%d" % i, x0, row, x1, row, kind == "moon")
    else:
        L.row(x0, x1, row, "#")
    gem((x0 + 1, row - 1))
L.row(73, 78, 4, "#")          # the exit ledge
L.on(77, 4, "X")
gem((74, 3), (75, 3), (76, 2), (60, 21), (66, 21), (72, 21), (76, 21), (3, 20))

L.write(__file__, "10_bonus_negative.txt")
