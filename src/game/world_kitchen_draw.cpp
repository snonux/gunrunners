// Level 24, Too Many Cooks: drawing (belts and their arrows, the chef's
// bells, the pantry flaps, sauce pans, the sink, the toaster, cleavers
// stuck in walls and the chef's hand). See world_kitchen.cpp for the logic.

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

} // namespace

void World::drawKitchenBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  (void)frame;
  const auto& k = mKitchen;
  // Sauce pans and the sink.
  for (const auto& s : k.sauces)
  {
    const float x = float(s.x0) * kTilePx - camX, y = float(s.y0) * kTilePx - camY;
    const float w = float(s.x1 - s.x0 + 1) * kTilePx, h = float(s.y1 - s.y0 + 1) * kTilePx;
    if (!offScreen(x, y, w, h))
      r.fillRect(x, y + h * 0.5f, w, h * 0.5f, rgb(170, 40, 30));
  }
  for (const auto& s : k.sinks)
  {
    const float x = float(s.x0) * kTilePx - camX, y = float(s.y0) * kTilePx - camY;
    const float w = float(s.x1 - s.x0 + 1) * kTilePx, h = float(s.y1 - s.y0 + 1) * kTilePx;
    if (!offScreen(x, y, w, h))
      r.fillRect(x, y, w, h, withAlpha(rgb(120, 170, 200), 120));
  }
  // The toaster and its lever.
  if (k.toaster.x0 >= 0)
  {
    const float x = float(k.toaster.x0) * kTilePx - camX, y = float(k.toaster.y0) * kTilePx - camY;
    const float w = float(k.toaster.x1 - k.toaster.x0 + 1) * kTilePx;
    if (k.popped > 0)
      r.fillRect(x + 24.0f, y - 40.0f, w - 48.0f, 48.0f, rgb(220, 170, 90));
    if (k.leverX >= 0)
      r.fillRect(float(k.leverX) * kTilePx - camX + 8.0f, float(k.leverY) * kTilePx - camY + (k.toast > 0 ? 40.0f : 16.0f),
        40.0f, 12.0f, rgb(60, 60, 60));
  }
}

void World::drawKitchenFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& k = mKitchen;
  // Belts: a band over their row with arrows that scroll their way.
  for (const auto& b : k.belts)
  {
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y) * kTilePx - camY;
    const float w = float(b.x1 - b.x0 + 1) * kTilePx;
    if (offScreen(x, y, w, kTilePx))
      continue;
    const Color band = b.hang ? rgb(150, 150, 160) : rgb(60, 60, 70);
    r.fillRect(x, y + (b.hang ? 24.0f : 0.0f), w, b.hang ? 16.0f : 20.0f, band);
    const bool flash = b.flash > 0 && (frame / 2) % 2 == 0;
    const Color arrow = flash ? rgb(255, 240, 120) : rgb(240, 240, 240);
    const float shift = std::fmod(float(b.phase) * 32.0f, 128.0f);
    for (float ax = x + (b.dir > 0 ? shift : 128.0f - shift) - 128.0f; ax < x + w; ax += 128.0f)
    {
      if (ax < x)
        continue;
      const float tip = b.dir > 0 ? ax + 20.0f : ax;
      r.drawLine(tip, y + 10.0f, tip - 12.0f * float(b.dir), y + 3.0f, 4.0f, arrow);
      r.drawLine(tip, y + 10.0f, tip - 12.0f * float(b.dir), y + 17.0f, 4.0f, arrow);
    }
  }
  // Bells.
  for (const auto& bell : k.bells)
  {
    const float x = float(bell.x) * kTilePx - camX, y = float(bell.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const float sway = bell.ding > 0 ? std::sin(float(frame) * 1.3f) * 8.0f : 0.0f;
    r.fillRect(x + 12.0f + sway, y + 16.0f, 40.0f, 32.0f, rgb(230, 190, 60));
    r.fillRect(x + 28.0f + sway, y + 48.0f, 8.0f, 8.0f, rgb(150, 110, 30));
  }
  // Flaps.
  for (const auto& f : k.flaps)
  {
    const float x = float(f.x) * kTilePx - camX, y = float(f.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, float(f.h) * kTilePx))
      continue;
    if (f.open > 0)
      r.fillRect(x, y, 10.0f, float(f.h) * kTilePx, rgb(150, 110, 70));
    else
      r.fillRect(x + 8.0f, y, 48.0f, float(f.h) * kTilePx, rgb(170, 125, 80));
  }
  // Cleavers in the wall.
  for (const auto& c : k.cleavers)
  {
    const float x = float(c.bx) * kTilePx - camX, y = float(c.by) * kTilePx - camY;
    r.fillRect(x, y, kTilePx, 14.0f, rgb(200, 205, 215));
    r.fillRect(c.dir > 0 ? x : x + 40.0f, y + 14.0f, 24.0f, 10.0f, rgb(90, 60, 40));
  }
  // The chef's hand.
  if (k.hand > 0)
  {
    const auto& p = mPlayer;
    const float x = float(p.x) * float(kCellSize) * kPixelScale - camX, y = float(p.y + 1) * float(kCellSize) * kPixelScale - camY;
    r.fillRect(x - 48.0f, y - 8.0f, 192.0f, 40.0f, rgb(240, 200, 170));
    r.fillRect(x + 40.0f, y + 32.0f, 64.0f, float(kScreenH), rgb(255, 255, 255));
  }
}

void World::drawKitchenHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& p = mPlayer;
  // The Pepper Grinder's spin-up.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::PepperGrinder) && p.charge > 0 && p.charge < kGrinderSpin)
  {
    const float t = float(p.charge) / float(kGrinderSpin);
    r.fillRect(float(kScreenW) / 2.0f - 60.0f, float(kScreenH) - 60.0f, 120.0f * t, 10.0f, rgb(240, 90, 60));
  }
}

// The Turntable bonus: the room's blocks turned about its centre, the
// grilles, and the gems (at the in-between angle).
void World::drawTurntableBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  const auto& t = mTurn;
  float da = t.ang - t.prevAng;
  if (da < -3.0f)
    da += 6.2831853f;
  const float a = t.prevAng + da * alpha;
  const float c = std::cos(a), s = std::sin(a);
  const float cellPx = float(kCellSize) * kPixelScale;
  // The microwave's glass glow behind it all.
  const float ccx = t.cx * cellPx - camX, ccy = t.cy * cellPx - camY;
  drawGlow(r, mArt, ccx, ccy, float(t.w) * kTilePx * 0.6f, rgb(255, 230, 150), 0.25f);
  DrawOpts o;
  o.angle = a * 57.29578f;
  o.scale = 1.04f;
  for (int by = 0; by < t.h; ++by)
    for (int bx = 0; bx < t.w; ++bx)
    {
      const unsigned char k = t.solid[std::size_t(by * t.w + bx)];
      if (k == 0)
        continue;
      if (k == 1)
      {
        const Texture& tex = mArt.solid[std::size_t((bx * 7 + by * 3) % 3)];
        const float qx = float(bx * kCellsPerTile) + tex.anchorX() / cellPx - t.cx;
        const float qy = float(by * kCellsPerTile) + tex.anchorY() / cellPx - t.cy;
        r.draw(tex, (t.cx + c * qx - s * qy) * cellPx - camX, (t.cy + s * qx + c * qy) * cellPx - camY, o);
      }
      else
      {
        // A grille: bars across the block.
        for (int i = 0; i < 4; ++i)
        {
          const float f = (float(i) + 0.5f) / 4.0f * float(kCellsPerTile);
          const bool across = bx > 0 && bx < t.w - 1; // in a top or bottom wall: bars run down
          const float x0 = float(bx * kCellsPerTile) + (across ? f : 0.0f) - t.cx;
          const float y0 = float(by * kCellsPerTile) + (across ? 0.0f : f) - t.cy;
          const float x1 = x0 + (across ? 0.0f : float(kCellsPerTile)), y1 = y0 + (across ? float(kCellsPerTile) : 0.0f);
          r.drawLine((t.cx + c * x0 - s * y0) * cellPx - camX, (t.cy + s * x0 + c * y0) * cellPx - camY,
            (t.cx + c * x1 - s * y1) * cellPx - camX, (t.cy + s * x1 + c * y1) * cellPx - camY, 6.0f, rgb(150, 155, 165));
        }
      }
    }
}

void World::drawTurntableFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mTurn;
  float da = t.ang - t.prevAng;
  if (da < -3.0f)
    da += 6.2831853f;
  const float a = t.prevAng + da * alpha;
  const float c = std::cos(a), s = std::sin(a);
  const float cellPx = float(kCellSize) * kPixelScale;
  for (std::size_t i = 0; i < t.gems.size(); ++i)
  {
    const auto& g = t.gems[i];
    if (g.gone)
      continue;
    const float qx = g.x - t.cx, qy = g.y - t.cy;
    const float x = (t.cx + c * qx - s * qy) * cellPx - camX, y = (t.cy + s * qx + c * qy) * cellPx - camY;
    drawGlow(r, mArt, x, y, 50, mArt.gemColor[std::size_t(g.variant % 4)], 0.45f + 0.2f * std::sin(float(frame) * 0.15f + float(i)));
    r.draw(mArt.items[std::size_t(kIconGem0 + g.variant % 4)], x - 32.0f, y - 32.0f);
  }
}

} // namespace gr
