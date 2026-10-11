// Level 33, Magma Mainframe: drawing (the rising lava and its glow, the
// coolant purges' steam, the corrupted rack's dust, the Fan Bots' gusts,
// Firewalls, Lava Daemons, Fan Bots, the RATINGS server, the DO NOT REBOOT
// rack and its reboot screen, the Heat Sink's cone, and the lava gauge).
// See world_magma.cpp for the logic.

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

void World::drawMagmaBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mMagma;
  // The corrupted rack and its dust.
  for (const auto& rk : s.racks)
  {
    box(r, rk.box(), camX, camY, rk.hp > 0 ? rgb(60, 90, 50) : rgb(50, 50, 56));
    if (rk.blowing())
      box(r, rk.dust, camX, camY, rgba(120, 255, 80, 70 + (frame % 4) * 10));
    else if (rk.spinning())
      box(r, rk.dust, camX, camY, rgba(120, 255, 80, 25));
  }
  // The DO NOT REBOOT rack and the RATINGS server.
  if (s.rebootX >= 0)
    box(r, s.rebootBox(), camX, camY, s.reboot > 0 ? rgb(40, 60, 160) : rgb(70, 74, 84));
  if (s.serverX >= 0)
    box(r, s.serverBox(), camX, camY, s.serverShot ? rgb(80, 80, 90) : rgb(230, 190, 60));
  // The Fan Bots' gusts.
  for (const auto& f : s.fans)
    if (f.zone >= 0 && mEnemies[std::size_t(f.enemy)].alive && mStats.frames % kFanPeriod < kFanOn)
      box(r, mZones[std::size_t(f.zone)].box, camX, camY, rgba(200, 220, 255, 40));
  // Purges venting.
  for (const auto& pu : s.purges)
    if (pu.steam > 0)
      box(r, {pu.x - 2, pu.y - 10, 6, 10}, camX, camY, rgba(230, 240, 255, 6 * pu.steam));
}

void World::drawMagmaFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mMagma;
  // Firewalls, Lava Daemons, Fan Bots.
  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const CellBox b = e.box();
    if (e.kind == EnemyKind::Firewall)
    {
      const int tall = e.dive ? kWallLow : kWallTall;
      const bool flick = e.tell > 0 && (frame / 2) % 2 == 0;
      box(r, {b.x, e.y - tall + 1, b.w, tall}, camX, camY, flick ? rgb(255, 240, 160) : rgb(255, 110, 30));
    }
    else if (e.kind == EnemyKind::LavaDaemon)
      box(r, b, camX, camY, e.tell > 0 ? rgb(255, 200, 60) : rgb(150, 40, 20));
    else if (e.kind == EnemyKind::FanBot)
    {
      box(r, b, camX, camY, rgb(110, 116, 130));
      const float spin = e.dive ? 1.0f : (e.tell > 0 ? 0.5f : 0.0f);
      box(r, {e.dir > 0 ? b.right() : b.x, b.y + 1, 1, b.h - 2}, camX, camY, rgba(230, 230, 240, int(120 + 120 * spin)));
    }
  }
  // The lava.
  if (s.rising)
  {
    const CellBox lava{s.x0, s.surface, s.x1 - s.x0 + 1, s.floorY - s.surface + 1};
    box(r, lava, camX, camY, rgba(255, 90, 20, 230));
    box(r, {s.x0, s.surface, s.x1 - s.x0 + 1, 1}, camX, camY, rgb(255, 200, 80));
  }
  if (s.gutter.w > 0)
    box(r, s.gutter, camX, camY, rgba(255, 120, 30, 220));
  // The Heat Sink's cone.
  const auto& p = mPlayer;
  if (s.sucking)
  {
    const int row = p.y - 2;
    const int mx = p.facing > 0 ? p.x + Player::kWidth : p.x - kSinkReach;
    box(r, {mx, row - 2, kSinkReach, 5}, camX, camY, rgba(255, 180, 80, 50));
  }
}

void World::drawMagmaHud(Renderer& r, int frame) const
{
  const auto& s = mMagma;
  if (s.reboot > 0)
  {
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(20, 40, 160, 200));
    r.drawText("REBOOTING...", float(kScreenW) / 2.0f, float(kScreenH) / 2.0f - 20.0f,
      {40.0f, rgb(255, 255, 255), kHudInk, true}, Align::Center);
  }
  if (!s.rising)
    return;
  // The lava gauge: blocks below the runner.
  char buf[32];
  std::snprintf(buf, sizeof(buf), "LAVA %d", lavaGap());
  const bool close = lavaGap() < 8 && (frame / 8) % 2 == 0;
  r.drawText(buf, float(kScreenW) - 30.0f, 150.0f, {26.0f, close ? rgb(255, 80, 40) : rgb(255, 190, 80), kHudInk}, Align::Right);
}

} // namespace gr
