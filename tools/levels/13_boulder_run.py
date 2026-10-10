#!/usr/bin/env python3
"""Level 13 - Boulder Run (SPEC.md 13). Twist: The Boulder. Prototype: Fan Darts.

Long and thin, left to right: one corridor (ground 22, ceiling row 9) in
four chases split by four alcoves, from the start alcove out of the temple
mouth to the exit under the cliff edge. The ceiling ends at x 250, where
the sunset sky opens.

Boulders (`@ boulder`, 7 x 7 blocks) wait in the ceiling hole over each
alcove and drop, after 15 frames of dust, when the runner crosses their
trigger line: a 10-block lead. They roll at 3/4 cell a frame (7/8 in chase
4) along the floor, bridging holes and riding steps, and crush what they
touch. A runner in an alcove's shelter is under the roll line and safe.
The boulder of the previous chase rolls over the alcove and drops into its
chute (`@ chute`), whose flaps open for it and shut 15 frames later.
While the runner has the Virus the boulders wait in their holes.

Changes from the spec, so the route plays as intended in this engine:
- The crack room (secret 1) is under the corridor beside A2, entered from
  above: the crack is in the shelter's floor (x 80-81), the room x 70-79
  rows 24-28 (ground 29) with a step (ground 27) under the crack; climbing
  out is +2 and +2. Stand in the shelter's east half to wait safely.
  A runner 5 cells tall cannot pass a 2-block wall gap beside the shelter.
- The snake tunnel is a block lower (rows 24-26, ground 27) so the floor
  over it is two blocks thick and the x 119 snake's hole has a bottom.
- The tunnel's ladder is at x 129 (its shaft x 128-129 is a hole in the
  floor to hop), so it does not cut the ground-20 step at x 122-127.
- The side room's ladder is at x 205 (shaft x 204-205) and both its floor
  hatch (x 199-200) and its ladder hatch are `@ leadhatch`; the floor hatch
  wants a 25-block lead (Turbo's double speed gets it), both stay shut
  while a boulder is within 8 blocks.
- BD3 carries the candid camera and stops over A4 for 90 frames.
"""

from lib import Level

W, H = 280, 30
L = Level(W, H,
          name="STAGE 13 - BOULDER RUN", episode=2, theme="collapsing_temple",
          music="theme_orchestral_panic", weapon="fan_darts", par=300, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def on(x, g, c):
    assert L.get(x, g - 1) == ".", (x, g - 1, L.get(x, g - 1))
    L.on(x, g, c)


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def alcove(name, a, hole=True):
    """A 4-block shelter at ground 25 (x a..a+3) and its step at ground 24 (a+4..a+5)."""
    room(a, a + 3, 22, 24)
    room(a + 4, a + 5, 22, 23)
    if hole:
        room(a, a + 6, 3, 9)       # the ceiling hole a boulder waits in
    L.at("alcove", a, 25, name)


def chute(name, x0, x1):
    """An 8-block shaft under the floor; its flaps are the floor row."""
    room(x0, x1, 23, H - 1)
    L.at("chute", None, None, name, x0=x0, x1=x1, row=22)


# The corridor, and the sky past the temple mouth.
room(0, 265, 10, 21)
room(250, W - 1, 0, 21)
room(266, W - 1, 22, H - 1)
L.fill(266, 266, 25, H - 1, "#")    # the exit shelter's east wall

# --- 0. Wrong way (x 0-11) and 1. Alcove A1, the start (x 12-17) ----------------------------
L.at("deco", kind="text", x=1, y=18, w=3, h=1, text="WRONG WAY")
L.put(3, 21, "Q")
alcove("A1", 12)
chute("K1", 22, 29)
L.put(14, 24, "P")
L.put(13, 24, "h")
L.put(15, 24, "W")
L.at("boulder", 0, 15, "BD0", size=7, speed="3/4", wake="A1", delay=45, chute="K1")

# --- 2. Chase 1 (x 18-79) ----------------------------------------------------------------------
L.at("boulder", 12, 3, "BD1", size=7, speed="3/4", trigger=28, chute="K2", teeter="A2")
for x in (34, 55, 74):
    L.at("spearrunner", x, 21)
L.put(45, 18, "1")                 # letter G
L.at("deco", 40, 20, kind="42", text="42", w=1, h=1)
on(60, 22, "m")
gem((20, 20), (24, 19), (31, 20), (37, 20), (42, 19), (46, 20), (50, 20), (53, 19), (58, 20), (63, 20),
    (66, 19), (69, 20), (72, 20), (77, 20))

# --- 3. Alcove A2 (x 80-85) and the crack room (secret 1) ---------------------------------------
alcove("A2", 80)
chute("K2", 90, 97)
L.put(82, 24, "c")
L.put(83, 24, "h")
L.put(87, 17, "B")
room(70, 79, 24, 28)               # the crack room, ground 29
room(80, 81, 26, 26)
L.fill(80, 81, 27, 28, "#")        # the step under the crack (ground 27)
L.at("crack", rect=(80, 25, 81, 25), alcove="A2", still=30)
L.put(72, 28, "3")                 # letter N
L.put(75, 26, "V")                 # the Virus over the gem pile
L.put(76, 28, "$")
gem((71, 28), (73, 28), (74, 28), (75, 28), (77, 28), (73, 27), (74, 27), (76, 27), (71, 26), (72, 26),
    (73, 26), (74, 26))

# --- 4. Chase 2 (x 86-147): the steps, the snake tunnel ---------------------------------------
L.at("boulder", 80, 3, "BD2", size=7, speed="3/4", trigger=96, chute="K3")
L.ground(98, 103, 20, 21)
L.ground(104, 109, 18, 21)
L.ground(110, 115, 20, 21)
L.ground(122, 127, 20, 21)
room(100, 101, 20, 23)             # the tunnel's hole
room(100, 129, 24, 26)             # the snake tunnel, ground 27
room(128, 128, 22, 26)
L.col(129, 22, 26, "H")            # its ladder back up
for x, g in ((107, 18), (113, 20), (119, 22), (125, 20), (137, 22)):
    L.put(x, g, ".")               # a snake's hole
    L.at("pitsnake", x, g)
for x in range(104, 125, 4):
    L.put(x, 27, ".")
    L.at("pitsnake", x, 27)
gem((96, 20), (99, 18), (102, 18), (105, 16), (108, 16), (111, 18), (114, 18), (117, 20), (120, 20),
    (123, 18), (126, 18), (133, 20))

# --- 5. Alcove A3 (x 148-153) -------------------------------------------------------------------
alcove("A3", 148)
chute("K3", 158, 165)
L.put(148, 24, "T")
L.put(149, 24, "h")
L.put(150, 24, "c")
L.put(151, 24, "W")

# --- 6. Chase 3 (x 154-215): totems, the side room (secret 2) ----------------------------------
L.at("boulder", 148, 3, "BD3", size=7, speed="3/4", trigger=164, chute="K4", halt=216, camera=1)
L.put(149, 5, "C")                 # the candid camera on BD3
for x in (168, 189, 210):
    L.at("totem", x, 18, heads=4)
room(195, 205, 23, 27)             # the side room, ground 28
room(204, 204, 22, 22)
L.col(205, 22, 27, "H")
L.at("leadhatch", rect=(199, 22, 200, 22), lead=25, near=8)
L.at("leadhatch", rect=(204, 22, 205, 22), lead=0, near=8, ladder=205)
L.put(197, 27, "2")                # letter U
on(200, 28, "m")
on(202, 28, "m")
gem((196, 26), (198, 26), (199, 25), (201, 25), (203, 26), (196, 24), (198, 24), (202, 24))
gem((157, 20), (161, 20), (165, 19), (172, 20), (178, 20), (183, 19), (193, 20), (198, 20), (205, 19), (213, 20))

# --- 7. Alcove A4 (x 216-221) -------------------------------------------------------------------
alcove("A4", 216)
chute("K4", 226, 233)
L.put(217, 24, "h")
L.put(218, 24, "c")
L.put(219, 24, "W")

# --- 8. Chase 4 (x 222-265): downhill to the cliff, the exit under the overhang -----------------
L.at("boulder", 216, 3, "BD4", size=7, speed="7/8", trigger=232)
L.ground(234, 241, 23, 23)         # the floor steps down (rock under it stays)
room(234, 241, 22, 22)
room(242, 265, 22, 23)
room(254, 265, 26, 28)             # the exit shelter, ground 29
room(256, 257, 24, 25)             # the drop hole
L.at("spearrunner", 238, 22)
L.put(262, 28, "X")
L.put(264, 28, "m")
gem((224, 20), (228, 20), (232, 20), (236, 21), (240, 21), (244, 22), (248, 22), (252, 22), (258, 27), (260, 27))

L.write(__file__, "13_boulder_run.txt")
