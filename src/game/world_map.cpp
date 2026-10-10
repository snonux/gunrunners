// The in-game map: which blocks the runners have seen, and drawing them as
// a clean schematic (walls, ledges, ladders, hazards, doors) for the map
// screen in frontend/map_view.cpp.

#include "game/world.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr Color kMapAir = rgb(26, 30, 56);
constexpr Color kMapWall = rgb(66, 76, 128);
constexpr Color kMapLadder = rgba(225, 228, 245, 200);
constexpr Color kMapPipe = rgba(170, 172, 196, 220);
constexpr Color kMapSpikes = rgb(255, 72, 72);
constexpr Color kMapField = rgb(70, 205, 255);
constexpr Color kMapDoor = rgb(255, 150, 40);
constexpr Color kMapLava = rgb(255, 96, 32);
constexpr Color kMapSludge = rgb(124, 204, 64);

// How far a runner sees in the dark, in blocks (sonar: only by touch).
constexpr int kDarkSight = 4;
constexpr int kSonarSight = 2;

bool wallTile(Tile t)
{
  return t == Tile::Solid || t == Tile::Grate;
}

} // namespace

bool World::explored(int tx, int ty) const
{
  if (!mExplored || tx < 0 || ty < 0 || tx >= mLevel->width || ty >= mLevel->height)
    return false;
  return (*mExplored)[std::size_t(ty * mLevel->width + tx)] != 0;
}

void World::markExplored()
{
  if (!mExplored || mSimulation)
    return;
  auto& seen = *mExplored;
  const int w = mLevel->width, h = mLevel->height;
  // Everything on screen counts as seen.
  const int bx0 = std::max(0, mCamera.x() / kCellsPerTile);
  const int by0 = std::max(0, mCamera.y() / kCellsPerTile);
  const int bx1 = std::min(w - 1, (mCamera.x() + int(std::ceil(kViewCellsW)) - 1) / kCellsPerTile);
  const int by1 = std::min(h - 1, (mCamera.y() + int(std::ceil(kViewCellsH)) - 1) / kCellsPerTile);
  // In a power cut (and in the sonar bonus) only what is lit, or close by.
  const bool dark = mSonar || !mSectors.empty();
  const int px = (mPlayer.x + 1) / kCellsPerTile;
  const int py = (mPlayer.y - 2) / kCellsPerTile;
  const int sight = mSonar ? kSonarSight : kDarkSight;
  for (int by = by0; by <= by1; ++by)
    for (int bx = bx0; bx <= bx1; ++bx)
    {
      auto& b = seen[std::size_t(by * w + bx)];
      if (b)
        continue;
      if (dark && std::max(std::abs(bx - px), std::abs(by - py)) > sight &&
          (mSonar || !litAt(bx * kCellsPerTile + 1, by * kCellsPerTile + 1)))
        continue;
      b = 1;
    }
}

std::vector<int> World::exploredRuns() const
{
  std::vector<int> runs;
  if (!mExplored || std::find(mExplored->begin(), mExplored->end(), 1) == mExplored->end())
    return runs;
  std::uint8_t cur = 0;
  int n = 0;
  for (const std::uint8_t v : *mExplored)
  {
    if ((v != 0) != (cur != 0))
    {
      runs.push_back(n);
      n = 0;
      cur = v ? 1 : 0;
    }
    ++n;
  }
  runs.push_back(n);
  return runs;
}

void World::restoreExplored(const std::vector<int>& runs)
{
  if (!mExplored)
    return;
  auto& seen = *mExplored;
  std::fill(seen.begin(), seen.end(), 0);
  // A save from before the map, or for a different layout: start blank.
  long total = 0;
  for (int n : runs)
  {
    if (n < 0)
      return;
    total += n;
  }
  if (total != long(seen.size()))
    return;
  std::size_t at = 0;
  std::uint8_t v = 0;
  for (int n : runs)
  {
    std::fill(seen.begin() + long(at), seen.begin() + long(at) + n, v);
    at += std::size_t(n);
    v = v ? 0 : 1;
  }
}

Texture World::bakeMap(Renderer& r, float s, int bx0, int by0, int bw, int bh) const
{
  const int w = mLevel->width, h = mLevel->height;
  const int bx1 = std::min(w, bx0 + bw), by1 = std::min(h, by0 + bh);
  bool any = false;
  for (int by = std::max(0, by0); by < by1 && !any; ++by)
    for (int bx = std::max(0, bx0); bx < bx1 && !any; ++bx)
      any = explored(bx, by);
  if (!any)
    return {};

  VectorImage img(std::max(1, int(std::ceil(float(bw) * s))), std::max(1, int(std::ceil(float(bh) * s))));
  cairo_t* cr = img.cr();
  cairo_translate(cr, -double(bx0) * s, -double(by0) * s);
  const double d = s;
  auto seen = [&](int bx, int by) { return explored(bx, by); };
  auto block = [&](int bx, int by) { return mMap.block(bx, by); };
  auto layer = [&](int bx, int by) {
    return bx >= 0 && by >= 0 && bx < w && by < h && mLayerMask[std::size_t(by * w + bx)] != 0;
  };
  auto each = [&](auto fn) {
    // One block of margin, so outlines and shapes that start next door
    // join up across the chunks.
    for (int by = std::max(0, by0 - 1); by < std::min(h, by1 + 1); ++by)
      for (int bx = std::max(0, bx0 - 1); bx < std::min(w, bx1 + 1); ++bx)
        if (seen(bx, by))
          fn(bx, by, double(bx) * d, double(by) * d);
  };

  // Explored space, a shade lighter than the unknown around it.
  each([&](int, int, double x, double y) { cairo_rectangle(cr, x, y, d, d); });
  setColor(cr, kMapAir);
  cairo_fill(cr);

  // Walls (switchable layers are drawn on their own below).
  each([&](int bx, int by, double x, double y) {
    if (wallTile(block(bx, by)) && !layer(bx, by))
      cairo_rectangle(cr, x, y, d, d);
  });
  setColor(cr, kMapWall);
  cairo_fill(cr);
  // ...outlined where they face open, explored space.
  auto wallAt = [&](int bx, int by) {
    return bx < 0 || by < 0 || bx >= w || by >= h || (wallTile(block(bx, by)) && !layer(bx, by));
  };
  each([&](int bx, int by, double x, double y) {
    if (!wallAt(bx, by))
      return;
    if (!wallAt(bx, by - 1) && seen(bx, by - 1))
    {
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x + d, y);
    }
    if (!wallAt(bx, by + 1) && seen(bx, by + 1))
    {
      cairo_move_to(cr, x, y + d);
      cairo_line_to(cr, x + d, y + d);
    }
    if (!wallAt(bx - 1, by) && seen(bx - 1, by))
    {
      cairo_move_to(cr, x, y);
      cairo_line_to(cr, x, y + d);
    }
    if (!wallAt(bx + 1, by) && seen(bx + 1, by))
    {
      cairo_move_to(cr, x + d, y);
      cairo_line_to(cr, x + d, y + d);
    }
  });
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width(cr, std::max(1.2, d * 0.14));
  setColor(cr, withAlpha(mTheme.accentA, 230));
  cairo_stroke(cr);

  // Fluids: sludge from its surface down, lava pools.
  for (const auto& f : mFluids)
  {
    for (int cy = std::max(f.surface, f.y0); cy <= f.y1; cy += kCellsPerTile)
      for (int cx = f.x0; cx <= f.x1; cx += kCellsPerTile)
        if (seen(cx / kCellsPerTile, cy / kCellsPerTile) && !wallTile(block(cx / kCellsPerTile, cy / kCellsPerTile)))
          cairo_rectangle(cr, double(cx / kCellsPerTile) * d, double(cy / kCellsPerTile) * d, d, d);
    setColor(cr, withAlpha(kMapSludge, 150));
    cairo_fill(cr);
  }
  for (const auto& l : mLavas)
  {
    for (int by = l.y0 / kCellsPerTile; by <= l.y1 / kCellsPerTile; ++by)
      for (int bx = l.x0 / kCellsPerTile; bx <= l.x1 / kCellsPerTile; ++bx)
        if (seen(bx, by) && !wallTile(block(bx, by)))
          cairo_rectangle(cr, double(bx) * d, double(by) * d, d, d);
    setColor(cr, withAlpha(kMapLava, 190));
    cairo_fill(cr);
  }

  // Ledges, ladders, hang bars, spikes, force fields and layers.
  const double line = std::max(1.5, d * 0.2);
  each([&](int bx, int by, double x, double y) {
    switch (block(bx, by))
    {
      case Tile::Platform:
        if (layer(bx, by))
          break;
        cairo_rectangle(cr, x, y, d, line);
        setColor(cr, mTheme.accentB);
        cairo_fill(cr);
        break;
      case Tile::Ladder:
        cairo_rectangle(cr, x + d * 0.22, y, d * 0.1, d);
        cairo_rectangle(cr, x + d * 0.68, y, d * 0.1, d);
        cairo_rectangle(cr, x + d * 0.22, y + d * 0.2, d * 0.56, d * 0.1);
        cairo_rectangle(cr, x + d * 0.22, y + d * 0.7, d * 0.56, d * 0.1);
        setColor(cr, kMapLadder);
        cairo_fill(cr);
        break;
      case Tile::Pipe:
        cairo_rectangle(cr, x, y + d * 0.2, d, std::max(1.0, d * 0.12));
        setColor(cr, kMapPipe);
        cairo_fill(cr);
        break;
      case Tile::Spikes:
        cairo_move_to(cr, x, y + d);
        for (int i = 0; i < 3; ++i)
        {
          cairo_line_to(cr, x + d * (i + 0.5) / 3.0, y + d * 0.35);
          cairo_line_to(cr, x + d * (i + 1.0) / 3.0, y + d);
        }
        cairo_close_path(cr);
        setColor(cr, kMapSpikes);
        cairo_fill(cr);
        break;
      case Tile::ForceField:
        if (!mMap.forceFieldsOn())
          break;
        cairo_rectangle(cr, x + d * 0.35, y, d * 0.3, d);
        setColor(cr, withAlpha(kMapField, 210));
        cairo_fill(cr);
        break;
      default:
        break;
    }
    if (layer(bx, by))
    {
      // Blocks that come and go (beat signs, timed bridges): a dashed box.
      const double in = d * 0.12;
      cairo_rectangle(cr, x + in, y + in, d - in * 2.0, d - in * 2.0);
      setColor(cr, withAlpha(mTheme.accentB, 70));
      cairo_fill_preserve(cr);
      const double dash[] = {d * 0.18, d * 0.12};
      cairo_set_dash(cr, dash, 2, 0.0);
      cairo_set_line_width(cr, std::max(1.0, d * 0.08));
      setColor(cr, withAlpha(mTheme.accentB, 220));
      cairo_stroke(cr);
      cairo_set_dash(cr, nullptr, 0, 0.0);
    }
  });

  // Doors still shut, once any of them has been seen.
  auto door = [&](int x0, int y0, int dw, int dh) {
    bool any = false;
    for (int by = y0; by < y0 + dh; ++by)
      for (int bx = x0; bx < x0 + dw; ++bx)
        any = any || seen(bx, by);
    if (!any)
      return;
    roundedRect(cr, double(x0) * d + d * 0.12, double(y0) * d + d * 0.04, double(dw) * d - d * 0.24,
      double(dh) * d - d * 0.08, d * 0.15);
    setColor(cr, withAlpha(kMapDoor, 225));
    cairo_fill(cr);
  };
  for (const auto& dr : mDoors)
    if (dr.solid)
      door(dr.x0, dr.y0, dr.w, dr.h);
  for (const auto& dr : mKeyDoors)
    if (!dr.open)
      door(dr.tx, dr.ty, 1, dr.h);
  for (const auto& dr : mSunDoors)
    if (!dr.open)
      door(dr.x, dr.y, dr.w, dr.h);

  return img.toTexture(r);
}

void World::drawMapMarks(Renderer& r, float ox, float oy, float s, int frame) const
{
  const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.12f);
  const float dot = std::max(4.0f, s * 0.45f);

  for (const auto& cp : mCheckpoints)
  {
    const int bx = cp.x / kCellsPerTile, by = (cp.y - 2) / kCellsPerTile;
    if (!explored(bx, by))
      continue;
    const float x = ox + (float(cp.x) + 1.0f) / float(kCellsPerTile) * s;
    const float y = oy + (float(cp.y) - 1.5f) / float(kCellsPerTile) * s;
    if (cp.active)
      drawGlow(r, mArt, x, y, dot * 2.6f, mTheme.accentB, 0.55f);
    r.fillRect(x - dot * 0.3f, y - dot, dot * 0.6f, dot * 2.0f, cp.active ? mTheme.accentB : rgb(150, 150, 170));
  }

  if ((!mBoss.on || mBoss.exitT >= 0) && explored(mLevel->exitTx, mLevel->exitTy))
  {
    const float x = ox + (float(mLevel->exitTx) + 0.5f) * s;
    const float y = oy + (float(mLevel->exitTy) + 0.5f) * s;
    drawGlow(r, mArt, x, y, dot * (3.0f + pulse), rgb(80, 255, 140), 0.7f);
    r.fillRect(x - dot * 0.8f, y - dot * 0.8f, dot * 1.6f, dot * 1.6f, rgb(80, 255, 140));
    r.drawText("EXIT", x, y - dot * 0.8f - 24.0f, {16.0f, rgb(80, 255, 140), rgb(10, 8, 20), true}, Align::Center);
  }

  // The runner: a beacon that pulses, and which way they face.
  const float px = ox + (float(mPlayer.x) + 1.5f) / float(kCellsPerTile) * s;
  const float py = oy + (float(mPlayer.y) - 2.0f) / float(kCellsPerTile) * s;
  drawGlow(r, mArt, px, py, dot * (3.0f + 1.5f * pulse), mTheme.accentA, 0.8f);
  r.fillRect(px - dot * 0.75f, py - dot * 0.75f, dot * 1.5f, dot * 1.5f, rgb(255, 255, 255));
  r.fillRect(px - dot * 0.5f, py - dot * 0.5f, dot, dot, mTheme.accentA);
  const float ax = px + float(mPlayer.facing) * dot * 1.4f;
  r.drawLine(px, py, ax, py, std::max(2.0f, dot * 0.3f), rgb(255, 255, 255));
  r.drawText(mCharacter.name, px, py - dot - 30.0f, {17.0f, rgb(255, 255, 255), rgb(10, 8, 20), true}, Align::Center);
}

} // namespace gr
