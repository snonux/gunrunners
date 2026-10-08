#!/usr/bin/env python3
"""Level 3 - Club Laserdisc (SPEC.md 03). Twist: Subwoofers.

A hub of floors: coat check bottom left, bar above it, the dance floor in
the atrium, the VIP lounge on the right, the laser room one floor up running
back left, then the drop chain up the atrium to the DJ booth, where the exit
is behind the decks.

The music runs in 16-bar phrases. Pads bump you on every beat and launch you
on their drop hit (bars 1, 2 and 3 of the phrase); the laser fans sweep only
during the chorus (bars 9-14).

Deviations from the spec, all for headroom or footing:
- the coat shelf sits on row 56 (row 54 left no room to stand under the F1
  slab) and the health box moved from under it to (17, 57);
- the ladder's opening in the laser room floor is x116-118 (x115 stays
  solid, so you can step off the top of the ladder);
- the stage ends at x55 and the stage-front gap is x56-59, so the Bass
  Cannon's 3-block knockback can push the booth Bouncer clear of the landing;
- a fourth W box on the booth landing, so a runner who spent the Bass
  Cannon on the way up can still shatter the big speaker.
"""

import math

from lib import Level

W, H = 120, 64
L = Level(W, H,
          name="STAGE 3 - CLUB LASERDISC", episode=1, theme="club_laserdisc",
          music="theme_clubhouse", weapon="bass_cannon", par=360, flags="")

L.fill(0, W - 1, 0, H - 1, "#")


def room(x0, x1, y0, y1):
    L.clear(x0, x1, y0, y1)


def gems(*pts):
    for x, y in pts:
        L.put(x, y, "g")


# --- Rooms (rows are block rows; a room's ground is the row below it) --------
room(1, 39, 53, 57)        # coat check, ground 58
room(41, 55, 54, 57)       # back room (secret), ground 58
room(1, 40, 46, 51)        # bar, ground 52, ceiling 45
room(41, 78, 18, 51)       # atrium, ceiling 17
room(79, 118, 46, 51)      # VIP lounge, ground 52, ceiling 45
room(79, 118, 39, 44)      # laser room, ground 45, ceiling 38

# --- 1. Coat check (F0) ----------------------------------------------------------
L.row(28, 36, 52, "=")                       # one-way up to the bar
L.row(20, 23, 56, "#")                       # coat shelf
L.on(3, 58, "P")
L.at("pad", 10, 58, "pad0", w=3, bump=2, launch="2.5j", fire=0)
L.at("pad", 31, 58, "pad1", w=3, bump=2, launch="2.5j", fire=1)
L.on(17, 58, "h")
gems((20, 55), (21, 55), (22, 55))
L.at("breakable", rect=(40, 53, 40, 57), hp=3, by="sound", look="speaker")  # speaker cabinet
L.at("deco", kind="text", x=4, y=54, w=8, h=1, text="COAT CHECK")

# --- 2. Bar (F1 left) ------------------------------------------------------------
L.at("deco", kind="text", x=6, y=47, w=13, h=1, text="LASERDISC BAR")
L.on(10, 52, "W")
L.at("virus", 16, 51, skin="spiked_drink", bob=0)
L.on(22, 52, "h")
L.on(25, 52, "c")
L.on(4, 52, "m")
L.put(30, 50, "1")                           # letter G

# --- 3. Dance floor (F1 atrium) -----------------------------------------------------
L.at("deco", kind="dancefloor", rect=(41, 52, 78, 52))
L.at("pad", 75, 52, "pad2", w=3, bump=2, launch="2.5j", fire=0)
L.fill(60, 62, 25, 27, "#")                  # the disco ball
L.at("breakable", rect=(60, 25, 62, 27), hp=3, by="any", look="ball")
L.put(61, 26, "C")                           # the candid camera inside it
for x in (48, 60, 70):
    L.at("glow_raver", x, 51)
for k in range(6):
    a = k * math.pi / 3
    gems((int(round(60 + 6 * math.cos(a))), int(round(45 + 3 * math.sin(a)))))

# --- 4. VIP lounge (F1 right) -------------------------------------------------------
L.at("deco", kind="text", x=79, y=47, w=4, h=1, text="VIP")
L.row(104, 110, 49, "=")                     # VIP balcony
L.clear(116, 118, 45, 45)                    # opening up to the laser room
L.col(117, 45, 51, "H")
L.at("bouncer", 81, 51)
L.at("bouncer", 95, 51)
L.on(86, 52, "c")
L.at("disco_drone", 92, 46)
L.on(96, 52, "W")
L.on(100, 52, "h")
L.on(107, 49, "T")
L.on(113, 52, "m")
L.on(118, 52, "Q")

# --- 5. Laser room (F2 right), walked right to left ----------------------------------
for x in (108, 98, 86):
    L.at("laserfan", x, 39, arc=(200, 340), len=10)
L.at("disco_drone", 96, 39)
L.on(88, 45, "W")
L.put(84, 42, "2")                           # letter U
gems((112, 43), (103, 43), (92, 43), (81, 43))

# --- 6. Drop chain (atrium) ---------------------------------------------------------
L.row(73, 78, 45, "#")                       # balcony
L.at("pad", 74, 45, "P1", w=3, bump=2, launch="2.5j", fire=1)
L.row(62, 76, 38, "=")                       # F3 tier
L.at("pad", 63, 38, "P2", w=3, bump=2, launch="2.5j", fire=2)
L.row(53, 66, 31, "=")                       # T4 tier
L.at("pad", 64, 31, "P3", w=3, bump=2, launch="2.5j", fire=3)
L.at("disco_drone", 68, 25)

# --- 7. DJ booth (row 24) -----------------------------------------------------------
L.row(44, 55, 24, "#")                       # stage
L.row(60, 70, 24, "=")                       # landing
L.row(71, 76, 24, "#")                       # speaker plinth
L.fill(73, 76, 19, 23, "#")                  # the biggest speaker
L.at("breakable", rect=(73, 19, 76, 23), hp=6, by="sound", look="speaker")
L.put(74, 21, "B")                           # behind the speaker
L.at("bouncer", 61, 23)
L.on(68, 24, "c")
L.on(70, 24, "h")
L.on(66, 24, "W")                            # ammo for the speaker
L.at("deco", kind="decks", x=49, y=22, w=2, h=2)
L.at("deco", kind="decks", x=52, y=22, w=2, h=2)
L.on(46, 24, "X")
L.at("deco", kind=42, x=48, y=19, w=6, h=1, text="BPM 120 - TRACK 42")

# --- Back room (secret) -------------------------------------------------------------
L.put(52, 57, "3")                           # letter N
L.on(54, 58, "$")
L.on(44, 58, "m")
L.on(47, 58, "m")

L.write(__file__, "03_club_laserdisc.txt")
