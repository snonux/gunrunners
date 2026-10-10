// Drawing for Idol Mines (world_mine.cpp) and Pinball Mine
// (world_pinball.cpp): tracks on their timber trestles, levers and their
// arrow signs, lanterns, bumpers, the cursed veins, the DAYS WITHOUT
// ACCIDENT sign, carts, Blasting Caps, and the pinball table.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
const Color kIronDark = rgb(52, 48, 56);
const Color kIronLight = rgb(150, 146, 156);
const Color kTimber = rgb(128, 88, 50);
const Color kTimberDark = rgb(80, 54, 30);
const Color kGold = rgb(255, 204, 70);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// A filled disc out of horizontal strips.
void fillDisc(Renderer& r, float cx, float cy, float rad, Color c)
{
  for (float dy = -rad; dy < rad; dy += 2.0f)
  {
    const float half = std::sqrt(std::max(0.0f, rad * rad - (dy + 1.0f) * (dy + 1.0f)));
    r.fillRect(cx - half, cy + dy, half * 2.0f, 2.0f, c);
  }
}

void ring(Renderer& r, float cx, float cy, float rad, float width, Color c)
{
  const int n = 20;
  for (int i = 0; i < n; ++i)
  {
    const float a0 = 6.2831853f * float(i) / float(n), a1 = 6.2831853f * float(i + 1) / float(n);
    r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad,
      width, c);
  }
}

} // namespace

void World::drawMineBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (mRails.empty() && mLevers.empty() && mVeins.empty() && mTrapdoors.empty() && mDaysSign.w == 0)
    return;
  // Tracks: timber sleepers, an iron rail on top, and trestle legs down to
  // the rock wherever the track runs over open air.
  for (const auto& rail : mRails)
  {
    float px = 0, py = 0, pa = 0;
    railPoint(rail, 0, px, py, pa);
    bool prevGap = overGap(rail, px);
    for (int s = 4; s <= rail.len + 3; s += 4)
    {
      float x = 0, y = 0, a = 0;
      railPoint(rail, std::min(s, rail.len), x, y, a);
      const bool gap = overGap(rail, x);
      const float sx0 = px * kCellPx - camX, sy0 = py * kCellPx - camY;
      const float sx1 = x * kCellPx - camX, sy1 = y * kCellPx - camY;
      if (visible(std::min(sx0, sx1), std::min(sy0, sy1), std::fabs(sx1 - sx0) + 1, std::fabs(sy1 - sy0) + 1) &&
          !gap && !prevGap)
      {
        // Up from the track, for the loop drawn the right way round.
        const float ux = std::sin(a), uy = -std::cos(a);
        if (s % 16 == 0)
          r.drawLine(sx1 - std::cos(a) * 10 - ux * 2, sy1 - std::sin(a) * 10 - uy * 2, sx1 + std::cos(a) * 10 - ux * 2,
            sy1 + std::sin(a) * 10 - uy * 2, 9.0f, kTimberDark);
        r.drawLine(sx0 + ux * 6, sy0 + uy * 6, sx1 + ux * 6, sy1 + uy * 6, 5.0f, kIronDark);
        r.drawLine(sx0 + ux * 8, sy0 + uy * 8, sx1 + ux * 8, sy1 + uy * 8, 2.0f, kIronLight);
        // Trestle legs every two blocks over open air.
        const int cx = int(std::floor(x)), cy = int(std::lround(y));
        if (s % 32 == 0 && rail.lever < 0 && !rail.resets && !mMap.solid(cx, cy))
        {
          int depth = 0;
          while (depth < 40 && !mMap.solid(cx, cy + depth) && (depth < 2 || !mMap.solidTop(cx, cy + depth)))
            ++depth;
          const float legY = sy1 + float(depth) * kCellPx;
          r.drawLine(sx1 - 14, sy1 + 4, sx1 - 18, legY, 7.0f, kTimber);
          r.drawLine(sx1 + 14, sy1 + 4, sx1 + 18, legY, 7.0f, kTimber);
          for (float by = sy1 + 50.0f; by + 40.0f < legY; by += 70.0f)
          {
            r.drawLine(sx1 - 16, by, sx1 + 16, by + 40.0f, 4.0f, kTimberDark);
            r.drawLine(sx1 + 16, by, sx1 - 16, by + 40.0f, 4.0f, kTimberDark);
          }
          r.fillRect(sx1 - 24, sy1, 48, 8, kTimber);
        }
      }
      px = x;
      py = y;
      prevGap = gap;
    }
  }
  // Bumpers: a buffer stop, timber with red and white stripes.
  for (const auto& b : mBumpers)
  {
    const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
    if (!visible(x, y, kTilePx, kTilePx))
      continue;
    r.fillRect(x + 10, y + 10, kTilePx - 20, kTilePx - 10, kTimberDark);
    r.fillRect(x + 4, y + 16, kTilePx - 8, 22, rgb(230, 230, 220));
    for (int k = 0; k < 4; ++k)
      r.fillRect(x + 4 + float(k) * 15.0f, y + 16, 7, 22, rgb(210, 40, 40));
    r.fillRect(x + 4, y + 16, kTilePx - 8, 3, rgb(255, 255, 255));
  }
  // Lanterns: a post with a red lamp, before every gap and dead end.
  for (const auto& [lx, ly] : mLanterns)
  {
    const float x = float(lx) * kCellPx - camX + kCellPx, y = float(ly + 1) * kCellPx - camY;
    if (!visible(x - 40, y - 100, 80, 100))
      continue;
    r.fillRect(x - 3, y - 70, 6, 70, kTimberDark);
    r.fillRect(x - 12, y - 92, 24, 24, rgb(60, 50, 50));
    const bool on = (frame / 20) % 3 != 0;
    r.fillRect(x - 9, y - 89, 18, 18, on ? rgb(255, 60, 50) : rgb(140, 30, 30));
    if (on)
      drawGlow(r, mArt, x, y - 80, 60, rgb(255, 60, 40), 0.7f);
  }
  // Levers, and the arrow over each showing where the next cart goes.
  for (std::size_t li = 0; li < mLevers.size(); ++li)
  {
    const auto& l = mLevers[li];
    const float x = float(l.x) * kCellPx - camX, y = float(l.y) * kCellPx - camY;
    if (!visible(x, y - 80, kTilePx, kTilePx * 3))
      continue;
    const float bx = x + kTilePx * 0.5f, by = y + kTilePx * 2.0f;
    r.fillRect(bx - 14, by - 12, 28, 12, rgb(70, 66, 72));
    const float a = l.state == 0 ? -0.6f : 0.6f;
    r.drawLine(bx, by - 8, bx + std::sin(a) * 50, by - 8 - std::cos(a) * 50, 6.0f, rgb(110, 100, 96));
    fillDisc(r, bx + std::sin(a) * 52, by - 8 - std::cos(a) * 52, 8, rgb(220, 40, 40));
    // The arrow sign: straight on, or the branch's way.
    float dirA = 0.0f;
    bool loop = false;
    for (const auto& rail : mRails)
      if (rail.lever == int(li) && l.state == rail.state && rail.pts.size() >= 2)
      {
        loop = rail.resets;
        dirA = std::atan2(float(rail.pts[1].second - rail.pts[0].second), float(rail.pts[1].first - rail.pts[0].first));
      }
    const float sx = bx, sy = y - 10;
    r.fillRect(sx - 30, sy - 22, 60, 44, rgb(236, 220, 170));
    r.fillRect(sx - 30, sy - 22, 60, 4, rgb(255, 245, 210));
    r.fillRect(sx - 2, sy + 22, 4, kTilePx * 1.2f, kTimberDark);
    const Color ink = rgb(40, 30, 20);
    if (loop)
    {
      ring(r, sx, sy, 12, 4.0f, ink);
      r.drawLine(sx + 12, sy, sx + 18, sy - 7, 4.0f, ink);
      r.drawLine(sx + 12, sy, sx + 5, sy - 6, 4.0f, ink);
    }
    else
    {
      const float ca = std::cos(dirA), sa = std::sin(dirA);
      r.drawLine(sx - ca * 18, sy - sa * 18, sx + ca * 18, sy + sa * 18, 5.0f, ink);
      r.drawLine(sx + ca * 18, sy + sa * 18, sx + ca * 8 - sa * 9, sy + sa * 8 + ca * 9, 5.0f, ink);
      r.drawLine(sx + ca * 18, sy + sa * 18, sx + ca * 8 + sa * 9, sy + sa * 8 - ca * 9, 5.0f, ink);
    }
  }
  // The cursed veins: gold in the rock, glowing a sickly green.
  for (const auto& v : mVeins)
  {
    const float x = float(v.x) * kCellPx - camX, y = float(v.y) * kCellPx - camY;
    const float w = float(v.w) * kCellPx, h = float(v.h) * kCellPx;
    if (!visible(x, y, w, h))
      continue;
    const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.12f);
    for (int k = 0; k < int(w / 26.0f); ++k)
    {
      const unsigned hsh = hash2(v.x * 7 + k, v.y);
      const float vx = x + float(k) * 26.0f, vy = y + h - 10.0f - float(hsh % 30u);
      r.drawLine(vx, vy, vx + 22, vy + float(int(hsh >> 5) % 16 - 8), 5.0f, kGold);
      drawGlow(r, mArt, vx + 11, vy, 26, rgb(140, 255, 70), 0.25f + 0.25f * pulse);
    }
  }
  // Rocco's trapdoor: planks over the hole.
  for (const auto& t : mTrapdoors)
  {
    if (t.open)
      continue;
    const float x = float(t.x0) * kTilePx - camX, y = float(t.y) * kTilePx - camY;
    const float w = float(t.x1 - t.x0 + 1) * kTilePx;
    if (!visible(x, y, w, kTilePx))
      continue;
    r.fillRect(x, y, w, kTilePx * 0.5f, kTimber);
    for (float px = x; px < x + w; px += 20.0f)
      r.fillRect(px, y, 2, kTilePx * 0.5f, kTimberDark);
    r.fillRect(x, y + kTilePx * 0.5f - 4, w, 4, kTimberDark);
  }
  // DAYS WITHOUT ACCIDENT.
  if (mDaysSign.w > 0)
  {
    const float x = float(mDaysSign.x) * kCellPx - camX, y = float(mDaysSign.y) * kCellPx - camY;
    const float w = float(mDaysSign.w) * kCellPx, h = float(mDaysSign.h) * kCellPx;
    if (visible(x, y, w, h))
    {
      r.fillRect(x - 4, y - 4, w + 8, h + 8, kTimberDark);
      r.fillRect(x, y, w, h, rgb(240, 226, 180));
      TextStyle small;
      small.size = 15.0f;
      small.color = rgb(60, 40, 20);
      r.drawText("DAYS WITHOUT ACCIDENT", x + w * 0.5f, y + 8, small, Align::Center);
      TextStyle big;
      big.size = 42.0f;
      big.color = mDays == 42 ? rgb(200, 140, 0) : rgb(190, 30, 30);
      r.drawText(std::to_string(mDays), x + w * 0.5f, y + 30, big, Align::Center);
    }
  }
}

void World::drawCart(Renderer& r, const Cart& c, float camX, float camY, float alpha, bool /*front*/) const
{
  if (c.lost != 0)
    return;
  float da = c.angle - c.prevAngle;
  if (da > 3.14159f)
    da -= 6.28318f;
  else if (da < -3.14159f)
    da += 6.28318f;
  const float a = c.prevAngle + da * alpha;
  const float fx = c.prevFx + (c.fx - c.prevFx) * alpha, fy = c.prevFy + (c.fy - c.prevFy) * alpha;
  const float x = fx * kCellPx - camX, y = fy * kCellPx - camY;
  if (!visible(x - 80, y - 120, 160, 160))
    return;
  DrawOpts o;
  o.angle = a * 57.2958f;
  const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "mine_cart", c.painted ? 1 : 0, 0, Cart::kW, Cart::kH).get(1);
  r.draw(*tex, x, y, o);
  if (c.speed > 16 && !c.falling && c.hop == 0)
    for (int k = 0; k < 2; ++k)
    {
      // Sparks off the wheels at speed.
      const unsigned hsh = hash2(int(fx * 7.0f) + k, mStats.frames);
      r.fillRect(x + (k ? 30.0f : -38.0f) + float(hsh % 9u), y - 4 - float((hsh >> 4) % 6u), 4, 4,
        rgb(255, 220, 120));
    }
}

void World::drawMineFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  for (const auto& c : mCarts)
    drawCart(r, c, camX, camY, alpha, true);
  for (const auto& cap : mCaps)
  {
    const float fx = cap.prevFx + (cap.fx - cap.prevFx) * alpha, fy = cap.prevFy + (cap.fy - cap.prevFy) * alpha;
    const float x = fx * kCellPx - camX, y = fy * kCellPx - camY;
    if (!visible(x - 30, y - 40, 60, 60))
      continue;
    // Blinks faster for the last 10 frames.
    const bool blink = cap.fuse <= 10 ? (frame / 2) % 2 == 0 : (frame / 6) % 2 == 0;
    const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "blasting_cap", blink ? 1 : 0, 0, 1, 1).get(1);
    DrawOpts o;
    o.angle = float((cap.bounces * 70 + int(fx * 40.0f)) % 360) * (cap.rail >= 0 || cap.vy == 0.0f ? 0.0f : 1.0f);
    r.draw(*tex, x, y, o);
    drawGlow(r, mArt, x + 6, y - 36, 18 + float(frame % 3) * 4.0f, rgb(255, 220, 120), blink ? 0.9f : 0.5f);
  }
}

void World::drawPinball(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& pin = mPin;
  // Rails and guides: timber with a brass edge.
  for (const auto& s : pin.segs)
  {
    const float x0 = s.x0 * kCellPx - camX, y0 = s.y0 * kCellPx - camY;
    const float x1 = s.x1 * kCellPx - camX, y1 = s.y1 * kCellPx - camY;
    r.drawLine(x0, y0, x1, y1, 14.0f, kTimberDark);
    r.drawLine(x0, y0, x1, y1, 6.0f, rgb(220, 170, 70));
  }
  // Bumpers: ore-crusher drums that light up when hit.
  for (const auto& b : pin.bumpers)
  {
    const float x = b.x * kCellPx - camX, y = b.y * kCellPx - camY, rad = b.r * kCellPx;
    if (b.flash > 0)
      drawGlow(r, mArt, x, y, rad * 2.2f, rgb(255, 220, 120), 0.8f);
    fillDisc(r, x, y, rad, rgb(90, 70, 60));
    fillDisc(r, x, y, rad * 0.78f, b.flash > 0 ? rgb(255, 230, 150) : rgb(200, 90, 50));
    ring(r, x, y, rad, 5.0f, kIronDark);
    fillDisc(r, x, y, rad * 0.25f, rgb(255, 250, 230));
  }
  // The six lanterns.
  for (const auto& l : pin.lamps)
  {
    const float x = l.x * kCellPx - camX, y = l.y * kCellPx - camY;
    r.drawLine(x, y - 40, x, y - 18, 3.0f, kIronDark);
    r.fillRect(x - 13, y - 18, 26, 34, rgb(60, 50, 50));
    r.fillRect(x - 9, y - 14, 18, 26, l.lit ? rgb(255, 210, 90) : rgb(70, 60, 50));
    if (l.lit)
      drawGlow(r, mArt, x, y, 50.0f + float(l.flash) * 4.0f, rgb(255, 190, 70), 0.75f);
  }
  // The gate: iron bars until all six are lit.
  {
    const float x = pin.gateX * kCellPx - camX, y = pin.gateY * kCellPx - camY;
    if (pin.gateOpen)
      drawGlow(r, mArt, x, y, 90.0f + 10.0f * std::sin(float(frame) * 0.2f), rgb(255, 220, 120), 0.8f);
    else
      for (int k = -2; k <= 2; ++k)
        r.drawLine(x + float(k) * 16.0f, y - 30, x + float(k) * 16.0f, y + 30, 6.0f, kIronDark);
  }
  // Flippers: timber paddles shod with iron.
  for (int side = 0; side < 2; ++side)
  {
    const float px = side == 0 ? pin.lx : pin.rx, py = side == 0 ? pin.ly : pin.ry;
    const int step = side == 0 ? pin.flipL : pin.flipR;
    const float t = float(step) / float(Pinball::kFlipSteps);
    const float a = 0.52f + (-0.42f - 0.52f) * t;
    const float dir = side == 0 ? 1.0f : -1.0f;
    const float x0 = px * kCellPx - camX, y0 = py * kCellPx - camY;
    const float x1 = (px + dir * std::cos(a) * pin.flipLen) * kCellPx - camX, y1 = (py + std::sin(a) * pin.flipLen) * kCellPx - camY;
    r.drawLine(x0, y0, x1, y1, 26.0f, kIronDark);
    r.drawLine(x0, y0, x1, y1, 18.0f, rgb(200, 60, 40));
    fillDisc(r, x0, y0, 12, kIronLight);
  }
  // The plunger's spring.
  {
    const float x = pin.plungerX * kCellPx - camX, y = pin.plungerY * kCellPx - camY;
    const float squeeze = pin.pull > 0 ? -float(pin.pull) * 0.4f : (pin.inPlunger ? 6.0f * std::sin(float(frame) * 0.3f) : 0.0f);
    const float pullPx = float(pin.pull) * 1.6f;
    for (int k = 0; k < 5; ++k)
      r.drawLine(x - 16, y + 6 + pullPx + float(k) * (10 + squeeze * 0.2f), x + 16,
        y + 11 + pullPx + float(k) * (10 + squeeze * 0.2f), 4.0f, kIronLight);
    // How hard it will go: a gauge up the side of the lane while you pull.
    if (pin.inPlunger)
    {
      const float h = 120.0f, gx = x + 40.0f, gy = y - h - 10.0f;
      r.fillRect(gx, gy, 10, h, rgba(0, 0, 0, 160));
      const float fill = h * float(pin.pull) / 15.0f;
      r.fillRect(gx + 2, gy + h - fill, 6, fill, rgb(255, 200 - pin.pull * 8, 60));
    }
  }
  // The ball: a gold nugget wearing the runner's stripe.
  const float bx = (pin.prevX + (pin.x - pin.prevX) * alpha) * kCellPx - camX;
  const float by = (pin.prevY + (pin.y - pin.prevY) * alpha) * kCellPx - camY + (pin.inPlunger ? float(pin.pull) * 1.6f : 0.0f);
  const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "pin_ball", mCharacterIndex, 0, 2, 2).get(1);
  DrawOpts o;
  o.angle = float(int((pin.x * 40.0f)) % 360);
  drawGlow(r, mArt, bx, by, 40, rgb(255, 220, 120), 0.35f);
  r.draw(*tex, bx, by + kCellPx, o);
}

void World::drawMineHud(Renderer& r, int frame) const
{
  if (!mPinball)
    return;
  int lit = 0;
  for (const auto& l : mPin.lamps)
    lit += l.lit;
  const float cx = float(kScreenW) * 0.5f + 150.0f, cy = 110.0f;
  for (std::size_t i = 0; i < mPin.lamps.size(); ++i)
  {
    const float x = cx + float(i) * 22.0f;
    r.fillRect(x - 8, cy - 10, 16, 20, rgb(50, 40, 40));
    r.fillRect(x - 5, cy - 7, 10, 14, mPin.lamps[i].lit ? rgb(255, 210, 90) : rgb(80, 70, 60));
  }
  if (mPin.gateOpen && (frame / 8) % 2)
  {
    TextStyle st;
    st.size = 18.0f;
    st.color = rgb(255, 220, 120);
    r.drawText("GATE OPEN", cx + 55.0f, cy + 16.0f, st, Align::Center);
  }
  (void)lit;
}

} // namespace gr
