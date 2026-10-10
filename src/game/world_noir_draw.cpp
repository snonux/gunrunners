// Level 27, The Long Rain: drawing (the dark club and the shadows, the
// flashlight and spotlight cones, the "?" over the runner, the alarm, the
// rain, the props: doors, jukebox, the lipstick note, the painting and its
// safe, the office alarm and shutter, the red evidence, the trench coat),
// and the Silent Picture bonus (peels, the pram, the ladder gag, hats,
// title cards). See world_noir.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kPi = 3.14159265f;

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

const Color kRed = rgb(220, 30, 40);

// A soft cone as a fan of thin additive lines from (ox, oy) along angle a
// (radians), half-width h, length len (px).
void drawCone(Renderer& r, float ox, float oy, float a, float h, float len, Color c)
{
  const int rays = 14;
  for (int i = 0; i <= rays; ++i)
  {
    const float t = a - h + 2.0f * h * float(i) / float(rays);
    const float edge = 1.0f - std::abs(float(i) / float(rays) * 2.0f - 1.0f);
    r.drawLine(ox, oy, ox + std::cos(t) * len, oy + std::sin(t) * len, 10.0f,
      withAlpha(c, std::uint8_t(18.0f + 30.0f * edge)), Blend::Add);
  }
}

} // namespace

void World::drawNoirBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& n = mNoir;
  // Goon doors.
  for (const auto& d : n.doors)
  {
    const float x = float(d.x) * kCellPx - camX - 8.0f, y = float(d.y - 6) * kCellPx - camY;
    if (offScreen(x, y, 112.0f, 224.0f))
      continue;
    r.fillRect(x, y, 112.0f, 224.0f, rgb(20, 20, 22));
    r.fillRect(x + 6.0f, y + 6.0f, 100.0f, 212.0f, rgb(46, 46, 50));
    r.fillRect(x + 84.0f, y + 112.0f, 10.0f, 10.0f, rgb(150, 150, 150));
    if (n.alarm > 0)
      r.fillRect(x + 40.0f, y - 18.0f, 32.0f, 14.0f, (frame / 4) % 2 ? kRed : rgb(90, 20, 20));
  }
  // The jukebox.
  if (n.jukeX >= 0)
  {
    const float x = float(n.jukeX) * kTilePx - camX, y = float(n.jukeY - 1) * kTilePx - camY;
    r.fillRect(x - 8.0f, y, 80.0f, 128.0f, rgb(70, 70, 74));
    r.fillRect(x, y + 10.0f, 64.0f, 40.0f, n.jukebox ? rgb(230, 230, 230) : rgb(150, 150, 150));
    for (int i = 0; i < 4; ++i)
      r.fillRect(x + 4.0f + float(i) * 16.0f, y + 70.0f, 8.0f, 50.0f, rgb(40, 40, 44));
  }
  // The lipstick note on the bar mirror.
  if (n.noteX >= 0)
  {
    const float x = float(n.noteX - 1) * kTilePx - camX, y = float(n.noteY - 1) * kTilePx - camY;
    r.fillRect(x, y, 192.0f, 120.0f, rgb(110, 110, 116));
    r.fillRect(x + 6.0f, y + 6.0f, 180.0f, 108.0f, rgb(170, 170, 176));
    r.drawText("BEHIND THE", x + 96.0f, y + 30.0f, {18.0f, kRed, rgb(0, 0, 0)}, Align::Center);
    r.drawText("PAINTING", x + 96.0f, y + 58.0f, {18.0f, kRed, rgb(0, 0, 0)}, Align::Center);
  }
  // The painting (and the open safe behind it).
  if (n.paintX >= 0)
  {
    const float x = float(n.paintX - 1) * kTilePx - camX, y = float(n.paintY) * kTilePx - camY;
    if (n.paintOpen)
    {
      r.fillRect(x + 32.0f, y, 128.0f, 128.0f, rgb(30, 30, 32));
      r.fillRect(x - 40.0f, y, 60.0f, 128.0f, rgb(90, 80, 70));
    }
    else
    {
      r.fillRect(x + 16.0f, y, 160.0f, 128.0f, rgb(120, 110, 90));
      r.fillRect(x + 28.0f, y + 12.0f, 136.0f, 104.0f, rgb(80, 80, 84));
      r.fillRect(x + 70.0f, y + 40.0f, 50.0f, 60.0f, rgb(150, 150, 150));
    }
  }
  // The office alarm bell.
  if (n.alarmX >= 0)
  {
    const float x = float(n.alarmX) * kTilePx - camX, y = float(n.alarmY) * kTilePx - camY;
    const float shake = n.alarm > 0 ? std::sin(float(frame) * 1.7f) * 4.0f : 0.0f;
    r.fillRect(x + 8.0f + shake, y + 8.0f, 48.0f, 48.0f, kRed);
    r.fillRect(x + 26.0f + shake, y + 56.0f, 12.0f, 40.0f, rgb(60, 60, 64));
  }
  // The shutter.
  if (n.shutterX >= 0 && !n.shutterOpen)
  {
    const float x = float(n.shutterX) * kTilePx - camX, y = float(n.shutterY0) * kTilePx - camY;
    const float h = float(n.shutterY1 - n.shutterY0 + 1) * kTilePx;
    r.fillRect(x, y, kTilePx, h, rgb(90, 90, 96));
    for (float yy = y + 8.0f; yy < y + h; yy += 16.0f)
      r.fillRect(x, yy, kTilePx, 4.0f, rgb(60, 60, 66));
  }
  // The red evidence.
  if (n.prizeX >= 0 && !n.prizeTaken)
  {
    const float x = float(n.prizeX) * kTilePx - camX, y = float(n.prizeY) * kTilePx - camY;
    r.fillRect(x + 8.0f, y + 16.0f + std::sin(float(frame) * 0.1f) * 4.0f, 48.0f, 40.0f, kRed);
  }
}

void World::drawNoirFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& n = mNoir;
  const auto& f = mFilm;
  const auto& p = mPlayer;
  if (n.on)
  {
    // The dark rooms and the shadows (lamps cut pools out of them).
    const int c0 = int(camX / kCellPx) - 1, r0 = int(camY / kCellPx) - 1;
    const int cols = int(float(kScreenW) / kCellPx) + 3, rows = int(float(kScreenH) / kCellPx) + 3;
    std::vector<float> a(std::size_t(cols * rows), 0.0f);
    bool any = false;
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < cols; ++i)
      {
        const int cx = c0 + i, cy = r0 + j;
        float v = 0.0f;
        for (const auto& b : n.darks)
          if (b.contains(cx, cy))
            v = 0.62f;
        for (const auto& b : n.shadows)
          if (b.contains(cx, cy))
            v = std::max(v, 0.42f);
        if (v > 0.0f)
          for (const auto& l : n.lights)
          {
            const float d = std::sqrt(float((cx - l.x) * (cx - l.x) + (cy - l.y) * (cy - l.y)));
            if (d < float(l.r) * 1.4f)
              v *= std::clamp(d / (float(l.r) * 1.4f), 0.0f, 1.0f);
          }
        a[std::size_t(j * cols + i)] = v;
        any = any || v > 0.0f;
      }
    if (any)
      r.drawAlphaGrid(a, cols, rows, float(c0) * kCellPx - camX, float(r0) * kCellPx - camY, kCellPx, kCellPx,
        rgb(0, 0, 0));
    // Flashlights.
    for (const auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || e.kind != EnemyKind::Flatfoot || e.dive > 0)
        continue;
      const CellBox b = e.box();
      const float ox = (float(e.dir > 0 ? b.right() + 1 : b.left()) ) * kCellPx - camX;
      const float oy = (float(b.top() + 2) + 0.5f) * kCellPx - camY;
      if (offScreen(ox - 600.0f, oy - 300.0f, 1200.0f, 600.0f))
        continue;
      drawCone(r, ox, oy, e.dir > 0 ? 0.0f : kPi, float(kFlashHalf) * kPi / 180.0f, float(kFlashLen) * kCellPx,
        rgb(255, 250, 220));
      // The badge.
      r.fillRect(float(b.x + 1) * kCellPx - camX, float(b.top() + 1) * kCellPx - camY + 8.0f, 12.0f, 12.0f, kRed);
    }
    // The club spotlight.
    for (const auto& s : n.spots)
    {
      const float ox = (float(s.x) + 0.5f) * kCellPx - camX, oy = (float(s.y) + 0.5f) * kCellPx - camY;
      if (offScreen(ox - 800.0f, oy, 1600.0f, 800.0f))
        continue;
      const float ang = std::atan2(float(s.floor - s.y), float(noirSpotX(s) - s.x));
      drawCone(r, ox, oy, ang, float(s.half) * kPi / 180.0f, float(s.len) * kCellPx, rgb(255, 255, 255));
      r.fillRect(ox - 20.0f, oy - 20.0f, 40.0f, 30.0f, rgb(80, 80, 84));
    }
    // Crows about to caw.
    for (const auto& e : mEnemies)
      if (e.alive && e.kind == EnemyKind::Crow && e.tell > 0)
        r.drawText("!", (float(e.x) + 1.0f) * kCellPx - camX, float(e.y - 3) * kCellPx - camY,
          {30.0f, kRed, rgb(0, 0, 0)}, Align::Center);
    // The "?" filling over the runner's head.
    if (n.suspicion > 0)
    {
      const float x = (float(p.x) + 1.5f) * kCellPx - camX, y = float(p.y - 7) * kCellPx - camY;
      const float fill = float(n.suspicion) / float(kSpotFill);
      r.drawText("?", x, y - 20.0f, {40.0f, rgb(255, 255, 255), rgb(0, 0, 0)}, Align::Center);
      r.fillRect(x - 24.0f, y + 26.0f, 48.0f, 8.0f, rgb(30, 30, 30));
      r.fillRect(x - 24.0f, y + 26.0f, 48.0f * fill, 8.0f, kRed);
    }
    // The trench coat and hat.
    if (n.coat && p.state != PlayerState::Dying)
    {
      const float x = (float(p.prevX) + float(p.x - p.prevX) * alpha) * kCellPx - camX;
      const float y = (float(p.prevY) + float(p.y - p.prevY) * alpha + 1.0f) * kCellPx - camY;
      r.fillRect(x + 4.0f, y - 5.0f * kCellPx + 2.0f, 88.0f, 12.0f, rgb(30, 30, 32));
      r.fillRect(x + 22.0f, y - 5.0f * kCellPx - 22.0f, 52.0f, 26.0f, rgb(30, 30, 32));
    }
    // Rain.
    for (const auto& b : n.rains)
    {
      const float bx = float(b.x) * kCellPx - camX, by = float(b.y) * kCellPx - camY;
      const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
      const float x0 = std::max(bx, -40.0f), x1 = std::min(bx + bw, float(kScreenW) + 40.0f);
      const float y0 = std::max(by, -40.0f), y1 = std::min(by + bh, float(kScreenH) + 40.0f);
      if (x0 >= x1 || y0 >= y1)
        continue;
      for (int i = 0; i < 90; ++i)
      {
        const float wx = std::fmod(float(i * 97 + int(camX)) + 4096.0f, float(kScreenW) + 80.0f) - 40.0f;
        const float wy = std::fmod(float(i * 53 + frame * 23 + int(camY)) + 4096.0f, float(kScreenH) + 80.0f) - 40.0f;
        if (wx < x0 || wx > x1 || wy < y0 || wy > y1)
          continue;
        r.drawLine(wx, wy, wx - 6.0f, wy + 26.0f, 2.0f, rgba(200, 200, 210, 90));
      }
    }
    // The alarm light.
    if (n.alarm > 0 && (frame / 6) % 2 == 0)
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(200, 20, 30, 28));
  }
  if (f.on)
  {
    for (const auto& pl : f.peels)
    {
      const float x = float(pl.x) * kCellPx - camX, y = float(pl.y) * kCellPx - camY;
      r.fillRect(x + 4.0f, y + 12.0f, 56.0f, 14.0f, rgb(230, 230, 220));
      r.fillRect(x + 20.0f, y + 6.0f, 20.0f, 10.0f, rgb(200, 200, 190));
    }
    if (f.pramOn)
    {
      const float x = f.pramX * kCellPx - camX, y = float(f.pramY - 2) * kCellPx - camY;
      r.fillRect(x, y, 96.0f, 60.0f, rgb(40, 40, 44));
      r.fillRect(x + 8.0f, y - 30.0f, 50.0f, 34.0f, rgb(60, 60, 66));
      r.fillRect(x + 6.0f, y + 60.0f, 24.0f, 24.0f, rgb(200, 200, 200));
      r.fillRect(x + 66.0f, y + 60.0f, 24.0f, 24.0f, rgb(200, 200, 200));
    }
    if (f.gagX >= 0 && f.gagState == 1)
    {
      const float a = std::min(1.0f, float(f.gagT) / 12.0f) * kPi * 0.5f;
      const float len = float((f.gagY1 - f.gagY0 + 1)) * kTilePx;
      const float px = float(f.gagX) * kTilePx - camX + 16.0f, py = float(f.gagY1 + 1) * kTilePx - camY;
      r.drawLine(px, py, px + std::sin(a) * len, py - std::cos(a) * len, 10.0f, rgb(150, 120, 90));
    }
    else if (f.gagX >= 0 && f.gagState == 2)
    {
      const float x = float(f.gagX + 1) * kTilePx - camX, y = float(f.gagY1 + 1) * kTilePx - camY;
      r.fillRect(x, y, float(f.gagLen) * kTilePx, 12.0f, rgb(150, 120, 90));
    }
    for (const auto& h : f.hats)
    {
      const float x = h.x * kCellPx - camX, y = h.y * kCellPx - camY;
      r.fillRect(x - 20.0f, y + 10.0f, 56.0f, 8.0f, rgb(20, 20, 22));
      r.fillRect(x - 6.0f, y - 10.0f, 28.0f, 22.0f, rgb(20, 20, 22));
    }
  }
}

void World::drawNoirHud(Renderer& r, int frame) const
{
  const auto& n = mNoir;
  const auto& f = mFilm;
  if (n.on)
  {
    const float x = float(kScreenW) * 0.5f, y = 96.0f;
    if (n.alarm > 0)
    {
      r.drawText("ALARM", x, y, {24.0f, (frame / 6) % 2 ? kRed : rgb(255, 255, 255), rgb(0, 0, 0)}, Align::Center);
      r.fillRect(x - 100.0f, y + 30.0f, 200.0f, 8.0f, rgb(40, 40, 40));
      r.fillRect(x - 100.0f, y + 30.0f, 200.0f * float(n.alarm) / float(kAlarmFrames), 8.0f, kRed);
    }
    else if (n.hidden)
      r.drawText("HIDDEN", x, y, {18.0f, rgb(170, 170, 175), rgb(0, 0, 0)}, Align::Center);
  }
  if (f.on)
  {
    // Film grain and a flickering vignette.
    for (int i = 0; i < 40; ++i)
    {
      const float gx = float((i * 7919 + frame * 104729) % kScreenW), gy = float((i * 6007 + frame * 15485863) % kScreenH);
      r.fillRect(gx, gy, 3.0f, 3.0f, rgba(255, 255, 255, 50));
    }
    if (frame % 7 == 0)
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgba(255, 255, 255, 12));
    if (f.card > 0)
    {
      r.fillRect(0.0f, 0.0f, float(kScreenW), float(kScreenH), rgb(8, 8, 8));
      r.fillRect(80.0f, 80.0f, float(kScreenW) - 160.0f, float(kScreenH) - 160.0f, rgb(18, 18, 18));
      r.drawText(f.cardText, float(kScreenW) * 0.5f, float(kScreenH) * 0.5f - 20.0f,
        {30.0f, rgb(240, 240, 230), rgb(0, 0, 0)}, Align::Center);
    }
  }
}

} // namespace gr
