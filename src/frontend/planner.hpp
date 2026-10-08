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
// field to the current goal: the access card while a force field blocks the
// way, then the exit. That lets one bot play levels with beat-timed signs,
// moving platforms and other twists without bespoke code for each.
class Planner
{
public:
  // Next logic frame's input. Plans when the queue runs dry.
  Input next(const World& world);
  // Frames of input left in the current plan.
  int queued() const { return int(mQueue.size()); }

private:
  struct Goal
  {
    int kind = 0; // 0 exit, 1 key
    int x = 0, y = 0;
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
  int mW = 0, mH = 0;
  int mGoalKind = -1;
  int mGoalKeyHash = 0;
  int mFails = 0;
};

} // namespace gr
