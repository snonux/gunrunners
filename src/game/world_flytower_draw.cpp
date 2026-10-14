// Level 40, The Fly Tower: nozzles and flames, truss chase lights, confetti
// cannons, followspot beams, laser sights, sandbags, the towel, the Q99
// cannon and the cue board; Trigger Happy's discs. Placeholder shapes until
// the art pass.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c, Blend bl = Blend::Alpha)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c, bl);
}

} // namespace

void World::drawFlyTowerBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mFly;
  // Nozzles (and the Q99 cannon with its fuse box), glowing before they burn.
  for (const auto& py : s.pyros)
  {
    const int heat = pyroHeat(py, kFtGlow);
    const CellBox nozzle{py.x * kCellsPerTile, py.y * kCellsPerTile + 1, py.dir != 0 ? 2 : py.w * kCellsPerTile, 1};
    if (!isOnScreen(nozzle, 4) && !isOnScreen(py.flame(), 2))
      continue;
    box(r, nozzle, camX, camY, rgb(60, 56, 60));
    if (heat == 1)
      box(r, nozzle, camX, camY, (frame / 3) % 2 ? rgb(255, 140, 30) : rgb(200, 60, 20));
    if (py.dir != 0)
    {
      box(r, {py.x * kCellsPerTile, py.y * kCellsPerTile - 1, 2, 3}, camX, camY, rgb(70, 70, 80));
      if (py.fuse > 0 || py.fuseT >= 0)
        box(r, py.fuseBox(), camX, camY,
            py.fuseT >= 0 && (frame / 2) % 2 ? rgb(255, 220, 60) : rgb(150, 40, 40));
    }
  }
  // Trusses: chase lights run along the top before one moves.
  for (const auto& t : s.trusses)
  {
    const auto& pl = mPlatforms[std::size_t(t.platform)];
    if (!isOnScreen(pl.box(), 4))
      continue;
    box(r, {pl.x, pl.y + 1, pl.w, 1}, camX, camY, rgb(30, 30, 34));
    bool chase = false;
    for (const auto& c : s.cues)
      if (c.kind == FtCueKind::Truss && std::find(c.targets.begin(), c.targets.end(), int(&t - s.trusses.data())) !=
            c.targets.end())
      {
        const int in = cueIn(c);
        chase = chase || (in > 0 && in <= kFtChase);
      }
    chase = chase || pl.y != t.stops[std::size_t(t.goal)];
    if (chase)
      for (int x = pl.x; x < pl.x + pl.w; ++x)
        if ((x + frame / 2) % 3 == 0)
          box(r, {x, pl.y, 1, 1}, camX, camY, rgb(255, 210, 80), Blend::Add);
    if (t.waiting && (frame / 4) % 2)
      box(r, {pl.x, pl.y, pl.w, 1}, camX, camY, withAlpha(rgb(255, 60, 40), 160));
  }
  // Confetti cannons: they rattle before the blast.
  for (const auto& cf : s.confetti)
  {
    int rattle = 0;
    for (const auto& c : s.cues)
      if (c.kind == FtCueKind::Confetti &&
          std::find(c.targets.begin(), c.targets.end(), int(&cf - s.confetti.data())) != c.targets.end())
      {
        const int in = cueIn(c);
        if (in > 0 && in <= kFtConfettiTell)
          rattle = (frame / 2) % 2 ? 1 : -1;
      }
    const float x = float(cf.x * kCellsPerTile) * kCellPx - camX + float(rattle) * 4.0f;
    const float y = float(cf.y * kCellsPerTile) * kCellPx - camY;
    r.fillRect(x, y + 16.0f, 2.0f * kCellPx, kCellPx * 1.5f, rgb(180, 40, 160));
    r.fillRect(cf.dir < 0 ? x - 16.0f : x + 2.0f * kCellPx, y + 20.0f, 16.0f, 24.0f, rgb(230, 230, 240));
  }
  // The towel.
  if (s.towelY >= 0)
  {
    box(r, {s.towelX - 1, s.towelY - 1, 4, 2}, camX, camY, s.towelUsed ? rgb(150, 150, 160) : rgb(250, 250, 255));
    r.drawText("42", float(s.towelX + 1) * kCellPx - camX, float(s.towelY - 1) * kCellPx - camY, {22.0f, rgb(40, 60, 160), 0},
               Align::Center);
  }
  // Trigger Happy's discs.
  for (const auto& t : s.targets)
  {
    if (t.hit)
      continue;
    const CellBox b = t.box();
    box(r, b, camX, camY, rgb(240, 240, 240));
    box(r, {b.x, b.y + 1, 2, 2}, camX, camY, rgb(220, 40, 40));
  }
}

void World::drawFlyTowerFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mFly;
  const auto& p = mPlayer;
  // Burning flames.
  for (const auto& py : s.pyros)
  {
    if (pyroHeat(py, 0) != 2 || !isOnScreen(py.flame(), 2))
      continue;
    const CellBox f = py.flame();
    box(r, f, camX, camY, withAlpha(rgb(255, 120, 20), 150), Blend::Add);
    for (int k = 0; k < 6; ++k)
    {
      const int fx = f.x + (k * 7 + frame) % std::max(1, f.w);
      const int fy = f.y + (k * 13 + frame * 3) % std::max(1, f.h);
      box(r, {fx, fy, 1, 2}, camX, camY, rgb(255, 230, 120), Blend::Add);
    }
  }
  // The confetti blast.
  for (const auto& c : s.cues)
  {
    if (c.kind != FtCueKind::Confetti || c.list < 0)
      continue;
    const auto& l = s.lists[std::size_t(c.list)];
    if (!l.started || l.t < c.t || l.t - c.t >= kFtConfettiPush)
      continue;
    for (int k : c.targets)
    {
      const CellBox lane = s.confetti[std::size_t(k)].lane();
      for (int i = 0; i < 24; ++i)
      {
        const int x = lane.x + (i * 11 + frame * 3) % lane.w, y = lane.y + (i * 5 + frame) % lane.h;
        static const Color kBits[] = {rgb(255, 80, 80), rgb(80, 220, 255), rgb(255, 230, 60), rgb(160, 255, 120)};
        box(r, {x, y, 1, 1}, camX, camY, kBits[i % 4]);
      }
    }
  }
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (!e.alive || !isOnScreen(e.box(), 12))
      continue;
    const float ex = (float(e.x) + float(e.w) * 0.5f) * kCellPx - camX, ey = float(e.y - e.h + 1) * kCellPx - camY;
    if (e.kind == EnemyKind::Followspot && a.lamp)
    {
      // The beam: white, amber once it has the runner.
      const Color c = a.lock >= kFtSpotLock ? rgb(255, 180, 60) : rgb(255, 255, 230);
      const float bx = float(a.beamX) * kCellPx - camX, by = float(p.y + 1) * kCellPx - camY;
      for (int k = -3; k <= 3; ++k)
        r.drawLine(ex, ey + 32.0f, bx + float(k) * 24.0f, by, 6.0f, withAlpha(c, 40), Blend::Add);
      r.fillRect(bx - 96.0f, by - 12.0f, 192.0f, 12.0f, withAlpha(c, 90), Blend::Add);
    }
    if (e.kind == EnemyKind::Rigger && a.aim > 0)
    {
      // The laser sight, the whole aim long.
      const float fx = float(e.dir > 0 ? e.x + e.w : e.x - 1) * kCellPx - camX, fy = float(e.y - 3) * kCellPx - camY;
      r.drawLine(fx, fy, float(a.aimX) * kCellPx - camX, float(a.aimY) * kCellPx - camY, 3.0f,
                 withAlpha(rgb(255, 30, 30), a.aim >= kFtRiggerAim - kFtRiggerBeep && (frame / 2) % 2 ? 240 : 150));
    }
    if (e.kind == EnemyKind::SandbagDrop && a.tell > 0)
    {
      // The bag hangs; its shadow grows on the floor below.
      box(r, {a.bagX, a.bagY, 4, 4}, camX, camY, rgb(150, 120, 80));
      int floor = a.bagY + 4;
      while (floor < mMap.height() && !mMap.solidTop(a.bagX + 1, floor) && !mMap.solid(a.bagX + 1, floor))
        ++floor;
      const float w = 4.0f * kCellPx * float(a.tell) / float(kFtBagTell);
      r.fillRect(float(a.bagX + 2) * kCellPx - camX - w * 0.5f, float(floor) * kCellPx - camY - 8.0f, w, 8.0f,
                 withAlpha(rgb(0, 0, 0), 140));
    }
  }
  for (const auto& b : s.bags)
  {
    box(r, b.box(), camX, camY, rgb(150, 120, 80));
    r.drawText("LB", float(b.x + 2) * kCellPx - camX, float(b.y + 1) * kCellPx - camY, {20.0f, rgb(60, 40, 20), 0},
               Align::Center);
  }
  // The Pyro Rig's flame stream.
  if (s.stream > 0 && p.weapon == Weapon::Proto && p.proto == int(ProtoId::PyroRig))
  {
    const CellBox cone = pyroStream();
    box(r, cone, camX, camY, withAlpha(rgb(255, 140, 30), 170), Blend::Add);
    box(r, {cone.x, cone.y + (frame / 2) % 2, cone.w, 1}, camX, camY, rgb(255, 240, 160), Blend::Add);
  }
}

void World::drawFlyTowerHud(Renderer& r, int frame) const
{
  const auto& s = mFly;
  if (s.trigger)
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "DISCS %d / %d", s.hits, int(s.targets.size()));
    r.drawText(buf, float(kScreenW) * 0.5f, 96.0f, {30.0f, rgb(255, 230, 120), rgb(0, 0, 0)}, Align::Center);
    return;
  }
  if (s.active < 0)
    return;
  // The cue board: the active zone's next three cues and their countdowns.
  struct Line
  {
    const FtCue* cue;
    int in;
  };
  std::vector<Line> lines;
  for (const auto& c : s.cues)
  {
    if (c.list != s.active)
      continue;
    if (c.kind == FtCueKind::Cut)
    {
      lines.push_back({&c, 1 << 20});
      continue;
    }
    const int in = cueIn(c);
    if (in < 0 || (c.warn > 0 && in > c.warn))
      continue;
    lines.push_back({&c, c.kind == FtCueKind::Wave ? -1 : in});
  }
  std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.in < b.in; });
  if (lines.size() > 3)
    lines.resize(3);
  if (lines.empty())
    return;
  const float y = 84.0f; // under the HUD panels
  r.fillRect(0.0f, y, float(kScreenW), 34.0f, withAlpha(rgb(10, 10, 14), 200));
  float x = 24.0f;
  for (const auto& ln : lines)
  {
    const FtCue& c = *ln.cue;
    char buf[96];
    if (c.kind == FtCueKind::Cut)
      std::snprintf(buf, sizeof(buf), "%s %s", c.id.c_str(), c.label.c_str());
    else if (c.kind == FtCueKind::Wave)
      std::snprintf(buf, sizeof(buf), "%s %s LIVE", c.id.c_str(), c.label.c_str());
    else
      std::snprintf(buf, sizeof(buf), "%s %s %d.%d", c.id.c_str(), c.label.c_str(), ln.in / 15, (ln.in % 15) * 10 / 15);
    bool struck = c.kind == FtCueKind::Cut;
    if (struck)
      for (int k : c.targets)
        if (s.pyros[std::size_t(k)].faced > 0)
          struck = false; // it flickers back while the runner faces its cannon
    const Color col = struck ? rgb(130, 130, 140) : ln.in >= 0 && ln.in <= 15 && (frame / 3) % 2 ? rgb(255, 80, 60)
                                                                                                  : rgb(255, 220, 140);
    const float w = r.drawText(buf, x, y + 4.0f, {24.0f, col, 0}, Align::Left);
    if (struck)
      r.fillRect(x, y + 17.0f, w, 3.0f, col);
    x += w + 40.0f;
  }
}

} // namespace gr
