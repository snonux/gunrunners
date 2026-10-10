// Level 15's drawing (world_station.cpp has the rules): the hull panels in
// their yellow-black frames, the vents' rush of air, handrails, crates,
// Breach Charges, weld seams, tether beams and everything tumbling out
// into space.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
const Color kHazard = rgb(255, 200, 30);
const Color kInkDark = rgb(16, 18, 26);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

void crateAt(Renderer& r, float x, float y, int gems, float alpha)
{
  const float a = std::clamp(alpha, 0.0f, 1.0f);
  auto c = [&](Color col) { return rgba(redOf(col), greenOf(col), blueOf(col), int(255 * a)); };
  r.fillRect(x + 2, y + 2, 60, 60, c(rgb(70, 82, 100)));
  r.fillRect(x + 6, y + 6, 52, 52, c(gems > 0 ? rgb(150, 120, 60) : rgb(120, 134, 152)));
  // Cross bracing and a frosted top.
  r.drawLine(x + 8, y + 8, x + 56, y + 56, 5, c(rgb(80, 92, 110)));
  r.drawLine(x + 56, y + 8, x + 8, y + 56, 5, c(rgb(80, 92, 110)));
  r.fillRect(x + 4, y + 2, 56, 6, c(rgb(220, 240, 255)));
  r.fillRect(x + 22, y + 24, 20, 16, c(gems > 0 ? rgb(90, 230, 160) : kHazard));
}

} // namespace

void World::drawStationBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& st = mStation;
  if (!st.on())
    return;
  // Handrails: a steel bar on posts at hand height.
  for (const auto& rl : st.rails)
  {
    const float x0 = float(rl.x0) * kTilePx - camX, x1 = float(rl.x1 + 1) * kTilePx - camX;
    const float y = float(rl.y) * kTilePx + 40.0f - camY;
    if (!visible(x0, y - 20, x1 - x0, 120))
      continue;
    for (int bx = rl.x0; bx <= rl.x1 + 1; bx += 3)
      r.fillRect(float(bx) * kTilePx - camX - 3, y, 6, 88, rgb(110, 120, 138));
    r.fillRect(x0, y - 4, x1 - x0, 9, rgb(190, 200, 214));
    r.fillRect(x0, y - 4, x1 - x0, 3, rgb(240, 248, 255));
  }
  // Weld seams: hot orange fading to red (green on the carrier's).
  for (const auto& s : st.seams)
  {
    const float x = float(s.x) * kCellPx - camX, y = float(s.y) * kCellPx - camY;
    if (!visible(x, y, kCellPx, kCellPx))
      continue;
    const float t = float(s.life) / 45.0f;
    const Color c = s.green ? rgb(120, 255, 90) : lerpColor(rgb(200, 30, 20), rgb(255, 210, 90), t);
    r.fillRect(x, y + kCellPx - 8, kCellPx, 8, c);
    if (s.life > 30)
      drawGlow(r, mArt, x + kCellPx * 0.5f, y + kCellPx - 4, 18, c, 0.5f * t);
  }
  (void)frame;
}

void World::drawStationFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& st = mStation;
  if (!st.on() && st.charges.empty())
    return;
  // Hull panels.
  for (const auto& p : st.panels)
  {
    const float x = float(p.bx) * kTilePx - camX, y = float(p.by) * kTilePx - camY;
    const float w = float(p.w) * kTilePx, h = float(p.h) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    if (p.open > 0)
    {
      // Open: space beyond, stars streaming past.
      r.fillRect(x, y, w, h, rgb(4, 4, 16));
      for (int i = 0; i < 10; ++i)
      {
        const float sx = x + float((i * 37 + frame * 3) % int(w)), sy = y + float((i * 53) % int(h));
        r.fillRect(sx, sy, 3, 3, rgb(230, 240, 255));
      }
      const int frames = p.vent >= 0 ? st.vents[std::size_t(p.vent)].frames : 60;
      if (p.open >= frames - 12 && (frame / 2) % 2 == 0)
        r.fillRect(x - 8, y - 8, w + 16, h + 16, rgba(255, 30, 30, 90), Blend::Add);
    }
    else
    {
      // Shut: a steel shutter, dented once it has slammed.
      r.fillRect(x, y, w, h, rgb(96, 106, 122));
      for (float k = 12; k < h; k += 16)
        r.fillRect(x + 6, y + k, w - 12, 4, rgb(70, 78, 92));
      if (p.dented)
        drawGlow(r, mArt, x + w * 0.4f, y + h * 0.5f, 18, rgb(40, 44, 54), 0.8f);
      if (p.bonus)
      {
        // The static the other side of the bonus panel shows at the seams.
        for (int i = 0; i < 6; ++i)
          r.fillRect(x + float((frame * 7 + i * 13) % int(w)), y + h * 0.5f + float(i % 3) * 6.0f, 6, 2,
            rgba(255, 255, 255, 140));
      }
    }
    // The yellow-black frame.
    const float t = 8.0f;
    for (float k = 0; k < w + 2 * t; k += 16)
    {
      const Color c = int(k / 16) % 2 ? kInkDark : kHazard;
      r.fillRect(x - t + k, y - t, std::min(16.0f, w + t - k), t, c);
      r.fillRect(x - t + k, y + h, std::min(16.0f, w + t - k), t, c);
    }
    for (float k = 0; k < h; k += 16)
    {
      const Color c = int(k / 16) % 2 ? kHazard : kInkDark;
      r.fillRect(x - t, y + k, t, std::min(16.0f, h - k), c);
      r.fillRect(x + w, y + k, t, std::min(16.0f, h - k), c);
    }
  }
  // The rush of air toward an open panel.
  for (const auto& p : st.panels)
  {
    if (p.open == 0 || p.vent < 0)
      continue;
    const VentZone& v = st.vents[std::size_t(p.vent)];
    const float px = (float(p.bx) + float(p.w) * 0.5f) * kTilePx - camX;
    const float py = (float(p.by) + float(p.h) * 0.5f) * kTilePx - camY;
    for (int i = 0; i < 40; ++i)
    {
      const float u = float((i * 97 + 13) % 100) / 100.0f, w = float((i * 61 + 7) % 100) / 100.0f;
      const float sx = (float(v.x0) + u * float(v.x1 - v.x0 + 1)) * kTilePx - camX;
      const float sy = (float(v.y0) + w * float(v.y1 - v.y0 + 1)) * kTilePx - camY;
      const float f = std::fmod(float(frame + i * 7) / 30.0f, 1.0f);
      const float x = sx + (px - sx) * f, y = sy + (py - sy) * f;
      if (!visible(x, y, 4, 4))
        continue;
      const float dx = (px - sx) * 0.04f, dy = (py - sy) * 0.04f;
      r.drawLine(x, y, x + dx, y + dy, 2, rgba(220, 240, 255, 110));
    }
  }
  // Crates.
  for (const auto& c : st.crates)
  {
    if (!c.alive)
      continue;
    if (c.held)
    {
      // Over a Loader Mech's head.
      for (const auto& e : mEnemies)
        if (e.alive && e.kind == EnemyKind::Loader && e.aimX == int(&c - st.crates.data()))
        {
          const float x = (float(e.x + e.w / 2) - 1.0f) * kCellPx - camX, y = float(e.y - e.h - 1) * kCellPx - camY;
          crateAt(r, x, y, c.gems, 1.0f);
        }
      continue;
    }
    const float x = (c.loose ? c.fx : float(c.bx * kCellsPerTile)) * kCellPx - camX;
    const float y = (c.loose ? c.fy : float(c.by * kCellsPerTile)) * kCellPx - camY;
    if (visible(x, y, kTilePx, kTilePx))
      crateAt(r, x, y, c.gems, 1.0f);
  }
  // Tether beams.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden || e.kind != EnemyKind::Tether || e.variant != 1 || e.attach < 0 ||
        !mEnemies[std::size_t(e.attach)].alive)
      continue;
    const CellBox b = tetherBeam(e);
    const float x = (float(b.x) + 1.0f) * kCellPx - camX, y0 = float(b.y) * kCellPx - camY;
    const float y1 = float(b.y + b.h) * kCellPx - camY;
    if (!visible(x - 20, y0, 40, y1 - y0))
      continue;
    if (e.dive == 0)
    {
      r.drawLine(x, y0, x, y1, 10, rgba(255, 60, 160, 120), Blend::Add);
      r.drawLine(x, y0, x, y1, 4, rgb(255, 210, 240));
    }
    else if (e.tell > 0 && (frame / 2) % 2 == 0)
      r.drawLine(x, y0, x, y1, 2, rgba(255, 120, 200, 160));
  }
  // Breach Charges: a puck with a blinking light.
  for (const auto& c : st.charges)
  {
    const float x = (c.prevFx + (c.fx - c.prevFx) * alpha) * kCellPx - camX;
    const float y = (c.prevFy + (c.fy - c.prevFy) * alpha) * kCellPx - camY;
    if (!visible(x, y, 16, 16))
      continue;
    r.fillRect(x - 11, y - 8, 22, 16, rgb(40, 44, 52));
    r.fillRect(x - 9, y - 6, 18, 4, kHazard);
    const bool lit = (frame / 6 + c.age) % 2 == 0;
    r.fillRect(x - 3, y, 6, 5, lit ? rgb(255, 60, 40) : rgb(120, 20, 20));
    if (lit)
      drawGlow(r, mArt, x, y + 2, 14, rgb(255, 60, 40), 0.7f);
  }
  // Out into space: tumbling, shrinking, fading.
  for (const auto& d : st.debris)
  {
    const float x = d.x * kCellPx - camX, y = d.y * kCellPx - camY;
    if (!visible(x, y, 32, 32))
      continue;
    const float f = std::clamp(float(d.life) / 40.0f, 0.0f, 1.0f);
    switch (d.kind)
    {
      case 0:
        crateAt(r, x - 32 * f, y - 32 * f, d.variant, f);
        break;
      case 1:
        drawGlow(r, mArt, x, y, 14 * f + 4, rgb(120, 255, 200), f);
        break;
      case 2:
      {
        const EnemyDef& def = enemyDef(d.variant);
        const Texture* tex = &styledEnemySprite(mArt, r, mTheme, def.key, 0, 0, def.w, def.h).get(1);
        DrawOpts o;
        o.angle = d.angle * 57.3f;
        o.scale = 0.3f + 0.7f * f;
        o.alpha = f;
        r.draw(*tex, x, y + float(def.h) * kCellPx * 0.5f * o.scale, o);
        break;
      }
      case 4:
      {
        // The sock.
        const float a = d.angle;
        const float cx = std::cos(a) * 12.0f, cy = std::sin(a) * 12.0f;
        r.drawLine(x - cx, y - cy, x + cx, y + cy, 10, rgb(240, 240, 240));
        r.drawLine(x + cx, y + cy, x + cx + 8, y + cy + 6, 10, rgb(240, 240, 240));
        r.drawLine(x - cx, y - cy, x - cx * 0.6f, y - cy * 0.6f, 10, rgb(230, 60, 60));
        break;
      }
      default:
        drawGlow(r, mArt, x, y, 10 * f + 4, rgb(255, 230, 140), f);
        break;
    }
  }
  // A runner caught by a panel's frame: the frame's clamps on them.
  if (st.caught >= 0)
  {
    const auto& p = mPlayer;
    const float x = float(p.x + 1) * kCellPx - camX, y = float(p.y - 2) * kCellPx - camY;
    drawGlow(r, mArt, x, y, 50, rgb(255, 200, 30), 0.35f + 0.15f * float(frame % 6) / 6.0f);
  }
}

void World::drawStationHud(Renderer& r, int frame) const
{
  const auto& st = mStation;
  bool klaxon = false, open = false;
  for (const auto& p : st.panels)
    if (p.open > 0)
    {
      open = true;
      const int frames = p.vent >= 0 ? st.vents[std::size_t(p.vent)].frames : 60;
      klaxon = klaxon || p.open >= frames - 12;
    }
  if (!open)
    return;
  // The red strobe at the screen's edge while a vent pulls.
  const int a = klaxon ? ((frame / 2) % 2 ? 110 : 30) : 40;
  r.fillRect(0, 0, float(kScreenW), 14, rgba(255, 30, 30, a));
  r.fillRect(0, float(kScreenH) - 14, float(kScreenW), 14, rgba(255, 30, 30, a));
  r.fillRect(0, 0, 14, float(kScreenH), rgba(255, 30, 30, a));
  r.fillRect(float(kScreenW) - 14, 0, 14, float(kScreenH), rgba(255, 30, 30, a));
  if (st.holding)
    r.drawText("HOLDING THE RAIL", float(kScreenW) * 0.5f, 120, {20.0f, rgb(255, 240, 200), rgb(30, 10, 10)},
      Align::Center);
}

} // namespace gr
