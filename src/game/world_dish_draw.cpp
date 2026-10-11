// Level 32, Dish Array: drawing (the satellite crossing the sky, the sand,
// the dishes' bowls behind their rims, the struts' groan, the Jammer's cone,
// scrap and its shadow, the mechs' aim lines and beams, Sand Sharks' fins,
// Vulture Drones, Scorpion Mechs, the minidishes, the dummy, the cactus and
// its hat, and Heatstroke's swap warning). See world_dish.cpp for the logic.

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

float px(int cells, float cam) { return float(cells) * kCellPx - cam; }

} // namespace

void World::drawDishBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mDish;
  // The satellite: a moving star across the sky, always on screen.
  {
    const float f = float(mStats.frames % std::max(1, s.satellitePeriod)) / float(std::max(1, s.satellitePeriod));
    const float x = 80.0f + f * float(kScreenW - 160), y = 70.0f + 30.0f * std::sin(f * 3.14159265f);
    r.fillRect(x - 4.0f, y - 4.0f, 8.0f, 8.0f, rgb(255, 255, 230));
    for (int i = 0; i < 3; ++i)
      r.fillRect(80.0f + float(i) * float(kScreenW - 160) / 2.0f - 2.0f, 60.0f, 4.0f, 4.0f, rgb(120, 140, 180));
  }
  // The sand.
  for (const auto& b : s.sand)
    box(r, b, camX, camY, rgb(220, 180, 110));
  // Each ride's dish bowl behind it, groaning before a turn.
  for (const auto& ride : s.rides)
  {
    const Platform& pl = mPlatforms[std::size_t(ride.platform)];
    const DishPhase ph = ridePhase(ride.dish);
    const float shake = ph.groan && (frame / 2) % 2 == 0 ? 2.0f : 0.0f;
    r.fillRect(px(pl.x, camX) + shake, px(pl.y + 2, camY), float(pl.w) * kCellPx, 3.0f * kCellPx, rgba(230, 230, 236, 120));
  }
  // The minidishes.
  for (const auto& m : s.minis)
  {
    const Color c = m.pose == 1 ? rgb(240, 240, 250) : rgb(190, 190, 200);
    box(r, {m.x, m.y - 1, 2, 2}, camX, camY, c);
    const int tip = m.pose == 0 ? 2 : (m.pose == 1 ? 1 : 0);
    r.fillRect(px(m.x + tip, camX) - 4.0f, px(m.y - 2, camY), 8.0f, 8.0f, rgb(90, 90, 100));
  }
  // The dummy.
  if (s.dummyX >= 0)
  {
    r.fillRect(px(s.dummyX, camX) - 32.0f, px(s.dummyY, camY) - 32.0f, 64.0f, 64.0f, rgb(250, 250, 250));
    r.fillRect(px(s.dummyX, camX) - 20.0f, px(s.dummyY, camY) - 20.0f, 40.0f, 40.0f, rgb(220, 40, 40));
    r.fillRect(px(s.dummyX, camX) - 8.0f, px(s.dummyY, camY) - 8.0f, 16.0f, 16.0f, rgb(250, 250, 250));
  }
  // The cactus and its cowboy hat.
  if (s.cactusX >= 0)
  {
    box(r, {s.cactusX, s.cactusY - 5, 2, 6}, camX, camY, rgb(70, 140, 70));
    const float lift = s.hatUp > 0 ? float(std::min(s.hatUp, 20 - s.hatUp)) * 6.0f : 0.0f;
    r.fillRect(px(s.cactusX, camX) - 8.0f, px(s.cactusY - 6, camY) - lift, 80.0f, 12.0f, rgb(140, 90, 50));
  }
}

void World::drawDishFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mDish;
  // Scrap and its shadow on the ground, shrinking as it falls.
  for (const auto& sc : s.scraps)
  {
    const int left = std::max(0, sc.groundY - sc.y);
    const float w = std::clamp(80.0f - float(left) * 1.0f, 16.0f, 80.0f);
    r.fillRect(px(sc.x + 1, camX) - w / 2.0f, px(sc.groundY + 2, camY) - 8.0f, w, 8.0f, rgba(0, 0, 0, 110));
    box(r, {sc.x, sc.y, 2, 2}, camX, camY, sc.carrier ? rgb(120, 170, 80) : rgb(150, 140, 130));
  }
  // Jammer cones.
  for (const auto& c : s.cones)
    box(r, c.box(), camX, camY, rgba(255, 220, 120, 40 + c.life * 12));
  // Beams.
  for (const auto& b : s.beams)
    r.drawLine(px(b.x0, camX), px(b.y0, camY), px(b.x1, camX), px(b.y1, camY), 6.0f, rgb(255, 60, 60), Blend::Add);
  // The enemies.
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const Enemy& e = mEnemies[i];
    if (!e.alive)
      continue;
    const CellBox b = e.box();
    const Color flash = rgb(255, 255, 255);
    bool jammed = false;
    const Robot* rb = nullptr;
    for (const auto& ro : s.robots)
      if (ro.enemy == int(i))
      {
        rb = &ro;
        jammed = ro.jam > 0;
      }
    switch (e.kind)
    {
      case EnemyKind::SandShark:
        if (e.hidden)
        {
          // Only the fin (still while the sand bulges).
          const bool tell = e.tell > 0;
          r.fillRect(px(e.x + e.w / 2 - 1, camX), px(e.y - (tell ? 0 : 1), camY), 48.0f, tell ? 16.0f : 32.0f,
            tell ? rgb(200, 160, 100) : rgb(110, 120, 140));
        }
        else
          box(r, b, camX, camY, e.flash > 0 ? flash : rgb(110, 120, 140));
        break;
      case EnemyKind::VultureDrone:
        box(r, b, camX, camY, e.flash > 0 ? flash : (jammed ? rgb(255, 220, 120) : (e.carrier ? rgb(120, 160, 90) : rgb(90, 90, 100))));
        break;
      case EnemyKind::ScorpionMech:
        box(r, b, camX, camY, e.flash > 0 ? flash : (jammed ? rgb(255, 220, 120) : rgb(160, 110, 70)));
        if (rb && rb->aimState > 0)
        {
          const int ox = e.x + e.w / 2, oy = e.y - e.h + 1;
          const bool locked = rb->aimState == 2;
          float dx = float(rb->ax - ox), dy = float(rb->ay - oy);
          const float len = std::max(0.5f, std::sqrt(dx * dx + dy * dy));
          dx = dx / len * float(kBeamLength);
          dy = dy / len * float(kBeamLength);
          if (!locked || (frame / 2) % 2 == 0)
            r.drawLine(px(ox, camX), px(oy, camY), px(ox, camX) + dx * kCellPx, px(oy, camY) + dy * kCellPx,
              locked ? 3.0f : 2.0f, rgba(255, 40, 40, locked ? 230 : 140));
        }
        break;
      default:
        break;
    }
  }
}

void World::drawDishHud(Renderer& r, int frame) const
{
  const auto& s = mDish;
  if (!s.swapLr)
    return;
  // The swap icon blinks for the 22 frames before a swap, and stays lit
  // (small) while swapped.
  const bool warn = s.swapT >= kSwapEvery - kSwapWobble;
  if (warn && (frame / 4) % 2 == 0)
    r.fillRect(float(kScreenW) / 2.0f - 40.0f, 90.0f, 80.0f, 24.0f, rgb(255, 120, 60));
  else if (s.swapped)
    r.fillRect(float(kScreenW) / 2.0f - 20.0f, 96.0f, 40.0f, 12.0f, rgb(255, 180, 120));
}

} // namespace gr
