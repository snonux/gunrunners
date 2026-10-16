#pragma once

// The guide arrow's route search (GUIDE ARROW in ASSISTS AND OPTIONS),
// apart from the World: what each cell is like for the runner (GuideGrid,
// kept from one search to the next while the map stays the same) and the
// cheapest-first search over the runner's own moves (GuideRouter). World
// (world_guide.cpp) says what to look for and turns the route into an arrow.

#include "game/collision.hpp"
#include "game/guide_fwd.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace gr
{

// Something the guide routes to.
struct GuideTarget
{
  CellBox box; // where the runner has to get to: the thing itself, or close enough to shoot it
  GuideKind kind = GuideKind::None;
  CellBox thing; // what the arrow is for
  bool shoot = false;
  bool crate = false; // on the floor: shot crouched, from the same floor
  // The access card (listed while a force field is up and the card isn't
  // held). Skipped for now: no goal cells (GuideRouter::markGoals) and not
  // pointed at as the crow flies, so the card is never routed to (a known
  // open issue).
  bool key = false;
};

// All that the guide's cached grids depend on besides the map's own tiles.
struct GuideStatics
{
  std::uint64_t mapRevision = 0;        // CollisionMap::revision()
  bool fieldsDown = false;              // the card is held: force fields go down as you walk up to them
  std::vector<std::array<int, 4>> open; // breakables any shot opens (blocks x0, y0, x1, y1)
  std::vector<std::array<int, 4>> lava; // lava pools that burn (cells x0, y0, x1, y1)
  bool operator==(const GuideStatics& o) const
  {
    return mapRevision == o.mapRevision && fieldsDown == o.fieldsDown && open == o.open && lava == o.lava;
  }
};

// What every cell is like for the route search. The parts that only the
// map's tiles, opened breakables, lava pools and the access card decide are
// worked out again only when one of those changes (GuideStatics);
// what stands on platforms (they move every frame) is laid over them for
// each search.
class GuideGrid
{
public:
  static constexpr int kBoxW = 3;   // the runner's box (Player::kWidth)
  static constexpr int kBoxH = 4;   // crouched: crawlspaces count
  static constexpr int kStandH = 5; // standing

  using Statics = GuideStatics;

  // Brings the grids up to date: the cached ones when `statics` differ from
  // last time (or `fresh`), and the support from `map`'s platforms and
  // floats and from `decks` (the world's platforms: x, top row, width).
  // Returns whether the cached grids were rebuilt.
  bool update(const CollisionMap& map, const Statics& statics, const std::vector<std::array<int, 3>>& decks, bool fresh = false);

  int width() const { return mW; }
  int height() const { return mH; }
  bool wrapX() const { return mWrapX; }
  bool wrapY() const { return mWrapY; }

  // The cell (x, y) after wrapping, -1 off the map.
  int cellAt(int x, int y) const
  {
    if (mWrapX)
      x = wrapIndex(x, mW);
    if (mWrapY)
      y = wrapIndex(y, mH);
    return x < 0 || x >= mW || y < 0 || y >= mH ? -1 : y * mW + x;
  }
  static int wrapIndex(int v, int n)
  {
    v %= n;
    return v < 0 ? v + n : v;
  }

  // Per cell (an index from cellAt):
  bool blocked(int c) const { return mBlocked[std::size_t(c)] != 0; } // a wall, spikes or burning lava
  int run(int c) const { return mRun[std::size_t(c)]; } // clear cells up the column to here (at most kStandH)
  bool fit(int c) const { return mFit[std::size_t(c)] != 0; }   // the crouched box with its bottom-left here is clear
  bool tall(int c) const { return mTall[std::size_t(c)] != 0; } // so is the standing box
  bool ladder(int c) const { return mLadder[std::size_t(c)] != 0; }
  bool pipe(int c) const { return mPipe[std::size_t(c)] != 0; }
  bool support(int c) const { return mSupport[std::size_t(c)] != 0; } // the box here stands on something
  // A ladder at (x, y), as the search read it from the map before the grids
  // were kept: x must be a column of the map (the caller wraps it), else
  // none; a row at or below the bottom never has one, even on a map that
  // wraps top to bottom; a row above the top wraps round to the bottom
  // only on such a map.
  bool ladderBelow(int x, int y) const;

private:
  void buildStatics(const CollisionMap& map, const Statics& statics);
  void buildBlocked(const CollisionMap& map, const Statics& statics);
  void buildFit(const CollisionMap& map);
  void standOn(const CellBox& top);

  int mW = 0, mH = 0;
  bool mWrapX = false, mWrapY = false;
  bool mBuilt = false;
  Statics mStatics;
  std::vector<std::uint8_t> mBlocked, mRun, mFit, mTall, mLadder, mPipe;
  std::vector<std::uint8_t> mBase;    // support from the tiles alone
  std::vector<std::uint8_t> mSupport; // mBase and what stands on platforms now
};

// The route found: the states from the runner to the goal (a state is
// cell * states + phase * 2 + facing; states is 1 searching with no gravity).
struct GuideRoute
{
  std::vector<int> states;
  int perCell = 1;     // states per cell: GuideRouter::kStates or 1
  int target = -1;     // index into the targets, -1 for no route
  bool walkable = false; // in the runner's own moves only
  int cell(std::size_t j) const { return states[j] / perCell; }
  int phase(std::size_t j) const { return perCell == 1 ? 0 : (states[j] % perCell) / 2; }
};

// Cheapest-first over what the runner can do a logic frame at a time, as
// player.cpp does it (see guide.cpp). The search's scratch arrays are kept
// between searches: a state counts as reached only when stamped in this
// search, so a new search clears nothing, except once every 255th search,
// when the byte-sized stamp wraps round and mSeen is cleared (begin()).
class GuideRouter
{
public:
  // tests/guide_test.cpp reads and sets the stamp to drive it round the
  // wrap (and to show what skipping the clear would do).
  friend struct GuideRouterTest;

  // Phases of a state. 0 standing; 1..8 jumping, that frame of the jump arc
  // next (a 0 in the arc hangs a frame at the top; 8: the arc is over,
  // falling starts); 9..11 falling (a cell a frame, a cell, then two); 12
  // floating; 13 on a ladder; 14 hanging from a pipe. Two facings each.
  static constexpr int kStates = 30;
  static constexpr int kFall = 9;
  static constexpr int kFloating = 12;
  static constexpr int kOnLadder = 13;
  static constexpr int kOnPipe = 14;

  // Where the runner is now in their own moves.
  struct Start
  {
    int cell = -1;
    int phase = 0;
    int facing = 1;
  };
  // How this runner moves: the jump arc and the cells a frame steered in the air.
  struct Moves
  {
    std::array<int, 8> arc{};
    int steer = 1;
  };

  // The route to the cheapest target from `start`: the runner's own moves
  // first, then floating where it must (a lift, a pulley, a door still
  // shut), then (or at once with `freeOnly`) through any open space.
  // `grid` and `targets` are only used during the call (not kept).
  GuideRoute route(const GuideGrid& grid, const std::vector<GuideTarget>& targets, const Start& start, const Moves& moves,
    bool freeOnly);

private:
  GuideRoute routeFrom(const Start& start, bool freeOnly);
  void markGoals(const std::vector<GuideTarget>& targets);
  int searchArcs(int s0, bool hybrid);
  int searchFree(int s0);
  template <class Expand> int search(int P, int s0, const Expand& expand);
  bool arrived(int s, int P) const;
  void visit(int ns, int from, int c);
  void walkBack(int found, int P, GuideRoute& r) const;
  void begin(std::size_t states);

  template <class Push> void expandFree(int s, const Push& push) const;
  template <class Push> void expandArcs(int s, const Push& push) const;
  template <class Push> void expandFloating(int i, int x, int y, int phase, int face, const Push& push) const;
  template <class Push> void expandStanding(int i, int x, int y, int face, const Push& push) const;
  template <class Push> void expandLadder(int i, int x, int y, int face, const Push& push) const;
  template <class Push> void expandPipe(int i, int x, int y, int face, const Push& push) const;
  template <class Push> void expandAir(int x, int y, int phase, int face, const Push& push) const;
  template <class Push> void grabLadder(int x, int y, int step, int face, const Push& push) const;
  template <class Push> void fallBy(int x, int y, int down, int next, int face, const Push& push) const;
  template <class Push> void jumpFrame(int x, int y, int dx, int f, int face, const Push& push) const;
  void steerBy(int& x, int y, int dx, int& face) const;

  bool fitAt(int x, int y) const
  {
    const int c = mGrid->cellAt(x, y);
    return c >= 0 && mGrid->fit(c);
  }
  bool tallAt(int x, int y) const
  {
    const int c = mGrid->cellAt(x, y);
    return c >= 0 && mGrid->tall(c);
  }
  bool ladderAt(int x, int y) const
  {
    const int c = mGrid->cellAt(x, y);
    return c >= 0 && mGrid->ladder(c);
  }
  bool pipeAt(int x, int y) const
  {
    const int c = mGrid->cellAt(x, y);
    return c >= 0 && mGrid->pipe(c);
  }
  bool wallAt(int x, int y) const
  {
    const int c = mGrid->cellAt(x, y);
    return c < 0 || mGrid->blocked(c);
  }
  bool supportAt(int c) const { return mGrid->support(c); }

  // The grid and the targets of the route() under way; null in between.
  const GuideGrid* mGrid = nullptr;
  const std::vector<GuideTarget>* mTargets = nullptr;
  Moves mMoves;
  bool mHybrid = false;
  std::vector<int> mGoal; // per cell: 1 + the index of the target it is a goal of, 0 for none
  // The search: the cost and the state before it of every state reached,
  // valid where mSeen holds this search's stamp; buckets of costs mod 32.
  // 9 bytes a state, cells * kStates states: about 15 MB on the biggest
  // map (extra_motor_pool, 56,000 cells), held by the World that shows the
  // arrow (copies start without; guideFresh makes its own for the test).
  // The search before kept 8 bytes a state per thread and cleared them for
  // every search; the stamp is a byte rather than a word so that clearing
  // nothing costs one byte a state more, not four.
  std::vector<int> mCost, mPrev;
  std::vector<std::uint8_t> mSeen;
  std::uint8_t mStamp = 0;
  std::array<std::vector<int>, 32> mBuckets;
  int mPending = 0;
};

// The guide's grids and search scratch, kept by the World between frames
// (GuideCacheSlot).
struct GuideCache
{
  GuideGrid grid;
  GuideRouter router;
};

} // namespace gr
