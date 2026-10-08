#pragma once

#include "game/input.hpp"

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

private:
  struct Goal
  {
    int kind = 0; // 0 exit, 1 key, 2 the level's prototype, 3 bonus entrance, 4 a breaker, 5 a Grid Leech
    int x = 0, y = 0;
    int w = 0, h = 0; // kind 3: the entrance's box
    int index = -1;   // kind 4: the breaker, kind 5: the Leech (enemy index)
  };
  void plan(const World& world);
  Goal chooseGoal(const World& world) const;
  void buildField(const World& world, const Goal& goal);
  int heuristic(const World& world) const;
  void dumpField() const;

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
};

} // namespace gr
