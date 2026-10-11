// Level 29, Dead Air: drawing (the colour bars, solid or ghost, flashing
// before a change; the RGB dials; the Test Card with its quadrants and
// beam; Ghost Images and their after-images; Static Swarms; snow over a
// quarter of the screen; the HUD's signal swatch and tint pips; the
// treasure). See world_signal.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

Color signalColor(int c);

namespace
{

constexpr float kTilePx = float(kTileSize) * kPixelScale; // 64
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

bool offScreen(float x, float y, float w, float h)
{
  return x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f;
}

// TV snow over a rect: scattered grey specks, a new pattern every frame.
void snow(Renderer& r, float x, float y, float w, float h, int frame, std::uint8_t alpha)
{
  r.fillRect(x, y, w, h, rgba(40, 40, 44, std::uint8_t(alpha / 2)));
  unsigned s = unsigned(frame) * 2654435761u + 12345u;
  const int n = int(w * h / 900.0f);
  for (int i = 0; i < n; ++i)
  {
    s = s * 1664525u + 1013904223u;
    const float px = x + float(s % 10000u) / 10000.0f * w;
    s = s * 1664525u + 1013904223u;
    const float py = y + float(s % 10000u) / 10000.0f * h;
    const std::uint8_t v = std::uint8_t(120 + (s >> 24) % 136);
    r.fillRect(px, py, 6.0f, 4.0f, rgba(v, v, v, alpha));
  }
}

} // namespace

void World::drawSignalBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mSignal;
  // Frame 4242: for exactly one frame the static thanks RigelEngine.
  if (s.egg)
    r.drawText("THANK YOU, RIGELENGINE", float(kScreenW) * 0.5f, float(kScreenH) * 0.4f,
      {40.0f, rgb(230, 230, 230), rgb(20, 20, 20), true}, Align::Center);
  const int next = s.pending >= 0 ? s.pending : (s.color + 1) % 3;
  bool warn = false;
  for (const auto& d : s.dials)
    warn = warn || (d.timed && d.rect.intersects(mPlayer.box()) && !s.cardDead &&
                     d.period - (d.t + d.phase) % d.period <= d.warn);
  for (const auto& b : s.bars)
  {
    const auto& l = mLayers[std::size_t(b.layer)];
    const float x = float(l.x0) * kTilePx - camX, y = float(l.y0) * kTilePx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kTilePx, h = float(l.y1 - l.y0 + 1) * kTilePx;
    if (offScreen(x, y, w, h))
      continue;
    const Color c = signalColor(b.color);
    // The incoming colour flashes (before a change, and before a timed step).
    const bool flashing = b.color == next && ((s.pending >= 0 && s.flash > 0) || warn) && (frame / 2) % 2 == 0;
    if (l.solid || flashing)
    {
      r.fillRect(x, y, w, h, flashing && !l.solid ? withAlpha(c, 140) : c);
      r.fillRect(x, y, w, 6.0f, withAlpha(rgb(255, 255, 255), 90));
      r.fillRect(x, y + h - 6.0f, w, 6.0f, withAlpha(rgb(0, 0, 0), 90));
      if (b.color == kSignalBlack)
        for (float k = 8.0f; k < w; k += 24.0f)
          r.fillRect(x + k, y + 8.0f, 4.0f, h - 16.0f, rgba(90, 90, 100, 120));
      continue;
    }
    // A ghost: an outline at 30%.
    const Color g = withAlpha(b.color == kSignalBlack ? rgb(160, 160, 170) : c, 76);
    r.fillRect(x + 4, y + 4, w - 8, 3, g);
    r.fillRect(x + 4, y + h - 7, w - 8, 3, g);
    r.fillRect(x + 4, y + 4, 3, h - 8, g);
    r.fillRect(x + w - 7, y + 4, 3, h - 8, g);
  }
  // The dials: three wedges round a hub in the signal colour.
  for (const auto& d : s.dials)
  {
    const float x = float(d.x) * kTilePx - camX, y = float(d.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    r.fillRect(x + 2.0f, y + 2.0f, kTilePx - 4.0f, kTilePx - 4.0f, rgb(30, 30, 34));
    for (int k = 0; k < 3; ++k)
      r.fillRect(x + 6.0f + float(k) * 18.0f, y + 6.0f, 16.0f, 14.0f, signalColor(k));
    const bool blink = (d.flash > 0 || (d.timed && warn)) && (frame / 2) % 2 == 0;
    r.fillRect(x + 14.0f, y + 26.0f, 36.0f, 30.0f, blink ? rgb(255, 255, 255) : signalColor(signalShown()));
    if (d.timed)
      r.fillRect(x + 14.0f, y + 58.0f, 36.0f * float((d.t + d.phase) % d.period) / float(d.period), 4.0f,
        rgb(230, 230, 230));
  }
  // Treasures.
  for (const auto& t : s.treasures)
  {
    if (t.taken)
      continue;
    const float x = float(t.x) * kTilePx - camX, y = float(t.y) * kTilePx - camY;
    if (offScreen(x, y, kTilePx, kTilePx))
      continue;
    const float bob = std::sin(float(frame) * 0.2f) * 4.0f;
    r.fillRect(x + 6.0f, y + 10.0f + bob, kTilePx - 12.0f, kTilePx - 20.0f, rgb(250, 210, 60));
    r.fillRect(x + 12.0f, y + 16.0f + bob, kTilePx - 24.0f, kTilePx - 32.0f, rgb(40, 40, 60));
    r.drawText("TV", x + kTilePx * 0.5f, y + 18.0f + bob, {20.0f, rgb(250, 210, 60), rgb(0, 0, 0), false},
      Align::Center);
  }
}

void World::drawSignalFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& s = mSignal;
  for (const auto& e : mEnemies)
  {
    if (!e.alive)
      continue;
    const float ex = (float(e.prevX) + float(e.x - e.prevX) * alpha) * kCellPx - camX;
    const float ey = (float(e.prevY) + float(e.y - e.prevY) * alpha + 1.0f) * kCellPx - camY;
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    if (offScreen(ex, ey - h, w, h))
      continue;
    if (e.kind == EnemyKind::TestCard)
    {
      // Four quadrants turning a quarter every second; dead ones grey; a
      // "please stand by" mouth after a hit.
      const float q = w * 0.5f;
      for (int pos = 0; pos < 4; ++pos)
      {
        const int c = ((pos - s.cardTurn) % 4 + 4) % 4;
        const bool dead = c < 3 && s.cardHp[c] == 0;
        const Color col = dead ? rgb(110, 110, 110) : (c == 3 ? rgb(240, 220, 40) : signalColor(c));
        const float qx = ex + (pos == 1 || pos == 2 ? q : 0.0f), qy = ey - h + (pos >= 2 ? q : 0.0f);
        r.fillRect(qx, qy, q, q, e.flash > 0 ? rgb(255, 255, 255) : col);
        if (c == s.color && !dead && (frame / 4) % 2 == 0)
          r.fillRect(qx + 6.0f, qy + 6.0f, q - 12.0f, q - 12.0f, withAlpha(rgb(255, 255, 255), 70));
      }
      r.fillRect(ex + w * 0.3f, ey - h * 0.62f, w * 0.4f, h * 0.24f, rgb(20, 20, 24));
      if (s.cardStandBy > 0)
        r.drawText(s.cardStandBy % 6 < 3 ? "PLEASE" : "STAND BY", ex + w * 0.5f, ey - h * 0.58f,
          {18.0f, rgb(240, 240, 240), rgb(0, 0, 0), false}, Align::Center);
      continue;
    }
    if (e.kind == EnemyKind::GhostImage)
    {
      // Two grey after-images behind it (snapping in during the tell), and
      // the tinted halo (gone for a while after it fires).
      const float lag = e.tell > 0 ? float(e.tell) / float(kImageTell) : 1.0f;
      for (int k = 2; k >= 1; --k)
        r.fillRect(ex - float(e.dir) * float(k) * 2.0f * kCellPx * lag, ey - h, w, h, rgba(160, 160, 170, 70));
      if (e.dive == 0)
      {
        const Color halo = signalColor((mStats.frames / kImageCycle) % 3);
        r.fillRect(ex - 8.0f, ey - h - 8.0f, w + 16.0f, h + 16.0f, withAlpha(halo, 90));
      }
      r.fillRect(ex, ey - h, w, h, e.flash > 0 ? rgb(255, 255, 255) : rgb(210, 210, 225));
      r.fillRect(ex + w * 0.2f, ey - h * 0.8f, w * 0.6f, h * 0.1f, rgb(40, 40, 50));
      continue;
    }
    if (e.kind == EnemyKind::StaticSwarm)
    {
      const std::uint8_t a = std::uint8_t(e.tell > 0 ? 230 : 170);
      snow(r, ex, ey - h, w, h, frame + e.id * 7, a);
      if (e.carrier)
        r.fillRect(ex, ey - h, w, h, rgba(90, 220, 60, 50));
    }
  }
  // The beam: a thin aim line during the tell, then two cells thick.
  if (s.beam.on)
  {
    const auto& b = s.beam;
    const Color c = b.quadrant == 3 ? rgb(240, 220, 40) : signalColor(b.quadrant);
    const float x0 = b.x0 * kCellPx - camX, y0 = b.y0 * kCellPx - camY;
    const float x1 = b.x1 * kCellPx - camX, y1 = b.y1 * kCellPx - camY;
    if (b.burning)
    {
      r.drawLine(x0, y0, x1, y1, 2.0f * kCellPx, withAlpha(c, 200));
      r.drawLine(x0, y0, x1, y1, kCellPx * 0.6f, rgb(255, 255, 255));
    }
    else if ((frame / 2) % 2 == 0)
      r.drawLine(x0, y0, x1, y1, 4.0f, withAlpha(c, 200));
  }
}

void World::drawSignalHud(Renderer& r, int frame) const
{
  const auto& s = mSignal;
  // A latched swarm's snow over a quarter of the screen.
  if (s.snow > 0)
  {
    const float w = float(kScreenW) * 0.5f, h = float(kScreenH) * 0.5f;
    snow(r, s.snowQuarter % 2 ? w : 0.0f, s.snowQuarter >= 2 ? h : 0.0f, w, h, frame, 210);
  }
  if (s.wrap)
    return;
  // The swatch (blinking between the two colours while a change waits) and
  // the three tint pips under it.
  const float x = float(kScreenW) - 150.0f, y = 96.0f;
  r.fillRect(x - 6.0f, y - 6.0f, 112.0f, 112.0f, rgb(10, 10, 12));
  const int shown = s.pending >= 0 && (frame / 4) % 2 == 0 ? s.pending : s.color;
  r.fillRect(x, y, 100.0f, 70.0f, signalColor(shown));
  for (int k = 0; k < 3; ++k)
    r.fillRect(x + float(k) * 34.0f, y + 78.0f, 30.0f, 18.0f,
      s.pips[k] > 0 ? signalColor(k) : withAlpha(signalColor(k), 50));
  if (mPlayer.weapon == Weapon::Proto && mPlayer.proto == int(ProtoId::TriToneBlaster))
  {
    r.fillRect(x - 40.0f, y + 20.0f, 28.0f, 28.0f, signalColor(s.tint));
    r.drawText("TINT", x - 26.0f, y - 6.0f, {16.0f, rgb(230, 230, 230), rgb(0, 0, 0), true}, Align::Center);
  }
}

} // namespace gr
