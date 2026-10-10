// Cell based collision and movement. The movement rules (stair stepping,
// one cell at a time vertical moves, solid-top platforms) follow
// RigelEngine's src/engine/movement.cpp and collision_checker.cpp
// (GPL-2.0-or-later, Copyright (C) 2017 Nikolai Wuttke).

#include "game/collision.hpp"

#include <cstdlib>

namespace gr
{

namespace
{

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int sgn(int v) { return (v > 0) - (v < 0); }

} // namespace

CollisionMap::CollisionMap(const Level& level) : mW(level.width), mH(level.height), mTiles(level.tiles) {}

Tile CollisionMap::block(int tx, int ty) const
{
  if (tx < 0 || tx >= mW)
    return Tile::Solid;
  if (ty < 0 || ty >= mH)
    return Tile::Empty;
  return mTiles[std::size_t(ty * mW + tx)];
}

void CollisionMap::setBlock(int tx, int ty, Tile t)
{
  if (tx >= 0 && ty >= 0 && tx < mW && ty < mH)
    mTiles[std::size_t(ty * mW + tx)] = t;
}

int CollisionMap::platformAt(int cx, int cy) const
{
  for (std::size_t i = 0; i < mPlatforms.size(); ++i)
  {
    const auto& p = mPlatforms[i];
    if (cy == p.top() && cx >= p.left() && cx <= p.right())
      return int(i);
  }
  return -1;
}

bool CollisionMap::floatTop(int cx, int cy) const
{
  for (const auto& f : mFloats)
    if (cy == f.top() && cx >= f.left() && cx <= f.right())
      return true;
  return false;
}

Tile CollisionMap::tileAt(int cx, int cy) const
{
  return block(floorDiv(cx, kCellsPerTile), floorDiv(cy, kCellsPerTile));
}

bool CollisionMap::solid(int cx, int cy) const
{
  // The map is closed on the left, right and top; falling out of the
  // bottom is deadly.
  if (cx < 0 || cx >= width() || cy < 0)
    return true;
  switch (tileAt(cx, cy))
  {
    case Tile::Solid:
    case Tile::Grate:
      return cy >= 0 && cy < height();
    case Tile::Spikes:
      return (cy & 1) == 1; // the base of the spike block
    case Tile::SpikesDown:
      return (cy & 1) == 0; // hung from the ceiling
    case Tile::ForceField:
      return mForceFieldsOn;
    default:
      return false;
  }
}

bool CollisionMap::solidTop(int cx, int cy) const
{
  if (solid(cx, cy))
    return true;
  if (!mPlatforms.empty() && platformAt(cx, cy) >= 0)
    return true;
  if (!mFloats.empty() && floatTop(cx, cy))
    return true;
  return tileAt(cx, cy) == Tile::Platform && (cy & 1) == 0;
}

bool CollisionMap::ladder(int cx, int cy) const
{
  // Ladders run up the left column of their block; that is the column the
  // player's centre snaps to.
  return tileAt(cx, cy) == Tile::Ladder && (cx & 1) == 0;
}

bool CollisionMap::climbable(int cx, int cy) const
{
  return tileAt(cx, cy) == Tile::Pipe && (cy & 1) == 0;
}

bool CollisionMap::hazard(int cx, int cy) const
{
  const Tile t = tileAt(cx, cy);
  return (t == Tile::Spikes && (cy & 1) == 0) || (t == Tile::SpikesDown && (cy & 1) == 1);
}

bool CollisionMap::forceField(int cx, int cy) const
{
  return mForceFieldsOn && tileAt(cx, cy) == Tile::ForceField;
}

bool CollisionMap::onSolidGround(const CellBox& b) const
{
  for (int x = b.left(); x <= b.right(); ++x)
    if (solidTop(x, b.bottom() + 1))
      return true;
  return false;
}

bool CollisionMap::touchingCeiling(const CellBox& b) const
{
  for (int x = b.left(); x <= b.right(); ++x)
    if (solid(x, b.top() - 1))
      return true;
  return false;
}

bool CollisionMap::touchingLeftWall(const CellBox& b) const
{
  for (int y = b.top(); y <= b.bottom(); ++y)
    if (solid(b.left() - 1, y))
      return true;
  return false;
}

bool CollisionMap::touchingRightWall(const CellBox& b) const
{
  for (int y = b.top(); y <= b.bottom(); ++y)
    if (solid(b.right() + 1, y))
      return true;
  return false;
}

bool CollisionMap::overlapsSolid(const CellBox& b) const
{
  for (int y = b.top(); y <= b.bottom(); ++y)
    for (int x = b.left(); x <= b.right(); ++x)
      if (solid(x, y))
        return true;
  return false;
}

bool CollisionMap::overlapsHazard(const CellBox& b) const
{
  for (int y = b.top(); y <= b.bottom(); ++y)
    for (int x = b.left(); x <= b.right(); ++x)
      if (hazard(x, y))
        return true;
  return false;
}

MoveResult CollisionMap::moveHorizontally(int& x, int bottomY, int w, int h, int amount) const
{
  const int step = sgn(amount);
  for (int i = 0; i < std::abs(amount); ++i)
  {
    const CellBox b = boxAt(x, bottomY, w, h);
    if ((step < 0 && touchingLeftWall(b)) || (step > 0 && touchingRightWall(b)))
      return i > 0 ? MoveResult::MovedPartially : MoveResult::Failed;
    x += step;
  }
  return MoveResult::Completed;
}

MoveResult CollisionMap::moveHorizontallyWithStairStepping(int& x, int& bottomY, int w, int h, int amount) const
{
  const int step = sgn(amount);
  for (int i = 0; i < std::abs(amount); ++i)
  {
    if (moveHorizontally(x, bottomY, w, h, step) == MoveResult::Completed)
      continue;
    // Walk up steps that are a single cell high.
    CellBox stepped = boxAt(x, bottomY - 1, w, h);
    const bool blocked = step < 0 ? touchingLeftWall(stepped) : touchingRightWall(stepped);
    if (blocked || touchingCeiling(boxAt(x, bottomY, w, h)))
      return i > 0 ? MoveResult::MovedPartially : MoveResult::Failed;
    x += step;
    bottomY -= 1;
  }
  return MoveResult::Completed;
}

MoveResult CollisionMap::moveVertically(int x, int& bottomY, int w, int h, int amount) const
{
  const int step = sgn(amount);
  for (int i = 0; i < std::abs(amount); ++i)
  {
    const CellBox b = boxAt(x, bottomY, w, h);
    if ((step > 0 && onSolidGround(b)) || (step < 0 && touchingCeiling(b)))
      return i > 0 ? MoveResult::MovedPartially : MoveResult::Failed;
    bottomY += step;
  }
  return MoveResult::Completed;
}

void CollisionMap::makeView(const CollisionMap& src, int g)
{
  const int W = src.mW, H = src.mH;
  const bool side = g == 2 || g == 3;
  mW = side ? H : W;
  mH = side ? W : H;
  mTiles.assign(std::size_t(mW * mH), Tile::Empty);
  mForceFieldsOn = src.mForceFieldsOn;
  for (int ty = 0; ty < H; ++ty)
    for (int tx = 0; tx < W; ++tx)
    {
      Tile t = src.mTiles[std::size_t(ty * W + tx)];
      // Spikes point the other way up when the world is turned over.
      if (g == 1 && (t == Tile::Spikes || t == Tile::SpikesDown))
        t = t == Tile::Spikes ? Tile::SpikesDown : Tile::Spikes;
      else if (side && (t == Tile::Spikes || t == Tile::SpikesDown))
        t = Tile::Solid;
      const int vx = g == 1 ? tx : (g == 2 ? ty : H - 1 - ty);
      const int vy = g == 1 ? H - 1 - ty : (g == 2 ? W - 1 - tx : tx);
      mTiles[std::size_t(vy * mW + vx)] = t;
    }
  auto turn = [&](const CellBox& b) {
    const int Wc = W * kCellsPerTile, Hc = H * kCellsPerTile;
    if (g == 1)
      return CellBox{b.x, Hc - b.y - b.h, b.w, b.h};
    if (g == 2)
      return CellBox{b.y, Wc - b.x - b.w, b.h, b.w};
    return CellBox{Hc - b.y - b.h, b.x, b.h, b.w};
  };
  mPlatforms.clear();
  for (const auto& b : src.mPlatforms)
    mPlatforms.push_back(turn(b));
  mFloats.clear();
  for (const auto& b : src.mFloats)
    mFloats.push_back(turn(b));
}

void CollisionMap::makeStack(const CollisionMap& src)
{
  CollisionMap up = src;
  up.makeView(src, 1);
  mW = src.mW;
  mH = src.mH * 2;
  mForceFieldsOn = src.mForceFieldsOn;
  mTiles = src.mTiles;
  mTiles.insert(mTiles.end(), up.mTiles.begin(), up.mTiles.end());
  mPlatforms = src.mPlatforms;
  mFloats = src.mFloats;
  const int Hc = src.mH * kCellsPerTile;
  for (auto b : up.mPlatforms)
  {
    b.y += Hc;
    mPlatforms.push_back(b);
  }
  for (auto b : up.mFloats)
  {
    b.y += Hc;
    mFloats.push_back(b);
  }
}

} // namespace gr
