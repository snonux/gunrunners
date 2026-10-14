#!/usr/bin/env python3
"""Level 38 - Writers' Room (SPEC.md 38). Twist: Strike It Out.
Prototype: Red Pen.

Three floors that zigzag upward: the desks (F1, ground 58) left to right,
the ladder up the right edge, the corkboard (F2, ground 40) right to left
along the red strings, the ladder up the left edge, and the script floor
(F3, ground 22) over the typewriter's bridge to the final draft. Words sit
on the things they name; a Red Pen shot strikes one out and the thing is
gone. Typos rewrite struck words; FLOOR and LADDER sketch themselves back.
Everything is in world_writers.cpp.

Changes from the spec, so it plays in this engine:
- Words sit at the height a standing runner's shot flies (a block above
  their feet), on or beside what they name: WALL (12, 56) on its wall,
  SPIKES (54, 53) over the pit, GUARD and BLOCK on their enemies' chests,
  WALL (121, 38), SECRET (4, 38), CRUSHED (115, 20) on the press's warning
  sign, LANCE WINS (134, 20) before its door. FLOOR (79, 55) and LADDER
  (145, 50), (12, 30) stay where only a jump or an up-shot reaches them.
- The sticky note is at row 56 and its W at (6, 55) so every runner reaches it.
- The FLOOR plank is one-way, x 74-85; the pit's ladder is x 86, rows 55-61.
- The filing cabinet: only its front (x 6, rows 36-39) is the SECRET rect;
  inside are U, two snacks and the snack drawer (the bonus door).
- No paper tray under the bridge: a runner who falls through the gap lands
  on the strings below and climbs back up by the left ladder.
- Keys are stomped (landed on) or shot; Typos don't un-type the bridge.
- The keyboard's Deadline and Writer's Block are left out (the keys are
  1-block steps they can't walk); the F3 Deadline patrols x 88-118.
- The press is x 120-123, its face resting at row 13 and slamming to 21.
- `@ step` lines are the bot's way through; the bot strikes words itself.
"""

from lib import Level

W, H = 150, 64
L = Level(W, H, name="STAGE 38 - WRITERS' ROOM", episode=6, theme="writers_room",
          music="theme_typewriter_clarinet", weapon="red_pen", par=420)


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gems(*pts):
    for x, y in pts:
        put(x, y, "g")


def ladder(x, top, bottom):
    """A ladder from the ledge row `top` down to `bottom`; the block to its
    left must never be solid."""
    L.col(x, top, bottom, "H")
    for y in range(top, bottom + 1):
        assert L.get(x - 1, y) in ".=^", (x, y, L.get(x - 1, y))


# The building: roof, outer walls.
L.fill(0, W - 1, 0, 1, "#")
L.fill(0, 0, 0, H - 1, "#")
L.fill(W - 1, W - 1, 0, H - 1, "#")
# F3's floor (rows 22-23) with the bridge gap; F2's floor (rows 40-41) open
# over the strings.
L.fill(1, 69, 22, 23, "#")
L.fill(86, W - 2, 22, 23, "#")
L.fill(1, 29, 40, 41, "#")
L.fill(120, W - 2, 40, 41, "#")

# --- F1: the doorway and the desks (left to right) ------------------------------------------
L.ground(1, 40, 58)
L.row(5, 7, 56, "=")                             # the sticky note
L.fill(12, 13, 42, 57, "#")                      # w1 WALL
L.at("word", 12, 56, "w1", text="WALL", rect=(12, 42, 13, 57))
put(2, 57, "P")
put(6, 55, "W")
gems((3, 56), (9, 56), (16, 56), (20, 56))

L.ground(41, 52, 55)                             # desk D1
L.ground(53, 58, 58)                             # the spike pit
L.row(53, 58, 57, "^")
L.at("word", 54, 53, "w2", text="SPIKES", rect=(53, 57, 58, 57))
L.ground(59, 70, 55)                             # desk D2
L.ground(71, 73, 55)                             # desk D3, left
L.row(74, 85, 55, "=")                           # w4 FLOOR, the plank
L.ground(74, 85, 62)                             # the pit under it
L.ground(86, 86, 62)
ladder(86, 55, 61)
L.ground(87, 90, 55)                             # desk D3, right
L.at("word", 79, 55, "w4", text="FLOOR", rect=(74, 55, 85, 55), load=1)
L.ground(91, 106, 55)                            # desk D4
L.ground(107, W - 2, 58)
L.put(144, 40, "=")                               # beside the ladder through the floor
L.put(144, 41, ".")
ladder(145, 40, 57)                              # w5 LADDER up to the corkboard
L.at("word", 145, 50, "w5", text="LADDER", rect=(145, 40, 145, 57), load=1)
put(36, 57, "h")
put(48, 54, "m")
L.at("sketch_guard", 64, 54)
put(93, 54, "c")
put(95, 54, "W")
L.at("penbox", 95, 54)
put(97, 54, "h")
put(103, 54, "Q")
L.at("duck", 103, 54, costume="glasses_pencil")
L.at("deadline", 100, 54, lane=(87, 106))
L.at("writersblock", 118, 57)
put(134, 57, "m")
gems((30, 56), (38, 56), (44, 53), (50, 53), (62, 53), (68, 53), (78, 53), (82, 53), (110, 56), (126, 56),
     (130, 56), (140, 56))

# --- F2: the corkboard (right to left) --------------------------------------------------------
PINS = [(119, 40), (104, 34), (90, 38), (75, 33), (60, 38), (45, 34), (30, 40)]
L.at("string", pins=[c for p in PINS for c in p])


def string_top(bx):
    """The string's top cell row over block bx (as linkWriters works it out)."""
    cx = bx * 2 + 1
    for (ax, ay), (bx2, by) in zip(PINS, PINS[1:]):
        x0, y0, x1, y1 = bx2 * 2 + 1, by * 2, ax * 2 + 1, ay * 2
        if x0 <= cx <= x1:
            return y0 + round((y1 - y0) * (cx - x0) / max(1, x1 - x0))
    return 80


def on_string(bx, up=0):
    """The block row a gem `up` blocks above a runner's feet on the string takes."""
    return (string_top(bx) - 3) // 2 - up


L.fill(121, 122, 36, 39, "#")                    # w6 WALL (4 blocks: Nova jumps it)
L.at("word", 121, 38, "w6", text="WALL", rect=(121, 36, 122, 39))
put(143, 39, "c")
put(134, 39, "h")
L.at("writersblock", 131, 39)
L.at("inkwell", 127, 39)
put(124, 39, "T")
L.at("penbox", 18, 39)
put(18, 39, "W")
put(20, 39, "c")
L.at("inkwell", 24, 39)
put(60, 36, "1")                                 # G, above the middle pin
L.at("word", 98, 30, "w8", text="EASTER EGG", target="egg")
L.at("card", 40, 26, text="LEVEL 43: THE TAX OFFICE")
L.at("card", 66, 25, text="LEVEL 44: JURY DUTY")
L.at("card", 108, 27, text="LEVEL 45: HOLD MUSIC")
# The filing cabinet at the far left (a dead end): its back and top stay,
# its front is the SECRET.
L.fill(2, 6, 35, 35, "#")
L.fill(2, 2, 36, 39, "#")
L.fill(6, 6, 36, 39, "#")
L.at("word", 4, 38, "w7", text="SECRET", rect=(6, 36, 6, 39))
put(3, 39, "2")                                  # U
put(4, 39, "m")
put(5, 39, "m")
put(4, 37, "B")
L.put(11, 22, "=")
L.put(11, 23, ".")
ladder(12, 22, 39)                               # w9 LADDER up to the script
L.at("word", 12, 30, "w9", text="LADDER", rect=(12, 22, 12, 39), load=1)
gems(*[(x, on_string(x)) for x in (112, 106, 100, 94, 86, 80, 70, 64, 55, 50, 40, 35)])
gems((138, 38), (128, 38))

# --- F3: the typewriter, then the final draft (left to right) ------------------------------------
# The top shelf (a Nova line): the original pitch. A staircase of steps 3
# blocks up and 4 across (4 up was more than Nova jumps, and a step right
# over the one below it bumps her head).
L.row(2, 3, 19, "#")
L.row(6, 7, 16, "#")
L.row(10, 11, 13, "#")
L.row(14, 16, 10, "#")
L.at("pitch", 15, 9)
KEYS = "ASDGERIPB "
for i, ch in enumerate(KEYS):
    kx = 28 + 4 * i
    L.row(kx, kx + 1, 21, "#")
    L.at("key", kx, 21, ch="SPACE" if ch == " " else ch)
L.at("bridge", rect=(70, 22, 85, 22), text="A BRIDGE APPEARS")
L.at("inkwell", 24, 21)
put(26, 21, "h")
put(55, 17, "3")                                 # N
put(67, 21, "m")
gems((16, 20), (20, 20), (31, 19), (39, 19), (47, 19), (59, 19), (63, 19), (68, 20), (6, 14), (15, 8))

put(88, 21, "c")
put(90, 21, "W")
L.at("penbox", 90, 21)
put(100, 21, "h")
L.row(96, 112, 19, "=")                          # script lines (optional)
L.row(100, 110, 16, "=")
L.at("deadline", 110, 21, lane=(88, 118))
L.at("crusher", 120, 13, w=4, to=21)
L.at("word", 115, 20, "w10", text="CRUSHED", target="crusher")
L.at("inkwell", 132, 21)
L.fill(140, 141, 16, 21, "#")                    # the LANCE WINS door
L.at("word", 134, 20, "w11", text="LANCE WINS", rect=(140, 16, 141, 21))
put(146, 21, "X")
L.at("deco", 128, 14, kind="42", text="PAGE 42", w=1, h=1)
gems((98, 18), (102, 18), (106, 18), (104, 15), (108, 15), (126, 20), (144, 20), (147, 18))

# The bot's way through (the words in the way it strikes as it comes to them).
for x, y, act, d in [
        (8, 57, "go", 1), (50, 54, "go", 1), (68, 54, "go", 1), (92, 54, "go", 1), (112, 57, "go", 1),
        (142, 57, "go", 1), (142, 39, "go", -1), (126, 39, "go", -1), (60, 37, "go", -1), (20, 39, "go", -1),
        (15, 21, "go", 1), (27, 21, "type", 1), (92, 21, "go", 1), (112, 21, "go", 1), (130, 21, "go", 1)]:
    L.at("step", x, y, act=act, dir=d)

L.write(__file__, "38_writers_room.txt")
