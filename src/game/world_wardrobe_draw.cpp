// Level 37, Wardrobe: drawing (booths and their curtains and signs, heavy
// plates, shutters, spring racks, the push rack and the vault door, the
// Gorgazilla suit rack, the bulb mirror, decoys, Mannequins, Powder Puffers
// and their clouds, Rack Runners, the suit and the mustache on the runner,
// each runner's hearts, and the Totem's stacked runners). See
// world_wardrobe.cpp for the logic.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr Color kHudInk = rgb(10, 8, 20);

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c);
}

Color runnerShade(int runner)
{
  switch (runner)
  {
    case 1:
      return rgb(230, 120, 50); // Rocco
    case 2:
      return rgb(150, 110, 240); // Nova
    case -2:
      return rgb(60, 60, 60); // OUT OF ORDER
    case -1:
      return rgb(230, 190, 90); // the stage door
    default:
      return rgb(60, 170, 240); // Dash
  }
}

const char* runnerWord(int index) { return index == 1 ? "ROCCO" : index == 2 ? "NOVA" : "DASH"; }

const char* boothSign(const Booth& b)
{
  if (b.exit)
    return "STAGE";
  switch (b.runner)
  {
    case 0:
      return "DASH";
    case 1:
      return "ROCCO";
    case 2:
      return "NOVA";
    case -2:
      return "OUT OF ORDER";
    default:
      return "";
  }
}

} // namespace

bool World::wardrobeDrawsBlock(int tx, int ty) const
{
  const auto& s = mWardrobe;
  for (const auto& sh : s.shutters)
    if (!sh.open && tx >= sh.x0 && tx <= sh.x1 && ty >= sh.y0 && ty <= sh.y1)
      return true;
  for (const auto& r : s.pushRacks)
    if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h)
      return true;
  return false;
}

void World::drawWardrobeBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWardrobe;
  // Booths: a cabinet, its curtain and the name on the sign.
  for (const auto& b : s.booths)
  {
    const CellBox c = b.box();
    box(r, c, camX, camY, rgb(70, 40, 30));
    box(r, {c.x, c.y, c.w, 1}, camX, camY, runnerShade(b.runner));
    const int sway = b.curtain > 0 ? (frame / 2) % 2 : 0;
    box(r, {c.x + 1 - sway, c.y + 1, c.w - 2, c.h - 1}, camX, camY,
      b.runner == -2 ? rgb(90, 80, 70) : b.exit ? rgb(40, 20, 30) : rgb(150, 30, 50));
    r.drawText(boothSign(b), (float(c.x) + 2.0f) * kCellPx - camX, float(c.y) * kCellPx - camY + 4.0f,
      {16.0f, rgb(255, 245, 220), kHudInk}, Align::Center);
  }
  // Spring racks: a rail on coiled springs.
  for (const auto& k : s.racks)
  {
    const int squash = k.bounce > 0 ? 1 : 0;
    box(r, {k.x, k.y + 1, k.w, 1}, camX, camY, rgb(190, 190, 200));
    box(r, {k.x, k.y - 3 + squash, k.w, 1}, camX, camY, rgb(220, 180, 60));
    box(r, {k.x + 1, k.y - 2 + squash, 1, 3 - squash}, camX, camY, rgb(160, 160, 170));
    box(r, {k.x + k.w - 2, k.y - 2 + squash, 1, 3 - squash}, camX, camY, rgb(160, 160, 170));
  }
  // Heavy plates (a weight stamped on them).
  for (const auto& pl : s.plates)
  {
    const CutTable* t = nullptr;
    for (const auto& tb : s.tables)
      if (tb.id == pl.onTable)
        t = &tb;
    const int y = t ? t->row * kCellsPerTile - 1 : pl.y;
    box(r, {pl.x, y + (pl.down ? 1 : 0), pl.w, 1}, camX, camY, pl.down ? rgb(90, 200, 90) : rgb(200, 60, 50));
  }
  // The cutting tables' legs and blades.
  for (const auto& t : s.tables)
  {
    const int y = t.row * kCellsPerTile;
    box(r, {t.x0 * kCellsPerTile, y, (t.x1 - t.x0 + 1) * kCellsPerTile, 1}, camX, camY, rgb(200, 170, 110));
    box(r, {t.x0 * kCellsPerTile + 1, y + 1, 1, 1}, camX, camY, rgb(220, 220, 230));
  }
  // Closed shutters roll up when opened.
  for (const auto& sh : s.shutters)
  {
    const int h = (sh.y1 - sh.y0 + 1) * kCellsPerTile;
    const int shown = sh.open ? h * (12 - sh.t) / 12 : h;
    if (shown <= 0)
      continue;
    const CellBox c{sh.x0 * kCellsPerTile, sh.y0 * kCellsPerTile, (sh.x1 - sh.x0 + 1) * kCellsPerTile, shown};
    box(r, c, camX, camY, rgb(110, 100, 90));
    for (int y = c.y + 1; y < c.y + c.h; y += 2)
      box(r, {c.x, y, c.w, 1}, camX, camY, rgb(80, 72, 66));
  }
  // The vault doors, the push rack.
  for (const auto& d : s.doors)
    box(r, boxAt(d.x, d.y, 2, 5), camX, camY, rgb(60, 60, 70));
  for (const auto& k : s.pushRacks)
  {
    const int wob = k.slide > 0 ? (frame % 2) : 0;
    const CellBox c{k.x * kCellsPerTile + wob, k.y * kCellsPerTile, k.w * kCellsPerTile, k.h * kCellsPerTile};
    box(r, c, camX, camY, rgb(110, 70, 40));
    for (int i = 0; i < 3; ++i)
      box(r, {c.x + i, c.y + 1, 1, c.h - 2}, camX, camY, i == 1 ? rgb(170, 60, 80) : rgb(60, 90, 160));
  }
  // The Gorgazilla suit on its rack, the bulb mirror.
  if (s.suitX >= 0)
  {
    box(r, boxAt(s.suitX, s.suitY - 5, 4, 1), camX, camY, rgb(180, 180, 180));
    if (!s.suitTouched && !s.suit)
      box(r, boxAt(s.suitX + 1, s.suitY - 1, 3, 5), camX, camY, rgb(60, 140, 60));
  }
  if (s.mirrorX >= 0)
  {
    box(r, boxAt(s.mirrorX, s.mirrorY - 2, 4, 4), camX, camY, rgb(150, 200, 220));
    for (int k = 0; k < 4; ++k)
      box(r, {s.mirrorX + k, s.mirrorY - 6, 1, 1}, camX, camY, (frame / 8 + k) % 3 ? rgb(255, 240, 180) : rgb(200, 170, 90));
  }
  // The Totem's runners who aren't the player.
  for (const auto& b : s.bodies)
  {
    if (b.active)
      continue;
    const auto& ca = mArt.runner(characterByIndex(b.runner));
    r.draw(ca.idle[0].get(1), (float(b.x) + 1.5f) * kCellPx - camX, float(b.y + 1) * kCellPx - camY);
  }
}

void World::drawWardrobeFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWardrobe;
  const auto& p = mPlayer;
  // Decoys: a mannequin dressed as the runner.
  for (const auto& d : s.decoys)
  {
    const auto& ca = mArt.runner(characterByIndex(d.look));
    DrawOpts o;
    o.tint = rgb(255, 225, 200);
    o.alpha = d.life < 15 && (frame / 2) % 2 ? 0.5f : 1.0f;
    r.draw((d.landed ? ca.idle[0] : ca.jump).get(d.facing), (d.x + 1.5f) * kCellPx - camX, (d.y + 1.0f) * kCellPx - camY, o);
  }
  // Enemies.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    const bool flash = e.flash > 0;
    if (e.kind == EnemyKind::Mannequin)
    {
      bool twitch = false;
      int pose = 0;
      for (const auto& m : s.mannequins)
        if (&mEnemies[std::size_t(m.enemy)] == &e)
        {
          twitch = m.twitch > 0;
          pose = m.pose;
        }
      const int shake = twitch ? (frame % 2) : 0;
      box(r, {b.x + 1 + shake, b.y, 1, 1}, camX, camY, rgb(230, 210, 190)); // head
      box(r, {b.x + shake, b.y + 1, b.w, 2}, camX, camY, flash ? rgb(255, 255, 255) : rgb(200, 180, 160));
      box(r, {b.x + (pose % 2), b.y + 3, 1, 2}, camX, camY, rgb(180, 160, 140));
      box(r, {b.x + 2 - (pose % 2), b.y + 3, 1, 2}, camX, camY, rgb(180, 160, 140));
    }
    else if (e.kind == EnemyKind::PowderPuffer)
    {
      const int squeeze = e.tell > 0 ? 1 : 0;
      box(r, {b.x + squeeze, b.y + squeeze, b.w - squeeze * 2, b.h - squeeze}, camX, camY,
        flash ? rgb(255, 255, 255) : e.carrier ? rgb(160, 200, 120) : rgb(250, 190, 210));
      box(r, {e.dir > 0 ? b.right() : b.x, b.y + 1, 1, 2}, camX, camY, rgb(255, 240, 245));
      if (e.carrier)
        r.drawText("EXP 09/26", (float(b.x) + 2.0f) * kCellPx - camX, float(b.y) * kCellPx - camY - 18.0f,
          {12.0f, rgb(240, 255, 200), kHudInk}, Align::Center);
    }
    else if (e.kind == EnemyKind::RackRunner)
    {
      const int rattle = e.tell > 0 ? (frame % 2) : 0;
      box(r, {b.x, b.y + rattle, b.w, 1}, camX, camY, rgb(190, 190, 200)); // rail
      for (int i = 0; i < b.w; ++i)
        box(r, {b.x + i, b.y + 1, 1, b.h - 2}, camX, camY,
          flash ? rgb(255, 255, 255) : (i % 3 == 0 ? rgb(170, 50, 70) : i % 3 == 1 ? rgb(60, 100, 170) : rgb(220, 180, 60)));
      box(r, {b.x, b.bottom(), 1, 1}, camX, camY, rgb(40, 40, 40));
      box(r, {b.right(), b.bottom(), 1, 1}, camX, camY, rgb(40, 40, 40));
    }
  }
  // Powder clouds: pink-white (green from the carrier).
  for (const auto& f : s.puffers)
  {
    if (f.cloud <= 0)
      continue;
    const bool carrier = mEnemies[std::size_t(f.enemy)].carrier;
    const float a = std::min(1.0f, float(f.cloud) / 20.0f) * 0.75f;
    const Color c = carrier ? rgba(150, 230, 120, int(a * 255)) : rgba(255, 225, 235, int(a * 255));
    box(r, {f.cloudX, f.cloudY, kPuffW, kPuffH}, camX, camY, c);
  }
  // Inside a cloud: only the runner's outline shows.
  if (s.puffed > 0)
  {
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(255, 230, 240, 170));
    box(r, p.box(), camX, camY, rgba(80, 40, 60, 160));
  }
  // The curtains over the OUT OF ORDER booth; thin when the runner is behind.
  for (const auto& c : s.curtains)
  {
    const bool inside = p.box().intersects(c);
    box(r, c, camX, camY, inside ? rgba(140, 30, 60, 90) : rgb(140, 30, 60));
    for (int x = c.x; x < c.x + c.w; x += 3)
      box(r, {x, c.y, 1, c.h}, camX, camY, inside ? rgba(100, 20, 40, 90) : rgb(100, 20, 40));
  }
  // Behind the curtain, the suit, the mustache.
  if (s.boothT > 0 && s.booth >= 0)
  {
    const CellBox c = s.booths[std::size_t(s.booth)].box();
    box(r, {c.x, c.y + 1, c.w, c.h - 1}, camX, camY, rgb(170, 40, 60)); // the runner is behind it
  }
  if (s.suit)
  {
    box(r, p.box(), camX, camY, rgba(60, 150, 60, 200));
    box(r, {p.facing > 0 ? p.x + 2 : p.x - 1, p.y - 4, 2, 1}, camX, camY, rgb(60, 150, 60)); // snout
  }
  if (s.mustache)
    box(r, {p.facing > 0 ? p.x + 1 : p.x, p.y - 3, 2, 1}, camX, camY, rgb(50, 30, 20));
  if (s.sit > 0)
    box(r, {p.x, p.y - 1, 3, 2}, camX, camY, rgb(120, 60, 40)); // the stool
  if (s.toon)
    box(r, {p.x, p.y - 4, 3, 1}, camX, camY, rgba(255, 255, 255, 120));
}

void World::drawWardrobeHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& s = mWardrobe;
  if (s.totem)
    return;
  // Each runner's hearts: full until they are used.
  float x = 24.0f;
  for (int i : {0, 1, 2})
  {
    const int pool = i == mCharacterIndex ? mPlayer.hp : s.pools[std::size_t(i)];
    const int maxHp = characterByIndex(i).maxHp;
    const int hp = pool < 0 ? maxHp : pool;
    char text[32];
    std::snprintf(text, sizeof text, "%s %d/%d", runnerWord(i), hp, maxHp);
    r.drawText(text, x, 660.0f, {18.0f, i == mCharacterIndex ? rgb(255, 240, 200) : rgb(170, 160, 150), kHudInk},
      Align::Left);
    x += 150.0f;
  }
}

} // namespace gr
