#pragma once

#include "game/input.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace gr
{

class World;

// Search-based autopilot. It plans by simulating copies of the world (the
// logic is deterministic) with short input "macros", guided by a distance
// field to the current goal: the level's prototype (the first box), the
// access card while a force field blocks the way, then the exit. That lets one bot play levels with beat-timed signs,
// moving platforms and other twists without bespoke code for each.
class Planner
{
public:
  // Next logic frame's input. Plans when the queue runs dry.
  Input next(const World& world);
  // Frames of input left in the current plan.
  int queued() const { return int(mQueue.size()); }
  // Also visit the bonus entrance on the way (for demos and tests).
  void setTakeBonus(bool take) { mTakeBonus = take; }
  bool wantsBonus() const { return mTakeBonus && !mSkipBonus; }
  // Drops the current plan (another controller drove in the meantime).
  void reset() { mQueue.clear(); }
  // Level 14: the altar to hold up beside now (an index into the world's
  // altars), or -1.
  int altarToHold(const World& world) const;

private:
  struct Goal
  {
    int kind = 0; // 0 exit, 1 key, 2 the level's prototype, 3 bonus entrance, 4 a breaker, 5 a Grid Leech
                  // (or a Sun Moth), 6 a stone key, 7 a plate to press (the bonus patch's), 8 a mirror to turn,
                  // 9 an alcove's shelter (index: the alcove) to wait in, 10 an altar to hold up beside for the
                  // bonus (give the gems away, then wake the entrance at the false altar), 11 the last altar
                  // to give gems back at before the golem (index: the altar), 12 a marked block to stand on
                  // and gild (Golden Touch; index: the block), 13 a grow lamp's switch to hit (index: the
                  // lamp; x, y, w, h: where to stand)
    int x = 0, y = 0;
    int w = 0, h = 0; // kind 3: the entrance's box; kind 5: w=1 shoot it from below; kind 8: w the
                      // shot's direction, h the angle wanted
    int index = -1;   // kind 4: the breaker, kind 5: the Leech (enemy index), kind 7: the plate, kind 8: the mirror
  };
  void plan(const World& world);
  Goal chooseGoal(const World& world) const;
  Goal altarGoal(const World& world) const;
  void buildField(const World& world, const Goal& goal);
  int heuristic(const World& world) const;
  void dumpField() const;
  // Level 10: the sun door the way on needs next, and what lights it: a
  // mirror to turn, moths to clear off the beam, or waiting for it.
  bool lightGoal(const World& world, Goal& goal);
  bool solveMirrors(const World& world, int door);
  // Level 17: a grow lamp the way on needs lit, the next one along it.
  bool lampGoal(const World& world, Goal& goal);

  std::deque<Input> mQueue;
  Input mPrev;
  // Distance field over player positions (bottom-left cell) and air budget.
  std::vector<int> mDist;
  std::vector<int> mWalls; // breakables in the way of the goal: shooting them is progress
  int mW = 0, mH = 0;
  int mGoalKind = -1;
  int mGoalIndex = -1;
  int mGoalKeyHash = 0;
  int mFails = 0;
  bool mSkipProto = false; // the prototype is out of reach: go without it
  bool mSkipHadKey = false;
  bool mTakeBonus = false;
  bool mSkipBonus = false;
  int mSkipBonusAt = -1; // where the search gave up on it (-1: for good)
  std::vector<int> mPaintSkip; // Golden Touch: marked blocks the search could not get to
  // Level 10.
  std::vector<char> mDoorOpen;  // sun doors the field treats as open
  std::vector<char> mRouteOpen; // while waiting on a door: it and the route doors after it
  std::uint64_t mLightKey = 0;
  int mLightDoor = -1;   // the door the way needs
  int mPendingDoor = -1; // the slab that will open (the door, or the hatch it opens): wait for it
  std::uint64_t mSolveKey = 0;
  bool mSolved = false;
  int mSolveBeam = -1;
  std::vector<int> mWant;  // the angle each mirror needs (-1: not on the way)
  std::vector<int> mOrder; // those mirrors in the order the beam meets them
  // Level 17: lamps the field takes as lit (empty: as they are).
  std::vector<char> mLampOn;
  std::uint64_t mLampKey = 0;
  int mLampWanted = -1;
};

} // namespace gr
