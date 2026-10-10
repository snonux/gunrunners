// The bot at the wheel (world_vehicle.cpp). It only takes vehicles a level
// marks `bot=1`: it walks over to one, climbs in, drives it to the vehicle's
// `drop=` point (or the exit) and climbs out. Driving is a short search over
// copies of the world, guided by a distance field over where the vehicle
// fits, so one routine drives all six of them.

#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace gr
{

namespace
{

constexpr int kFar = std::numeric_limits<int>::max() / 4;
constexpr int kMacroFrames = 3;

PlayerInput simInput(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump = {in.jump, in.jump && !prev.jump};
  p.fire = {in.fire, in.fire && !prev.fire};
  p.use = {in.use, in.use && !prev.use};
  return p;
}

Input macroInput(int dx, int dy, bool jump)
{
  Input in;
  in.left = dx < 0;
  in.right = dx > 0;
  in.up = dy < 0;
  in.down = dy > 0;
  in.jump = jump;
  in.fire = true; // the guns cool down on their own
  return in;
}

bool flies(VehicleKind k)
{
  return k == VehicleKind::Heli || k == VehicleKind::Ship || k == VehicleKind::Sub;
}

} // namespace

int Bot::botVehicle(const World& world) const
{
  const auto& p = world.player();
  if (p.vehicle >= 0 || p.cart >= 0 || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return -1;
  const auto& vs = world.vehicles();
  int best = -1, bestD = 1 << 30;
  for (std::size_t i = 0; i < vs.size(); ++i)
  {
    const Vehicle& v = vs[i];
    if (!v.bot || v.occupied || v.wreck > 0 || (i < mVehDone.size() && mVehDone[i]))
      continue;
    // Close by and on our floor (a submarine: anywhere in its water).
    const int dx = std::abs(v.x + v.w / 2 - (p.x + 1));
    const int dy = std::abs(v.y - p.y);
    // A Bounder is how a level 48 runner gets on at all: walk a long way to one.
    const int reach = v.kind == VehicleKind::Bounder ? 96 : 32;
    const bool near = v.kind == VehicleKind::Sub ? (dx <= 40 && dy <= 24) : (dx <= reach && dy <= 3);
    if (!near)
      continue;
    if (dx + dy < bestD)
    {
      bestD = dx + dy;
      best = int(i);
    }
  }
  return best;
}

Input Bot::approach(const World& world, int index)
{
  const auto& p = world.player();
  const Vehicle& v = world.vehicles()[std::size_t(index)];
  Input in;
  if (world.vehicleInReach() == index && (p.state == PlayerState::OnGround || p.state == PlayerState::Swim))
  {
    in.use = !mDrivePrev.use;
    mDrivePrev = in;
    return in;
  }
  const int cx = v.x + v.w / 2 - 1;
  in.left = p.x > cx;
  in.right = p.x < cx;
  // A rock in the way: jump it (and hold the jump on the way up); an alien
  // in the way: shoot it.
  const int dir = in.right ? 1 : (in.left ? -1 : 0);
  const auto& map = world.map();
  const int aheadX = dir > 0 ? p.x + Player::kWidth : p.x - 1;
  const bool gap = dir != 0 && !map.solidTop(aheadX, p.y + 1) && !map.solidTop(aheadX + dir, p.y + 1);
  if (p.state == PlayerState::OnGround && dir != 0 &&
      (gap || (dir > 0 ? map.touchingRightWall(p.box()) : map.touchingLeftWall(p.box()))))
    in.jump = !mDrivePrev.jump;
  else if (p.state == PlayerState::Jumping)
    in.jump = mDrivePrev.jump;
  if (dir != 0)
    for (const auto& e : world.enemies())
    {
      const int ahead = dir > 0 ? e.x - (p.x + 2) : p.x - (e.x + e.w - 1);
      if (e.alive && e.active && !e.hidden && ahead >= 0 && ahead <= 10 && std::abs(e.y - p.y) <= 3)
      {
        in.fire = !mDrivePrev.fire;
        break;
      }
    }
  if (p.state == PlayerState::Swim)
  {
    in.up = p.y > v.y;
    in.down = p.y < v.y - 2;
  }
  // Given up on it if we stop getting closer.
  const int d = std::abs(p.x - cx) + std::abs(p.y - v.y);
  if (d < mApproachBest)
  {
    mApproachBest = d;
    mApproachStall = 0;
  }
  else if (++mApproachStall > 45)
  {
    if (mVehDone.size() < world.vehicles().size())
      mVehDone.resize(world.vehicles().size(), 0);
    mVehDone[std::size_t(index)] = 1;
    mApproachBest = kFar;
    mApproachStall = 0;
  }
  mDrivePrev = in;
  return in;
}

void Bot::buildVehicleField(const World& world, int index)
{
  const Vehicle& v = world.vehicles()[std::size_t(index)];
  const auto& map = world.map();
  const int W = map.width(), H = map.height();
  mVehW = W;
  mVehH = H;
  mVehField = index;
  // Cells a vehicle can't be in: solid ones, unless a wall its guns break.
  std::vector<int> blocked(std::size_t(W * H), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      blocked[std::size_t(y * W + x)] = map.solid(x, y) ? 1 : 0;
  for (const auto& b : world.breakables())
  {
    if (b.broken || b.by == 3 || b.by == 4)
      continue;
    for (int y = b.y0 * kCellsPerTile; y < (b.y1 + 1) * kCellsPerTile && y < H; ++y)
      for (int x = b.x0 * kCellsPerTile; x < (b.x1 + 1) * kCellsPerTile && x < W; ++x)
        blocked[std::size_t(y * W + x)] = 0;
  }
  // Summed area table: any box's blocked count in O(1).
  std::vector<int> sum(std::size_t((W + 1) * (H + 1)), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      sum[std::size_t((y + 1) * (W + 1) + x + 1)] = blocked[std::size_t(y * W + x)] +
        sum[std::size_t(y * (W + 1) + x + 1)] + sum[std::size_t((y + 1) * (W + 1) + x)] -
        sum[std::size_t(y * (W + 1) + x)];
  auto fits = [&](int x, int y) {
    const int x0 = x, y0 = y - v.h + 1, x1 = x + v.w, y1 = y + 1;
    if (x0 < 0 || y0 < 0 || x1 > W || y1 > H)
      return false;
    const int n = sum[std::size_t(y1 * (W + 1) + x1)] - sum[std::size_t(y0 * (W + 1) + x1)] -
      sum[std::size_t(y1 * (W + 1) + x0)] + sum[std::size_t(y0 * (W + 1) + x0)];
    if (n > 0)
      return false;
    if (v.kind == VehicleKind::Sub)
    {
      const int cx = x + v.w / 2;
      const int surface = world.seaSurface(cx);
      return surface >= 0 && world.inSea(cx, y) && world.inSea(x + 1, y) && world.inSea(x + v.w - 2, y) &&
        y0 >= surface - 1;
    }
    return true;
  };

  // Where it is going: its drop point, else the exit.
  int tx = v.dropX >= 0 ? v.dropX : world.level().exitTx * kCellsPerTile;
  int ty = v.dropX >= 0 ? v.dropY : (world.level().exitTy + 1) * kCellsPerTile - 1;
  mVehDist.assign(std::size_t(W * H), kFar);
  std::vector<int> queue;
  for (int tol = 1; tol <= 8 && queue.empty(); tol *= 2)
    for (int y = std::max(0, ty - tol); y <= std::min(H - 1, ty + tol); ++y)
      for (int x = 0; x < W; ++x)
        if (std::abs(x + v.w / 2 - 1 - tx) <= tol && fits(x, y))
        {
          mVehDist[std::size_t(y * W + x)] = 0;
          queue.push_back(y * W + x);
        }
  // Breadth first over where it fits.
  for (std::size_t head = 0; head < queue.size(); ++head)
  {
    const int at = queue[head];
    const int x = at % W, y = at / W;
    const int d = mVehDist[std::size_t(at)];
    // Four ways only: with diagonals a climb through the air looks as
    // short as driving up to the foot of it.
    static const int kSteps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& st : kSteps)
    {
      const int nx = x + st[0], ny = y + st[1];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H || mVehDist[std::size_t(ny * W + nx)] != kFar || !fits(nx, ny))
        continue;
      mVehDist[std::size_t(ny * W + nx)] = d + 1;
      queue.push_back(ny * W + nx);
    }
  }
}

int Bot::vehicleDistance(const World& world) const
{
  const Vehicle* v = world.riding();
  if (!v)
    return kFar;
  if (v->x < 0 || v->y < 0 || v->x >= mVehW || v->y >= mVehH)
    return kFar;
  return mVehDist[std::size_t(v->y * mVehW + v->x)];
}

Input Bot::drive(const World& world)
{
  const Vehicle* v = world.riding();
  const int index = int(v - world.vehicles().data());
  if (mVehDone.size() < world.vehicles().size())
    mVehDone.resize(world.vehicles().size(), 0);
  if (mVehField != index)
  {
    buildVehicleField(world, index);
    mDriveQueue.clear();
    mDriveBest = kFar;
    mDriveStall = 0;
  }
  const int here = vehicleDistance(world);
  if (here < mDriveBest)
  {
    mDriveBest = here;
    mDriveStall = 0;
  }
  else
    ++mDriveStall;
  // There (or stuck for good): climb out.
  const bool dropHere = v->dropX >= 0 && here <= 2 && world.canLeaveVehicle();
  // (In open space there is no getting out: keep flying.)
  if (mDriveStall > 240 && v->pilot && !world.canLeaveVehicle())
    mDriveStall = 0;
  if (dropHere || mDriveStall > 240)
  {
    mVehDone[std::size_t(index)] = 1;
    mDriveQueue.clear();
    Input in;
    in.use = !mDrivePrev.use;
    mDrivePrev = in;
    return in;
  }
  if (mDriveQueue.empty())
  {
    // The moves this vehicle has: eight ways for the fliers, along the
    // ground and jumps (aiming up, for the tank) for the rest.
    struct Macro
    {
      int dx, dy;
      bool jump;
    };
    std::vector<Macro> macros;
    if (flies(v->kind))
    {
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
          macros.push_back({dx, dy, false});
    }
    else
    {
      macros = {{0, 0, false}, {1, 0, false}, {-1, 0, false}};
      if (v->kind == VehicleKind::Tank)
        macros.insert(macros.end(), {{1, -1, false}, {-1, -1, false}});
      else
        macros.insert(macros.end(), {{0, 0, true}, {1, 0, true}, {-1, 0, true}});
    }
    const int hp = v->hp;
    auto score = [&](const World& sim) {
      if (sim.state() != WorldState::Playing)
        return sim.player().state == PlayerState::Teleporting ? -kFar : kFar;
      const Vehicle* sv = sim.riding();
      if (!sv)
        return kFar;
      const int d = vehicleDistance(sim);
      return (d >= kFar ? 100000 : d * 10) + (hp - sv->hp) * 60;
    };
    auto runMacro = [&](World& sim, const Macro& m, Input& prev) {
      for (int f = 0; f < kMacroFrames; ++f)
      {
        const Input in = macroInput(m.dx, m.dy, m.jump);
        sim.update(simInput(in, prev));
        prev = in;
        if (sim.state() != WorldState::Playing || !sim.riding())
          break;
      }
    };
    int bestScore = kFar;
    std::size_t best = 0;
    for (std::size_t a = 0; a < macros.size(); ++a)
    {
      auto first = world.cloneForSim();
      Input prev = mDrivePrev;
      runMacro(*first, macros[a], prev);
      int s = score(*first);
      if (first->state() == WorldState::Playing && first->riding())
        for (std::size_t b = 0; b < macros.size(); ++b)
        {
          auto second = first->cloneForSim();
          Input prev2 = prev;
          runMacro(*second, macros[b], prev2);
          s = std::min(s, score(*second) + 1);
        }
      if (std::getenv("GR_DRIVE_DEBUG"))
        std::fprintf(stderr, "drive v=%d,%d here=%d macro %zu (%d,%d,%d) score %d\n", v->x, v->y, here, a, macros[a].dx, macros[a].dy, int(macros[a].jump), s);
      if (s < bestScore)
      {
        bestScore = s;
        best = a;
      }
    }
    for (int f = 0; f < kMacroFrames; ++f)
      mDriveQueue.push_back(macroInput(macros[best].dx, macros[best].dy, macros[best].jump));
  }
  Input in = mDriveQueue.front();
  mDriveQueue.pop_front();
  mDrivePrev = in;
  return in;
}

} // namespace gr
