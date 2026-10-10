#!/usr/bin/env python3
"""Level 15 - Hangar Bay (SPEC.md 15). Twist: Breach and Vent. Prototype: Breach Charge.

Episode 3 opens on Station Zero. A hub: the training bay in the top-left
corner, the lift down to the hangar floor, east past three docked shuttles,
up three gantries against the east hull, across to the control tower, up it
to the control cab under the roof's bay doors, and west along the gangway
through a docked shuttle to the exit at the top-left.

Breach and Vent: a hull panel (`@ panel`, yellow-black frame) opens only to
a Breach Charge blast. For 60 frames its vent (`@ vent rect=`) pulls every
loose thing inside the rect toward it (enemies, crates, items, the runner
unless holding a `@ rail` or a ladder) and out into space; then the shutter
slams. A runner who reaches an open panel is caught by its frame until it
shuts, except at the bonus panel, which lets them through.

Changes from the spec, so it plays in this engine:
- The lift waits for its rider at each end (wait=rider, the Level 11
  elevator) instead of pausing 30 frames.
- Ladders come up through 3-block holes in the decks above them (x 152-154,
  124-126, 104-106 and 113-115): ladders hold the runner on the left half
  of their block, so a shaft needs the block left of the ladder open too.
- The docking collar has a bulkhead at x 41, rows 4-10: without it the
  exit is one jump from the training bay's door.
- Handrails are `@ handrail` (`@ rail` is the mine rail of Level 11).
- The cab's crate pile stands at x 91, a block clear of rail 92-97, and
  the first ammo box at (44, 49): (46, 49) is inside the crate step.
"""

from lib import Level

W, H = 160, 56
L = Level(W, H,
          name="STAGE 15 - HANGAR BAY", episode=3, theme="station_hangar",
          music="theme_cold_ambient", weapon="breach_charge", par=200, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def gem(*pts):
    for x, y in pts:
        assert L.get(x, y) == ".", (x, y, L.get(x, y))
        L.put(x, y, "g")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def panel(pid, x, y, w, h, vent, rect, pull=1, bonus=0, exclude=None):
    keys = dict(w=w, h=h, vent=vent, reseal=90)
    if bonus:
        keys["bonus"] = 1
    L.at("panel", x, y, pid, **keys)
    vkeys = dict(rect=rect, pull=pull, frames=60)
    if exclude:
        vkeys["exclude"] = ";".join(",".join(str(v) for v in r) for r in exclude)
    L.at("vent", None, None, vent, **vkeys)


# --- The hangar: one big room inside the hull --------------------------------------------
L.clear(2, 155, 4, 49)

# --- 1. Training bay (x 2-37), ground 14 --------------------------------------------------
L.fill(2, 37, 14, 15, "#")              # its deck
L.fill(36, 37, 4, 10, "#")              # east wall; the door gap is rows 11-13
put(8, 13, "P")
put(12, 13, "W")
panel("PA", 0, 12, 2, 2, "VA", (2, 4, 35, 13))
L.at("handrail", x0=6, x1=20, y=12)
for x, y in ((22, 13), (23, 13), (23, 12)):
    L.at("crate", x, y)
L.at("deco", kind="text", x=4, y=8, w=10, h=1, text="CHARGE THE YELLOW FRAME - HOLD THE RAIL")
L.at("deco", kind="hatch", x=7, y=4, w=3, h=1)
put(28, 13, "h")
put(30, 13, "m")
gem(*[(x, 10) for x in range(14, 20)])

# --- Lift (x 38-40) -----------------------------------------------------------------------
L.at("platform", 38, 14, "L1", w=3, mode="pingpong", path=[(38, 14), (38, 50)], speed="1", wait="rider")

# --- 2. Hangar floor (x 2-117), ground 50 -------------------------------------------------
# Shuttle A, KESTREL, and its cargo hold (secret 1).
L.fill(10, 30, 39, 45, "#")
L.clear(12, 27, 42, 44)
L.at("breakable", rect=(28, 42, 30, 44), hp=1, by="explosion", look="hatch")
L.fill(31, 33, 47, 49, "#")             # the crate step up to its rear hatch
L.at("deco", kind="text", x=14, y=40, w=8, h=1, text="KESTREL")
put(14, 44, "1")                         # letter G
put(18, 44, "m")
put(22, 44, "m")
# Shuttle B, NOVA'S RIDE, and the crate steps up to its roof.
L.fill(50, 64, 41, 46, "#")
L.at("deco", kind="text", x=52, y=43, w=10, h=1, text="NOVA'S RIDE")
L.fill(46, 47, 47, 49, "#")
L.fill(48, 49, 44, 49, "#")
# The crane-hung pallet, and shuttle C, HAULER 42.
L.row(66, 71, 38, "=")
L.fill(76, 90, 38, 46, "#")
L.at("deco", kind=42, x=83, y=42, w=4, h=1, text="HAULER 42")
put(88, 37, "Q")                         # the rubber duck in a hard hat
put(43, 49, "c")
put(44, 49, "W")                         # spec (46, 49) is inside the crate step
L.at("weld_drone", 44, 49, path=(44, 66))
L.at("weld_drone", 57, 40, "WD2")
L.at("loader_mech", 68, 49, "LM1")
L.at("cratepile", 72, 49, n=4)
L.at("loader_mech", 99, 49, "LM2")
L.at("cratepile", 103, 49, n=4)
panel("PF", 96, 50, 2, 2, "VF", (82, 40, 103, 49))
L.at("handrail", x0=84, x1=94, y=48)
L.at("handrail", x0=104, x1=112, y=48)
put(60, 49, "m")
put(88, 49, "h")
gem((51, 40), (54, 40), (58, 39), (61, 40), (64, 40), (68, 37), (77, 37), (80, 37), (84, 37), (86, 36))
# The control tower on its legs.
L.fill(104, 115, 27, 44, "#")
L.at("deco", kind="text", x=106, y=36, w=8, h=1, text="FLIGHT CONTROL")

# --- 3. Gantries (x 118-155) --------------------------------------------------------------
L.fill(118, 119, 47, 49, "#")
L.fill(120, 121, 44, 49, "#")
L.fill(122, 155, 42, 43, "#")           # G1, ground 42
L.fill(124, 155, 34, 35, "#")           # G2, ground 34
L.clear(152, 154, 34, 35)
L.col(153, 34, 41, "H")
L.fill(104, 130, 26, 27, "#")           # G3 and the bridge to the tower, ground 26
L.clear(124, 126, 26, 27)
L.col(125, 26, 33, "H")
put(116, 49, "c")
L.at("tether_pair", 128, 41, "TP1", gap=5, patrol=(128, 150))
L.at("handrail", x0=122, x1=140, y=40)
put(140, 41, "h")
panel("PG1", 156, 40, 2, 2, "VG1", (122, 36, 155, 41))
L.at("tether_pair", 130, 33, "TP2", gap=5, patrol=(130, 150))
L.at("weld_drone", 132, 33, "WD3", path=(124, 152))
L.at("weld_drone", 148, 33, "WD4", path=(124, 152))
L.at("handrail", x0=146, x1=155, y=32)
panel("PG2", 156, 32, 2, 2, "VG2", (124, 28, 155, 33))
put(150, 33, "m")
put(120, 25, "2")                        # letter U
gem((108, 25), (111, 25), (117, 24), (122, 25), (128, 24), (129, 25))

# --- 4. Control tower (x 104-120) ---------------------------------------------------------
L.col(105, 18, 25, "H")
L.fill(104, 115, 18, 19, "#")           # landing 18
L.clear(104, 106, 18, 19)
L.col(105, 18, 19, "H")
L.col(114, 12, 17, "H")
L.fill(69, 120, 12, 13, "#")            # the cab deck and the gangway, ground 12
L.clear(113, 115, 12, 13)
L.col(114, 12, 13, "H")
put(107, 25, "c")
L.at("weld_drone", 106, 17, "WDV", path=(106, 113), carrier=1)
put(108, 17, "h")
put(117, 11, "T")
put(119, 11, "W")
L.at("respawn", 119, 11, frames=150)   # the refill rack
L.at("handrail", x0=92, x1=97, y=10)
L.at("handrail", x0=104, x1=120, y=10)
L.at("loader_mech", 101, 11, "LM3")
L.at("cratepile", 91, 11, n=3)
L.at("crate", 96, 11, gems=12)
panel("BV", 98, 2, 6, 2, "VB", (2, 4, 155, 49), pull=3,
      exclude=[(2, 4, 37, 13), (48, 5, 68, 11), (123, 4, 155, 11)])
L.at("setpiece", None, None, "cab", x0=69, x1=112, y=11, drone=(103, 40), tether=(80, 20))
put(100, 4, "C")                         # outside the bay doors: shown while BV is open
L.at("weld_drone", 103, 40, "WD5", wall=1, sleep=1)
L.at("tether_pair", 80, 23, "TP4", gap=3, patrol=(84, 110), sleep=1)
# Secret 2: the maintenance crawl behind the cab's east wall, and the ledge.
L.fill(121, 155, 4, 13, "#")           # the crawl and the ledge, ground 12
L.at("breakable", rect=(121, 4, 122, 11), hp=1, by="explosion", look="crack")
L.clear(123, 139, 9, 11)
L.clear(140, 155, 4, 11)
put(150, 11, "$")
panel("PB", 156, 9, 2, 3, "VPB", (140, 4, 155, 11), bonus=1)
L.put(156, 9, "B")
gem(*[(x, 10) for x in (142, 144, 146, 148, 151, 152, 153, 154)])

# --- 5. Shuttle ramp (x 41-91) ------------------------------------------------------------
L.fill(48, 68, 5, 11, "#")              # the TENDER, docked
L.clear(50, 66, 7, 10)
L.clear(67, 68, 8, 10)                  # rear hatch
L.clear(48, 49, 8, 10)                  # front hatch
L.at("deco", kind="text", x=54, y=5, w=8, h=1, text="TENDER")
L.fill(41, 47, 11, 13, "#")             # the docking collar, ground 11
L.fill(41, 41, 4, 10, "#")              # its bulkhead: the way in is through the shuttle
put(93, 11, "c")
L.at("tether_pair", 52, 10, "TP5", gap=3, patrol=(52, 64))
put(58, 10, "h")
put(64, 10, "m")
put(62, 10, "3")                         # letter N
put(43, 10, "X")

L.write(__file__, "15_hangar_bay.txt")
