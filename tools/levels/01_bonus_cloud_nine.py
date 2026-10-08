#!/usr/bin/env python3
"""Level 1 bonus - Cloud Nine (SPEC.md 01, "Bonus level").

Rule airjump: every jump press in mid-air starts a fresh jump. One-way
clouds rise to the moon balloon at the top right while three gusts blow
left. 60 seconds; falling off the bottom ends the bonus (nothing is lost).

The spec asks for 18 clouds each 2-3 rows above the last, which does not
fit in 24 rows, so the climb rises in two waves with a dip between them;
every step up is still 2-3 rows.
"""

from lib import Level

L = Level(80, 24,
          name="BONUS - CLOUD NINE", episode=1, theme="neon_rooftops",
          music="bonus_cloud_nine", rules="airjump", timer=60, goal="exit", flags="clouds")

# Start cloud.
for x in range(0, 6):
    L.put(x, 22, "=")
L.on(2, 22, "P")

# 18 clouds: (x0, width, row). Clouds that share columns are at least 3
# rows apart, so there is always room to stand on the lower one.
CLOUDS = [
    (7, 4, 19), (12, 4, 17), (17, 4, 14), (22, 4, 12), (27, 4, 9), (32, 4, 7),
    (37, 4, 10), (42, 4, 13), (47, 4, 16), (52, 4, 18),
    (57, 4, 15), (62, 4, 13), (57, 4, 11), (62, 4, 8),
    (67, 4, 11), (67, 4, 6), (71, 4, 9), (72, 4, 5),
]
assert len(CLOUDS) == 18
for i, (ax, aw, ar) in enumerate(CLOUDS):
    for bx, bw, br in CLOUDS[i + 1:]:
        if ax < bx + bw and bx < ax + aw:
            assert abs(ar - br) >= 3, (ax, ar, bx, br)
    for x in range(ax, ax + aw):
        L.put(x, ar, "=")

# 60 gems: arcs over the hops between clouds, then a row over each cloud.
cands = []
path = [(0, 6, 22)] + CLOUDS
for (ax, aw, ar), (bx, bw, br) in zip(path, path[1:]):
    x0, x1 = ax + aw // 2, bx + bw // 2
    top = min(ar, br) - 3
    for i in range(1, 4):
        t = i / 4.0
        cands.append((round(x0 + (x1 - x0) * t), round(top - 2 * (1 - (2 * t - 1) ** 2))))
for x0, w, row in CLOUDS:
    cands += [(x, row - 1) for x in range(x0 + 1, x0 + w - 1)]
placed = set()
for x, y in cands:
    if len(placed) == 60:
        break
    if not (0 <= x < 80 and 1 <= y < 23) or (x, y) in placed or L.get(x, y) != ".":
        continue
    placed.add((x, y))
    L.put(x, y, "g")
assert len(placed) == 60, len(placed)

# Gusts push left at half a cell per frame.
L.at("wind", rect=(20, 8, 28, 20), push="1/2", dir="l")
L.at("wind", rect=(44, 4, 52, 16), push="1/2", dir="l")
L.at("wind", rect=(64, 2, 70, 12), push="1/2", dir="l")

# The moon balloon: the exit on its own little cloud.
for x in range(75, 80):
    L.put(x, 3, "=")
L.on(76, 3, "X")
L.at("deco", 74, 0, kind="text", text="MOON", w=6, h=2)

if __name__ == "__main__":
    L.write(__file__, "01_bonus_cloud_nine.txt")
