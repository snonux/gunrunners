#pragma once

#include "data/level.hpp"

#include <vector>

namespace gr
{

// Axis-aligned box on the 8 px cell grid. Like in Duke Nukem II, actor
// positions refer to the bottom-left cell of the box.
struct CellBox
{
  int x = 0;
  int y = 0; // top row
  int w = 1;
  int h = 1;

  int left() const { return x; }
  int right() const { return x + w - 1; }
  int top() const { return y; }
  int bottom() const { return y + h - 1; }
  bool intersects(const CellBox& o) const
  {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
  }
  bool contains(int cx, int cy) const { return cx >= x && cx < x + w && cy >= y && cy < y + h; }
};

inline CellBox boxAt(int x, int bottomY, int w, int h) { return {x, bottomY - h + 1, w, h}; }

enum class MoveResult
{
  Failed,
  MovedPartially,
  Completed,
};

// Answers "what is in this cell" for the level and does the step-by-step
// movement that RigelEngine's engine/collision_checker.cpp and
// engine/movement.cpp implement for Duke Nukem II.
class CollisionMap
{
public:
  // Copies the level's tiles: switchable layers change them at run time,
  // and a copied World (the bot plans on copies) gets its own.
  explicit CollisionMap(const Level& level);

  int width() const { return mW * kCellsPerTile; }
  int height() const { return mH * kCellsPerTile; }
  int widthBlocks() const { return mW; }
  int heightBlocks() const { return mH; }

  // Block (16 px) access, same edge rules as Level::at.
  Tile block(int tx, int ty) const;
  void setBlock(int tx, int ty, Tile t);

  // Moving platforms: boxes whose top row can be stood on, rebuilt by the
  // world every frame.
  void clearPlatforms() { mPlatforms.clear(); }
  void addPlatform(const CellBox& b) { mPlatforms.push_back(b); }
  // Index of the platform whose top row is (cx, cy), -1 if none.
  int platformAt(int cx, int cy) const;

  bool solid(int cx, int cy) const;    // blocks from every side
  bool solidTop(int cx, int cy) const; // can be stood on
  bool ladder(int cx, int cy) const;
  bool climbable(int cx, int cy) const; // pipes you can hang from
  bool hazard(int cx, int cy) const;    // spikes
  bool forceField(int cx, int cy) const;

  bool forceFieldsOn() const { return mForceFieldsOn; }
  void disableForceFields() { mForceFieldsOn = false; }

  bool onSolidGround(const CellBox& b) const;
  bool touchingCeiling(const CellBox& b) const;
  bool touchingLeftWall(const CellBox& b) const;
  bool touchingRightWall(const CellBox& b) const;
  bool overlapsSolid(const CellBox& b) const;
  bool overlapsHazard(const CellBox& b) const;

  // x/bottomY are the actor position (bottom-left), w/h its box size.
  MoveResult moveHorizontally(int& x, int bottomY, int w, int h, int amount) const;
  MoveResult moveHorizontallyWithStairStepping(int& x, int& bottomY, int w, int h, int amount) const;
  MoveResult moveVertically(int x, int& bottomY, int w, int h, int amount) const;

private:
  Tile tileAt(int cx, int cy) const;

  int mW = 0, mH = 0;
  std::vector<Tile> mTiles;
  std::vector<CellBox> mPlatforms;
  bool mForceFieldsOn = true;
};

} // namespace gr
