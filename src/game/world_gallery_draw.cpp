// Level 39, The Gallery: monitors, cables, boom arcs, the static wipe; the
// Rewind bonus's crushers and tape rewind. Placeholder shapes until the
// art pass.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c);
}

// R-U-N in Morse on a tally light: dot 4, dash 12, gap 4, letter gap 12, 28 dark.
bool tallyOn(int t)
{
  static const int kRun[] = {4, -4, 12, -4, 4, -12, 4, -4, 4, -4, 12, -12, 12, -4, 4, -28};
  int sum = 0;
  for (int v : kRun)
    sum += std::abs(v);
  int u = t % sum;
  for (int v : kRun)
  {
    if (u < std::abs(v))
      return v > 0;
    u -= std::abs(v);
  }
  return false;
}

} // namespace

void World::drawGalleryBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mGallery;
  for (const auto& m : s.monitors)
  {
    const CellBox b = m.box();
    box(r, {b.x - 1, b.y - 1, b.w + 2, b.h + 1}, camX, camY, rgb(20, 22, 26));
    if (m.dead)
    {
      box(r, b, camX, camY, (frame / 2) % 2 ? rgb(90, 90, 96) : rgb(60, 60, 66));
      continue;
    }
    box(r, b, camX, camY, m.virus ? ((frame / 4) % 2 ? rgb(200, 40, 40) : rgb(60, 10, 10)) : rgb(40, 70, 110));
    // Where it leads: a dot for the exit spot; a panning camera's moves.
    int x = 0, y = 0;
    if (monitorExit(m, x, y))
    {
      const float cx = float(b.x) * kCellPx - camX + 2.0f * kCellPx;
      const float cy = float(b.y) * kCellPx - camY + 3.0f * kCellPx;
      float off = 0.0f;
      if (m.cam >= 0)
      {
        const CamSpot& c = s.cams[std::size_t(m.cam)];
        const float len = float(std::max(1, std::abs(c.x1 - c.x0)));
        off = (float(this->camX(c) - std::min(c.x0, c.x1)) / len - 0.5f) * 3.0f * kCellPx;
      }
      r.fillRect(cx + off - 8.0f, cy - 8.0f, 16.0f, 16.0f, rgb(240, 240, 255));
    }
    if (!m.label.empty())
      r.drawText(m.label, float(b.x) * kCellPx - camX + 2.0f * kCellPx, float(b.y) * kCellPx - camY - 26.0f,
                 {18.0f, rgb(220, 230, 255), 0}, Align::Center);
    // The tally light: red while an enemy is awake where it looks.
    bool awake = false;
    if (monitorExit(m, x, y))
      for (const auto& e : mEnemies)
        if (e.alive && e.active && std::abs(e.x - x) < 24 && std::abs(e.y - y) < 14)
          awake = true;
    r.fillRect(float(b.x + 3) * kCellPx - camX, float(b.y) * kCellPx - camY + 4.0f, 14.0f, 14.0f,
               awake && tallyOn(frame / 4) ? rgb(255, 40, 40) : rgb(60, 20, 20));
  }
  for (const auto& c : s.crushers)
  {
    box(r, c.box(), camX, camY, rgb(90, 96, 110));
    box(r, {c.x0 * 2, c.face, c.w * 2, 1}, camX, camY, rgb(230, 200, 60));
  }
}

void World::drawGalleryFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& s = mGallery;
  for (const auto& a : s.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (!e.alive)
      continue;
    if (e.kind == EnemyKind::CableWrangler)
      box(r, {a.cableX0, a.cableY - (a.whip > 0 && !a.dropped ? 2 : 0), a.cableX1 - a.cableX0 + 1, 1}, camX, camY,
          a.dropped ? rgb(60, 60, 60) : rgb(20, 20, 20));
    if (e.kind == EnemyKind::BoomOp && a.pass > 0)
    {
      const CellBox arc = boomArc(e);
      if (a.pass > kGlBoomPass)
        box(r, arc, camX, camY, withAlpha(rgb(200, 200, 210), 180));
      else
        box(r, {arc.x, e.y, arc.w, 1}, camX, camY, withAlpha(rgb(0, 0, 0), 90)); // the mic's shadow
    }
    if (e.kind == EnemyKind::FloorManager && a.count > 0)
    {
      const int total = mPlayer.turbo > 0 ? kGlCountTurbo : kGlCount;
      const int point = total * 7 / 22;
      const int left = a.count <= point ? 3 : std::max(1, 3 - (a.count - point) / std::max(1, (total - point) / 3));
      r.drawText(std::to_string(left), float(e.x + 1) * kCellPx - camX + 16.0f, float(e.y - 7) * kCellPx - camY,
                 {40.0f, rgb(255, 230, 80), 0}, Align::Center);
    }
  }
  (void)frame;
}

void World::drawGalleryHud(Renderer& r, int frame) const
{
  const auto& s = mGallery;
  if (s.hop > 0)
  {
    // The static wipe.
    for (int y = 0; y < kScreenH; y += 16)
    {
      const int v = ((y * 7 + frame * 13) % 5) * 40 + 40;
      r.fillRect(0.0f, float(y), float(kScreenW), 16.0f, rgb(v, v, v));
    }
  }
  if (s.rewinding > 0)
  {
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), withAlpha(rgb(20, 20, 60), 120));
    r.drawText("<< REWIND", 60.0f, 80.0f, {48.0f, rgb(255, 255, 255), 0}, Align::Left);
  }
  if (s.trip > 0)
    r.drawText("TRIPPED!", float(kScreenW) * 0.5f, 140.0f, {36.0f, rgb(255, 230, 80), 0}, Align::Center);
}

} // namespace gr
