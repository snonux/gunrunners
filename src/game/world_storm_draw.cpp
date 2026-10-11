// Level 31, Storm Relay: drawing (metal glowing before a strike, the bolts
// and arcs, machines, stuck Tesla rods, Kite Drones on their tethers, Storm
// Gulls, Foghorns and their blasts, the fry gull, the lamp and the ship's
// Morse, and Bungee's rope, post and rings). See world_storm.cpp for the
// logic.

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

void World::drawStormBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mStorm;
  // The ship on the horizon answers the lamp: G-U-N.
  if (s.morse >= 0 && s.lamp >= 0)
  {
    const Machine& m = s.machines[std::size_t(s.lamp)];
    const float x = px(m.box.x + 40, camX * 0.5f), y = px(m.box.y + 6, camY * 0.5f);
    r.fillRect(x - 40.0f, y + 8.0f, 80.0f, 10.0f, rgb(30, 36, 48));
    if (morseLight(s.morse))
      r.fillRect(x - 6.0f, y - 6.0f, 12.0f, 12.0f, rgb(255, 250, 200));
  }
  // Machines: dead grey housings; powered ones hum.
  for (const auto& m : s.machines)
  {
    const bool hum = m.powered && (frame / 8) % 2 == 0;
    box(r, m.box, camX, camY, m.powered ? (hum ? rgb(150, 190, 230) : rgb(120, 150, 190)) : rgb(90, 96, 104));
    if (m.kind == MachineKind::Lamp && m.powered)
      box(r, {m.box.x - 1, m.box.y - 1, m.box.w + 2, m.box.h + 2}, camX, camY, rgba(255, 240, 160, 90));
  }
  // Kite Drones' tethers.
  for (const auto& k : s.kites)
  {
    if (std::size_t(k.enemy) >= mEnemies.size() || !mEnemies[std::size_t(k.enemy)].alive)
      continue;
    const Enemy& e = mEnemies[std::size_t(k.enemy)];
    const Color c = k.spark > 0 ? rgb(255, 255, 160) : rgb(70, 70, 80);
    r.drawLine(px(k.ax, camX), px(k.ay, camY), px(e.x + e.w / 2, camX), px(e.y - e.h / 2, camY), k.spark > 0 ? 4.0f : 2.0f,
      c);
    r.fillRect(px(k.ax, camX) - 8.0f, px(k.ay, camY) - 8.0f, 16.0f, 16.0f, rgb(60, 60, 66));
  }
  // The fry gull on its post (gone once it flaps off).
  if (s.fryX >= 0 && s.fryFlown < 60)
  {
    const float x = px(s.fryX, camX), y = px(s.fryY - 3 - s.fryFlown / 2, camY) ;
    r.fillRect(x, y, 48.0f, 32.0f, rgb(230, 230, 236));
    r.fillRect(x + 40.0f, y + 8.0f, 14.0f, 4.0f, rgb(240, 200, 60)); // the fry
  }
  // Bungee: the post and its rings.
  if (s.bungee)
  {
    box(r, s.tower, camX, camY, rgb(200, 196, 186));
    r.fillRect(px(int(s.postX) - 1, camX), px(int(s.postY) - 1, camY), 64.0f, 64.0f, rgb(180, 60, 50));
    for (std::size_t i = 0; i < s.rings.size(); ++i)
    {
      if (s.ringTaken[i])
        continue;
      const float x = px(s.rings[i].first, camX), y = px(s.rings[i].second, camY);
      const Color c = rgb(120, 200, 255);
      r.drawLine(x - 40.0f, y, x, y - 40.0f, 6.0f, c);
      r.drawLine(x, y - 40.0f, x + 40.0f, y, 6.0f, c);
      r.drawLine(x + 40.0f, y, x, y + 40.0f, 6.0f, c);
      r.drawLine(x, y + 40.0f, x - 40.0f, y, 6.0f, c);
    }
  }
}

void World::drawStormFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mStorm;
  // Metal glowing before its strike, and crackling while it hurts.
  for (const auto& z : s.zones)
  {
    if (z.glowing() && (frame / 4) % 2 == 0)
      for (const auto& b : struckMetal(z, z.target))
        box(r, b, camX, camY, rgba(170, 210, 255, 110));
    if (z.hurt > 0)
    {
      const float x = px(z.boltX, camX);
      float y = px(z.boltY, camY);
      for (float top = y; top > -64.0f; top -= 48.0f)
      {
        const float jag = ((int(top) / 48) % 2 == 0) ? 12.0f : -12.0f;
        r.drawLine(x + jag, top - 48.0f, x - jag, top, 8.0f, rgb(255, 255, 255), Blend::Add);
      }
      for (const auto& b : z.live)
        box(r, b, camX, camY, rgba(255, 255, 255, 160));
    }
  }
  // Bungee: the rope from the post to the runner.
  if (s.bungee)
  {
    const auto& p = mPlayer;
    const float dx = s.postX - (float(p.x) + 1.5f), dy = s.postY - (float(p.y) - 2.0f);
    const bool taut = std::sqrt(dx * dx + dy * dy) > kBungeeRest;
    r.drawLine(s.postX * kCellPx - camX, s.postY * kCellPx - camY, (float(p.x) + 1.5f) * kCellPx - camX,
      (float(p.y) - 2.0f) * kCellPx - camY, taut ? 4.0f : 3.0f, taut ? rgb(240, 200, 80) : rgb(200, 170, 90));
  }
  for (const auto& a : s.arcs)
    r.drawLine(px(a.x0, camX), px(a.y0, camY), px(a.x1, camX), px(a.y1, camY), 4.0f, rgb(255, 255, 255), Blend::Add);
  // Stuck rods.
  for (const auto& rd : s.rods)
    box(r, rd.box(), camX, camY, rd.life < 60 && (frame / 4) % 2 == 0 ? rgb(120, 130, 160) : rgb(170, 200, 255));
  // The enemies.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    const Color flash = rgb(255, 255, 255);
    switch (e.kind)
    {
      case EnemyKind::StormGull:
        box(r, b, camX, camY, e.flash > 0 ? flash : (e.dive == 1 ? rgb(255, 230, 230) : rgb(200, 204, 214)));
        break;
      case EnemyKind::KiteDrone:
        box(r, b, camX, camY, e.flash > 0 ? flash : (e.tell > 0 ? rgb(255, 160, 90) : rgb(120, 130, 150)));
        break;
      case EnemyKind::Foghorn:
      {
        box(r, b, camX, camY, e.flash > 0 ? flash : (e.carrier ? rgb(120, 170, 110) : rgb(170, 60, 50)));
        if (e.dive > 0)
        {
          const int dir = e.dir < 0 ? -1 : 1;
          const CellBox cone{dir < 0 ? e.x - kFogLength : e.x + e.w, e.y - kFogHeight + 1, kFogLength, kFogHeight};
          box(r, cone, camX, camY, e.carrier ? rgba(120, 255, 120, 60) : rgba(220, 230, 240, 60));
        }
        break;
      }
      default:
        break;
    }
  }
}

void World::drawStormHud(Renderer& r, int frame) const
{
  (void)r;
  (void)frame;
}

} // namespace gr
