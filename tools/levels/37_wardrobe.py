#!/usr/bin/env python3
"""Level 37 - Wardrobe (SPEC.md 37). Twist: Changing Booths.
Prototype: Decoy Launcher.

Left to right: the fitting room (start), the costume racks, then the makeup
hall, the sewing room and the archive, with the exit on a high ledge at the
archive's left end. Up inside a booth turns the runner into the one on its
sign; each runner keeps their own hearts. Heavy plates go down only under
Rocco (or a Turbo runner); spring racks throw each runner a different
height, under ceiling spikes. Everything is in world_wardrobe.cpp.

Changes from the spec, so it plays in this engine:
- The swap's 12-frame curtain is mercy frames; the Decoy Launcher and its
  ammo pass to the new runner (no per-runner starting kits). The C key's
  mid-level runner switch is off here: runners change in booths.
- Spring racks launch the runner when they arrive on one (walked on or
  landed), not on jump; a runner who tumbled off the ceiling spikes lands
  on the rack without being thrown again.
- Costume racks: the booth trio sits at x 39, 41, 43 so the spring rack
  at x 45-46 has room. The Rack Runner can't be ridden; it knocks you back.
- Sewing room: the cutting tables are one-way blocks; TB2 is x 118-121, so
  the ladder at x 122 keeps an open block on its left. PL2 is 1 block wide.
- Archive: the Dash spring rack is at x 146-147 under ledge 42, which runs
  x 140-145 (spikes rect 145,35,148,35); the push rack and the vault door
  are out in the open at x 152, so pushing the rack either way never shuts
  the route. A door at (144, 54) leads back out of the vault. The Mannequin
  waits at x 149.
- `@ step` lines are the bot's way through: the booths and plates in order.
"""

from lib import Level

W, H = 160, 56
L = Level(W, H, name="STAGE 37 - WARDROBE", episode=6, theme="wardrobe", music="theme_jazz_bass",
          weapon="decoy_launcher", par=420)


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gems(*pts):
    for x, y in pts:
        put(x, y, "g")


def booth(x, y, runner, **kw):
    """A changing booth, 2 x 3 blocks; (x, y) its bottom-left block."""
    for dy in range(3):
        for dx in range(2):
            assert L.get(x + dx, y - dy) in ".g", ("booth", x, y, L.get(x + dx, y - dy))
    L.at("booth", x, y, runner=runner, **kw)


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

# --- 1. Fitting room (0-36) ----------------------------------------------------------------------
L.ground(1, 9, 48)
L.ground(10, 28, 44)                             # Nova ledge (+4), stalls B and C
L.ground(29, 36, 38)                             # the spring ledge (+6)
L.fill(15, 24, 32, 33, "#")                      # the stall ceiling
L.at("shutter", None, None, "D0", rect=(21, 34, 21, 43))
L.row(25, 30, 31, "v")                           # ceiling spikes over the rack
put(2, 47, "P")
booth(5, 47, "nova")
booth(12, 43, "rocco")
put(16, 43, "W")
L.at("switch", 18, 43, "PL0", kind="plate", weight=2, latch=1, target="D0", w=2)
booth(23, 43, "dash")
L.at("rack", 27, 43, w=2)
put(8, 47, "m")
put(34, 37, "h")
gems((3, 45), (7, 45), (11, 41), (14, 41), (31, 36), (33, 36))

# --- 2. Costume racks (37-68) --------------------------------------------------------------------
L.ground(37, 68, 44)
L.fill(47, 66, 38, 39, "#")                      # the rack-top walkway (ground 38)
put(38, 43, "c")
booth(39, 43, "dash")
booth(41, 43, "rocco")
booth(43, 43, "nova")
L.at("rack", 45, 43, w=2)
for x in (50, 57, 63):
    L.at("mannequin", x, 43)
L.at("rackrunner", 66, 43, lane=(38, 67))
put(60, 43, "h")
L.at("suit", 62, 43)
put(53, 37, "m")
put(52, 37, "1")                                 # G
booth(64, 37, "toon")
L.at("curtain", None, None, rect=(60, 34, 66, 37))
L.at("deco", 54, 41, kind="42", text="SIZE 42", w=1, h=1)
gems((48, 37), (50, 37), (55, 37), (57, 37), (59, 37), (61, 37), (49, 35), (56, 35))

# --- 3. Makeup hall (69-100) ---------------------------------------------------------------------
L.ground(69, 89, 48)
L.row(75, 78, 44, "=")                           # F1
L.row(80, 83, 40, "=")                           # F2
L.row(85, 88, 36, "=")                           # F3
L.ground(90, 100, 32)                            # the top shelf
put(69, 47, "c")
booth(71, 47, "nova")
put(74, 47, "T")
put(76, 43, "W")
L.at("mirror", 77, 43)
L.at("powder_puffer", 80, 47)
L.at("powder_puffer", 86, 35)
L.at("powder_puffer", 98, 31, carrier=1)
booth(91, 31, "dash")
put(94, 31, "Q")
L.at("duck", 94, 31, costume="boa_top_hat")
put(95, 31, "h")
put(87, 35, "2")                                 # U
gems((73, 45), (76, 41), (78, 41), (81, 37), (83, 37), (85, 33), (88, 33), (90, 29), (92, 27), (96, 29))

# --- 4. Sewing room (101-130) --------------------------------------------------------------------
L.ground(101, 112, 48)
L.ground(113, 122, 52)                           # the needle pit
L.row(113, 120, 51, "^")
ladder(122, 48, 51)
L.ground(123, 130, 48)
L.at("table", None, None, "TB1", rect=(113, 40, 117, 40), to=48)
L.at("table", None, None, "TB2", rect=(118, 40, 121, 40), to=48)
put(101, 47, "c")
booth(102, 47, "rocco")
put(105, 47, "T")
L.at("switch", 110, 47, "PL1", kind="plate", weight=2, latch=1, target="TB1", w=2)
L.at("switch", 117, 47, "PL2", kind="plate", weight=2, latch=1, target="TB2", w=1, table="TB1")
L.at("mannequin", 125, 47)
put(124, 47, "h")
put(126, 47, "W")
put(128, 47, "c")
put(130, 47, "m")
gems((104, 45), (107, 45), (109, 45), (114, 46), (116, 46), (119, 46), (121, 46), (127, 45))

# --- 5. Archive (131-159) ------------------------------------------------------------------------
L.ground(131, 142, 48)
L.fill(143, 158, 48, 49, "#")                    # the floor over the vault
L.ground(143, 158, 55)                           # the vault's floor
L.at("shutter", None, None, "D3", rect=(141, 44, 141, 47))
L.fill(140, 145, 42, 43, "#")                    # ledge 42
L.fill(131, 138, 38, 39, "#")                    # the Nova ledge (ground 38)
L.row(145, 148, 35, "v")
booth(131, 47, "dash")
booth(133, 47, "rocco")
booth(135, 47, "nova")
L.at("switch", 138, 47, "PL3", kind="plate", weight=2, latch=1, target="D3", w=2)
booth(144, 47, "dash")
L.at("rack", 146, 47, w=2)
L.at("mannequin", 149, 47)
put(150, 47, "h")
L.at("pushrack", 152, 45, "PR1", w=2, h=3)
L.at("vaultdoor", 152, 47, to=(145, 54))
L.at("vaultdoor", 144, 54, to=(150, 47))
booth(142, 41, "nova")
booth(134, 37, "any", exit=1)
put(132, 37, "X")
put(150, 54, "3")                                # N
put(156, 54, "B")
gems((141, 40), (144, 40), (136, 36), (138, 36), (147, 53), (153, 53))

# The bot's way through.
for x, y, act, arg in [
        (5, 47, "booth", "nova"), (12, 43, "booth", "rocco"), (18, 43, "plate", "PL0"), (23, 43, "booth", "dash"),
        (71, 47, "booth", "nova"), (102, 47, "booth", "rocco"), (110, 47, "plate", "PL1"),
        (117, 47, "plate", "PL2"), (138, 47, "plate", "PL3"), (144, 47, "booth", "dash"),
        (142, 41, "booth", "nova")]:
    L.at("step", x, y, act=act, arg=arg)

L.header["tip"] = "PRESS UP IN A BOOTH TO BECOME THE RUNNER ON ITS SIGN. HEAVY PLATES NEED ROCCO"  # HINTS: the level's first 12 seconds
L.write(__file__, "37_wardrobe.txt")
