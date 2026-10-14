#pragma once

#include "frontend/planner.hpp"
#include "game/input.hpp"

#include <deque>
#include <vector>

namespace gr
{

class World;

// A small autopilot used for attract mode and for recording gameplay clips
// headlessly. It plays the same way a human would: by producing Input, once
// per 15 Hz logic frame. (RigelEngine records and replays input for its demo
// loop; a bot keeps working while the level layout is still changing.)
class Bot
{
public:
  // The search planner plays; the old reactive rules remain as a fallback
  // for the PoC level (playRules).
  Input play(const World& world);
  Input playRules(const World& world);
  Input menu(int cursor, int target, int ticksInMenu) const;
  // Black Halo's arena: short look-ahead fights instead of the planner.
  Input fightBoss(const World& world);
  Input fightGolem(const World& world);
  // Level 49: the Hive Mother (bot_mother.cpp).
  Input fightMother(const World& world);
  // Level 21: ZERO (bot_zero.cpp).
  Input fightZero(const World& world);
  // Pilot Seat: fly at the cardboard.
  Input fly(const World& world);
  // Trapmaster: spring the trap that will catch the most hunters.
  Input trapmaster(const World& world);
  // Pinball Mine: time the flippers by trying them in copies of the world.
  Input pinball(const World& world);
  // The Floor Is Lava: pick each bounce by trying moves in copies of the
  // world, looking one head further on.
  Input floorLava(const World& world);
  Input surf(const World& world);
  Input drift(const World& world);
  // Planetoids: from each planetoid, try every walk-then-jump in copies of
  // the world and take the one that gets a ship part or lands nearest one.
  Input orbit(const World& world);
  Input turntable(const World& world); // bot_kitchen.cpp
  bool kitchen(const World& world, Input& in);
  bool garden(const World& world, Input& in); // bot_garden.cpp
  Input glide(const World& world);
  bool city(const World& world, Input& in); // bot_city.cpp
  bool noir(const World& world, Input& in); // bot_noir.cpp
  // Fiber Trench (bot_trench.cpp): at an anchor ring's planting spot, plant
  // a harpoon and shoot the ring, so the planner walks the line.
  bool trench(const World& world, Input& in);
  std::deque<Input> mTrenchQueue;
  // Bubble Up: each short steer (and taps of jump to sink) tried two deep,
  // scored by a distance field to the exit that keeps clear of the spikes.
  Input bubbleUp(const World& world);
  std::vector<int> mBubbleField;
  Input mBubblePrev;
  // Storm Relay (bot_storm.cpp): a rod in the dead generator (for the
  // bonus); Bungee: the fling that gets the most rings.
  bool storm(const World& world, Input& in);
  Input bungee(const World& world);
  std::deque<Input> mStormQueue;
  // Dish Array (bot_dish.cpp): on a dish's ride, stand still through a turn
  // when the next pose is nearer the goal.
  bool dish(const World& world, Input& in);
  int mDishWait = 0;
  int mDishChecked = -1;
  // Magma Mainframe (bot_magma.cpp): shoot the RATINGS server from the
  // nook for the bonus.
  bool magma(const World& world, Input& in);
  std::deque<Input> mMagmaQueue;
  // Zeppelin Relay (bot_fleet.cpp): the grapple chain, the bomb bay for the
  // bonus; Human Cannonball: each shot's angle and steer tried in copies.
  bool fleet(const World& world, Input& in);
  Input cannonball(const World& world);
  bool fleetQueue(Input& in);
  std::deque<Input> mFleetQueue;
  Input mFleetPrev;
  bool mFleetLast = false;
  int mFleetRingX = -1; // the furthest chain ring hooked since the last deck
  // The Transmitter (bot_tower.cpp): the crawl space's doors, the
  // Halcyon Titan (moves tried in copies, like Gorgazilla), Copycat.
  bool tower(const World& world, Input& in);
  // Prop Warehouse (bot_warehouse.cpp): levers, cranes, crates to push;
  // Throw Everything's stacks.
  bool warehouse(const World& world, Input& in);
  Input mWarePrev;
  // Wardrobe (bot_wardrobe.cpp): up in each booth on the route, wait for a
  // cutting table; the Totem's throws, switches and restacks.
  bool wardrobe(const World& world, Input& in);
  Input totem(const World& world);
  bool mRobePrevUp = false, mRobePrevJump = false;
  // Writers' Room (bot_writers.cpp): strike the words in the way, shoot
  // Typos, type the bridge; Breakout's paddle.
  bool writers(const World& world, Input& in);
  // Level 39 (bot_gallery.cpp): monitors, panning cameras, cables; Rewind's crushers.
  bool gallery(const World& world, Input& in);
  bool mGalleryUp = false;
  // Level 40 (bot_flytower.cpp): truss rides, waiting out the pyro, the
  // jetpack lane; Trigger Happy's discs.
  bool flyTower(const World& world, Input& in);
  Input triggerHappy(const World& world);
  bool mFlyPrevFire = false, mFlyJet = false;
  int mFlyDisc = -1;
  // Level 41 (bot_sweeps.cpp): the Virus's V, Plants standing up; MIRROR MODE.
  bool sweeps(const World& world, Input& in);
  bool mSwPrevFire = false;
  // Level 42 (bot_finale.cpp): roll-ins, plates, monitors, the glass cover; Lance and the Encore Engine.
  bool finale(const World& world, Input& in);
  Input fightFinale(const World& world);
  Input mFnPrev;
  Input breakout(const World& world);
  bool mPenPrev = false, mTypePrevJump = false;
  int mStrikeWait = 0;
  bool mWarePrevFire = false;
  Input fightTitan(const World& world);
  Input copycat(const World& world);
  int mTowerTurns = -1;
  Input mTowerPrev;
  Input fightLizard(const World& world);
  Input fightBuddy(const World& world); // bot_toon.cpp
  Input inkwell(const World& world);
  Input fightCard(const World& world); // bot_signal.cpp
  Input wraparound(const World& world);
  std::deque<Input> mInkQueue;
  Input mInkPrev;
  Input topDown(const World& world);
  std::deque<Input> mTopQueue;
  Input mTopPrev;
  std::deque<Input> mGlideQueue;
  Input mKitchenPrev;
  std::deque<Input> mTurnQueue;
  Input mTurnPrev;
  // Air Hockey: stand still, face the pucks, crouch and shoot.
  Input hockey(const World& world);
  // Gun Gravity: try shots (each turns gravity) then walks and jumps in
  // copies of the world, and take the one that gets a gem or ends nearest one.
  Input gunGravity(const World& world);
  // Reactor Core: hold up at a valve, raise the Bracer as a ring comes, or
  // wait in a booth for it; false when the planner should drive.
  bool reactor(const World& world, Input& in);
  // Dry Gulch (bot_west.cpp): back off from a barrel about to blow, wait out
  // a burning fuse, fight a Duelist or a Tumble Mine close by; false when
  // the planner should drive. High Noon: fire on the second ring.
  bool west(const World& world, Input& in);
  Input highNoon(const World& world);
  bool manor(const World& world, Input& in);
  Input split(const World& world);
  // Idol Mines: climb into a cart that is going your way and ride it,
  // hopping the gaps and ducking under what hangs low.
  Input ride(const World& world);
  int boardable(const World& world) const;
  // Vehicles marked bot=1 (bot_drive.cpp): walk to one, climb in, drive it
  // to its drop point or the exit, climb out.
  int botVehicle(const World& world) const;
  Input approach(const World& world, int index);
  Input drive(const World& world);
  void setTakeBonus(bool take) { mPlanner.setTakeBonus(take); }

private:
  void buildVehicleField(const World& world, int index);
  int vehicleDistance(const World& world) const;

  Planner mPlanner;
  std::deque<Input> mFightQueue;
  std::deque<Input> mWestQueue;
  Input mWestPrev;
  std::deque<Input> mSplitQueue;
  Input mManorPrev;
  std::deque<Input> mTrapQueue;
  std::deque<Input> mPinQueue;
  std::deque<Input> mLavaQueue;
  Input mLavaPrev;
  std::deque<Input> mSurfQueue;
  std::deque<Input> mDriftQueue;
  Input mDriftPrev;
  std::deque<Input> mOrbitQueue;
  Input mOrbitPrev;
  std::deque<Input> mGunQueue;
  Input mGunPrev;
  Input mSurfPrev;
  std::deque<Input> mRideQueue;
  bool mRiding = false;
  bool mFighting = false;
  Input mFightPrev;
  int mJumpHold = 0;
  int mJumpRelease = 0;
  int mStuck = 0;
  int mLastX = -1;
  int mJetpackUntilX = -1;
  bool mFiredLast = false;
  // Driving.
  std::vector<int> mVehDist; // distance field over the vehicle's bottom-left cell
  int mVehW = 0, mVehH = 0;
  int mVehField = -1;           // the vehicle mVehDist is for
  std::vector<char> mVehDone;   // vehicles already driven (or given up on)
  std::deque<Input> mDriveQueue;
  Input mDrivePrev;
  int mDriveBest = 0, mDriveStall = 0;
  int mApproachBest = 1 << 30, mApproachStall = 0;
  bool mDriving = false;
  bool mSeekFlamer = false; // stuck at a jetpack wall: go back for the flamethrower
};

} // namespace gr
