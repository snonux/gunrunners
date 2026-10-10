// The bot in Level 24: the pantry detour to the bonus door, and the
// Turntable bonus.
//
// The pantry is shut by a flap only a belt pushes you through: with the
// bonus wanted, the bot walks onto that belt, goes to its bell's `stand`
// block, shoots the bell, and rides the reversed belt in; inside, the
// planner walks to the door.
//
// In the Turntable bonus A greedy step runs along whatever
// is the floor to the gem that is easiest to reach on screen, hopping for
// the ones a little above; every few frames the bot tries each way of
// starting off (left, right or stay, with a jump or without) in a copy of
// the world, carries on greedily for a second and keeps the start that
// gathers the most gems.

#include "frontend/bot.hpp"
#include "game/world.hpp"

#include <cmath>

namespace gr
{

namespace
{

PlayerInput turnInput(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump.pressed = in.jump;
  p.jump.triggered = in.jump && !prev.jump;
  p.fire.pressed = in.fire;
  p.fire.triggered = in.fire && !prev.fire;
  return p;
}

Input greedyTurn(const World& world, bool prevJump)
{
  const auto& t = world.turntable();
  Input in;
  const float c = std::cos(t.ang), s = std::sin(t.ang);
  float best = 1e9f, tx = t.sx, ty = t.sy;
  for (const auto& g : t.gems)
  {
    if (g.gone)
      continue;
    const float gx = t.cx + c * (g.x - t.cx) - s * (g.y - t.cy);
    const float gy = t.cy + s * (g.x - t.cx) + c * (g.y - t.cy);
    const float dx = gx - t.sx, dy = gy - t.sy;
    // Below or level is cheap; above costs a jump (out of reach is a last resort).
    const float cost = std::fabs(dx) + (dy < -1.0f ? -dy * 3.0f : std::fabs(dy) * 0.5f) + (dy < -8.0f ? 60.0f : 0.0f);
    if (cost < best)
    {
      best = cost;
      tx = gx;
      ty = gy;
    }
  }
  const float dx = tx - t.sx, dy = ty - t.sy;
  in.right = dx > 0.8f;
  in.left = dx < -0.8f;
  in.jump = t.ground && dy < -2.5f && std::fabs(dx) < 6.0f && !prevJump;
  return in;
}

constexpr int kTurnStep = 6;   // frames each start is held
constexpr int kTurnAhead = 24; // greedy frames after it

} // namespace

bool Bot::kitchen(const World& world, Input& in)
{
  const auto& k = world.kitchen();
  const auto& p = world.player();
  if (p.state == PlayerState::Dying)
    return false;
  const Prop* door = nullptr;
  for (const auto& pr : world.props())
    if (pr.kind == PropKind::BonusDoor && !pr.dormant)
      door = &pr;
  const ChefBell* bell = nullptr;
  for (const auto& b : k.bells)
    if (b.bot == 2 && b.standX >= 0)
      bell = &b;
  if (!door || !bell)
    return false;
  // The belt the stand is on, and the flap at its end.
  const Belt* belt = nullptr;
  for (const auto& b : k.belts)
    if (b.station == bell->station && !b.hang && bell->standX >= b.x0 && bell->standX <= b.x1)
      belt = &b;
  if (!belt)
    return false;
  const Flap* flap = nullptr;
  for (const auto& f : k.flaps)
    if ((f.x == belt->x0 - 1 || f.x == belt->x1 + 1) && f.y + f.h == belt->y)
      flap = &f;
  if (!flap)
    return false;
  const int toFlap = flap->x < belt->x0 ? -1 : 1;
  const int bx = (p.x + 1) / kCellsPerTile;
  const int fx = flap->x * kCellsPerTile;
  const int feet = belt->y * kCellsPerTile - 1;
  if (!mPlanner.wantsBonus() || door->used)
  {
    // On the way out with the belt still running into the flap: from the
    // ledge over the flap, jump well out onto the belt and walk on against
    // it (dropping at its end would push the runner straight back in).
    if (belt->dir != toFlap)
      return false;
    const int away = -toFlap;
    const bool ledge = p.state == PlayerState::OnGround && p.y < feet - 2 && std::abs(bx - flap->x) <= 3;
    const bool nearEnd = p.y >= feet - 2 && p.y <= feet && std::abs(bx - flap->x) <= 4 &&
      (toFlap < 0 ? bx >= flap->x : bx <= flap->x);
    if (!ledge && !nearEnd)
      return false;
    in = Input{};
    in.right = away > 0;
    in.left = away < 0;
    in.jump = ledge && (away > 0 ? p.x + Player::kWidth >= fx + kCellsPerTile - 1 : p.x <= fx) && !mKitchenPrev.jump;
    mKitchenPrev = in;
    return true;
  }
  // Past the flap: walk to the door and tune in (the planner would go for
  // other things first and leave by the ladder).
  if ((toFlap < 0 && p.x + Player::kWidth <= fx) || (toFlap > 0 && p.x >= fx + kCellsPerTile))
  {
    if (p.y != feet || p.state != PlayerState::OnGround)
      return false;
    in = Input{};
    const int dx = (door->x + door->w / 2) - (p.x + 1);
    if (std::abs(dx) <= 1)
      in.up = !mKitchenPrev.up;
    else
    {
      in.right = dx > 0;
      in.left = dx < 0;
    }
    mKitchenPrev = in;
    return true;
  }
  if (bx > belt->x1 + 3)
    return false;
  in = Input{};
  const bool onBelt = p.state == PlayerState::OnGround && p.y == feet && bx >= belt->x0 && bx <= belt->x1;
  if (belt->dir == toFlap || bell->ding > 0)
  {
    // Rung: ride it in.
    if (belt->dir == toFlap && (onBelt || std::abs(bx - flap->x) <= 1))
    {
      in.left = toFlap < 0;
      in.right = toFlap > 0;
    }
    mKitchenPrev = in;
    return true;
  }
  if (p.state != PlayerState::OnGround)
  {
    mKitchenPrev = in;
    return true; // let the jump or fall finish
  }
  if (!onBelt)
  {
    // Onto the belt: off whichever edge is toward it.
    in.right = bx < belt->x0 + 1;
    in.left = !in.right;
    mKitchenPrev = in;
    return true;
  }
  const int faceBell = bell->x > bx ? 1 : -1;
  if (bx < bell->standX - 1 || bx > bell->standX + 1 || p.facing != faceBell)
  {
    const int want = bx < bell->standX - 1 ? 1 : (bx > bell->standX + 1 ? -1 : faceBell);
    in.right = want > 0;
    in.left = want < 0;
    mKitchenPrev = in;
    return true;
  }
  // The Pepper Grinder is held (it spins up); anything else is tapped.
  const bool grinder = p.weapon == Weapon::Proto && p.proto == int(ProtoId::PepperGrinder);
  in.fire = grinder || !mKitchenPrev.fire;
  mKitchenPrev = in;
  return true;
}

Input Bot::turntable(const World& world)
{
  if (!mTurnQueue.empty())
  {
    mTurnPrev = mTurnQueue.front();
    mTurnQueue.pop_front();
    return mTurnPrev;
  }
  const int gems0 = world.stats().gems;
  double bestScore = -1e18;
  Input bestStart;
  for (int dir = -1; dir <= 1; ++dir)
    for (int jump = 0; jump <= 1; ++jump)
    {
      Input start;
      start.left = dir < 0;
      start.right = dir > 0;
      auto sim = world.cloneForSim();
      Input prev = mTurnPrev;
      double score = 0.0;
      for (int f = 0; f < kTurnStep + kTurnAhead && sim->state() == WorldState::Playing; ++f)
      {
        Input in = f < kTurnStep ? start : greedyTurn(*sim, prev.jump);
        if (f < kTurnStep)
          in.jump = jump == 1 && f == 0;
        sim->update(turnInput(in, prev));
        prev = in;
        // Sooner is better.
        score += double(sim->stats().gems - gems0) * (1000.0 - f);
      }
      score -= double(sim->turntable().lost) * 300.0;
      if (score > bestScore)
      {
        bestScore = score;
        bestStart = start;
        bestStart.jump = jump == 1;
      }
    }
  for (int f = 0; f < kTurnStep; ++f)
  {
    Input in = bestStart;
    in.jump = bestStart.jump && f == 0;
    mTurnQueue.push_back(in);
  }
  mTurnPrev = mTurnQueue.front();
  mTurnQueue.pop_front();
  return mTurnPrev;
}

} // namespace gr
