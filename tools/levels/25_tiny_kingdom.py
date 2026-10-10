#!/usr/bin/env python3
"""Level 25 - Tiny Kingdom (SPEC.md 25). Twist: Time-Lapse. Prototype:
Chrono Mister.

A walk along the soil (ground 44) of a back garden at ant height: the sunny
patch, the lawn with its hollow stem, the ditch and the ant pantry under
it, the flowerbed pond (day flowers or dusk mushrooms across it), up and
over the compost heap, to the dandelion that carries the runner up to the
exit on a petal ledge.

The garden's 810-frame day/dusk cycle is the `day_N` and `dusk_N` timer
layers (world_garden.cpp keeps the sundial, the sweep and the pond).

Changes from the spec, so it plays in this engine:
- The pantry's root ladder is at x 79 (rows 44-48), not 78 up through the
  anthill: a ladder shaft needs the block left of it open, so the anthill
  moves to x 76-77 and AC2 marches x 64-75 into it. The ladder comes out
  in the lawn beside the anthill; x 78 is a one-way hatch beside it.
- The seed head is `mode=pingpong wait=rider`, hidden until the dandelion
  goes to seed, instead of `mode=once`: it waits at the dandelion's foot
  for the runner (comes back down when called from there) and parks up
  beside the petal ledge, straight up from its foot; the ledge reaches
  from x 180.
- N is at (152, 32), on the dusk mushroom over the heap's 36 step (row 33
  is the mushroom itself).
- B, the bonus door, stands on the heap at (156, 32); the ladybug in front
  of it covers x 156-157, rows 30-32.
- Turbo's stalk-climbing hook is left out.
"""

from lib import Level

W, H = 190, 50
L = Level(W, H,
          name="STAGE 25 - TINY KINGDOM", episode=4, theme="macro_garden",
          music="theme_string_quartet", weapon="chrono_mister", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def day(n, x0, y0, x1, y1):
    L.at("layer", None, None, "day_%d" % n, rect=(x0, y0, x1, y1), tile="=", driver="timer",
         on=405, off=405, phase=0, color="pink")


def dusk(n, x0, y0, x1, y1):
    L.at("layer", None, None, "dusk_%d" % n, rect=(x0, y0, x1, y1), tile="=", driver="timer",
         on=405, off=405, phase=405, color="cyan")


# --- 1. Sunny patch (x 0-30) -----------------------------------------------------------------------
L.ground(0, 30, 44)
day(0, 14, 40, 18, 40)
put(3, 43, "P")
put(24, 43, "W")
put(20, 43, "m")
put(27, 43, "h")
gem((7, 43), (10, 43), (15, 39), (17, 39), (13, 43), (19, 43))
L.at("narrate", None, None, "n1", rect=(0, 30, 10, 43), runner="any",
     line="Dawn in the tiny kingdom. Something stirs.")

# --- 2. Lawn (x 31-80) -----------------------------------------------------------------------------
L.ground(31, 49, 44)
L.row(38, 48, 40, "#")                      # the hollow stem's roof (inner rows 41-43)
L.ground(58, 59, 44)                        # the ditch's east bank, walling off the pantry
L.ground(60, 80, 49)                        # the ant pantry's floor
L.ground(62, 77, 44, 44)                    # the lawn over the pantry
L.row(80, 80, 44, "#")
L.row(78, 78, 44, "=")                      # a hatch beside the root ladder
L.col(79, 44, 48, "H")                      # the root ladder, out of the pantry
L.fill(76, 77, 42, 43, "#")                 # the anthill
L.ground(50, 57, 48)                        # the ditch (floor 48)
day(1, 50, 44, 57, 44)                      # a leaf over it by day
dusk(1, 53, 45, 55, 45)                     # a mushroom in it by dusk
L.at("antcolumn", None, None, "AC1", path=((38, 43), (48, 43)), count=5, period=300)
L.at("antcolumn", None, None, "AC2", path=((64, 43), (75, 43)), count=5, period=300, carrier=1)
L.at("bombardier_beetle", 63, 43, "BB1")
L.at("deco", 45, 41, kind="42", text="LOT 42")
put(32, 43, "c")
put(80, 43, "h")
put(70, 48, "G")
put(74, 48, "$")
put(66, 48, "m")
gem((34, 43), (36, 43), (41, 43), (44, 43), (52, 43), (55, 43), (65, 43), (72, 43))
gem((61, 48), (62, 48), (63, 48), (64, 48), (68, 48), (69, 48), (72, 48), (73, 48), (76, 48), (77, 48))

# --- 3. Flowerbed (x 81-130) -----------------------------------------------------------------------
L.ground(81, 89, 44)
L.ground(90, 121, 49)                       # the pond's bed
L.ground(122, 130, 44)
L.at("fluid", kind="pond", rect=(90, 45, 121, 48))
for n, (x0, y) in enumerate(((93, 42), (99, 41), (105, 42), (111, 41), (117, 42)), start=2):
    day(n, x0, y, x0 + 2, y)
for n, x0 in enumerate((92, 98, 104, 110, 116), start=2):
    dusk(n, x0, 43, x0 + 2, 43)
for x in (96, 102, 108, 114):
    L.at("bud", x, 39, w=3)
L.at("seed", x=86, y=43, h=24)              # the Nova line: a stalk up to the leaf
for y in (40, 36, 32, 28, 24):
    L.put(87, y, "=")                       # nubs on the stalk
L.row(87, 123, 20, "=")                     # the leaf over the pond
L.at("antcolumn", None, None, "AC3", path=((124, 43), (130, 43)), count=5, period=300)
L.at("bombardier_beetle", 127, 43, "BB2", patrol=(127, 130))
L.at("prize", kind="rare_butterfly", path=((118, 30), (142, 22), (130, 26)), speed="1/2", score=10000)
L.at("narrate", None, None, "n2", rect=(86, 30, 89, 43), runner="nova",
     line="The female Nova approaches the watering hole with caution.")
L.at("narrate", None, None, "n3", rect=(86, 30, 89, 43), runner="dash",
     line="The male Dash drinks in the view. Not the water.")
L.at("narrate", None, None, "n4", on="hurt", runner="rocco",
     line="The male Rocco has eaten a hazard. He is unbothered.")
put(82, 43, "c")
put(84, 43, "W")
put(112, 37, "U")
put(124, 43, "h")
gem((94, 41), (100, 40), (106, 41), (112, 40), (118, 41), (92, 42), (98, 42), (104, 42), (110, 42), (116, 42))
gem((92, 19), (96, 19), (100, 19), (104, 19), (108, 19), (112, 19), (116, 19), (120, 19))

# --- 4. Compost heap (x 131-165) -------------------------------------------------------------------
L.ground(131, 139, 44)
L.ground(140, 144, 42)
L.ground(145, 149, 39)
L.ground(150, 154, 36)
L.ground(155, 159, 33)
L.ground(160, 165, 44)
dusk(7, 142, 39, 143, 39)
dusk(8, 152, 33, 153, 33)
L.at("seed", x=139, y=43, h=6)              # a stalk to the gem line at row 37
L.at("jumping_spider", 147, 38, "JS1")
L.at("jumping_spider", 157, 32, "JS2")
L.at("bombardier_beetle", 163, 43, "BB3")
L.at("ladybug", 156, 30)
L.at("narrate", None, None, "n5", rect=(131, 20, 135, 43), runner="any", line="Dusk. The hunters wake.")
put(131, 43, "c")
put(133, 43, "T")
put(138, 43, "W")
put(152, 32, "N")
put(156, 32, "B")
put(158, 32, "m")
put(162, 43, "h")
gem((139, 37), (140, 37), (141, 37), (142, 41), (147, 38), (151, 35), (154, 35), (164, 43))

# --- 5. Dandelion (x 166-189) ----------------------------------------------------------------------
L.ground(166, W - 1, 44)
L.row(180, W - 1, 6, "=")                   # the petal ledge
L.at("dandelion", None, None, "dl", x=176, y=43)
L.at("platform", 178, 43, "seedhead", w=2, path=((178, 43), (178, 6)), speed="1/2",
     mode="pingpong", wait="rider")
L.at("narrate", None, None, "n6", on="seedhead", runner="any", line="Remarkable.")
put(166, 43, "c")
put(168, 43, "W")
put(170, 43, "h")
put(186, 5, "X")
put(188, 43, "Q")
gem((179, 38), (179, 32), (179, 26), (179, 20), (179, 14), (181, 5))

L.write(__file__, "25_tiny_kingdom.txt")
