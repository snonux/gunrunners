// Level 26, Kaiju Tonight: drawing (the model buildings, their cracks,
// debris and toppling towers, the power lines, the Atomic Breath, the Maser
// beams, the Toy Jets growing in the backdrop, Gorgazilla and the harbor
// dish, the damage meter). See world_city.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

Color facade(BuildingKind k)
{
  switch (k)
  {
    case BuildingKind::House:
      return rgb(230, 200, 160);
    case BuildingKind::Block:
      return rgb(150, 160, 180);
    case BuildingKind::Roof:
      return rgb(190, 120, 90);
    case BuildingKind::Tower:
      return rgb(200, 70, 60);
    case BuildingKind::Prize:
      return rgb(240, 210, 90);
  }
  return rgb(200, 200, 200);
}

// A model building's front: a wall, a roof line and lit windows.
void drawFacade(Renderer& r, float x, float y, float w, float h, Color c, bool topDown, int seed)
{
  r.fillRect(x, y, w, h, c);
  r.fillRect(x, y, w, 8.0f, lerpColor(c, rgb(0, 0, 0), 0.35f));
  if (topDown)
  {
    r.fillRect(x + 8.0f, y + 8.0f, w - 16.0f, h - 16.0f, lerpColor(c, rgb(255, 255, 255), 0.15f));
    return;
  }
  for (float wy = y + 18.0f; wy + 16.0f < y + h - 6.0f; wy += 30.0f)
    for (float wx = x + 10.0f; wx + 12.0f < x + w - 6.0f; wx += 22.0f)
    {
      const bool lit = ((int(wx) * 7 + int(wy) * 13 + seed) / 5) % 3 != 0;
      r.fillRect(wx, wy, 12.0f, 16.0f, lit ? rgb(255, 230, 140) : rgb(60, 70, 90));
    }
}

} // namespace

void World::drawCityBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& c = mCity;
  // Water.
  for (const auto& w : c.ponds)
  {
    const float x = float(w.x) * kCellPx - camX, y = float(w.y) * kCellPx - camY;
    const float ww = float(w.w) * kCellPx, h = float(w.h) * kCellPx;
    if (offScreen(x, y, ww, h))
      continue;
    r.fillRect(x, y, ww, h, withAlpha(rgb(40, 90, 160), 210));
    for (int i = 0; i < int(ww / 80.0f); ++i)
      r.fillRect(x + float(i) * 80.0f + std::sin(float(frame + i * 23) * 0.06f) * 10.0f, y + 6.0f, 34.0f, 4.0f,
        withAlpha(rgb(200, 230, 255), 150));
  }
  // Power lines (and their pylons' tops).
  for (const auto& l : c.lines)
  {
    const float x0 = float(l.x0) * kCellPx - camX, x1 = float(l.x1 + 1) * kCellPx - camX;
    const float y = (float(l.y) + 0.5f) * kCellPx - camY;
    if (offScreen(std::min(x0, x1), y - 40.0f, std::abs(x1 - x0), 80.0f))
      continue;
    if (!l.snapped)
    {
      const int n = 16;
      for (int i = 0; i < n; ++i)
      {
        const float a = float(i) / float(n), b = float(i + 1) / float(n);
        const float sa = std::sin(a * 3.14159f) * 14.0f, sb = std::sin(b * 3.14159f) * 14.0f;
        r.drawLine(x0 + (x1 - x0) * a, y + sa, x0 + (x1 - x0) * b, y + sb, 3.0f, rgb(30, 30, 40));
      }
    }
    else
    {
      r.drawLine(x0, y, x0 + 20.0f, y + 70.0f, 3.0f, rgb(30, 30, 40));
      r.drawLine(x1, y, x1 - 20.0f, y + 70.0f, 3.0f, rgb(30, 30, 40));
      if (l.sparks > 0 || (frame / 6) % 5 == 0)
      {
        r.fillRect(x0 + 16.0f, y + 64.0f, 10.0f, 10.0f, rgb(255, 255, 160), Blend::Add);
        r.fillRect(x1 - 26.0f, y + 64.0f, 10.0f, 10.0f, rgb(255, 255, 160), Blend::Add);
      }
    }
    r.fillRect(x0 - 6.0f, y - 10.0f, 12.0f, 20.0f, rgb(80, 80, 90));
    r.fillRect(x1 - 6.0f, y - 10.0f, 12.0f, 20.0f, rgb(80, 80, 90));
  }
  // Toy Jets growing in the backdrop before a run.
  for (const auto& f : c.formations)
    if (f.state == 1)
    {
      const float s = 0.2f + 0.8f * float(f.t) / float(kJetIntro);
      const float cx = float(kScreenW) * 0.5f, cy = float(f.row) * kCellPx - camY - 80.0f * (1.0f - s);
      for (int i = 0; i < 3; ++i)
        r.fillRect(cx + float(i - 1) * 60.0f * s, cy + float(i % 2) * 20.0f * s, 60.0f * s, 16.0f * s,
          withAlpha(rgb(200, 210, 230), int(120 + 120 * s)));
    }
  // The trailer's photo.
  if (c.photoX >= 0)
  {
    const float x = float(c.photoX) * kCellPx - camX, y = float(c.photoY - 2) * kCellPx - camY;
    const float sw = c.photoSwing > 0 ? std::sin(float(c.photoSwing) * 0.5f) * 10.0f : 0.0f;
    r.fillRect(x + sw, y, 2.0f * kCellPx, 2.5f * kCellPx, rgb(240, 230, 200));
    r.fillRect(x + sw + 8.0f, y + 8.0f, 2.0f * kCellPx - 16.0f, 1.6f * kCellPx, rgb(90, 160, 90));
  }
}

void World::drawCityFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& c = mCity;
  const bool top = mTop.on;
  // Buildings.
  for (std::size_t i = 0; i < c.buildings.size(); ++i)
  {
    const auto& b = c.buildings[i];
    const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
    const float w = float(b.x1 - b.x0 + 1) * kTilePx, h = float(b.y1 - b.y0 + 1) * kTilePx;
    const Color col = facade(b.kind);
    switch (b.state)
    {
      case BuildingState::Standing:
        if (offScreen(x, y, w, h))
          break;
        drawFacade(r, x, y, w, h, col, top, int(i) * 31);
        if (b.cracks > 0)
          for (int k = 0; k < b.cracks; k += 3)
          {
            const float cx = x + std::fmod(float(k * 37 + int(i) * 11), w);
            r.drawLine(cx, y, cx + 10.0f, y + 12.0f + float(k), 2.0f, rgb(40, 30, 30));
          }
        if (b.burn > 0)
          r.fillRect(x, y, w, h, withAlpha(rgb(255, 120, 40), 40 + 30 * b.burn), Blend::Add);
        if (b.kind == BuildingKind::Tower)
        {
          // An arrow decal on the street pointing at it.
          const float ax = b.fall > 0 ? x - 70.0f : x + w + 10.0f, ay = y + h - 40.0f;
          r.fillRect(ax, ay, 60.0f, 12.0f, rgb(255, 220, 60));
          r.fillRect(b.fall > 0 ? ax + 48.0f : ax, ay - 8.0f, 12.0f, 28.0f, rgb(255, 220, 60));
        }
        if (int(i) == c.diner && c.wave > 0)
          for (int k = 0; k < 3; ++k)
            r.fillRect(x + 20.0f + float(k) * 50.0f, y - 20.0f - ((frame / 4 + k) % 2 ? 8.0f : 0.0f), 10.0f, 20.0f,
              rgb(255, 255, 255));
        break;
      case BuildingState::Crumbling:
      {
        const float t = float(b.t) / float(kCrumbleFrames);
        if (!offScreen(x, y, w, h))
          r.fillRect(x, y + h * t, w, h * (1.0f - t), withAlpha(col, int(255 * (1.0f - t))));
        break;
      }
      case BuildingState::Down:
        if (!offScreen(x, y, w, h) && !top)
          for (float dx = 0.0f; dx < w; dx += 24.0f)
            r.fillRect(x + dx, y + h - 14.0f - std::fmod(dx * 7.0f, 10.0f), 20.0f, 14.0f + std::fmod(dx * 7.0f, 10.0f),
              lerpColor(col, rgb(60, 50, 50), 0.4f));
        else if (!offScreen(x, y, w, h))
          r.fillRect(x + 6.0f, y + 6.0f, w - 12.0f, h - 12.0f, withAlpha(lerpColor(col, rgb(60, 50, 50), 0.5f), 160));
        break;
      case BuildingState::Leaning:
      case BuildingState::Falling:
      {
        // Swings down about its foot: drawn as a run of segments.
        const float t = b.state == BuildingState::Leaning ? 0.06f * std::sin(float(frame) * 0.8f) + 0.04f
                                                          : 0.1f + 0.9f * float(b.t) / float(kTowerFall);
        const float ang = t * 1.5708f * float(b.fall);
        const float fx = b.fall > 0 ? x + w : x, fy = y + h;
        const int segs = b.y1 - b.y0 + 1;
        for (int k = 0; k < segs; ++k)
        {
          const float d = (float(k) + 0.5f) * kTilePx;
          const float cx = fx + std::sin(ang) * d - (b.fall > 0 ? w * 0.5f : -w * 0.5f) * std::cos(ang);
          const float cy = fy - std::cos(ang) * d;
          r.fillRect(cx - w * 0.5f, cy - kTilePx * 0.5f, w, kTilePx, col);
        }
        break;
      }
      case BuildingState::Lying:
      {
        const float bx = float(b.bx0) * kTilePx - camX, by = float(b.by0) * kTilePx - camY;
        const float bw = float(b.bx1 - b.bx0 + 1) * kTilePx, bh = float(b.by1 - b.by0 + 1) * kTilePx;
        if (!offScreen(bx, by, bw, bh))
        {
          r.fillRect(bx, by, bw, bh, col);
          for (float wx = bx + 20.0f; wx + 16.0f < bx + bw; wx += 30.0f)
            r.fillRect(wx, by + bh * 0.4f, 16.0f, 12.0f, rgb(60, 70, 90));
        }
        break;
      }
    }
  }
  // The full heal.
  if (c.fullhealX >= 0 && !c.fullhealUsed)
  {
    const float x = float(c.fullhealX) * kTilePx - camX, y = float(c.fullhealY) * kTilePx - camY;
    r.fillRect(x + 8.0f, y + 8.0f, kTilePx - 16.0f, kTilePx - 8.0f, rgb(250, 250, 250));
    r.fillRect(x + 26.0f, y + 14.0f, 12.0f, 40.0f, rgb(230, 40, 60));
    r.fillRect(x + 12.0f, y + 28.0f, 40.0f, 12.0f, rgb(230, 40, 60));
  }
  // Tanks' and trucks' tells, the Maser beams.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const float ex = float(e.x) * kCellPx - camX, ey = float(e.y - e.h + 1) * kCellPx - camY;
    if (e.kind == EnemyKind::ModelTank && e.tell > 0 && (frame / 2) % 2)
      r.fillRect(ex + float(e.w) * kCellPx * 0.5f - 10.0f, ey - 14.0f, 20.0f, 14.0f, rgb(255, 240, 120), Blend::Add);
    if (e.kind == EnemyKind::MaserTruck)
    {
      const bool dark = sectorDark(e.variant);
      const float dx = ex + (e.dir > 0 ? float(e.w) * kCellPx - 20.0f : 20.0f), dy = ey + kCellPx * 1.5f;
      if (e.tell > 0)
      {
        const float ring = 40.0f * (1.0f - float(e.tell) / float(enemyDef(e.def).tell));
        r.fillRect(dx - ring * 0.5f, dy - ring * 0.5f, ring, ring, withAlpha(rgb(120, 220, 255), 120), Blend::Add);
      }
      if (dark && (frame / 5) % 4 == 0)
        r.fillRect(dx - 8.0f, dy - 8.0f, 16.0f, 16.0f, rgb(120, 220, 255), Blend::Add);
      if (e.dive > 0)
      {
        const float x0 = e.dir > 0 ? ex + float(e.w) * kCellPx : ex, x1 = (float(e.aimX) + 0.5f) * kCellPx - camX;
        const float y = (float(e.aimY) + 0.5f) * kCellPx - camY;
        r.drawLine(x0, y, x1, y, 14.0f, withAlpha(rgb(120, 220, 255), 200), Blend::Add);
        r.drawLine(x0, y, x1, y, 5.0f, rgb(255, 255, 255));
      }
    }
  }
  // The Atomic Breath.
  if (c.breath > 0)
  {
    const float x0 = c.beamX0 * kCellPx - camX, y0 = c.beamY0 * kCellPx - camY;
    const float x1 = c.beamX1 * kCellPx - camX, y1 = c.beamY1 * kCellPx - camY;
    r.drawLine(x0, y0, x1, y1, 26.0f + 4.0f * std::sin(float(frame) * 0.9f), withAlpha(rgb(90, 255, 200), 150), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 8.0f, rgb(230, 255, 245));
  }
  // Gorgazilla.
  const auto& z = c.lizard;
  if (z.on)
  {
    // The harbor Maser dish on the liner, its ring and its beam.
    const float dx = float(z.dishX) * kCellPx - camX, dy = float(z.dishY - 3) * kCellPx - camY;
    r.fillRect(dx - 6.0f, dy, 12.0f, 3.0f * kCellPx, rgb(90, 90, 100));
    r.fillRect(dx - 40.0f, dy - 20.0f, 60.0f, 30.0f, rgb(200, 200, 210));
    if (z.state == LizardState::Roar && z.t >= kRoarLen - 22)
    {
      const float ring = 50.0f * float(z.t - (kRoarLen - 22)) / 22.0f;
      r.fillRect(dx - 10.0f - ring * 0.5f, dy - 5.0f - ring * 0.5f, ring, ring, withAlpha(rgb(120, 220, 255), 140), Blend::Add);
    }
    if (z.beam > 0 && z.beamRow >= 0)
    {
      const float y = (float(z.beamRow) + 0.5f) * kCellPx - camY;
      const float x0 = float(z.x0) * kTilePx - camX;
      r.drawLine(x0, y, dx - 30.0f, y, 18.0f, withAlpha(rgb(120, 220, 255), 200), Blend::Add);
      r.drawLine(x0, y, dx - 30.0f, y, 6.0f, rgb(255, 255, 255));
    }
    const float lx = (z.prevX + (z.x - z.prevX) * alpha) * kCellPx - camX;
    const float ly = float(z.floor - Lizard::kH + 1) * kCellPx - camY;
    const float lw = float(Lizard::kW) * kCellPx, lh = float(Lizard::kH) * kCellPx;
    if (z.state == LizardState::Under)
    {
      if (z.t >= kUnder - 20 && z.splashX >= 0)
        for (int k = 0; k < 3; ++k)
        {
          const float rw = 40.0f + float((frame + k * 7) % 21) * 4.0f;
          r.fillRect(float(z.splashX) * kCellPx - camX + lw * 0.5f - rw * 0.5f, float(z.floor + 1) * kCellPx - camY - 6.0f, rw,
            4.0f, withAlpha(rgb(220, 240, 255), 160));
        }
    }
    else if (z.state == LizardState::Leave || z.state == LizardState::Done)
    {
      // The empty suit, and a tired man walking off set.
      r.fillRect(float(z.x) * kCellPx - camX, ly + lh * 0.5f, lw, lh * 0.5f, rgb(70, 130, 70));
      if (z.state == LizardState::Leave)
      {
        const float mx = z.manX * kCellPx - camX;
        r.fillRect(mx + 30.0f, ly + lh * 0.3f, 30.0f, lh * 0.7f, rgb(220, 220, 230));
        r.fillRect(mx + 32.0f, ly + lh * 0.2f, 26.0f, 26.0f, rgb(240, 200, 170));
      }
    }
    else
    {
      const Color skin = z.flash > 0 && (frame / 2) % 2 ? rgb(255, 255, 255) : rgb(70, 150, 70);
      const float lean = z.state == LizardState::Roar ? -10.0f * float(z.dir) : 0.0f;
      r.fillRect(lx + lean, ly + 30.0f, lw, lh - 30.0f, skin);                 // body
      r.fillRect(lx + lean + (z.dir > 0 ? lw * 0.4f : -lw * 0.1f), ly, lw * 0.7f, 50.0f, skin); // head
      for (int k = 0; k < 4; ++k)
        r.fillRect(lx + lean + (z.dir > 0 ? 0.0f : lw - 14.0f), ly + 40.0f + float(k) * 34.0f, 14.0f, 18.0f, rgb(40, 100, 50));
      if (z.state == LizardState::Roar)
        r.fillRect(lx + lean + (z.dir > 0 ? lw * 0.8f : -lw * 0.1f), ly + 30.0f, 30.0f, 16.0f, rgb(200, 40, 40));
      if (z.state == LizardState::Swipe)
      {
        const bool strike = z.t >= kSwipeTell;
        const float tw = strike ? float(Lizard::kW + 16) * kCellPx : lw * 0.6f;
        r.fillRect(strike ? lx - 8.0f * kCellPx : lx - tw * 0.5f, float(z.floor) * kCellPx - camY - 24.0f, tw, 22.0f,
          strike ? rgb(120, 200, 120) : rgb(60, 120, 60));
      }
      if (z.state == LizardState::Sit)
        r.fillRect(lx + lw * 0.45f, ly + 30.0f, 6.0f, (lh - 30.0f) * float(z.t) / 45.0f, rgb(200, 200, 200));
    }
  }
}

void World::drawCityHud(Renderer& r, int frame) const
{
  const auto& c = mCity;
  if (mTop.on || c.buildings.empty())
    return;
  // The damage meter.
  const float x = float(kScreenW) * 0.5f, y = 112.0f;
  const Color col = c.ratingFlash > 0 && (frame / 3) % 2 ? rgb(255, 255, 255) : rgb(255, 200, 60);
  r.drawText("DAMAGE " + std::to_string(c.rating), x, y, {20.0f, col, rgb(0, 0, 0)}, Align::Center);
  if (lizardFight())
  {
    const float bw = 500.0f, bx = (float(kScreenW) - bw) * 0.5f, by = 150.0f;
    r.fillRect(bx - 8, by - 24, bw + 16, 40, rgba(8, 6, 12, 200));
    r.drawText("GORGAZILLA", bx, by - 21, {15.0f, rgb(140, 230, 140), rgb(0, 0, 0)});
    r.fillRect(bx, by, bw, 10, rgba(255, 255, 255, 40));
    r.fillRect(bx, by, bw * float(lizardHp()) / float(3 * Lizard::kHp), 10, rgb(90, 220, 90));
  }
}

} // namespace gr
