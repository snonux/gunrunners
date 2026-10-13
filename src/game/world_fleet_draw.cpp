// Level 34, Zeppelin Relay: drawing (the clouds, the ships with their hulls,
// balloons and names, the rings, the Grapple Gun's line, Sky Pirates, Peck
// Parrots, Balloon Mines and their shrapnel, the captain's log, and the
// Human Cannonball's cannon and nets). See world_fleet.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr Color kHudInk = rgb(10, 8, 20);

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c);
}

} // namespace

void World::drawFleetBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  const auto& s = mFleet;
  for (const auto& sh : s.ships)
  {
    // Where the deck is now, at the in-between time.
    float dx = float(sh.baseX), dy = float(sh.baseY);
    if (sh.platform >= 0)
    {
      const auto& pl = mPlatforms[std::size_t(sh.platform)];
      if (pl.hidden)
        continue;
      dx = float(pl.prevX) + (float(pl.x) - float(pl.prevX)) * alpha;
      dy = float(pl.prevY) + (float(pl.y) - float(pl.prevY)) * alpha;
    }
    const float w = float(sh.deckW());
    const float x = dx * kCellPx - camX, y = dy * kCellPx - camY;
    if (x + w * kCellPx < -200.0f || x > float(kScreenW) + 200.0f)
      continue;
    // Balloon and rigging.
    if (sh.balloon)
    {
      const float by = y - 16.0f * kCellPx, bh = 8.0f * kCellPx;
      r.fillRect(x + 2.0f * kCellPx, by, (w - 4.0f) * kCellPx, bh, rgb(200, 170, 140));
      r.fillRect(x + 2.0f * kCellPx, by + bh * 0.6f, (w - 4.0f) * kCellPx, bh * 0.4f, rgb(170, 140, 116));
      for (int k = 0; k < 4; ++k)
      {
        const float rx = x + (w * kCellPx) * (0.15f + 0.23f * float(k));
        r.drawLine(rx, by + bh, rx, y, 3.0f, rgb(90, 70, 56));
      }
    }
    // Hull, under the deck.
    r.fillRect(x, y + kCellPx, w * kCellPx, 7.0f * kCellPx, rgb(110, 74, 50));
    r.fillRect(x + kCellPx, y + 8.0f * kCellPx, (w - 2.0f) * kCellPx, kCellPx, rgb(84, 56, 38));
    if (!sh.name.empty())
      r.drawText(sh.name, x + w * kCellPx * 0.5f, y + 3.0f * kCellPx, {24.0f, rgb(250, 220, 150), kHudInk}, Align::Center);
    // The deck.
    r.fillRect(x, y, w * kCellPx, kCellPx * 1.0f, rgb(170, 122, 82));
  }
  // Rings.
  for (const auto& rg : s.rings)
    r.fillRect((float(rg.x) - 0.5f) * kCellPx - camX, (float(rg.y) - 0.5f) * kCellPx - camY, 2.0f * kCellPx,
      2.0f * kCellPx, rgb(220, 180, 90));
  // The captain's log.
  if (s.logX >= 0 && !s.logTaken)
    box(r, boxAt(s.logX, s.logY, 2, 2), camX, camY, rgb(240, 220, 160));
  // Human Cannonball: the nets and the cannon.
  for (const auto& n : s.nets)
    box(r, {n.x0, n.y, n.x1 - n.x0 + 1, 1}, camX, camY, n.score >= 5000 ? rgb(255, 90, 90) : rgb(240, 240, 240));
  if (s.cannonball)
    box(r, boxAt(s.cannonX - 1, s.cannonY, 5, 4), camX, camY, rgb(60, 60, 70));
}

void World::drawFleetFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mFleet;
  const auto& p = mPlayer;
  // The Grapple Gun's line: to the ring, or to the hook in flight.
  const float hx = (float(p.x) + 1.5f) * kCellPx - camX, hy = float(p.y - Player::kHeight + 1) * kCellPx - camY;
  if (s.ring >= 0)
  {
    const Ring& rg = s.rings[std::size_t(s.ring)];
    r.drawLine(float(rg.x) * kCellPx - camX + 16.0f, float(rg.y) * kCellPx - camY + 16.0f, hx, hy, 4.0f, rgb(60, 50, 40));
  }
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::GrappleGun))
      r.drawLine(hx, hy, pr.fx * kCellPx - camX, pr.fy * kCellPx - camY, 3.0f, rgb(60, 50, 40));
  // Sky Pirates, Peck Parrots, Balloon Mines.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    if (e.kind == EnemyKind::SkyPirate)
      box(r, b, camX, camY, e.tell > 0 ? rgb(255, 220, 120) : rgb(120, 60, 140));
    else if (e.kind == EnemyKind::PeckParrot)
      box(r, b, camX, camY, e.carrier ? rgb(80, 220, 60) : (e.tell > 0 ? rgb(255, 240, 80) : rgb(230, 60, 50)));
    else if (e.kind == EnemyKind::BalloonMine)
      box(r, b, camX, camY, e.tell > 0 && (frame / 2) % 2 == 0 ? rgb(255, 40, 40) : rgb(90, 90, 100));
  }
  for (const auto& sh : s.shards)
    r.fillRect(sh.x * kCellPx - camX - 6.0f, sh.y * kCellPx - camY - 6.0f, 12.0f, 12.0f, rgb(200, 200, 210));
  // The cloud sea.
  if (s.killY < (1 << 19))
    r.fillRect(0, float(s.killY - 6) * kCellPx - camY, float(kScreenW), float(kScreenH), rgba(255, 240, 245, 150));
}

void World::drawFleetHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& s = mFleet;
  if (!s.cannonball || s.flying)
    return;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "ANGLE %d", s.angle);
  r.drawText(buf, float(kScreenW) - 30.0f, 150.0f, {26.0f, rgb(255, 220, 140), kHudInk}, Align::Right);
}

} // namespace gr
