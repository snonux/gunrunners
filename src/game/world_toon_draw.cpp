// Level 28, Toon Town Throwdown: drawing (painted tunnels, the Portable
// Hole, pianos and their shadows, the "uh-oh" sign, bonk stars, the canyon
// sign, the ink, Buddy Boom with his mallet, pies, pencil and eraser, the
// cream over half the screen), and the Inkwell bonus (the pen, the ink
// meter, refills). See world_toon.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

const Color kInk = rgb(24, 18, 16);
const Color kPaper = rgb(252, 244, 222);

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

// A filled disc as a stack of horizontal lines.
void disc(Renderer& r, float cx, float cy, float rad, Color c, float squash = 1.0f)
{
  const float h = rad * squash;
  for (float y = -h; y <= h; y += 3.0f)
  {
    const float w = rad * std::sqrt(std::max(0.0f, 1.0f - (y / h) * (y / h)));
    r.fillRect(cx - w, cy + y - 1.5f, w * 2.0f, 3.0f, c);
  }
}

void ring(Renderer& r, float cx, float cy, float rad, float width, Color c)
{
  const int n = 24;
  for (int i = 0; i < n; ++i)
  {
    const float a0 = float(i) / float(n) * 6.2831853f, a1 = float(i + 1) / float(n) * 6.2831853f;
    r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, width, c);
  }
}

// Bonk stars circling over a head.
void stars(Renderer& r, float cx, float cy, int frame)
{
  for (int i = 0; i < 3; ++i)
  {
    const float a = float(frame) * 0.4f + float(i) * 2.094f;
    const float x = cx + std::cos(a) * 40.0f, y = cy + std::sin(a) * 12.0f;
    r.drawLine(x - 8.0f, y, x + 8.0f, y, 4.0f, rgb(255, 230, 80));
    r.drawLine(x, y - 8.0f, x, y + 8.0f, 4.0f, rgb(255, 230, 80));
  }
}

} // namespace

void World::drawToonBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& t = mToon;
  // Painted tunnels: an arch of black paint; a real one has its shadow.
  for (std::size_t i = 0; i < t.tunnels.size(); ++i)
  {
    const auto& tn = t.tunnels[i];
    const float x = float(tn.x) * kTilePx - camX, y = float(tn.y) * kTilePx - camY;
    if (offScreen(x, y, 128.0f, 192.0f))
      continue;
    if (t.hole.open && t.hole.tunnel == int(i))
    {
      // Behind the paint: the PoC's first tiles and its placeholder runner.
      for (int k = 0; k < 6; ++k)
        r.fillRect(x + float(k % 2) * 64.0f, y + float(k / 2) * 64.0f, 62.0f, 62.0f,
          (k + frame / 8) % 2 ? rgb(90, 90, 200) : rgb(60, 160, 60));
      r.fillRect(x + 44.0f, y + 70.0f, 40.0f, 96.0f, rgb(255, 0, 255));
      continue;
    }
    if (!tn.real)
      r.fillRect(x, y, 128.0f, 192.0f, rgb(150, 110, 80));
    disc(r, x + 64.0f, y + 80.0f, 60.0f, kInk, 1.0f);
    r.fillRect(x + 4.0f, y + 80.0f, 120.0f, 112.0f, kInk);
    if (tn.real)
      r.fillRect(x + 4.0f, y + 182.0f, 120.0f, 10.0f, rgb(0, 0, 0)); // its shadow
    else
      r.drawLine(x + 30.0f, y + 40.0f, x + 50.0f, y + 70.0f, 4.0f, rgb(70, 60, 60)); // a brush stroke
  }
  // The canyon sign.
  if (t.signX >= 0)
  {
    const float x = float(t.signX) * kTilePx - camX, y = float(t.signY - 1) * kTilePx - camY;
    if (!offScreen(x - 120.0f, y - 60.0f, 300.0f, 200.0f))
    {
      r.fillRect(x + 26.0f, y + 40.0f, 12.0f, 88.0f, rgb(120, 80, 50));
      r.fillRect(x - 100.0f, y - 30.0f, 264.0f, 76.0f, rgb(230, 210, 160));
      r.drawText("GRAVITY HASN'T", x + 32.0f, y - 22.0f, {16.0f, kInk, rgb(255, 255, 255)}, Align::Center);
      r.drawText("BEEN INVENTED YET", x + 32.0f, y + 6.0f, {16.0f, kInk, rgb(255, 255, 255)}, Align::Center);
    }
  }
  // Buddy's sketch and the full heal.
  if (t.prizeX >= 0 && !t.prizeTaken)
  {
    const float x = float(t.prizeX) * kTilePx - camX, y = float(t.prizeY - 1) * kTilePx - camY;
    r.fillRect(x, y + 20.0f, 64.0f, 80.0f, kPaper);
    disc(r, x + 32.0f, y + 50.0f, 18.0f, kInk);
    r.drawLine(x + 22.0f, y + 70.0f, x + 42.0f, y + 70.0f, 3.0f, kInk);
  }
  if (t.fullhealX >= 0 && !t.fullhealUsed)
  {
    const float x = float(t.fullhealX) * kTilePx - camX, y = float(t.fullhealY - 1) * kTilePx - camY;
    r.fillRect(x + 8.0f, y + 40.0f, 48.0f, 80.0f, rgb(250, 90, 110));
    r.fillRect(x + 26.0f, y + 56.0f, 12.0f, 48.0f, rgb(255, 255, 255));
    r.fillRect(x + 12.0f, y + 74.0f, 40.0f, 12.0f, rgb(255, 255, 255));
  }
  // The ink.
  for (const auto& p : t.ponds)
  {
    const float x = float(p.x) * kCellPx - camX, y = float(p.y) * kCellPx - camY;
    r.fillRect(x, y + 8.0f, float(p.w) * kCellPx, float(p.h) * kCellPx - 8.0f, kInk);
    for (int i = 0; i < p.w / 4; ++i)
      r.fillRect(x + float(i) * 4.0f * kCellPx + float((frame + i * 7) % 24), y + 4.0f, 40.0f, 6.0f, rgb(60, 50, 70));
  }
  // Pianos: the shadow grows, then the piano.
  for (const auto& pn : t.pianos)
  {
    const float x = float(pn.x) * kCellPx - camX;
    if (pn.state == 0)
    {
      const float k = 1.0f - float(pn.t) / float(kPianoTell);
      disc(r, x + 96.0f, float(pn.floor) * kCellPx - camY - 4.0f, 30.0f + 70.0f * k, rgba(0, 0, 0, 120), 0.18f);
      continue;
    }
    const float y = pn.y * kCellPx - camY;
    if (pn.state == 1)
      disc(r, x + 96.0f, float(pn.floor) * kCellPx - camY - 4.0f, 100.0f, rgba(0, 0, 0, 120), 0.18f);
    r.fillRect(x, y, 192.0f, 128.0f, kInk);
    r.fillRect(x + 8.0f, y + 8.0f, 176.0f, 40.0f, rgb(250, 250, 240));
    for (int k = 0; k < 10; ++k)
      r.fillRect(x + 14.0f + float(k) * 17.0f, y + 8.0f, 8.0f, 24.0f, kInk);
    r.fillRect(x + 12.0f, y + 100.0f, 14.0f, 28.0f, kInk);
    r.fillRect(x + 166.0f, y + 100.0f, 14.0f, 28.0f, kInk);
  }
}

void World::drawToonFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& t = mToon;
  const auto& p = mPlayer;
  const float px = (float(p.prevX) + float(p.x - p.prevX) * alpha) * kCellPx - camX;
  const float py = (float(p.prevY) + float(p.y - p.prevY) * alpha) * kCellPx - camY;
  // The open Portable Hole and the one in flight.
  if (t.hole.open)
  {
    const float x = float(t.hole.x0) * kTilePx - camX, y = float(t.hole.y0) * kTilePx - camY;
    const float w = float(t.hole.x1 - t.hole.x0 + 1) * kTilePx, h = float(t.hole.y1 - t.hole.y0 + 1) * kTilePx;
    const float fade = t.hole.t < 20 && (frame / 3) % 2 ? 0.5f : 1.0f;
    disc(r, x + w * 0.5f, y + h * 0.5f, std::max(w, h) * 0.55f, withAlpha(rgb(20, 14, 26), std::uint8_t(80 * fade)),
      std::min(w, h) / std::max(w, h));
    ring(r, x + w * 0.5f, y + h * 0.5f, std::max(w, h) * 0.5f, 4.0f, rgba(150, 120, 170, std::uint8_t(160 * fade)));
  }
  if (t.toss.on)
  {
    disc(r, t.toss.fx * kCellPx - camX, t.toss.fy * kCellPx - camY, 18.0f, rgb(20, 14, 26), 0.5f);
    ring(r, t.toss.fx * kCellPx - camX, t.toss.fy * kCellPx - camY, 18.0f, 3.0f, rgb(150, 120, 170));
  }
  // The runner's cartoon business: the "uh-oh" sign, bonk stars, the hat.
  if (p.state != PlayerState::Dying && !p.hidden)
  {
    const float cx = px + 1.5f * kCellPx, top = py - 4.0f * kCellPx;
    if (t.uhOh > 0)
    {
      r.fillRect(cx - 4.0f, top - 90.0f, 8.0f, 70.0f, rgb(120, 80, 50));
      r.fillRect(cx - 70.0f, top - 130.0f, 140.0f, 50.0f, rgb(250, 240, 210));
      r.drawText("UH-OH", cx, top - 122.0f, {24.0f, kInk, rgb(255, 255, 255)}, Align::Center);
    }
    if (t.bonk > 0)
      stars(r, cx, top - 10.0f, frame);
    if (t.tip > 0)
    {
      const float lift = float(std::min(t.tip, 30 - t.tip)) * 2.0f;
      r.fillRect(cx - 26.0f, top - 18.0f - lift, 52.0f, 10.0f, kInk);
      r.fillRect(cx - 16.0f, top - 40.0f - lift, 32.0f, 24.0f, kInk);
    }
  }
  // Buddy Boom.
  const auto& b = t.buddy;
  if (b.on && b.state != BuddyState::Done)
  {
    const float bx = (b.prevX + (b.x - b.prevX) * alpha) * kCellPx - camX;
    const float by = (b.prevY + (b.y - b.prevY) * alpha + 1.0f) * kCellPx - camY; // his feet
    const float w = float(Buddy::kW) * kCellPx, h = float(Buddy::kH) * kCellPx * (1.0f - 0.7f * b.deflate);
    const bool flash = b.flash > 0 && (frame / 2) % 2;
    const Color body = flash ? rgb(255, 255, 255) : rgb(240, 120, 70);
    const float spin = b.state == BuddyState::Spin ? std::sin(float(frame) * 0.8f) : 1.0f;
    const float cx = bx + w * 0.5f;
    // The pencil under him on the way up.
    if (b.state == BuddyState::Pencil || b.state == BuddyState::Draw)
    {
      r.fillRect(cx - 20.0f, by, 40.0f, 200.0f, rgb(250, 200, 60));
      r.drawLine(cx - 20.0f, by + 200.0f, cx, by + 240.0f, 6.0f, kInk);
      r.drawLine(cx + 20.0f, by + 200.0f, cx, by + 240.0f, 6.0f, kInk);
    }
    // Legs, body (shirt No. 42), head, ears.
    r.fillRect(cx - w * 0.3f, by - h * 0.25f, w * 0.18f, h * 0.25f, kInk);
    r.fillRect(cx + w * 0.12f, by - h * 0.25f, w * 0.18f, h * 0.25f, kInk);
    r.fillRect(cx - w * 0.45f * std::abs(spin), by - h * 0.65f, w * 0.9f * std::abs(spin), h * 0.42f, body);
    r.drawText("42", cx, by - h * 0.55f, {28.0f, kPaper, kInk}, Align::Center);
    disc(r, cx, by - h * 0.8f, w * 0.32f, flash ? rgb(255, 255, 255) : rgb(250, 230, 200));
    disc(r, cx - w * 0.3f, by - h * 0.97f, w * 0.14f, kInk);
    disc(r, cx + w * 0.3f, by - h * 0.97f, w * 0.14f, kInk);
    disc(r, cx + float(b.dir) * w * 0.12f, by - h * 0.84f, 7.0f, kInk);
    // The nose: red while it flashes after a slam.
    const CellBox nose = buddyNose();
    const Color noseC = b.nose > 0 ? ((frame / 3) % 2 ? rgb(255, 40, 40) : rgb(200, 0, 0)) : rgb(60, 40, 40);
    disc(r, (float(nose.x) + 2.0f) * kCellPx - camX, (float(nose.y) + 1.5f) * kCellPx - camY, 22.0f, noseC);
    // The mallet: raised overhead in the tell.
    if (b.phase == 1)
    {
      const bool up = b.state == BuddyState::Slam && b.t < kSlamTell;
      const float hx = cx + float(b.dir) * w * 0.6f, hy = up ? by - h * 1.25f : by - h * 0.3f;
      r.drawLine(cx + float(b.dir) * w * 0.3f, by - h * 0.55f, hx, hy, 10.0f, rgb(150, 100, 60));
      r.fillRect(hx - 40.0f, hy - 30.0f, 80.0f, 60.0f, rgb(170, 120, 80));
    }
    if (b.state == BuddyState::Dizzy)
      stars(r, cx, by - h - 20.0f, frame);
    if (b.state == BuddyState::Pie && b.cool == 0)
      disc(r, cx + float(b.dir) * w * 0.55f, by - h * 0.7f, 26.0f, rgb(255, 250, 240), 0.5f);
  }
  if (b.on)
  {
    // His drawing: an outline, then solid ink.
    if (b.lineY >= 0)
    {
      const float x = float(b.lineX0) * kTilePx - camX, y = float(b.lineY) * kTilePx - camY;
      const float w = float(b.lineX1 - b.lineX0 + 1) * kTilePx;
      if (b.lineSolid)
        r.fillRect(x, y, w, 20.0f, kInk);
      else if ((frame / 3) % 2)
        r.fillRect(x, y + 6.0f, w, 6.0f, rgba(24, 18, 16, 140));
      if (b.state == BuddyState::Dizzy && b.dizzyHits > 0)
        for (int k = 0; k < b.dizzyHits; ++k)
          r.drawLine(x + w * 0.5f + float(k * 9 - 40), y, x + w * 0.5f + float(k * 9 - 34), y + 20.0f, 3.0f, kPaper);
    }
    // The eraser's shadow on a floor segment.
    if (b.state == BuddyState::Erase && b.eraseSeg >= 0)
    {
      const float x = float(b.segX0 + b.eraseSeg * b.segW) * kTilePx - camX, y = float(b.floorY) * kTilePx - camY;
      const float k = float(b.t) / float(kEraseTell);
      r.fillRect(x, y - 12.0f, float(b.segW) * kTilePx, 12.0f, rgba(0, 0, 0, std::uint8_t(60 + 120 * k)));
      r.fillRect(x + 40.0f, y - 200.0f + 120.0f * k, float(b.segW) * kTilePx - 80.0f, 60.0f, rgb(250, 160, 170));
    }
    for (const auto& w : b.waves)
    {
      const float x = w.x * kCellPx - camX, y = float(w.y) * kCellPx - camY;
      r.fillRect(x, y - 8.0f, 64.0f, 40.0f, rgba(255, 255, 255, 200));
      r.drawLine(x, y + 32.0f, x + 32.0f, y - 8.0f, 4.0f, kInk);
      r.drawLine(x + 32.0f, y - 8.0f, x + 64.0f, y + 32.0f, 4.0f, kInk);
    }
    for (const auto& pie : b.pies)
    {
      const float x = pie.x * kCellPx - camX, y = pie.y * kCellPx - camY;
      disc(r, x + 32.0f, y + 32.0f, 30.0f, rgb(255, 250, 240), 0.55f);
      r.fillRect(x + 6.0f, y + 36.0f, 52.0f, 14.0f, rgb(200, 150, 90));
    }
  }
  // The Inkwell: the pen at work.
  if (mInk.on && mInk.drawing)
  {
    const float x = mInk.penX * kCellPx - camX, y = mInk.penY * kCellPx - camY;
    r.drawLine(x, y, x + 20.0f, y - 50.0f, 8.0f, kInk);
    disc(r, x, y, 6.0f, kInk);
  }
  if (mInk.on)
    for (const auto& rf : mInk.refills)
      if (!rf.taken)
      {
        const float x = float(rf.x) * kTilePx - camX, y = float(rf.y - 1) * kTilePx - camY;
        r.fillRect(x + 12.0f, y + 50.0f, 40.0f, 70.0f, kInk);
        r.fillRect(x + 18.0f, y + 40.0f, 28.0f, 14.0f, rgb(120, 100, 90));
      }
}

void World::drawToonHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& b = mToon.buddy;
  // A pie in the face: half the screen white with cream.
  if (b.whiteL > 0)
    r.fillRect(0.0f, 0.0f, float(kScreenW) * 0.5f, float(kScreenH), rgba(255, 252, 240, std::uint8_t(std::min(255, b.whiteL * 12))));
  if (b.whiteR > 0)
    r.fillRect(float(kScreenW) * 0.5f, 0.0f, float(kScreenW) * 0.5f, float(kScreenH),
      rgba(255, 252, 240, std::uint8_t(std::min(255, b.whiteR * 12))));
  if (buddyFight())
  {
    const float w = 600.0f, x = float(kScreenW) * 0.5f - w * 0.5f, y = float(kScreenH) - 70.0f;
    r.fillRect(x - 4.0f, y - 4.0f, w + 8.0f, 32.0f, kInk);
    r.fillRect(x, y, w * float(buddyHp()) / float(3 * kBuddyHp), 24.0f, rgb(240, 120, 70));
    r.drawText("BUDDY BOOM", float(kScreenW) * 0.5f, y - 40.0f, {24.0f, kPaper, kInk, true}, Align::Center);
  }
  if (mInk.on)
  {
    const float x = 40.0f, y = float(kScreenH) - 80.0f, w = 320.0f;
    r.fillRect(x - 4.0f, y - 4.0f, w + 8.0f, 32.0f, kInk);
    r.fillRect(x, y, w * float(mInk.ink) / float(kInkMax), 24.0f, rgb(60, 50, 90));
    r.drawText("INK", x, y - 36.0f, {22.0f, kPaper, kInk, true}, Align::Left);
  }
}

} // namespace gr
