// Level 36, Prop Warehouse: drawing (the cranes, their rails, cables and
// magnets, the metal crates, the levers, the Magnet Gun's beam, Forklift
// Bots, Stagehands and their flats, Prop Mimics, the props of the earlier
// levels and their labels, the TURBODUDES crate, and the Throw Everything
// bonus's lifted and flying blocks). See world_warehouse.cpp for the logic.

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

// The props of levels 1-35: a word or two for each label.
const char* propLabel(int level)
{
  static const char* kLabels[] = {"",
    "EP 1 - ROOFTOP SIGN", "EP 1 - GLASS PANE", "EP 1 - HOLO AD", "EP 1 - CLUB SPEAKER", "EP 1 - SLUDGE VALVE",
    "EP 1 - MAGLEV SEAT", "EP 1 - BLACK HALO DECAL", "EP 2 - PULLEY HOOK", "EP 2 - TEMPLE MASK", "EP 2 - LIGHT MIRROR",
    "EP 2 - MINE LANTERN", "EP 2 - LAVA ROCK", "EP 2 - SURFBOARD", "EP 2 - GOLEM HAND", "EP 3 - DOCK CLAMP",
    "EP 3 - CRYO POD", "EP 3 - GROW LAMP", "EP 3 - HULL RIVET", "EP 3 - GRAV SWITCH", "EP 3 - REACTOR ROD",
    "EP 3 - ZERO'S EYE", "EP 4 - WANTED POSTER", "EP 4 - GHOST SHEET", "EP 4 - GIANT WHISK", "EP 4 - GARDEN GNOME",
    "EP 4 - GORGAZILLA FOOT", "EP 4 - FEDORA", "EP 4 - INK BOTTLE", "EP 5 - TEST CARD", "EP 5 - ANCHOR RING",
    "EP 5 - TESLA ROD", "EP 5 - MINIDISH", "EP 5 - SERVER BLADE", "EP 5 - PROPELLER", "EP 5 - TITAN BROCHURE"};
  return level >= 1 && level <= 35 ? kLabels[level] : "";
}

} // namespace

bool World::warehouseDrawsBlock(int tx, int ty) const
{
  for (const auto& c : mWarehouse.crates)
    if (c.stamped && tx >= c.x / kCellsPerTile && tx < c.x / kCellsPerTile + 2 && ty >= c.y / kCellsPerTile &&
        ty < c.y / kCellsPerTile + 2)
      return true;
  return false;
}

void World::drawWarehouseBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)frame;
  (void)alpha;
  const auto& s = mWarehouse;
  // The props on the shelves.
  for (const auto& d : s.props)
    box(r, boxAt(d.x, d.y, 4, 4), camX, camY, rgba(150, 120, 90, 120));
  // The TURBODUDES crate.
  if (s.tdX >= 0)
  {
    box(r, boxAt(s.tdX, s.tdY, 4, 4), camX, camY, rgb(150, 110, 60));
    if (s.tdShow > 0)
      box(r, boxAt(s.tdX, s.tdY - 1, 4, 2), camX, camY, (frame / 3) % 2 ? rgb(255, 80, 200) : rgb(60, 220, 255));
  }
  // Levers.
  for (const auto& l : s.levers)
  {
    box(r, boxAt(l.x, l.y, 2, 3), camX, camY, rgb(70, 70, 80));
    box(r, boxAt(l.x + (l.on ? 1 : 0), l.y - 2, 1, 2), camX, camY, l.flash > 0 ? rgb(255, 255, 255) : rgb(230, 60, 40));
  }
}

void World::drawWarehouseFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  const auto& s = mWarehouse;
  const auto& p = mPlayer;
  // Cranes: rail, trolley, cable, magnet.
  for (const auto& c : s.cranes)
  {
    box(r, {c.x0, c.rail - 1, c.x1 - c.x0 + kCraneHead, 1}, camX, camY, rgb(230, 190, 40));
    box(r, {c.x, c.rail - 2, kCraneHead, 2}, camX, camY, rgb(200, 150, 30));
    const float cx = (float(c.x) + 2.0f) * kCellPx - camX;
    r.drawLine(cx, float(c.rail) * kCellPx - camY, cx, float(c.hook) * kCellPx - camY, 3.0f, rgb(40, 40, 40));
    const bool ringing = c.phase == CranePhase::Bell || c.phase == CranePhase::RideBell;
    box(r, {c.x, c.hook, kCraneHead, 1}, camX, camY, ringing && (frame / 2) % 2 ? rgb(255, 255, 120) : rgb(200, 40, 40));
    if (c.ride && c.platform >= 0)
    {
      const auto& pl = mPlatforms[std::size_t(c.platform)];
      const float x = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha) * kCellPx - camX;
      r.fillRect(x, float(pl.y) * kCellPx - camY, float(kCrateSize) * kCellPx, float(kCrateSize) * kCellPx, rgb(110, 120, 135));
      r.fillRect(x + 6, float(pl.y) * kCellPx - camY + 6, float(kCrateSize) * kCellPx - 12, 8, rgb(170, 180, 195));
    }
  }
  // Metal crates.
  for (const auto& c : s.crates)
  {
    if (c.state == CrateState::Gone || (c.blink > 0 && (frame / 2) % 2))
      continue;
    box(r, c.box(), camX, camY, rgb(110, 120, 135));
    box(r, {c.x + 1, c.y + 1, 2, 2}, camX, camY, rgb(150, 160, 175));
  }
  // The Magnet Gun's beam, the push pulse.
  if (s.pulling && s.beam > 0)
  {
    const float y = float(p.stance == Stance::Crouched ? p.y - 1 : p.y - 2) * kCellPx + 16.0f - camY;
    const float x0 = float(p.facing > 0 ? p.x + Player::kWidth : p.x) * kCellPx - camX;
    const float x1 = x0 + float(p.facing * s.beam) * kCellPx;
    r.drawLine(x0, y, x1, y, 6.0f, (frame / 2) % 2 ? rgba(255, 80, 80, 200) : rgba(120, 160, 255, 200));
  }
  // Flats, enemies.
  for (const auto& f : s.flats)
    if (f.alive)
      box(r, f.box(), camX, camY, rgb(24, 22, 26));
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox b = e.box();
    if (e.kind == EnemyKind::Forklift)
    {
      box(r, b, camX, camY, e.tell > 0 && (frame / 2) % 2 ? rgb(255, 230, 120) : rgb(240, 180, 30));
      const CellBox forks = e.dir > 0 ? CellBox{b.right() + 1, b.bottom(), 3, 1} : CellBox{b.x - 3, b.bottom(), 3, 1};
      box(r, forks, camX, camY, rgb(60, 60, 60));
    }
    else if (e.kind == EnemyKind::Stagehand)
    {
      box(r, b, camX, camY, rgb(14, 12, 16));
      const int gx = e.dir > 0 ? b.right() : b.x;
      box(r, {gx, b.y + 1, 1, 1}, camX, camY, e.tell > 0 ? rgb(255, 255, 255) : rgb(200, 200, 200));
    }
    else if (e.kind == EnemyKind::Mimic)
    {
      int breath = 0;
      for (const auto& m : s.mimics)
        if (&mEnemies[std::size_t(m.enemy)] == &e)
          breath = m.breath < kMimicBreathe ? 1 : 0;
      box(r, {b.x, b.y - breath - (e.tell > 0 ? 2 : 0), b.w, 1}, camX, camY, rgb(170, 120, 70));
      box(r, {b.x, b.y + 1, b.w, b.h - 1}, camX, camY, rgb(150, 105, 60));
      if (e.tell > 0 || breath)
        box(r, {b.x, b.y, b.w, 1}, camX, camY, e.carrier ? rgb(80, 220, 80) : rgb(240, 240, 240));
    }
  }
  // Throw Everything: what the runner holds, what is in the air.
  if (s.carry >= 0)
  {
    const int lift = s.liftT > 0 ? s.liftT / 2 : 0;
    box(r, {p.x, p.y - p.height() - 2 + lift, 2, 2}, camX, camY, s.carry == 1 ? rgb(230, 200, 170) : rgb(180, 140, 90));
  }
  for (const auto& t : s.thrown)
    r.fillRect(t.x * kCellPx - camX, t.y * kCellPx - camY, 2.0f * kCellPx, 2.0f * kCellPx,
      t.kind == 1 ? rgb(230, 200, 170) : rgb(180, 140, 90));
}

void World::drawWarehouseHud(Renderer& r, int frame) const
{
  (void)frame;
  const auto& s = mWarehouse;
  for (const auto& d : s.props)
    if (d.label > 0)
    {
      r.drawText(propLabel(d.level), float(kScreenW) * 0.5f, 190.0f, {24.0f, rgb(255, 240, 200), kHudInk}, Align::Center);
      break;
    }
}

} // namespace gr
