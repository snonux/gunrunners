// Level 35, The Transmitter: drawing (the lattice behind the tower and the
// cylinder's darkened edges, the doors, Mast Climbers, Relay Drones,
// Feedback Nodes and their rings, the Satellite Swarm, the Halcyon Titan
// with its banners, crosshairs, flames, steam and shockwave, the brochure,
// the flag, and the Copycat bonus's dummies and pits). See world_tower.cpp
// for the logic.

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

void World::drawTowerBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  (void)alpha;
  const auto& s = mTower;
  if (!s.wrap)
    return;
  // The back of the tower through the gaps: a faint lattice that turns the
  // other way to the front.
  const float step = 4.0f * kCellPx;
  const float off = std::fmod(camX * 0.6f, step);
  const float offY = std::fmod(camY, step);
  for (float x = -off - step; x < float(kScreenW) + step; x += step)
    r.drawLine(x, 0, x, float(kScreenH), 3.0f, rgba(120, 40, 40, 70));
  for (float y = -offY; y < float(kScreenH) + step; y += step)
  {
    r.drawLine(0, y, float(kScreenW), y, 2.0f, rgba(120, 40, 40, 50));
    for (float x = -off - step; x < float(kScreenW) + step; x += step)
      r.drawLine(x, y, x + step, y + step, 2.0f, rgba(120, 40, 40, 40));
  }
  // Doors: hatches in the lattice.
  for (const auto& d : s.doors)
  {
    const CellBox b{d.x, d.y - 5, 2, 6};
    box(r, b, camX, camY, rgb(60, 66, 80));
    box(r, {b.x, b.y, 2, 1}, camX, camY, rgb(255, 200, 60));
  }
  // Copycat pits.
  for (const auto& pit : s.pits)
    box(r, pit, camX, camY, rgba(255, 220, 80, 60));
}

void World::drawTowerFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mTower;
  const auto& p = mPlayer;
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    if (e.kind == EnemyKind::MastClimber)
      box(r, b, camX, camY, s.copycat ? rgb(220, 200, 170) : (e.tell > 0 ? rgb(255, 220, 120) : rgb(240, 160, 40)));
    else if (e.kind == EnemyKind::RelayDrone)
      box(r, b, camX, camY, e.tell > 0 && (frame / 2) % 2 == 0 ? rgb(255, 255, 200) : rgb(110, 120, 140));
    else if (e.kind == EnemyKind::FeedbackNode)
    {
      int state = 0, t = 0;
      for (const auto& n : s.nodes)
        if (&mEnemies[std::size_t(n.enemy)] == &e)
        {
          state = n.state;
          t = n.t;
        }
      const float glow = state == 1 ? float(t) / float(kNodeTell) : 0.0f;
      box(r, b, camX, camY, lerpColor(rgb(80, 90, 120), rgb(255, 255, 255), glow));
      if (state == 2)
      {
        const float cx = (float(b.x) + 2.0f) * kCellPx - camX, cy = (float(b.y) + 2.0f) * kCellPx - camY;
        const float rad = float(kNodeRadius) * kCellPx * float(t + 1) / 8.0f;
        for (int k = 0; k < 24; ++k)
        {
          const float a0 = 6.2831853f * float(k) / 24.0f, a1 = 6.2831853f * float(k + 1) / 24.0f;
          r.drawLine(cx + rad * std::cos(a0), cy + rad * std::sin(a0), cx + rad * std::cos(a1), cy + rad * std::sin(a1),
            4.0f, rgba(200, 240, 255, 180));
        }
      }
    }
  }
  // The Satellite Swarm.
  if (p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::SatelliteSwarm)
    for (const auto& d : s.swarm)
      r.fillRect(d.x * kCellPx - camX - 10.0f, d.y * kCellPx - camY - 10.0f, 20.0f, 20.0f,
        d.offline > 0 ? rgb(120, 120, 120) : rgb(160, 255, 255));
  // The brochure, the flag.
  if (s.brochureX >= 0 && !s.brochureTaken)
    box(r, boxAt(s.brochureX, s.brochureY, 2, 2), camX, camY, rgb(255, 240, 200));
  if (s.flagX >= 0)
  {
    const float fx = float(s.flagX) * kCellPx - camX, fy = float(s.flagY) * kCellPx - camY;
    r.drawLine(fx, fy, fx, fy - 2.0f * kCellPx, 3.0f, rgb(200, 200, 200));
    r.fillRect(fx, fy - 2.0f * kCellPx, 24.0f, 14.0f, rgb(120, 255, 120));
  }
  // The Halcyon Titan.
  const auto& t = s.titan;
  if (t.on && t.phase != TitanPhase::Waiting && t.phase != TitanPhase::Done)
  {
    const CellBox b = t.box();
    box(r, b, camX, camY, t.flash > 0 ? rgb(255, 255, 255) : rgb(230, 230, 236));
    // The core, the panels.
    if (t.phase == TitanPhase::Demo)
      box(r, {b.x + 3, b.y + 3, 4, 3}, camX, camY, t.core > 0 ? rgb(80, 180, 255) : rgb(60, 60, 70));
    if (t.phase == TitanPhase::Overheat)
      for (int i = 0; i < 4; ++i)
      {
        if (t.panel[std::size_t(i)] <= 0)
          continue;
        const bool back = i >= 2;
        const int side = (i % 2 == 0 ? -1 : 1);
        const int px = side < 0 ? b.x : b.right();
        const Color c = t.open == i ? (t.openT > kPanelTell ? rgb(255, 60, 30) : rgb(255, 160, 80)) : rgb(150, 150, 160);
        box(r, {px, b.y + (back ? 6 : 2), 1, 3}, camX, camY, c);
      }
    // Laser beam, flames.
    const float midY = float(t.deckY - Player::kHeight + 1) * kCellPx - camY;
    if (t.move == TitanMove::Laser && t.moveT > kLaserTell)
    {
      const float x0 = float(t.facing > 0 ? b.right() + 1 : b.x) * kCellPx - camX;
      r.fillRect(t.facing > 0 ? x0 : x0 - float(kLaserReach) * kCellPx, midY, float(kLaserReach) * kCellPx, 10.0f,
        rgb(255, 60, 60));
    }
    if (t.move == TitanMove::Flame && t.moveT > kFlameTell)
    {
      const float x0 = float(t.facing > 0 ? b.right() + 1 : b.x - kFlameReach) * kCellPx - camX;
      r.fillRect(x0, float(t.deckY - 6) * kCellPx - camY, float(kFlameReach) * kCellPx, 6.0f * kCellPx,
        rgba(255, 140, 40, 160));
    }
    if (t.move == TitanMove::Missiles)
      for (int c : t.cross)
      {
        const int cx = p.x + towerDx(p.x, c);
        box(r, {cx - 1, t.deckY - 1, 3, 1}, camX, camY, (frame / 2) % 2 ? rgb(255, 40, 40) : rgb(255, 200, 200));
      }
    if (t.wave >= 0)
    {
      const float mid = (float(t.x) + float(kTitanW) * 0.5f) * kCellPx - camX;
      for (int sd : {-1, 1})
        r.fillRect(mid + float(sd * t.wave) * kCellPx - 8.0f, float(t.deckY - 2) * kCellPx - camY, 16.0f, 2.0f * kCellPx,
          rgba(255, 255, 255, 160));
    }
  }
  // The tower is round: its sides fall away into shadow.
  if (s.wrap)
    for (int k = 0; k < 8; ++k)
    {
      const float w = float(kScreenW) * 0.03f;
      const int a = 20 + k * 18;
      r.fillRect(float(7 - k) * w, 0, w, float(kScreenH), rgba(10, 8, 30, a));
      r.fillRect(float(kScreenW) - float(8 - k) * w, 0, w, float(kScreenH), rgba(10, 8, 30, a));
    }
}

void World::drawTowerHud(Renderer& r, int frame) const
{
  const auto& s = mTower;
  const auto& t = s.titan;
  if (t.banner > 0 && !t.bannerText.empty())
    r.drawText(t.bannerText.c_str(), float(kScreenW) * 0.5f, 150.0f, {30.0f, rgb(120, 220, 255), kHudInk}, Align::Center);
  if (titanFight() && t.phase != TitanPhase::Intro)
  {
    const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 54.0f;
    r.fillRect(bx - 10, by - 30, bw + 20, 52, rgba(8, 6, 22, 200));
    r.drawText("HALCYON TITAN", bx, by - 26, {17.0f, rgb(120, 220, 255), kHudInk});
    const float frac = float(t.total()) / 92.0f;
    r.fillRect(bx, by, bw, 12, rgba(255, 255, 255, 40));
    r.fillRect(bx, by, bw * frac, 12, (t.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : rgb(80, 180, 255));
    for (const float cut : {32.0f / 92.0f, 62.0f / 92.0f})
      r.fillRect(bx + bw * cut - 1.0f, by - 3, 3, 18, rgb(20, 16, 30));
  }
  if (s.copycat)
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "CAUGHT %d", s.caught);
    r.drawText(buf, float(kScreenW) - 30.0f, 150.0f, {26.0f, rgb(255, 220, 140), kHudInk}, Align::Right);
  }
}

} // namespace gr
