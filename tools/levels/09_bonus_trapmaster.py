#!/usr/bin/env python3
"""Level 9 bonus - Trapmaster (SPEC.md 09, "Bonus level").

Rule trapmaster: the runner is off the map. Left and right pan the camera
over the gallery, up and down move a glyph cursor along the 12 plates and
fire works the plate's trap (each re-arms 45 frames later). 30 treasure
hunters come in from the left in 6 waves, alternately on the upper floor
(ground 10) and the lower floor (ground 22), and walk to the idol at
(76, 21). Each one stopped scores 200: stop 20 of the 30 for the 4000
goal. A hunter that reaches the idol takes a coin and leaves; nothing is
lost. 90 seconds.

Each floor has two stones, two blades and two spike strips, each worked by
the plate just before it. The upper floor ends in a drop at x72 onto the
lower floor, by the idol.

Changes from the spec: hunters walking over a plate don't fire it here
(only the cursor does), so the plates sit right by their traps.
"""

from lib import Level

W, H = 80, 24
L = Level(W, H,
          name="BONUS - TRAPMASTER", episode=2, theme="sandstone_traps",
          music="bonus_trapmaster", rules="trapmaster", timer=90, goal="score:4000")

L.fill(0, W - 1, 0, H - 1, "#")
L.clear(0, 71, 5, 9)                         # upper floor, ground 10
L.clear(72, 78, 5, 21)                       # the drop by the idol
L.clear(0, 78, 17, 21)                       # lower floor, ground 22


def deco(kind, x, y, w=1, h=1, **keys):
    L.at("deco", x, y, kind=kind, w=w, h=h, **keys)


# Traps in the order the cursor walks them: upper floor left to right,
# then the lower floor.
traps = [
    # (plate x, floor ground, trap keys)
    (5, 10, dict(kind="spikes", rect=(7, 9, 11, 9))),
    (14, 10, dict(kind="blade", x=17)),
    (19, 10, dict(kind="stone", groove="21..34", row=9, dir="l")),
    (37, 10, dict(kind="spikes", rect=(39, 9, 44, 9))),
    (48, 10, dict(kind="blade", x=51)),
    (54, 10, dict(kind="stone", groove="56..70", row=9, dir="l")),
    (5, 22, dict(kind="spikes", rect=(7, 21, 11, 21))),
    (13, 22, dict(kind="stone", groove="15..30", row=21, dir="l")),
    (33, 22, dict(kind="blade", x=36)),
    (40, 22, dict(kind="spikes", rect=(42, 21, 47, 21))),
    (49, 22, dict(kind="stone", groove="51..66", row=21, dir="l")),
    (67, 22, dict(kind="blade", x=70)),
]
for i, (px, g, keys) in enumerate(traps):
    pid = "P%d" % (i + 1)
    L.at("plate", px, g, pid)
    keys = dict(keys)
    x = keys.pop("x", None)
    if keys["kind"] == "blade":
        L.at("trap", x, g, "T%d" % (i + 1), plate=pid, **keys)
    else:
        L.at("trap", None, None, "T%d" % (i + 1), plate=pid, **keys)
L.at("idol", 76, 21)
for x in (10, 30, 50, 66):
    deco("torch", x, 6, 1, 2)
    deco("torch", x, 18, 1, 2)
deco("text", 30, 2, 20, 1, text="UP/DOWN PICK A TRAP - FIRE TO SPRING IT")

L.write(__file__, "09_bonus_trapmaster.txt")
