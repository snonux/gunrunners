#!/usr/bin/env python3
"""Level 14 - The Idol Awakens (SPEC.md 14). Twist: Gold Fever. Prototype: Jade Bow.

Left to right through the golden halls (ground 30, ceiling row 14) to the
round arena at the right end, where Kaan-Tolok, the Idol Golem, sits.

Gold Fever: every gem collected here fills the greed meter (HUD, 0-99).
Holding up at an offering altar (`@ altar kind=offer`) gives a gem back
every 4 frames; the false altar opens a trapdoor into the treasury, or
with no gems held, the bonus entrance. The golem starts the fight with an
armor plate over its chest for every 10 on the meter (6 at most); win with
the meter at 10 or more and every gem of the level counts double.

Changes from the spec, so the route plays as intended in this engine:
- The treasury's ladder comes up through a 2-block shaft (x 74-75, the
  ladder in the right column), as every ladder shaft does.
- Corridor C2 ends at x 118: the 2-block shaft x 119-120 drops from it to
  the Rocco corridor's end, where the step up to the last altar's ledge
  (ground 28) has headroom. The masters of GS2 and GS3 are carved in it.
- The arena door is at x 150, in the arena's west wall (rows 27-29).
- The second drummer stands at x 72, clear of the treasury shaft, and the
  false altar's trapdoor is 6 blocks wide (x 62-67) so it swallows the
  runner wherever it stands on the altar.
- The gem over the trapdoor sits at row 25, out of reach on the walk back
  from A2 (picking it up would undo the offering for the bonus door).
- No "MADE IN NEON CITY" sign: the golem's back carries the text itself.
"""

from lib import Level

W, H = 190, 40
L = Level(W, H,
          name="STAGE 14 - THE IDOL AWAKENS", episode=2, theme="gold_sanctum",
          music="theme_war_drums", weapon="jade_bow", par=420, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


# --- The halls -------------------------------------------------------------------------
room(1, 149, 15, 29)                 # golden halls, ground 30
# --- 1. Golden hall (x 0-40) ----------------------------------------------------------
for x in (8, 14, 20, 26, 32):
    L.row(x, x + 1, 26, "=")         # pillar capitals (the Nova line)
L.put(2, 29, "P")
L.put(6, 29, "h")
L.put(10, 29, "W")
L.at("altar", 20, 29, "A1", kind="offer")
for x in (25, 30, 35):
    L.at("coinheap", x, 29, waves=2, count=2)
L.at("deco", 26, 28, kind="42", text="42", w=1, h=1)
L.put(38, 29, "m")
L.put(40, 29, "c")
gem((4, 27), (12, 27), (9, 25), (15, 25), (21, 25), (27, 25), (33, 25), (17, 28), (23, 27), (37, 27))

# --- 2. Drummer hall (x 41-80) and the treasury (secret 1) ------------------------------
for x in (48, 58, 68):
    L.row(x, x + 1, 26, "=")
L.fill(70, 79, 22, 23, "#")          # the incense loft (secret 2), ground 22
L.put(45, 25, "2")                   # letter U
for x in (45, 60, 70):
    L.at("coinheap", x, 29, waves=2, count=2)
L.put(48, 28, "h")
L.at("drummer", 52, 29)
L.at("drummer", 72, 29)          # on its dais, clear of the treasury shaft
L.at("altar", 64, 29, "FA", kind="false", trapdoor=(62, 30, 67, 31))
L.put(61, 27, "B")
L.at("altar", 78, 29, "A2", kind="offer")
L.put(80, 29, "c")
L.put(75, 21, "$")
L.put(78, 21, "m")
gem((43, 27), (49, 25), (54, 27), (56, 27), (59, 25), (66, 25), (69, 25), (72, 21), (73, 21), (76, 21), (71, 26),
    (77, 26))
room(58, 74, 32, 38)                 # 2a. the priests' treasury, ground 39
room(74, 75, 30, 31)                 # its ladder shaft
L.col(75, 30, 38, "H")
L.put(66, 38, "1")                   # letter G
L.put(60, 38, "$")
L.put(72, 38, "$")
L.put(62, 38, "m")
gem(*[(x, 38) for x in (59, 61, 63, 64, 65, 67, 68, 69, 70, 71, 73)],
    *[(x, 37) for x in (59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73)],
    *[(x, 35) for x in (61, 63, 65, 67, 69)])

# --- 3. Sentinel corridors (x 81-120) --------------------------------------------------
L.fill(81, 97, 15, 23, "#")          # rock over C1
L.fill(98, 120, 15, 19, "#")         # rock over C2
room(98, 100, 20, 26)                # the junction up to C2
L.fill(86, 87, 27, 29, "#")          # C1's niches, ground 27
L.fill(93, 94, 27, 29, "#")
L.row(98, 100, 27, "=")              # one-way ledge up to C2
L.row(101, 118, 26, "#")             # C2's floor slab, ground 26 (the Rocco corridor's ceiling)
L.fill(106, 107, 23, 25, "#")        # C2's niches, ground 23
L.fill(113, 114, 23, 25, "#")
room(119, 120, 20, 29)               # the shaft at C2's end
L.put(84, 29, "W")
L.at("sentinel", 96, 28, "GS1", row=(82, 95, 28))
L.put(104, 25, "h")
L.put(110, 20, "3")                  # letter N
L.at("sentinel", 119, 24, "GS2", row=(101, 118, 25))
L.at("sentinel", 119, 28, "GS3", row=(98, 118, 29), heads=3)
gem((83, 27), (89, 27), (91, 27), (96, 25), (103, 24), (109, 24), (111, 24), (116, 24), (102, 28), (110, 28))

# --- 4. Last altar (x 121-149) ---------------------------------------------------------
L.ground(121, 130, 28, 29)
L.put(122, 27, "c")
L.put(125, 27, "W")
L.put(128, 27, "m")
L.put(133, 29, "Q")
L.put(135, 29, "h")
L.at("altar", 138, 29, "A3", kind="offer")
L.put(138, 28, "T")
L.at("refill", 144, 29)
L.put(147, 29, "c")
gem((123, 25), (127, 25), (129, 25), (132, 27), (136, 27), (141, 27), (143, 27), (146, 27))

# --- 5. The arena (x 150-189) ----------------------------------------------------------
room(151, 188, 8, 29)
room(150, 150, 27, 29)               # the arena door
L.at("golem", 166, 20, "kaan_tolok", arena=(151, 8, 188, 29), door=(150, 27, 150, 29), exit=(170, 29))
L.put(168, 21, "C")
L.put(170, 29, "X")                  # the exit, in the idol's mouth once it falls
gem((155, 27), (158, 25), (162, 27), (177, 27), (181, 25), (185, 27))

L.write(__file__, "14_idol_awakens.txt")
