#pragma once

#include "data/level.hpp"

#include <cstdint>
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
  const std::vector<CellBox>& platforms() const { return mPlatforms; }
  // Index of the platform whose top row is (cx, cy), -1 if none.
  int platformAt(int cx, int cy) const;

  // Things floating on sludge (its surface, bubbles, the raft): one-way
  // tops like platforms, rebuilt by world_sludge.cpp every frame.
  void clearFloats() { mFloats.clear(); }
  void addFloat(const CellBox& b) { mFloats.push_back(b); }
  const std::vector<CellBox>& floats() const { return mFloats; }
  bool floatTop(int cx, int cy) const;

  // Checks only (navcheck --no-jump-through): blocks that a runner going up
  // can't pass from below, as if they were a ceiling, though they are one
  // way or blink; with `platforms`, moving platforms too. A ladder still
  // climbs through them. Empty and false (the game): nothing extra stops a
  // jump.
  void setNoPassUp(std::vector<std::uint8_t> blocks, bool platforms)
  {
    mNoPassUp = std::move(blocks);
    mNoPassUpPlatforms = platforms;
  }

  // Level 38's red strings: one-way slopes, the top row of each cell column
  // (-1: none). Walking along one follows it a cell up or down.
  void setSlopes(std::vector<int> tops)
  {
    mSlopes = std::move(tops);
    touch();
  }
  bool slope(int cx, int cy) const
  {
    return cx >= 0 && cx < int(mSlopes.size()) && mSlopes[std::size_t(cx)] == cy;
  }
  bool hasSlopes() const { return !mSlopes.empty(); }

  bool solid(int cx, int cy) const;    // blocks from every side
  bool solidTop(int cx, int cy) const; // can be stood on
  bool solidTopTiles(int cx, int cy) const; // solidTop by the tiles and slopes alone, not platforms or floats
  bool ladder(int cx, int cy) const;
  bool climbable(int cx, int cy) const; // pipes you can hang from
  bool hazard(int cx, int cy) const;    // spikes
  bool forceField(int cx, int cy) const;

  // The access card's switch: on until the runner walks up to a field with
  // the card, on even in a map with no force field at all (so it alone
  // doesn't say a field is in the way: see hasForceFields).
  bool forceFieldsOn() const { return mForceFieldsOn; }
  // A force field is up somewhere: the fields are on and the map has a
  // force field tile (switching them off leaves the tiles, see solid()).
  // What makes the access card worth fetching (the guide arrow, every
  // logic frame, and the autopilot's planner, per goal it chooses). The
  // tile scan is cached on the revision, so a call is cheap.
  bool hasForceFields() const { return mForceFieldsOn && hasForceFieldTiles(); }
  // Level 29's Wraparound: the edges join up, left to right and top to bottom.
  void setWrap(bool wrap)
  {
    mWrap = wrap;
    touch();
  }
  bool wraps() const { return mWrap; }
  // Level 35's turning tower: only the left and right edges join, and the
  // world turns the map round the runner (rotateX, in blocks).
  void setWrapX(bool wrap)
  {
    mWrapX = wrap;
    touch();
  }
  bool wrapsX() const { return mWrapX; }
  void rotateX(int blocks);
  void disableForceFields()
  {
    mForceFieldsOn = false;
    touch();
  }

  // Changes whenever the tiles, slopes, wrapping or force fields do (not
  // the platforms or floats): a copy has its source's, and no two states
  // of any maps share one. The guide arrow keeps its grids while it holds,
  // and hasForceFields its tile scan.
  std::uint64_t revision() const { return mRevision; }

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

  // Level 19: this map as seen by a runner whose down is `g` (1 up: turned
  // over top to bottom; 2 left, 3 right: turned a quarter so that wall is
  // the floor). Cells map as World::toView says.
  void makeView(const CollisionMap& src, int g);
  // For the planner: `src` with its turned-over view (makeView 1) stacked
  // under it, rows H.. of this map.
  void makeStack(const CollisionMap& src);

private:
  static std::uint64_t nextRevision();
  void touch() { mRevision = nextRevision(); }
  // Any ForceField tile in mTiles, the switch aside. Scanned again only
  // after the revision changed: every mutator of mTiles (setBlock,
  // rotateX, makeView, makeStack) calls touch().
  bool hasForceFieldTiles() const;
  Tile tileAt(int cx, int cy) const;
  bool oneWayTile(int cx, int cy) const; // a slope or a one-way platform tile
  void followSlope(int x, int& bottomY, int w, int h) const;

  int mW = 0, mH = 0;
  std::vector<Tile> mTiles;
  std::vector<CellBox> mPlatforms;
  std::vector<CellBox> mFloats;
  std::vector<int> mSlopes;
  std::vector<std::uint8_t> mNoPassUp;
  bool mNoPassUpPlatforms = false;
  bool mForceFieldsOn = true;
  bool mWrap = false;
  bool mWrapX = false;
  std::uint64_t mRevision = nextRevision();
  // hasForceFieldTiles' answer and the revision it holds for (0, never a
  // revision: not scanned yet). Copied with the map, so a copy's cache
  // matches its copied tiles and revision. Plain mutable members, no lock:
  // a CollisionMap is only ever used by one thread (the game loop's; the
  // planner works on its own World copies, on that thread too, and the
  // only other threads are the audio workers and the cloud saves, which
  // never see a map).
  mutable std::uint64_t mFieldTilesRevision = 0;
  mutable bool mFieldTiles = false;
};

} // namespace gr
