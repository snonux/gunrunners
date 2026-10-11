// Level 30, Fiber Trench: drawing (the water's tint and the air pockets'
// wobbling surfaces, currents' bubble streams, the abyss and the whale, the
// harpoon line, harpoons and anchor rings, Anglerfish and their lures, Box
// Jellies, Cable Eels in their junction boxes, the dark hold, the porthole's
// alien, the HUD's stroke pips, and Bubble Up's bubble and rings). See
// world_trench.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c);
}

// A soft round blob from stacked rects.
void blob(Renderer& r, float cx, float cy, float rad, Color c, std::uint8_t a)
{
  for (int k = 0; k < 4; ++k)
  {
    const float s = rad * (1.0f - float(k) * 0.18f);
    const float hgt = rad * (0.55f + float(k) * 0.15f);
    r.fillRect(cx - s, cy - hgt, s * 2.0f, hgt * 2.0f, withAlpha(c, std::uint8_t(a / 4 + 10)));
  }
  r.fillRect(cx - rad * 0.6f, cy - rad * 0.6f, rad * 1.2f, rad * 1.2f, withAlpha(c, a));
}

} // namespace

void World::drawTrenchBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& t = mTrench;
  // The abyss: a deepening gradient, and the whale's silhouette crossing it.
  for (const auto& a : t.abyss)
  {
    const float x = float(a.x) * kCellPx - camX, y = float(a.y) * kCellPx - camY;
    const float w = float(a.w) * kCellPx, h = float(a.h) * kCellPx;
    if (offScreen(x, y, w, h))
      continue;
    for (int k = 0; k < 8; ++k)
      r.fillRect(x, y + h * float(k) / 8.0f, w, h / 8.0f + 1.0f, rgba(0, 4, 14, std::uint8_t(60 + k * 24)));
    const int ph = frame % kWhaleEvery;
    if (ph < 360)
    {
      const float wx = x - 360.0f + (w + 720.0f) * float(ph) / 360.0f, wy = y + h * 0.45f;
      const Color c = rgba(10, 22, 40, 170);
      r.fillRect(wx - 180.0f, wy - 40.0f, 300.0f, 80.0f, c);
      r.fillRect(wx - 140.0f, wy - 56.0f, 200.0f, 112.0f, c);
      r.fillRect(wx + 120.0f, wy - 24.0f, 50.0f, 48.0f, c);
      r.fillRect(wx + 160.0f, wy - 50.0f, 30.0f, 100.0f, c);
    }
  }
  // The porthole's alien waves from behind the glass.
  if (t.portX >= 0)
  {
    const float x = float(t.portX) * kCellPx - camX, y = float(t.portY - 3) * kCellPx - camY;
    if (!offScreen(x, y, 96.0f, 96.0f))
    {
      r.fillRect(x - 8.0f, y - 8.0f, 112.0f, 112.0f, rgb(150, 120, 70));
      r.fillRect(x, y, 96.0f, 96.0f, rgb(20, 60, 90));
      if (t.wave > 0)
      {
        r.fillRect(x + 28.0f, y + 30.0f, 40.0f, 50.0f, rgb(120, 220, 120));
        r.fillRect(x + 34.0f, y + 40.0f, 8.0f, 8.0f, rgb(10, 10, 10));
        r.fillRect(x + 54.0f, y + 40.0f, 8.0f, 8.0f, rgb(10, 10, 10));
        const float arm = (t.wave / 6) % 2 ? -14.0f : 0.0f;
        r.fillRect(x + 68.0f, y + 24.0f + arm, 10.0f, 30.0f, rgb(120, 220, 120));
      }
      r.fillRect(x + 8.0f, y + 8.0f, 24.0f, 10.0f, rgba(255, 255, 255, 70));
    }
  }
}

void World::drawTrenchFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& t = mTrench;
  const auto& p = mPlayer;
  // The water's tint (not in the pockets: their surface wobbles at the top).
  if (t.water)
  {
    const float x = std::max(0.0f, float(t.waterBox.x) * kCellPx - camX);
    const float y = std::max(0.0f, float(t.waterBox.y) * kCellPx - camY);
    const float x1 = std::min(float(kScreenW), float(t.waterBox.x + t.waterBox.w) * kCellPx - camX);
    const float y1 = std::min(float(kScreenH), float(t.waterBox.y + t.waterBox.h) * kCellPx - camY);
    if (x1 > x && y1 > y)
      r.fillRect(x, y, x1 - x, y1 - y, rgba(20, 90, 140, 38));
    for (const auto& pk : t.pockets)
    {
      const float px = float(pk.x) * kCellPx - camX, py = float(pk.y) * kCellPx - camY;
      const float pw = float(pk.w) * kCellPx;
      if (offScreen(px, py, pw, float(pk.h) * kCellPx))
        continue;
      r.fillRect(px, py, pw, float(pk.h) * kCellPx, rgba(200, 230, 255, 20));
      for (float sx = 0.0f; sx < pw; sx += 16.0f)
      {
        const float wob = std::sin((sx + float(frame) * 6.0f) * 0.05f) * 5.0f;
        r.fillRect(px + sx, py + wob, 16.0f, 4.0f, rgba(200, 240, 255, 160));
      }
    }
    // Currents: bubble streams drifting their way.
    for (std::size_t i = 0; i < t.currents.size(); ++i)
    {
      const auto& c = t.currents[i];
      const float x0 = float(c.rect.x) * kCellPx - camX, y0 = float(c.rect.y) * kCellPx - camY;
      const float w = float(c.rect.w) * kCellPx, h = float(c.rect.h) * kCellPx;
      if (offScreen(x0, y0, w, h))
        continue;
      const float speed = 4.0f / float(c.every);
      for (int k = 0; k < int(w * h / 9000.0f) + 4; ++k)
      {
        const unsigned s = unsigned(k) * 2654435761u + unsigned(i) * 97u;
        float bx = float(s % 1000u) / 1000.0f * w, by = float((s >> 10) % 1000u) / 1000.0f * h;
        bx = std::fmod(bx + float(c.dx) * speed * float(frame) + w * 4.0f, w);
        by = std::fmod(by + float(c.dy) * speed * float(frame) + h * 4.0f, h);
        const float rad = 3.0f + float((s >> 20) % 4u);
        r.fillRect(x0 + bx, y0 + by, rad * 2.0f, rad * 2.0f, rgba(210, 240, 255, 110));
      }
    }
  }
  // The quiet junction box.
  if (t.quietX >= 0)
  {
    const float x = float(t.quietX) * kCellPx - camX, y = float(t.quietY - 3) * kCellPx - camY;
    r.fillRect(x, y, kTilePx, kTilePx, rgb(70, 80, 90));
    r.fillRect(x + 6.0f, y + 6.0f, kTilePx - 12.0f, kTilePx - 12.0f, rgb(46, 54, 62));
    if (t.quietT > 0 && (frame / 4) % 2 == 0)
      r.fillRect(x + 26.0f, y + 26.0f, 12.0f, 12.0f, rgb(120, 255, 200));
  }
  // Enemies.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
    const float w = float(b.w) * kCellPx, h = float(b.h) * kCellPx;
    if (offScreen(x, y, w, h))
      continue;
    const bool hit = e.flash > 0 && (frame / 2) % 2 == 0;
    if (e.kind == EnemyKind::Anglerfish)
    {
      const Color body = hit ? rgb(255, 255, 255) : rgb(36, 40, 52);
      r.fillRect(x + 8.0f, y + 12.0f, w - 16.0f, h - 20.0f, body);
      const float tail = e.dir > 0 ? x - 10.0f : x + w - 20.0f;
      r.fillRect(tail, y + 20.0f, 30.0f, h - 40.0f, body);
      const float jaw = e.dir > 0 ? x + w - 50.0f : x + 10.0f;
      for (int k = 0; k < 4; ++k)
        r.fillRect(jaw + float(k) * 10.0f, y + h - 30.0f, 5.0f, 10.0f, rgb(230, 230, 220));
      if (e.tell > 0 || e.dive > 0)
      {
        const float ex = e.dir > 0 ? x + w - 70.0f : x + 50.0f;
        r.fillRect(ex, y + 30.0f, 10.0f, 10.0f, rgb(255, 240, 160));
        r.fillRect(ex + 18.0f * float(e.dir), y + 30.0f, 10.0f, 10.0f, rgb(255, 240, 160));
      }
      continue;
    }
    if (e.kind == EnemyKind::BoxJelly)
    {
      const float squeeze = e.tell > 0 ? 10.0f : 0.0f;
      const Color c = e.carrier ? rgba(120, 255, 120, 170) : rgba(200, 160, 255, 160);
      r.fillRect(x + 6.0f + squeeze, y + 6.0f, w - 12.0f - squeeze * 2.0f, h * 0.55f, hit ? rgb(255, 255, 255) : c);
      for (int k = 0; k < 4; ++k)
      {
        const float sway = std::sin(float(frame + k * 5) * 0.2f) * 4.0f;
        r.fillRect(x + 14.0f + float(k) * (w - 28.0f) / 3.0f + sway, y + h * 0.55f, 4.0f, h * 0.5f, withAlpha(c, 120));
      }
      continue;
    }
    if (e.kind == EnemyKind::CableEel)
    {
      const Color c = hit ? rgb(255, 255, 255) : rgb(40, 200, 220);
      r.fillRect(x, y, w, h, withAlpha(c, 220));
      for (float k = 0.0f; k < std::max(w, h); k += 24.0f)
      {
        if (b.w > b.h)
          r.fillRect(x + k, y + 4.0f, 8.0f, h - 8.0f, rgba(220, 255, 255, 160));
        else
          r.fillRect(x + 4.0f, y + k, w - 8.0f, 8.0f, rgba(220, 255, 255, 160));
      }
      continue;
    }
  }
  // Junction boxes: humming and sparking before an eel comes out.
  for (const auto& eb : t.eels)
  {
    const float x = float(eb.bx) * kTilePx - camX, y = float(eb.by) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    r.fillRect(x, y, kTilePx, kTilePx, rgb(60, 66, 76));
    r.fillRect(x + 4.0f, y + 4.0f, kTilePx - 8.0f, kTilePx - 8.0f, rgb(30, 34, 42));
    const float mx = x + kTilePx * 0.5f + float(eb.fx) * kTilePx * 0.4f - 6.0f;
    const float my = y + kTilePx * 0.5f + float(eb.fy) * kTilePx * 0.4f - 6.0f;
    r.fillRect(mx, my, 12.0f, 12.0f, eb.state == 1 && (frame / 2) % 2 ? rgb(255, 255, 140) : rgb(10, 12, 16));
    if (eb.state == 1)
      for (int k = 0; k < 3; ++k)
        r.drawLine(mx + 6.0f, my + 6.0f, mx + 6.0f + float((frame * 7 + k * 31) % 40 - 20),
          my + 6.0f + float((frame * 11 + k * 17) % 40 - 20), 3.0f, rgb(220, 255, 255));
  }
  // Anchor rings, harpoons and the line.
  for (const auto& a : t.anchors)
  {
    const float x = float(a.x) * kCellPx - camX, y = float(a.y) * kCellPx - camY;
    r.fillRect(x + 4.0f, y + 4.0f, kTilePx - 8.0f, kTilePx - 8.0f, rgb(200, 170, 90));
    r.fillRect(x + 14.0f, y + 14.0f, kTilePx - 28.0f, kTilePx - 28.0f, rgb(30, 40, 50));
  }
  for (const auto& h : t.harpoons)
  {
    const float x = float(h.x) * kCellPx - camX, y = float(h.y) * kCellPx - camY;
    r.fillRect(x - 4.0f, y - 4.0f, 16.0f, 16.0f, rgb(220, 230, 240));
  }
  if (t.line)
  {
    const float y = float(t.lineY) * kCellPx - camY + 4.0f;
    const float x0 = float(t.lineX0) * kCellPx - camX, x1 = float(t.lineX1 + 1) * kCellPx - camX;
    r.drawLine(x0, y, x1, y, 6.0f, rgb(230, 225, 200));
    r.drawLine(x0, y + 3.0f, x1, y + 3.0f, 2.0f, rgb(140, 130, 110));
  }
  // The dark: deep shadow, but a pool of light around the runner.
  for (const auto& d : t.darks)
  {
    const int cx0 = std::max(d.x, int(std::floor(camX / kCellPx)) - 1);
    const int cy0 = std::max(d.y, int(std::floor(camY / kCellPx)) - 1);
    const int cx1 = std::min(d.x + d.w - 1, int((camX + float(kScreenW)) / kCellPx) + 1);
    const int cy1 = std::min(d.y + d.h - 1, int((camY + float(kScreenH)) / kCellPx) + 1);
    if (cx1 < cx0 || cy1 < cy0)
      continue;
    const int cols = cx1 - cx0 + 1, rows = cy1 - cy0 + 1;
    std::vector<float> shade(std::size_t(cols * rows));
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < cols; ++i)
      {
        const float dx = float(cx0 + i) - (float(p.x) + 1.5f), dy = float(cy0 + j) - (float(p.y) - 2.0f);
        const float dd = std::sqrt(dx * dx + dy * dy);
        shade[std::size_t(j * cols + i)] = std::clamp((dd - 5.0f) / 8.0f, 0.0f, 1.0f) * 0.9f;
      }
    r.drawAlphaGrid(shade, cols, rows, float(cx0) * kCellPx - camX, float(cy0) * kCellPx - camY, kCellPx, kCellPx,
      rgb(1, 3, 10));
  }
  // Anglerfish lures glow through the dark: a gem-shaped light, pulsing.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Anglerfish)
      continue;
    const CellBox b = e.box();
    const float lx = float(e.dir > 0 ? b.x + b.w + 1 : b.x - 2) * kCellPx - camX, ly = float(b.y - 1) * kCellPx - camY;
    if (offScreen(lx, ly, 40.0f, 40.0f))
      continue;
    const bool pulsing = e.tell == 0 && e.dive == 0;
    const float a = pulsing ? 0.6f + 0.4f * float(frame % 30) / 30.0f : 1.0f;
    const float sx = float(b.x + b.w / 2) * kCellPx - camX;
    r.drawLine(sx, float(b.y) * kCellPx - camY + 8.0f, lx + 12.0f, ly + 12.0f, 3.0f, rgba(60, 70, 80, 200));
    blob(r, lx + 12.0f, ly + 12.0f, 14.0f, rgb(120, 255, 220), std::uint8_t(200.0f * a));
  }
  // The abyss's return: a fade from black.
  if (t.fade > 0)
    r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(0, 4, 12, std::uint8_t(t.fade * 255 / kAbyssFade)));
  // Bubble Up: the rings, and the bubble round the runner.
  if (t.bubble)
  {
    for (int row : t.rings)
    {
      const float y = float(row) * kCellPx - camY;
      if (y < -40.0f || y > float(kScreenH) + 40.0f)
        continue;
      const bool passed = row >= t.ringY;
      r.fillRect(0.0f, y - 2.0f, float(kScreenW), 4.0f, passed ? rgba(120, 255, 200, 90) : rgba(230, 240, 255, 60));
    }
    if (t.pop == 0 && p.state != PlayerState::Dying)
    {
      const float cx = (float(p.x) + 1.5f) * kCellPx - camX, cy = (float(p.y) - 1.5f) * kCellPx - camY;
      const float rad = 100.0f + std::sin(float(frame) * 0.3f) * 4.0f;
      for (int k = 0; k < 24; ++k)
      {
        const float a0 = float(k) * 6.2832f / 24.0f, a1 = float(k + 1) * 6.2832f / 24.0f;
        r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad, cx + std::cos(a1) * rad,
          cy + std::sin(a1) * rad, 4.0f, rgba(220, 245, 255, 180));
      }
      r.fillRect(cx - rad * 0.55f, cy - rad * 0.65f, 26.0f, 16.0f, rgba(255, 255, 255, 150));
    }
  }
}

void World::drawTrenchHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& t = mTrench;
  const auto& p = mPlayer;
  // Stroke pips while swimming (none in Turbo: strokes are unlimited).
  if (!t.water || !underwater() || p.turbo > 0)
    return;
  const int max = strokesMax();
  const float x = float(kScreenW) - 40.0f - float(max) * 34.0f, y = 96.0f;
  r.drawText("STROKES", x, y - 30.0f, {18.0f, rgb(210, 240, 255), rgb(0, 20, 40), true});
  for (int k = 0; k < max; ++k)
    r.fillRect(x + float(k) * 34.0f, y, 26.0f, 26.0f,
      k < max - t.strokes ? rgb(140, 230, 255) : rgba(140, 230, 255, 50));
}

} // namespace gr
