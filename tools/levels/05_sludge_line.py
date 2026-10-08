#!/usr/bin/env python3
"""Level 5 - Sludge Line (SPEC.md 05). Twist: The Tide.

Down and right through the storm drains: a high catwalk, a tunnel that
floods (Z1), the low tunnels with their islands (Z2), the flood room with a
Valve Keeper (Z3), the highest catwalk with the Turbo, the great drain (Z4)
and the outflow pipe to the exit. Four tide zones share one 300-frame clock.
Floors marked % are grates: solid to walk on, sludge rises through them.

Deviations from the spec, all for footing:
- the ladder shafts are two blocks wide (x28-29, x110-111, x118-119), with
  the ladders on their right-hand column, as in Blackout;
- the low tunnels meet the flood room through a taller opening (x81-85 rows
  26-33), and the culvert's way in is a 2-block hole in the flood room's
  floor (x82-83) rather than at x80 in the low tunnels' floor;
- the highest catwalk ends at x117 so its shaft to the drain is open air;
  its checkpoint is at (108, 14) and the gem cache at (114, 14);
- the drain's pillars and mid island stand on legs (rows 37-39 open), so
  the Turbo line can walk the drain floor under them as the spec describes;
- the gator under the raft moved to (144, 37), and the bonus entrance to
  (151, 24) so that a jump from the raft at high tide reaches it for every
  runner;
- the Z1 channel stops at x34 (x35 is the grate on a solid foot).
"""

from lib import Level

W, H = 200, 44
L = Level(W, H,
          name="STAGE 5 - SLUDGE LINE", episode=1, theme="sludge_line",
          music="theme_dub", weapon="bubble_gun", par=360)

# Everything is solid unless carved out.
L.fill(0, W - 1, 0, H - 1, "#")


def gems(*pts):
    for x, y in pts:
        L.put(x, y, "g")


# --- 1. High catwalk and first tide (0-35) ---------------------------------------
L.clear(0, 27, 6, 11)                        # above the catwalk (row 12)
L.clear(28, 29, 6, 18)                       # the shaft down to the tunnel
L.col(28, 11, 25, "H")
L.clear(7, 35, 19, 25)                       # the tunnel
L.row(7, 35, 26, "%")                        # its grate floor
L.clear(7, 34, 27, 28)                       # the channel under it
L.clear(1, 5, 23, 25)                        # the secret room (floor 26 solid)
L.at("breakable", None, None, rect=(6, 23, 6, 25), hp=2, by="any", look="mark")
L.at("deco", kind="interior", rect=(1, 23, 5, 25))
L.put(2, 11, "P")
L.put(14, 11, "W")
L.put(20, 11, "h")
L.put(26, 11, "1")                           # letter G
gems((5, 10), (9, 10), (12, 10), (17, 10), (23, 10))
L.put(8, 25, "m")
L.put(34, 25, "c")
L.put(1, 25, "2")                            # letter U
for x in (2, 3, 4):
    L.put(x, 25, "m")

# --- 2. Low tunnels (36-80) ------------------------------------------------------
L.clear(36, 80, 23, 29)
L.row(36, 80, 30, "%")
L.clear(36, 80, 31, 32)
for x0 in (42, 52, 62, 72):                  # islands I1-I4, top row 27
    L.fill(x0, x0 + 3, 27, 29, "#")
for x0 in (47, 57):                          # pool gaps in the grate
    L.clear(x0, x0 + 2, 30, 30)
L.put(54, 26, "W")
L.put(64, 26, "h")
L.put(79, 29, "c")
gems((43, 25), (45, 25), (53, 25), (55, 25), (63, 25), (65, 25), (73, 25), (75, 25))

# --- 3. Flood room (81-112) ----------------------------------------------------------
L.clear(81, 111, 28, 33)
L.clear(81, 85, 26, 27)                      # the opening from the low tunnels
L.row(81, 111, 34, "%")
L.clear(82, 83, 34, 34)                      # the way down into the culvert
L.clear(82, 89, 35, 37)                      # the culvert
L.clear(90, 111, 35, 36)                     # the channel under the grate
L.clear(90, 92, 34, 34)                      # the carrier's pool gap
L.col(89, 31, 37, "H")                       # out of the culvert
L.fill(86, 88, 31, 33, "#")                  # crates
L.fill(100, 103, 31, 33, "#")
L.put(88, 30, "h")
L.put(95, 33, "W")
L.put(101, 30, "m")
gems((87, 29), (96, 31), (102, 29))

# --- 4. Highest catwalk (104-120) ----------------------------------------------------
L.clear(110, 111, 15, 27)                    # the shaft up from the flood room
L.col(111, 14, 33, "H")
L.clear(104, 117, 11, 14)                    # above the catwalk (row 15)
L.put(108, 14, "c")
L.put(116, 14, "T")
L.put(105, 14, "Q")
L.put(114, 14, "$")

# --- 5. Great drain (113-165) --------------------------------------------------------
L.clear(113, 165, 14, 39)                    # ceiling 13, floor 40
L.clear(118, 120, 11, 13)                    # open above the catwalk's end
L.fill(113, 119, 34, 39, "#")                # entry ledge E
L.col(119, 15, 33, "H")
for x0 in (123, 128, 133, 146, 151, 156):    # pillars on legs
    L.fill(x0, x0 + 1, 34, 36, "#")
L.fill(138, 142, 32, 36, "#")                # the mid island, on legs
L.fill(161, 170, 32, 39, "#")                # the exit ledge
L.col(160, 31, 39, "H")
L.col(150, 14, 22, "#")                      # the ceiling pipe
L.col(153, 14, 22, "#")
L.put(140, 31, "h")
L.put(140, 16, "C")
L.at("deco", kind=42, x=130, y=20, w=6, h=1, text="SECTOR 42 - NO SWIMMING")
L.put(163, 31, "c")
gems((123, 32), (128, 32), (133, 32), (146, 32), (151, 32), (156, 32))
L.put(151, 24, "B")

# --- 6. Outflow pipe (166-199) -------------------------------------------------------
L.clear(166, 170, 14, 31)                    # the outflow mouth and waterfall
L.clear(171, 182, 29, 31)                    # ground 32, ceiling 28
L.clear(183, 189, 29, 33)                    # ground 34
L.clear(190, 198, 29, 35)                    # ground 36
L.at("deco", kind="waterfall", rect=(166, 14, 170, 31))
L.put(178, 31, "3")                          # letter N
L.put(180, 31, "h")
L.put(196, 35, "X")
gems((185, 32), (188, 32), (192, 34), (195, 34))

# --- Fluids (before what lives in them), valves (before their Keepers) -------------
L.at("fluid", None, None, "Z1", kind="tide", rect=(1, 24, 35, 28), low=28, high=24)
L.at("fluid", None, None, "Z2", kind="tide", rect=(36, 28, 80, 32), low=32, high=28)
L.at("fluid", None, None, "Z3", kind="tide", rect=(81, 32, 111, 36), low=36, high=32)
L.at("fluid", None, None, "Z4", kind="tide", rect=(120, 32, 165, 39), low=36, high=32, current=1)
L.at("fluid", None, None, "pool1", kind="sludge", rect=(47, 31, 49, 32))
L.at("fluid", None, None, "pool2", kind="sludge", rect=(57, 31, 59, 32))
L.at("fluid", None, None, "pool3", kind="sludge", rect=(90, 35, 92, 36))
L.at("fluid", None, None, "culvert", kind="sludge", rect=(82, 35, 89, 37))
L.at("valve", 98, 33, "v1", zone="Z3")
L.at("valve", 162, 31, "v2", zone="Z4")

L.at("sludge_gator", 48, 32, look="sunglasses")
L.at("sludge_gator", 58, 32)
L.at("sludge_gator", 91, 36, carrier=1)
for x in (126, 136, 144):
    L.at("sludge_gator", x, 37)
L.at("ratpipe", 68, 29, dir="l", count=5)
L.at("ratpipe", 84, 33, dir="r", count=5)
L.at("valve_keeper", 110, 33, valve="v1")
L.at("valve_keeper", 176, 31, valve="v2")
L.at("devnull", 82, 31)
L.at("raft", 148, 36, w=3)
L.at("bubble", 140, 16, hp=1)                # the candid camera's bubble

L.write(__file__, "05_sludge_line.txt")
