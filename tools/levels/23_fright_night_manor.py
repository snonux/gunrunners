#!/usr/bin/env python3
"""Level 23 - Fright Night Manor (SPEC.md 23). Twist: Mirror World. Prototype:
Silver Crossbow.

Five storeys around the central stair hall (x 47-60): attic (ground 12),
ballroom (26), gallery (40), ground floor (54), cellar (68). The level is on
one side at a time, the real manor or its reflection; `@ layer ... side=`
tiles are solid only on their side, `@ sideitem` tags a box or item on one
side, `side=` on an enemy keeps it to its side. Mirrors (`@ mirror`, 2 x 3
blocks) flip the side with Up.

The route: front door (real) -> mi1 -> reflection (Silver Crossbow) -> mi2
-> real, through the hall and the library -> mi3 -> reflection past the
bookcase -> dining room -> servants' stair -> east gallery, west over the
reflected gallery's hole on two pinned Portrait Ghosts -> down the
reflection's flights to the cellar (lever) -> back up to the gallery
landing -> mi7 -> real, up the real flight -> mi8 -> reflection, up to the
attic (lever) -> back down to the ballroom door -> the ballroom hop over
alternating floor segments, mirror to mirror -> down the laundry chute into
the sealed reflected vestibule, whose front door is the exit.

Changes from the spec, so it plays in this engine:
- Side-only tiles never overlap plain tiles: the ballroom floor is cut into
  both-side parts (where the mirrors stand) and side-only parts, and the
  real side's chute floor covers x 2-8 so no fall in the real ballroom ends
  in the sealed shaft.
- The Portrait Ghosts' portraits hang low over the gallery hole (x 64 and 68),
  so the ghosts come out over it toward a runner at its east edge.
- The refill box is `@ boltbox` (`@ refill` is Level 14's).
- One Turbo box in the foyer (both sides) instead of one on each side.
- The levers throw once (Up at one, or a shot) and stay thrown.
- The Nova line's real-only beams in the hall are left out (they would
  overlap the reflection's flight at row 18).
- `@ botroute` lists the mirrors and levers the bot takes in order.
"""

from lib import Level

W, H = 110, 70
L = Level(W, H,
          name="STAGE 23 - FRIGHT NIGHT MANOR", episode=4, theme="haunted_manor",
          music="theme_theremin", weapon="silver_crossbow", par=420, flags="")


def put(x, y, c):
    assert L.get(x, y) == ".", (x, y, c, L.get(x, y))
    L.put(x, y, c)


def gem(*pts):
    for x, y in pts:
        put(x, y, "g")


def layer(lid, x0, y0, x1, y1, side, tile="#"):
    L.at("layer", None, None, lid, rect=(x0, y0, x1, y1), tile=tile, driver="script", side=side)


def mirror(mid, x, y, **k):
    L.at("mirror", x, y, mid, **k)


def deco(kind, rect=None, x=None, y=None, text=None, side=None):
    L.at("manordeco", x, y, kind=kind, rect=rect, text=text, side=side)


# --- Shell: roof, floors, outer walls --------------------------------------------------------------
L.fill(0, W - 1, 0, 5, "#")                 # roof
for g in (12, 26, 40, 54, 68):
    L.fill(0, W - 1, g, g + 1, "#")         # floor slabs, 2 rows thick
L.fill(0, 1, 0, H - 1, "#")
L.fill(W - 2, W - 1, 0, H - 1, "#")

# --- The stair hall (x 47-60): landings x 47-49 and 58-60 on every storey ---------------------------
for g in (12, 26, 40, 54):
    L.clear(50, 57, g, g + 1)               # the shaft between the landings
layer("hall_gf_floor", 50, 54, 57, 55, "real")
step = {}
# Real: gallery east -> ballroom west.
for i, row in enumerate((38, 36, 34, 32, 30, 28)):
    x = 55 if i % 2 == 0 else 50
    layer("rf%d" % i, x, row, x + 2, row, "real", "=")
# Reflection: ballroom west -> attic east; gallery east -> ground floor west
# -> cellar east.
for i, row in enumerate((24, 22, 20, 18, 16, 14)):
    x = 50 if i % 2 == 0 else 55
    layer("mfa%d" % i, x, row, x + 2, row, "mirror", "=")
for i, row in enumerate((42, 44, 46, 48, 50, 52)):
    x = 55 if i % 2 == 0 else 50
    layer("mfg%d" % i, x, row, x + 2, row, "mirror", "=")
for i, row in enumerate((56, 58, 60, 62, 64, 66)):
    x = 50 if i % 2 == 0 else 55
    layer("mfc%d" % i, x, row, x + 2, row, "mirror", "=")
layer("hall_w", 46, 42, 46, 53, "mirror")
layer("hall_e", 61, 42, 61, 53, "mirror")

# --- 1. Foyer (x 2-46, ground 54) -----------------------------------------------------------------
layer("vest_wall", 8, 42, 8, 53, "mirror")
layer("foyer_wall", 40, 42, 40, 53, "real")
deco("frontdoor", rect=(2, 50, 3, 53))
L.at("deco", 3, 49, kind="42", text="42")
put(4, 53, "P")
mirror("mi1", 12, 51)
put(32, 53, "W")
L.at("sideitem", 32, 53, side="mirror")
put(20, 53, "T")
mirror("mi2", 42, 51)
put(36, 53, "h")
deco("chandelier", rect=(18, 42, 22, 44))
gem((15, 53), (17, 52), (24, 52), (27, 53), (30, 52), (38, 52))

# --- 2a. Library (x 61-84) ------------------------------------------------------------------------
put(59, 53, "c")
L.fill(66, 78, 48, 48, "=")                 # shelves
L.at("poltergeist", 72, 47, "P1", object="book", side="real")
mirror("mi3", 76, 51)
put(66, 53, "m")
put(72, 45, "1")
layer("bookcase", 80, 42, 80, 53, "real")
deco("bookshelf", rect=(62, 42, 79, 53))
gem((67, 47), (69, 47), (71, 47), (74, 47), (76, 47), (78, 47), (64, 53), (70, 53))

# --- 2b. Dining room (x 85-107) -------------------------------------------------------------------
mirror("mi4", 86, 51)
L.fill(89, 98, 51, 51, "=")                 # the table
L.at("poltergeist", 92, 50, "P2", object="tureen", side="real")
L.at("poltergeist", 96, 50, "P3", object="candelabra", side="mirror")
put(90, 53, "h")
gem((89, 50), (91, 50), (94, 50), (98, 50), (85, 53), (95, 53))
# The servants' stair (x 100-107) up to the gallery.
for i, row in enumerate((52, 50, 48, 46, 44, 42)):
    x = 101 if i % 2 == 0 else 104
    L.fill(x, x + 2, row, row, "=")
L.fill(100, 107, 40, 40, "=")
L.clear(100, 107, 41, 41)

# --- 3. East gallery (x 61-107, ground 40) --------------------------------------------------------
put(104, 39, "c")
layer("gallery_wall", 75, 28, 75, 39, "real")
layer("gallery_floor", 62, 40, 73, 41, "real")
L.clear(62, 73, 40, 41)
L.at("portrait_ghost", 64, 35, "PG1")
L.at("portrait_ghost", 68, 35, "PG2")
L.at("ghostbridge", edge=74, landing=61, row=40)
mirror("mi6", 84, 37)
L.at("boltbox", 88, 39, respawn=300)
put(96, 39, "h")
gem((65, 36), (66, 36), (69, 37), (70, 37), (78, 39), (82, 38), (92, 39), (100, 38))

# --- 4a. Hall landing and the west gallery (x 9-60, ground 40) -----------------------------------
put(59, 39, "c")
mirror("mi7", 58, 37)
L.fill(8, 8, 28, 39, "#")                   # the chute shaft's wall
layer("chute_floor", 2, 40, 7, 41, "real")
L.clear(2, 7, 40, 41)
put(12, 39, "m")
L.at("prize", 20, 33, kind="lance_vampire", score=10000, side="mirror")
gem((14, 38), (17, 38), (23, 38), (26, 39), (29, 38), (32, 38), (35, 39), (38, 38), (41, 38), (44, 39))

# --- 4b. Cellar (x 16-107, ground 68) and the crypt (x 2-14) --------------------------------------
L.fill(2, 15, 56, 56, "#")
L.fill(15, 15, 56, 67, "#")                 # the crypt wall
put(64, 67, "h")
put(44, 67, "W")
put(40, 67, "m")
L.at("haunted_armor", 84, 67, "HA5", side="mirror")
L.at("coffin", rect=(68, 66, 71, 67), sign=(70, 65))
mirror("bm1", 30, 65, kind="broken", to="cm", land=(12, 67))
L.at("switch", id="lever_cellar", x=100, y=67, kind="lever", states=2)
gem((20, 67), (24, 66), (34, 67), (48, 66), (54, 67), (74, 67), (90, 66), (96, 67))
# The crypt: U, eight gems, its mirror back, the bonus mirror.
put(8, 67, "2")
mirror("cm", 12, 65, kind="crypt", to="bm1")
put(4, 65, "B")
gem((3, 63), (5, 62), (7, 63), (9, 62), (10, 64), (11, 63), (13, 62), (14, 63))

# --- 4c. Attic (x 2-107, ground 12) ---------------------------------------------------------------
L.fill(64, 96, 9, 9, "=")                   # rafters
L.fill(50, 53, 12, 12, "=")                 # boards over the west half of the stair shaft
mirror("mi8", 47, 23)
put(62, 11, "h")
put(30, 11, "m")
L.at("haunted_armor", 70, 11, "HA1", side="mirror")
L.at("haunted_armor", 80, 11, "HA2", side="mirror")
L.at("haunted_armor", 90, 11, "HA3", side="mirror")
put(95, 8, "3")
L.at("switch", id="lever_attic", x=100, y=11, kind="lever", states=2)
put(6, 11, "Q")
gem((66, 8), (70, 8), (74, 8), (78, 8), (82, 8), (86, 8), (90, 8), (93, 8), (20, 11), (40, 11))

# --- 5. Ballroom (x 2-46, ground 26) --------------------------------------------------------------
L.fill(61, 107, 14, 25, "#")                # a closed set wall east of the hall
put(48, 25, "c")
put(45, 25, "h")
L.at("layer", None, None, "ballroom_door", rect=(46, 14, 46, 25), tile="#", driver="switch",
     switch="lever_attic,lever_cellar", state=1)
# Alternating floor: both-side parts where the mirrors stand, side-only between.
L.clear(2, 46, 26, 27)
for x0, x1 in ((2, 3), (8, 10), (17, 18), (25, 26), (33, 34), (41, 46)):
    L.fill(x0, x1, 26, 27, "#")
layer("seg_r1", 4, 26, 7, 27, "real")       # the real chute floor
layer("seg_r2", 11, 26, 16, 27, "real")
layer("seg_r3", 27, 26, 32, 27, "real")
layer("seg_m1", 19, 26, 24, 27, "mirror")
layer("seg_m2", 35, 26, 40, 27, "mirror")
for mid, x in (("bm_a", 41), ("bm_b", 33), ("bm_c", 25), ("bm_d", 17), ("bm_e", 9)):
    mirror(mid, x, 23)
L.at("haunted_armor", 29, 25, "HA4", side="real")
deco("chandelier", rect=(20, 14, 26, 16))
gem((44, 24), (37, 24), (34, 23), (29, 24), (26, 23), (21, 24), (18, 23), (10, 23))

# The exit: the front door, in the reflection only.
put(3, 53, "X")
L.at("exitside", side="mirror")

L.at("botroute", steps="mi1:mirror,mi2:real,mi3:mirror,lever_cellar,mi7:real,mi8:mirror,lever_attic,"
     "bm_a:mirror,bm_b:real,bm_c:mirror,bm_d:real,bm_e:mirror")

L.write(__file__, "23_fright_night_manor.txt")
