// Level 25, Tiny Kingdom: drawing (the dusk tint and the time-lapse sweep,
// the pond, buds, seeds and stalks, the dandelion, the rare butterfly, the
// ladybug, the Chrono Mister's mist, the beetles' jets, the sundial and the
// narrator's subtitles) and the Dandelion Drift bonus. See world_garden.cpp
// and world_glide.cpp for the logic.

#include "game/world.hpp"

#include "assets/art.hpp"

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

// A soft blob made of stacked rectangles (petals, wings, shells).
void blob(Renderer& r, float cx, float cy, float rw, float rh, Color c)
{
  for (int i = 0; i < 6; ++i)
  {
    const float t = float(i) / 6.0f;
    const float w = rw * std::sqrt(1.0f - t * t);
    r.fillRect(cx - w, cy - rh * t - rh / 6.0f, w * 2.0f, rh / 3.0f, c);
    r.fillRect(cx - w, cy + rh * t - rh / 6.0f, w * 2.0f, rh / 3.0f, c);
  }
}

} // namespace

void World::drawGardenBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& g = mGarden;
  // The pond.
  for (const auto& pd : g.ponds)
  {
    const float x = float(pd.x0) * kCellPx - camX, y = float(pd.y0) * kCellPx - camY;
    const float w = float(pd.x1 - pd.x0 + 1) * kCellPx, h = float(pd.y1 - pd.y0 + 1) * kCellPx;
    if (offScreen(x, y, w, h))
      continue;
    r.fillRect(x, y, w, h, withAlpha(rgb(60, 120, 170), 200));
    for (int i = 0; i < int(w / 96.0f); ++i)
    {
      const float wx = x + float(i) * 96.0f + std::sin(float(frame + i * 17) * 0.05f) * 12.0f;
      r.fillRect(wx, y + 6.0f, 40.0f, 4.0f, withAlpha(rgb(200, 235, 255), 140));
    }
  }
  // The dandelion's stalk and head.
  if (g.dandelionX >= 0)
  {
    const float x = (float(g.dandelionX) + 0.5f) * kTilePx - camX;
    const float yb = float(g.dandelionY + 1) * kTilePx - camY, yt = 10.0f * kTilePx - camY;
    r.fillRect(x - 6.0f, yt, 12.0f, yb - yt, rgb(110, 160, 70));
    if (!g.seeded)
      blob(r, x, yt, 90.0f, 90.0f, rgb(250, 210, 40));
    else
    {
      for (int i = 0; i < 24; ++i)
      {
        const float a = float(i) * 0.2618f;
        r.drawLine(x, yt, x + std::cos(a) * 90.0f, yt + std::sin(a) * 90.0f, 2.0f, withAlpha(rgb(255, 255, 255), 160));
      }
    }
  }
}

void World::drawGardenFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& g = mGarden;
  const int clock = gardenClock();
  const bool dusk = gardenDusk();
  const int inHalf = clock % kGardenHalf;
  // Buds: closed (a pink bulb) or open (petals, the platform).
  for (const auto& b : g.buds)
  {
    const float x = float(b.x) * kTilePx - camX, y = float(b.y) * kTilePx - camY;
    const float w = float(b.w) * kTilePx;
    if (offScreen(x, y, w, kTilePx * 2.0f))
      continue;
    if (b.open > 0)
    {
      const Color petal = b.open < 30 && (frame / 4) % 2 ? rgb(240, 150, 190) : rgb(255, 180, 215);
      r.fillRect(x, y, w, 18.0f, petal);
      r.fillRect(x + w * 0.4f, y + 18.0f, w * 0.2f, kTilePx, rgb(110, 160, 70));
    }
    else
    {
      r.fillRect(x + w * 0.5f - 4.0f, y + 20.0f, 8.0f, kTilePx * 1.5f, rgb(110, 160, 70));
      blob(r, x + w * 0.5f, y + 20.0f, 18.0f, 26.0f, rgb(220, 110, 160));
    }
  }
  // Seeds and their stalks.
  for (const auto& s : g.seeds)
  {
    const float x = float(s.x) * kTilePx - camX, y = float(s.y + 1) * kTilePx - camY;
    if (!s.grown)
    {
      if (!offScreen(x, y - 30.0f, kTilePx, 30.0f))
        blob(r, x + kTilePx * 0.5f, y - 12.0f, 16.0f + (s.sprout > 0 ? 6.0f : 0.0f), 10.0f, rgb(150, 110, 60));
      continue;
    }
    const float top = float(s.y - s.h + 1) * kTilePx - camY;
    if (offScreen(x, top, kTilePx, y - top))
      continue;
    r.fillRect(x + kTilePx * 0.5f - 6.0f, top, 12.0f, y - top, rgb(90, 170, 70));
    for (float ly = top + 40.0f; ly < y; ly += 96.0f)
      r.fillRect(x + kTilePx * 0.5f, ly, 24.0f, 8.0f, rgb(120, 200, 90));
  }
  // The rare butterfly.
  if (g.butterfly && g.bflyState != 1)
  {
    const float x = g.bx * kCellPx - camX, y = g.by * kCellPx - camY;
    if (!offScreen(x, y, 40.0f, 40.0f))
    {
      const float flap = std::abs(std::sin(float(frame) * 0.5f));
      blob(r, x - 12.0f * flap, y, 12.0f * flap + 3.0f, 14.0f, rgb(80, 160, 255));
      blob(r, x + 12.0f * flap, y, 12.0f * flap + 3.0f, 14.0f, rgb(80, 160, 255));
      r.fillRect(x - 2.0f, y - 10.0f, 4.0f, 20.0f, rgb(30, 30, 40));
    }
  }
  // The ladybug.
  if (g.ladybugX >= 0)
  {
    const float x = float(g.ladybugX) * kTilePx - camX, y = float(g.ladybugY) * kTilePx - camY;
    if (!offScreen(x, y, kTilePx * 2.0f, kTilePx * 3.0f))
    {
      const float cx = x + kTilePx, cy = y + kTilePx * 2.4f;
      blob(r, cx, cy, kTilePx * 0.9f, kTilePx * 0.55f, rgb(210, 40, 40));
      r.fillRect(cx - 2.0f, cy - kTilePx * 0.55f, 4.0f, kTilePx * 1.1f, rgb(30, 20, 20));
      if (g.ladybug == 0)
        for (int i = 0; i < 6; ++i)
          r.fillRect(cx - 36.0f + float(i % 3) * 26.0f, cy - 14.0f + float(i / 3) * 20.0f, 10.0f, 10.0f, rgb(30, 20, 20));
      else
        r.drawText(g.ladybug == 1 ? "G-U-N" : "4-2", cx, cy - 12.0f, {18.0f, rgb(30, 20, 20)}, Align::Center);
    }
  }
  // The Chrono Mister's mist.
  if (g.mist > 0)
  {
    const CellBox c = mistBox();
    const float x = float(c.x) * kCellPx - camX, y = float(c.y) * kCellPx - camY;
    for (int i = 0; i < 10; ++i)
    {
      const float t = float((frame * 3 + i * 7) % 20) / 20.0f;
      const float px = g.mistFace > 0 ? x + t * float(c.w) * kCellPx : x + (1.0f - t) * float(c.w) * kCellPx;
      const float spread = (0.3f + t) * float(c.h) * kCellPx * 0.5f;
      const float py = y + float(c.h) * kCellPx * 0.5f + std::sin(float(i) * 2.1f) * spread;
      r.fillRect(px - 6.0f, py - 6.0f, 12.0f, 12.0f, withAlpha(rgb(170, 255, 220), int(200.0f * (1.0f - t))));
    }
  }
  // Bombardier Beetles: the swelling and the jet.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::BombardierBeetle)
      continue;
    const CellBox b = e.box();
    const float bx = float(e.dir > 0 ? b.left() : b.right() + 1) * kCellPx - camX;
    const float by = float(b.top()) * kCellPx - camY + 30.0f;
    if (e.attach == 1)
      drawGlow(r, mArt, bx, by, 40.0f + float(enemyDef(e.def).tell - e.tell) * 4.0f, rgb(255, 140, 40), 0.7f);
    else if (e.cool > enemyDef(e.def).cooldown - 6)
      for (int i = 0; i < 8; ++i)
        r.fillRect(bx - float(e.dir) * float(i) * 24.0f - 10.0f, by - 10.0f + std::sin(float(i * 3 + frame)) * 14.0f,
          20.0f, 20.0f, withAlpha(rgb(255, 245, 220), 200 - i * 20));
  }
  // Soldier Ant columns: the antennae flare.
  for (const auto& c : g.columns)
    if (c.tell > 0)
      for (int a : c.ants)
      {
        const Enemy& e = mEnemies[std::size_t(a)];
        if (!e.alive || e.hidden)
          continue;
        drawGlow(r, mArt, float(e.x + 1) * kCellPx - camX, float(e.y - 1) * kCellPx - camY, 30.0f, rgb(255, 80, 60), 0.8f);
      }
  // Dusk: the garden darkens; the time-lapse sweeps a band of light across.
  if (g.cycle)
  {
    if (dusk)
      r.fillRect(0, 0, float(kScreenW), float(kScreenH), withAlpha(rgb(30, 20, 70), 70));
    if (inHalf >= kGardenHalf - kGardenSweep)
    {
      const float t = float(inHalf - (kGardenHalf - kGardenSweep)) / float(kGardenSweep);
      r.fillRect(t * float(kScreenW) - 120.0f, 0, 240.0f, float(kScreenH),
        withAlpha(dusk ? rgb(255, 230, 160) : rgb(120, 110, 200), 60));
    }
  }
}

void World::drawGardenHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& g = mGarden;
  // The sundial: how much of the half is left.
  if (g.cycle)
  {
    const int inHalf = gardenClock() % kGardenHalf;
    const float cx = float(kScreenW) - 70.0f, cy = 150.0f;
    blob(r, cx, cy, 36.0f, 36.0f, gardenDusk() ? rgb(60, 60, 110) : rgb(230, 210, 150));
    const float a = -1.5708f + 6.2832f * float(inHalf) / float(kGardenHalf);
    r.drawLine(cx, cy, cx + std::cos(a) * 30.0f, cy + std::sin(a) * 30.0f, 4.0f, rgb(40, 30, 20));
    if (inHalf >= kGardenHalf - kGardenDroop)
      r.drawText(gardenDusk() ? "DAWN" : "DUSK", cx, cy + 42.0f, {16.0f, rgb(255, 240, 200), rgb(0, 0, 0)}, Align::Center);
  }
  // The narrator.
  if (g.subtitle > 0)
    r.drawText(g.subtitleText, float(kScreenW) / 2.0f, float(kScreenH) - 120.0f,
      {24.0f, rgb(255, 250, 230), rgb(0, 0, 0), true}, Align::Center, std::min(1.0f, float(g.subtitle) / 10.0f));
}

// --- Dandelion Drift -----------------------------------------------------------------------

void World::drawGlideBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& gl = mGlide;
  // Gusts: drifting streaks.
  for (const auto& w : gl.winds)
  {
    const float x = float(w.x0) * kCellPx - camX, y = float(w.y0) * kCellPx - camY;
    const float ww = float(w.x1 - w.x0 + 1) * kCellPx, h = float(w.y1 - w.y0 + 1) * kCellPx;
    if (offScreen(x, y, ww, h))
      continue;
    r.fillRect(x, y, ww, h, withAlpha(rgb(255, 255, 255), 24));
    for (int i = 0; i < 6; ++i)
    {
      const float t = std::fmod(float(frame) * 0.02f * float(w.push) + float(i) / 6.0f, 1.0f);
      const float sy = w.dir < 0 ? y + h * (1.0f - t) : y + h * t;
      r.fillRect(x + ww * (0.15f + 0.13f * float(i)), sy, 4.0f, 30.0f, withAlpha(rgb(255, 255, 255), 90));
    }
  }
  // Dragonflies.
  for (const auto& d : gl.flies)
  {
    const float x = (d.prevX + (d.x - d.prevX) * alpha) * kCellPx - camX;
    const float y = (d.prevY + (d.y - d.prevY) * alpha) * kCellPx - camY;
    if (offScreen(x - 40.0f, y - 20.0f, 80.0f, 40.0f))
      continue;
    r.fillRect(x - 40.0f, y - 4.0f, 80.0f, 8.0f, rgb(40, 160, 150));
    const float flap = std::abs(std::sin(float(frame) * 0.9f)) * 20.0f;
    r.fillRect(x - 24.0f, y - 6.0f - flap, 40.0f, 6.0f, withAlpha(rgb(220, 250, 255), 160));
    r.fillRect(x - 24.0f, y + flap, 40.0f, 6.0f, withAlpha(rgb(220, 250, 255), 160));
  }
}

void World::drawGlideFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  const auto& p = mPlayer;
  // The seed head the runner hangs from.
  const float x = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
  const float y = (float(p.prevY) + float(p.y - p.prevY) * alpha - 5.0f) * kCellPx - camY;
  r.drawLine(x, y, x, y + 40.0f, 3.0f, rgb(230, 230, 210));
  for (int i = 0; i < 12; ++i)
  {
    const float a = 3.1416f + float(i) * 0.2856f;
    r.drawLine(x, y, x + std::cos(a) * 50.0f, y + std::sin(a) * 40.0f, 2.0f, withAlpha(rgb(255, 255, 255), 200));
  }
}

} // namespace gr
