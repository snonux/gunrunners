// Level 16's drawing (world_cryo.cpp has the rules): shiny ice on the
// floors, kickers, cold draughts, the labs' glass and pods, the Lab Arms'
// rail and cables, frozen blocks, the power boxes and the DO NOT OPEN pod,
// and Air Hockey's goals and score.

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64
const Color kIce = rgb(190, 232, 255);
const Color kIceDeep = rgb(110, 180, 230);
const Color kSteel = rgb(150, 162, 180);
const Color kSteelDark = rgb(70, 80, 98);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

unsigned hashCell(int x, int y)
{
  unsigned h = unsigned(x) * 73856093u ^ unsigned(y) * 19349663u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  return h ^ (h >> 15);
}

// A capsule pod standing in a 2 x 3 block alcove (x, y its top-left).
void podShell(Renderer& r, float x, float y, Color glow)
{
  r.fillRect(x + 4, y + 2, 120, 188, kSteelDark);
  r.fillRect(x + 10, y + 8, 108, 176, kSteel);
  r.fillRect(x + 20, y + 22, 88, 140, rgb(40, 64, 90));
  r.fillRect(x + 10, y + 8, 108, 6, rgb(220, 230, 244));
  r.fillRect(x + 10, y + 168, 108, 16, rgb(96, 108, 128));
  for (int k = 0; k < 3; ++k)
    r.fillRect(x + 28 + float(k) * 18, y + 172, 10, 6, glow);
}

void frostOver(Renderer& r, float x, float y, float w, float h, float amount, Color tint, int seed)
{
  if (amount <= 0.0f)
    return;
  r.fillRect(x, y, w, h, withAlpha(tint, int(200 * amount)));
  for (int k = 0; k < 9; ++k)
  {
    const unsigned hsh = hashCell(seed, k);
    const float fx = x + float(hsh % 100u) / 100.0f * w, fy = y + float((hsh >> 8) % 100u) / 100.0f * h;
    r.drawLine(fx, fy, fx + 14.0f, fy - 10.0f, 2.0f, rgba(255, 255, 255, int(160 * amount)));
    r.drawLine(fx, fy, fx - 10.0f, fy - 12.0f, 2.0f, rgba(255, 255, 255, int(120 * amount)));
  }
}

} // namespace

void World::drawCryoBack(Renderer& r, float camX, float camY, int frame) const
{
  const auto& c = mCryo;
  if (!c.on)
    return;
  const auto& p = mPlayer;

  // Lab furniture: glass partitions, the crew's pods, the HOST (SPARE) pod.
  for (const auto& d : c.decos)
  {
    const float x = float(d.bx) * kTilePx - camX, y = float(d.by - 2) * kTilePx - camY;
    if (!visible(x, y, 128, 192))
      continue;
    if (d.kind == 0)
    {
      r.fillRect(x + 22, y, 20, 192, rgba(200, 236, 255, 70));
      r.fillRect(x + 20, y, 4, 192, rgba(240, 250, 255, 150));
      r.fillRect(x + 40, y, 4, 192, rgba(240, 250, 255, 110));
      for (int k = 0; k < 4; ++k)
        r.drawLine(x + 24, y + 30 + float(k) * 44, x + 40, y + 18 + float(k) * 44, 2.0f, rgba(255, 255, 255, 90));
      continue;
    }
    const bool host = d.kind == 2;
    podShell(r, x, y, host ? rgb(255, 190, 60) : rgb(90, 255, 140));
    // Somebody asleep inside.
    const float bob = std::sin(float(frame + d.bx * 13) * 0.05f) * 2.0f;
    r.fillRect(x + 50, y + 40 + bob, 28, 26, rgb(200, 186, 170));
    r.fillRect(x + 42, y + 68 + bob, 44, 80, host ? rgb(60, 70, 110) : rgb(90, 120, 140));
    if (host && c.hostSeen)
    {
      // A Lance lookalike: the quiff, the jaw, the scarf.
      r.fillRect(x + 46, y + 32 + bob, 36, 12, rgb(250, 214, 90));
      r.fillRect(x + 54, y + 52 + bob, 4, 4, rgb(30, 30, 40));
      r.fillRect(x + 70, y + 52 + bob, 4, 4, rgb(30, 30, 40));
      r.fillRect(x + 44, y + 70 + bob, 40, 8, rgb(220, 40, 50));
    }
    frostOver(r, x + 20, y + 22, 88, 140, host && c.hostSeen ? 0.0f : 0.85f, rgb(214, 238, 255), d.bx * 7 + d.by);
    r.fillRect(x + 22, y + 146, 84, 14, rgba(10, 20, 30, 200));
    r.drawText(host ? "HOST (SPARE)" : "CRYO CREW", x + 64, y + 147, {11.0f, host ? rgb(255, 200, 80) : kIce,
      rgb(0, 0, 0)}, Align::Center);
  }

  // The Lab Arms' rail and their cables down to the claws.
  if (c.railY >= 0)
  {
    const float x0 = float(c.railX0) * kTilePx - camX, x1 = float(c.railX1 + 1) * kTilePx - camX;
    const float y = float(c.railY + 1) * kTilePx - camY;
    if (visible(x0, y - 20, x1 - x0, 40))
    {
      r.fillRect(x0, y - 4, x1 - x0, 18, kSteelDark);
      r.fillRect(x0, y - 4, x1 - x0, 4, rgb(200, 210, 226));
      for (float k = x0 + 16; k < x1; k += 48)
        r.fillRect(k, y + 4, 6, 6, rgb(40, 46, 58));
    }
    for (const auto& e : mEnemies)
    {
      if (!e.alive || e.kind != EnemyKind::LabArm)
        continue;
      const float cx = (e.drawX + float(e.w) * 0.5f) * kCellPx - camX;
      const float top = (e.drawY - float(e.h) + 1.0f) * kCellPx - camY;
      if (!visible(cx - 20, y, 40, top - y))
        continue;
      r.fillRect(cx - 18, y - 6, 36, 20, rgb(110, 120, 138)); // the trolley
      r.fillRect(cx - 3, y + 10, 6, std::max(0.0f, top - y - 8), rgb(60, 66, 80));
    }
  }

  // Ice: a pale, shiny top on every ice block in view.
  if (!c.ice.empty())
  {
    const int bx0 = std::max(0, int(camX / kTilePx) - 1), by0 = std::max(0, int(camY / kTilePx) - 1);
    const int bx1 = std::min(c.w - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
    const int by1 = std::min(mLevel->height - 1, int((camY + float(kScreenH)) / kTilePx) + 1);
    for (int by = by0; by <= by1; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        if (!c.iceAt(bx, by) || !mMap.solid(bx * kCellsPerTile, by * kCellsPerTile))
          continue;
        const float x = float(bx) * kTilePx - camX, y = float(by) * kTilePx - camY;
        r.fillRect(x, y, kTilePx, 14, kIce);
        r.fillRect(x, y + 14, kTilePx, 6, withAlpha(kIceDeep, 180));
        r.fillRect(x, y, kTilePx, 3, rgb(255, 255, 255));
        const int glint = (frame / 3 + bx * 11) % 48;
        if (glint < 8)
          r.fillRect(x + float(glint) * 7.0f, y + 4, 12, 4, rgba(255, 255, 255, 220), Blend::Add);
      }
  }

  // Kickers: a wedge of ice rising the way it throws.
  for (const auto& k : c.kickers)
  {
    const float x = float(k.bx) * kTilePx - camX, y = float(k.by + 1) * kTilePx - camY;
    if (!visible(x, y - 64, 64, 64))
      continue;
    for (int s = 0; s < 8; ++s)
    {
      const float h = 4.0f + float(s) * 3.0f;
      const float sx = k.dir > 0 ? x + float(s) * 8.0f : x + 56.0f - float(s) * 8.0f;
      r.fillRect(sx, y - h, 8, h, kIce);
      r.fillRect(sx, y - h, 8, 2, rgb(255, 255, 255));
    }
    drawGlow(r, mArt, x + 32, y - 12, 30, rgb(160, 230, 255), 0.25f + 0.15f * float((frame / 8) % 2));
  }

  // Cold draughts: snow blowing through.
  for (const auto& f : c.frost)
  {
    const float x = float(f.x0) * kTilePx - camX, y = float(f.y0) * kTilePx - camY;
    const float w = float(f.x1 - f.x0 + 1) * kTilePx, h = float(f.y1 - f.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    r.fillRect(x, y, w, h, rgba(160, 210, 255, 40));
    const int flakes = int(w * h / 3000.0f);
    for (int i = 0; i < flakes; ++i)
    {
      const unsigned hsh = hashCell(i, f.x0);
      const float fx = x + std::fmod(float(hsh % 1000u) / 1000.0f * w + float(frame) * (1.5f + float(i % 3)), w);
      const float fy = y + std::fmod(float((hsh >> 10) % 1000u) / 1000.0f * h + float(frame) * 0.6f, h);
      r.fillRect(fx, fy, 3, 3, rgba(240, 250, 255, 180));
    }
  }

  // The goal vent in the rink's bumper.
  for (const auto& v : c.vents)
  {
    const float x = float(v.bx) * kTilePx - camX, y = float(v.by - v.h + 1) * kTilePx - camY;
    const float w = float(v.w) * kTilePx, h = float(v.h) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    r.fillRect(x + 8, y + 10, w - 16, h - 20, rgb(20, 26, 36));
    for (float k = y + 16; k < y + h - 12; k += 10)
      r.fillRect(x + 10, k, w - 20, 4, kSteel);
    if (c.cheer > 0)
      r.drawText("YAY", x + w * 0.5f, y - 22 - float(30 - c.cheer), {12.0f, rgba(255, 255, 255, c.cheer * 8),
        rgb(0, 0, 0)}, Align::Center);
  }

  // Air Hockey's goals: a net behind each opening.
  for (const auto& g : c.goals)
  {
    const float x = float(g.x0) * kTilePx - camX, y = float(g.y0) * kTilePx - camY;
    const float w = float(g.x1 - g.x0 + 1) * kTilePx, h = float(g.y1 - g.y0 + 1) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    r.fillRect(x, y, w, h, rgba(255, 60, 70, c.scoredFlash > 0 ? 90 : 36));
    for (float k = x; k < x + w; k += 16)
      r.fillRect(k, y, 2, h, rgba(255, 255, 255, 90));
    for (float k = y; k < y + h; k += 16)
      r.fillRect(x, k, w, 2, rgba(255, 255, 255, 90));
    r.fillRect(x, y, w, 6, rgb(230, 40, 50));
  }
  (void)p;
}

void World::drawCryoFront(Renderer& r, float camX, float camY, int frame) const
{
  const auto& c = mCryo;
  if (!c.on)
    return;
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const float x = e.drawX * kCellPx - camX, y = (e.drawY - float(e.h) + 1.0f) * kCellPx - camY;
    const float w = float(e.w) * kCellPx, h = float(e.h) * kCellPx;
    if (!visible(x, y, w, h))
      continue;
    if (e.frozen > 0)
    {
      // A block of ice: it cracks and flashes in its last 22 frames.
      const bool ending = !c.zeroFriction && e.frozen <= 22;
      const int a = ending && (frame / 2) % 2 ? 90 : 150;
      r.fillRect(x - 4, y - 4, w + 8, h + 8, rgba(170, 225, 255, a));
      r.fillRect(x - 4, y - 4, w + 8, 5, rgba(255, 255, 255, 220));
      r.fillRect(x - 4, y - 4, 4, h + 8, rgba(255, 255, 255, 160));
      r.drawLine(x + 6, y + h * 0.3f, x + w * 0.4f, y + 6, 3.0f, rgba(255, 255, 255, 170));
      if (ending)
        for (int k = 0; k < 3; ++k)
          r.drawLine(x + w * (0.2f + 0.3f * float(k)), y, x + w * (0.35f + 0.25f * float(k)), y + h, 2.0f,
            rgba(255, 255, 255, 220));
      if (e.vx != 0.0f)
        for (int k = 0; k < 4; ++k)
          r.fillRect(e.vx > 0.0f ? x - 12.0f - float(k) * 14.0f : x + w + 4.0f + float(k) * 14.0f,
            y + h - 8.0f - float(k % 2) * 10.0f, 10, 3, rgba(220, 245, 255, 160 - k * 30));
    }
    if (e.kind == EnemyKind::LabArm && e.attach == 1)
      drawGlow(r, mArt, x + w * 0.5f, y + 18, 34, rgb(255, 40, 40), 0.9f - float(e.tell) * 0.04f);
    if (e.kind == EnemyKind::Puck && e.attach == 1)
      drawGlow(r, mArt, x + w * 0.5f, y + h * 0.5f, 50, rgb(140, 220, 255), 0.6f);
  }
}

void World::drawCryoBreakable(Renderer& r, const Breakable& b, float camX, float camY, int frame) const
{
  const float x = float(b.x0) * kTilePx - camX, y = float(b.y0) * kTilePx - camY;
  const float w = float(b.x1 - b.x0 + 1) * kTilePx, h = float(b.y1 - b.y0 + 1) * kTilePx;
  if (!visible(x, y, w, h))
    return;
  if (b.look == 8)
  {
    // The DO NOT OPEN pod: a red-stencilled capsule, frosted shut.
    r.fillRect(x + 2, y + 2, w - 4, h - 4, kSteelDark);
    r.fillRect(x + 8, y + 8, w - 16, h - 16, kSteel);
    r.fillRect(x + 16, y + 24, w - 32, h - 60, rgb(40, 56, 80));
    frostOver(r, x + 16, y + 24, w - 32, h - 60, 0.9f, rgb(214, 238, 255), b.x0 * 3);
    r.fillRect(x + 8, y + h - 34, w - 16, 22, rgb(200, 30, 40));
    r.drawText("DO NOT OPEN", x + w * 0.5f, y + h - 32, {12.0f, rgb(255, 240, 230), rgb(60, 0, 0)}, Align::Center);
    const int dmg = std::max(0, 3 - b.hp);
    for (int k = 0; k < dmg; ++k)
      r.drawLine(x + 20 + float(k) * 26, y + 30, x + 40 + float(k) * 20, y + h - 50, 3.0f, rgba(255, 255, 255, 200));
    return;
  }
  if (b.look == 9)
  {
    // A cube of ice with something inside it.
    r.fillRect(x + 2, y + 2, w - 4, h - 4, rgba(170, 225, 255, 170));
    r.fillRect(x + 2, y + 2, w - 4, 5, rgba(255, 255, 255, 230));
    r.fillRect(x + 2, y + 2, 4, h - 4, rgba(255, 255, 255, 170));
    r.drawLine(x + 10, y + 30, x + 30, y + 10, 3.0f, rgba(255, 255, 255, 190));
    if (b.hp < 2)
      r.drawLine(x + 14, y + h - 10, x + w - 18, y + 14, 2.0f, rgba(255, 255, 255, 230));
    return;
  }
  // A Lab Arm's power box: a grey cabinet with a hazard band and a lamp.
  r.fillRect(x + 2, y + 4, w - 4, h - 4, kSteelDark);
  r.fillRect(x + 6, y + 8, w - 12, h - 12, rgb(120, 128, 140));
  for (int s = 0; s < 4; ++s)
    r.fillRect(x + 6 + float(s) * 14, y + 10, 7, 6, rgb(255, 196, 30));
  drawGlow(r, mArt, x + w * 0.5f, y + 36, 14, (frame / 10) % 2 ? rgb(90, 255, 120) : rgb(40, 160, 70), 0.9f);
  const int dmg = std::max(0, 3 - b.hp);
  for (int k = 0; k < dmg; ++k)
    r.drawLine(x + 10 + float(k) * 16, y + 20, x + 22 + float(k) * 12, y + h - 8, 2.0f, rgba(255, 255, 255, 170));
}

void World::drawCryoHud(Renderer& r, int frame) const
{
  const auto& c = mCryo;
  if (!c.zeroFriction || c.goalTarget <= 0)
    return;
  const float x = float(kScreenW) * 0.5f - 130.0f, y = 150.0f;
  r.fillRect(x, y, 260, 48, rgba(8, 14, 30, 190));
  const Color col = c.scoredFlash > 0 && (frame / 3) % 2 ? rgb(255, 255, 255) : rgb(160, 225, 255);
  r.drawText("GOALS " + std::to_string(c.scored) + " / " + std::to_string(c.goalTarget), x + 130, y + 8,
    {24.0f, col, rgb(4, 10, 24)}, Align::Center);
}

} // namespace gr
