"""Tips the level scripts add by themselves (HINTS, src/game/world_hints.cpp).

Every gadget a player has to work in a way they can't see (press up at it,
shoot it, jump into it, stand on it) gets a tip next to it: an `@ hint` rect
around the gadget whose text says what to do. Level.write() adds them to
campaign levels (bonus levels have rules of their own and say them on the
HUD); a script can turn it off with `L.auto_tips = False`, and its own
`L.hint(...)` lines come first, so they win where they overlap.
"""

import re

# Tip rects reach this many blocks round a gadget: about where a runner
# stands to use it.
SIDE, ABOVE, BELOW = 3, 4, 2
MAX_SAME = 3  # gadgets of a kind with a tip, per level

_INT = re.compile(r"^-?\d+$")


def _parse(line):
    """'@ kind ids.. [x y] k=v ..' -> (kind, (x, y) or None, keys)."""
    parts = re.findall(r'[^\s"]+="[^"]*"|\S+', line)
    kind = parts[1]
    keys, bare = {}, []
    for p in parts[2:]:
        if "=" in p:
            k, v = p.split("=", 1)
            keys[k] = v.strip('"')
        else:
            bare.append(p)
    pos = None
    for i in range(len(bare) - 1):
        if _INT.match(bare[i]) and _INT.match(bare[i + 1]):
            pos = (int(bare[i]), int(bare[i + 1]))
            break
    if pos is None and "x" in keys and "y" in keys:
        pos = (int(keys["x"]), int(keys["y"]))
    return kind, pos, keys


def _first_point(keys, name="path"):
    if name not in keys:
        return None
    x, y = keys[name].split(";")[0].split(",")[:2]
    return int(x), int(y)


def _rect(keys):
    if "rect" not in keys:
        return None
    return tuple(int(v) for v in keys["rect"].split(",")[:4])


def _tip(kind, keys, theme):
    """What to say about a gadget, or None."""
    k = keys.get("kind", "")
    if kind == "breaker":
        return "PRESS UP AT THE BREAKER TO THROW IT AND BRING THE POWER BACK"
    if kind == "valve" and theme == "sludge_line":
        return "HOLD UP BESIDE THE VALVE TO PADLOCK IT: NO MORE FLOODS HERE"
    if kind == "valve" and theme == "station_reactor":
        return "HOLD UP AT THE VALVE TO SHUT IT. SHUT ALL THREE TO STOP THE PULSES AND OPEN THE LIFT"
    if kind == "mirror" and theme == "haunted_manor":
        if k == "broken":
            return "SHOOT THE BROKEN MIRROR, THEN PRESS UP AT IT QUICKLY TO STEP THROUGH"
        return "PRESS UP AT THE MIRROR TO STEP THROUGH TO THE OTHER SIDE"
    if kind == "mirror" and theme == "marble_observatory":
        return "SHOOT A MIRROR STATUE TO TURN IT AND SEND THE SUNBEAM ON TO THE SUN DOOR"
    if kind == "booth":
        return "PRESS UP IN THE BOOTH TO BECOME THE RUNNER ON ITS SIGN"
    if kind == "portal" and theme in ("gallery", "main_stage"):
        return "PRESS UP IN FRONT OF THE MONITOR TO HOP TO THE ONE IT SHOWS"
    if kind == "altar" and k == "offer":
        return "HOLD UP AT THE ALTAR TO GIVE GEMS BACK: FEWER GEMS, LESS ARMOR ON THE GOLEM"
    if kind == "lever" and theme == "gold_mines":
        return "SHOOT THE LEVER TO SWITCH THE TRACK FOR THE NEXT CART"
    if kind == "switch":
        if k == "panel":
            return "PRESS UP AT THE SWITCH, OR SHOOT IT, TO TURN THIS CHAMBER'S GRAVITY OVER"
        if k == "lever" and theme == "prop_warehouse":
            return "PRESS UP AT THE LEVER TO WAKE THE CRANE"
        if k == "lever":
            return "PRESS UP AT THE LEVER, OR SHOOT IT, TO THROW IT"
        if k == "plate":
            return "THIS PLATE ONLY GOES DOWN UNDER ROCCO (OR A TURBO RUNNER): CHANGE IN A BOOTH"
        if theme == "station_greenhouse":
            return "SHOOT THE SWITCH TO LIGHT ITS GROW LAMPS: THE PLANT UNDER THEM GROWS"
        if theme == "station_servers":
            return "SHOOT THE KILL SWITCH"
        if theme == "tv_kitchen" and "station" in keys:
            return "SHOOT THE BELL: THE BELTS OF ITS STATION RUN THE OTHER WAY"
        return None
    if kind == "crystal" and keys.get("hidden") != "1":
        return "SHOOT THE CRYSTAL TO SWAP PLACES WITH IT"
    if kind == "bounder":
        return "WALK UP TO THE BOUNDER AND PRESS {USE} TO RIDE IT OVER THE THORNS"
    if kind == "tube" and keys.get("look") != "pore":
        if keys.get("breath") == "1":
            return "STEP INTO THE MOUTH TO RIDE THE TUBE: IT OPENS ONLY WHILE THE HIVE BREATHES IN"
        return "STEP INTO THE MOUTH TO RIDE THE GULLET TUBE"
    if kind == "silk":
        if keys.get("spider") == "1":
            return "SHOOT THE SPIDER FIRST, OR IT CUTS THE SILK LINE UNDER YOU"
        return "JUMP INTO THE SILK LINE, OR STEP OFF THE LEDGE UNDER IT, TO SLIDE DOWN IT"
    if kind == "goo":
        return "JUMP INTO THE GOO HOLDING TOWARD IT TO STICK. {JUMP} KICKS YOU UP AND OFF"
    if kind == "fuse" and keys.get("hidden") != "1":
        return "SHOOT THE FUSE TO LIGHT IT: IT BURNS TO ITS TNT BARREL"
    if kind == "cart":
        return "JUMP INTO THE CART TO RIDE IT. {JUMP} HOPS, DOWN DUCKS"
    if kind == "vine":
        return "JUMP INTO THE VINE TO GRAB IT, HOLD THE WAY IT SWINGS, THEN JUMP AT THE END OF A BIG SWING"
    if kind == "rgbswitch" and keys.get("mode") == "shoot":
        return "SHOOT THE DIAL TO CHANGE THE SIGNAL: ONLY BARS OF ITS COLOUR ARE SOLID"
    if kind == "panel" and keys.get("bonus") != "1":
        return "A BREACH CHARGE BLOWS THE PANEL OPEN: HOLD A RAIL OR A LADDER WHILE IT VENTS"
    if kind == "platform" and keys.get("mode") == "pulley" and "pair" in keys:
        return "STAND ON A GONDOLA: IT SINKS AND ITS PARTNER RISES. JUMP ACROSS TO THE RISEN ONE - IT WAITS A MOMENT - AND ON UP"
    if kind == "platform" and keys.get("mode") == "sink":
        return "THE STONES SINK UNDER YOU: KEEP MOVING"
    if kind == "door" and theme == "broadcast_mast":
        return "PRESS UP AT THE DOOR TO GO THROUGH"
    if kind == "kicker":
        return "RUN FAST ACROSS THE KICKER TO BE LAUNCHED"
    return None


def _where(kind, pos, keys):
    """The gadget's blocks (x0, y0, x1, y1), or None."""
    if kind in ("tube", "fuse"):
        p = _first_point(keys)
        return (p[0], p[1], p[0] + 1, p[1]) if p else None
    if kind == "silk" and pos:
        return (pos[0], pos[1] + 1, pos[0], pos[1] + 3)  # under its top end, where you jump into it
    r = _rect(keys)
    if r and kind in ("goo",):
        return r
    if pos is None:
        return r
    w = int(keys.get("w", "1"))
    if kind in ("booth", "portal", "mirror"):
        # 2 x 3 blocks, from its top-left (monitors) or bottom-left (booths)
        return (pos[0], pos[1] - 2, pos[0] + 1, pos[1] + 2)
    return (pos[0], pos[1], pos[0] + max(1, w) - 1, pos[1])


def auto_tips(entities, theme):
    """`@ hint` lines for the gadgets among `entities` (entity lines)."""
    out = []
    for line in entities:
        if not line.startswith("@ "):
            continue
        kind, pos, keys = _parse(line)
        text = _tip(kind, keys, theme)
        if text is None:
            continue
        where = _where(kind, pos, keys)
        if where is None:
            continue
        x0, y0, x1, y1 = where
        rect = (max(0, x0 - SIDE), max(0, y0 - ABOVE), x1 + SIDE, y1 + BELOW)
        out.append('@ hint rect=%d,%d,%d,%d text="%s"' % (rect + (text,)))
    # The first few of a kind say it; by then the player knows.
    count, unique = {}, []
    for h in out:
        text = h.split(' text=', 1)[1]
        count[text] = count.get(text, 0) + 1
        if count[text] <= MAX_SAME and h not in unique:
            unique.append(h)
    return unique
