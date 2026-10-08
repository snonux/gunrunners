#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <cstdlib>

namespace gr
{

namespace
{

int jumpHeight(const World& w)
{
  // Turbo and the virus change the jump arc (see World::jumpArc).
  if (w.player().turbo > 0)
    return 11;
  if (w.player().virus > 0)
    return 5;
  int h = 0;
  for (int v : w.character().jumpArc)
    h += v;
  return h;
}

// How many cells of wall rise in column x, counting up from row bottomY.
int wallHeight(const CollisionMap& map, int x, int bottomY)
{
  int h = 0;
  while (h < 40 && map.solid(x, bottomY - h))
    ++h;
  return h;
}

} // namespace

Input Bot::menu(int cursor, int target, int ticksInMenu) const
{
  Input in;
  // Browse through all three runners first, then settle on the target.
  if (ticksInMenu > 40 && ticksInMenu % 36 < 4)
  {
    const int wanted = ticksInMenu < 40 + 36 * 2 ? 2 : target;
    if (cursor < wanted)
      in.right = true;
    else if (cursor > wanted)
      in.left = true;
  }
  if (ticksInMenu > 40 + 36 * 4 && cursor == target && ticksInMenu % 10 == 0)
    in.confirm = true;
  return in;
}

Input Bot::play(const World& world)
{
  // Campaign levels (with a header) get the planner; the PoC level keeps
  // its rule-based bot, which knows about the flamer's jetpack.
  if (world.level().episode > 0 || !world.level().rules.empty())
    return mPlanner.next(world);
  return playRules(world);
}

Input Bot::playRules(const World& world)
{
  const auto& p = world.player();
  const auto& map = world.map();
  const CellBox b = p.box();
  Input in;

  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return in;

  const bool rapid = p.rapidFire > 0 || p.weapon == Weapon::Flame;
  auto fire = [&]() {
    // Without rapid fire, every shot needs a fresh press.
    in.fire = rapid || !mFiredLast;
  };
  auto startJump = [&](int frames) {
    if (mJumpHold == 0 && mJumpRelease == 0)
      mJumpHold = frames;
  };
  if (mJumpRelease > 0)
    --mJumpRelease;

  // --- Climbing and hanging -------------------------------------------------
  if (p.state == PlayerState::Ladder)
  {
    const bool atTop = !map.ladder(b.left() + 1, b.top() - 1);
    if (atTop)
    {
      in.right = true;
      in.jump = true;
    }
    else
    {
      in.up = true;
    }
    mFiredLast = false;
    return in;
  }

  if (p.state == PlayerState::Pipe)
  {
    in.right = true;
    mFiredLast = false;
    return in;
  }

  // --- Jetpack over walls nothing else gets past --------------------------
  if (mJetpackUntilX >= 0)
  {
    if (p.x >= mJetpackUntilX || p.weapon != Weapon::Flame)
    {
      mJetpackUntilX = -1;
    }
    else
    {
      in.down = true;
      in.fire = true;
      in.right = true;
      mFiredLast = true;
      return in;
    }
  }

  // --- Going back for a flamethrower we skipped -----------------------------
  if (mSeekFlamer)
  {
    if (p.weapon == Weapon::Flame)
    {
      mSeekFlamer = false;
    }
    else
    {
      int targetX = -1;
      bool isBox = false;
      for (const auto& it : world.items())
        if (it.kind == ItemKind::Flame)
          targetX = it.x;
      if (targetX < 0)
        for (const auto& box : world.boxes())
          if (box.alive && box.content == ItemKind::Flame)
          {
            targetX = box.x;
            isBox = true;
          }
      if (targetX < 0)
      {
        mSeekFlamer = false; // nothing left to fetch
      }
      else if (p.state == PlayerState::OnGround)
      {
        const int dx = targetX - b.left();
        const bool facingIt = (dx < 0) == (p.facing < 0);
        if (isBox && std::abs(dx) <= 10 && facingIt)
        {
          in.down = true; // boxes sit on the floor: crouch to hit them
          in.fire = !mFiredLast;
          mFiredLast = in.fire;
          return in;
        }
        if (dx < 0)
          in.left = true;
        else if (dx > 0)
          in.right = true;
        mFiredLast = false;
        return in;
      }
    }
  }

  // --- Targets --------------------------------------------------------------
  enum class Aim
  {
    None,
    Stand,
    Crouch,
    Up,
  } aim = Aim::None;
  bool holdPosition = false;
  const int gunRow = b.bottom() - 2;
  const int crouchRow = b.bottom() - 1;

  for (const auto& e : world.enemies())
  {
    if (!e.alive || !e.active)
      continue;
    const CellBox eb = e.box();
    const int dx = eb.left() - b.right();
    if (e.kind == EnemyKind::Flyer && eb.bottom() < b.top() && std::abs((eb.x + 1) - (b.x + 2)) <= 1)
    {
      aim = Aim::Up;
      holdPosition = true;
      break;
    }
    if (dx < 0 || dx > 18)
      continue;
    if (eb.top() <= gunRow && eb.bottom() >= gunRow)
    {
      aim = Aim::Stand;
      holdPosition = holdPosition || dx < 9;
    }
    else if (eb.top() <= crouchRow && eb.bottom() >= crouchRow && p.state == PlayerState::OnGround)
    {
      aim = Aim::Crouch;
      holdPosition = true;
    }
  }
  if (aim == Aim::None && p.state == PlayerState::OnGround)
  {
    for (const auto& box : world.boxes())
    {
      if (!box.alive)
        continue;
      const CellBox bb = box.box();
      const int dx = bb.left() - b.right();
      if (dx >= 0 && dx < 12 && bb.bottom() == b.bottom())
      {
        // Crouching with the flamer would start the jetpack; its fat
        // flames reach the floor anyway.
        aim = p.weapon == Weapon::Flame ? Aim::Stand : Aim::Crouch;
        holdPosition = true;
        break;
      }
    }
  }

  if (aim != Aim::None && p.state == PlayerState::OnGround && mJumpHold == 0)
  {
    if (aim == Aim::Crouch)
      in.down = true;
    else if (aim == Aim::Up)
      in.up = true;
    if (!holdPosition)
      in.right = true;
    fire();
    mFiredLast = in.fire && !rapid;
    mStuck = 0;
    mLastX = p.x;
    return in;
  }
  mFiredLast = false;

  // --- Moving right ---------------------------------------------------------
  in.right = true;
  if (p.facing < 0)
  {
    mLastX = p.x;
    return in;
  }

  if (p.state == PlayerState::OnGround)
  {
    const int ahead = b.right() + 1;
    const int wall = wallHeight(map, ahead, b.bottom());
    // A gap is only worth jumping if there is no safe floor a short drop
    // below; otherwise just walk off the edge.
    auto safeDrop = [&](int x) {
      for (int y = b.bottom() + 1; y <= b.bottom() + 10; ++y)
        if (map.solidTop(x, y))
          return !map.hazard(x, y - 1);
      return false;
    };
    const bool gapAhead = !map.solidTop(ahead, b.bottom() + 1) && !map.solidTop(ahead + 1, b.bottom() + 1) &&
      !(safeDrop(ahead + 1) && safeDrop(ahead + 2));
    const bool hazardAhead = map.hazard(ahead, b.bottom()) || map.hazard(ahead + 1, b.bottom()) ||
      map.hazard(ahead, b.bottom() + 1) || map.hazard(ahead + 1, b.bottom() + 1);

    bool ladderHere = false;
    for (int x = b.left(); x <= b.right(); ++x)
      ladderHere = ladderHere || map.ladder(x, b.top());

    if (wall > jumpHeight(world))
    {
      if (p.weapon != Weapon::Flame && !ladderHere && !map.forceField(ahead, b.bottom()))
      {
        mSeekFlamer = true;
      }
      else if (ladderHere)
      {
        in.right = false;
        in.up = true;
      }
      else if (p.weapon == Weapon::Flame && !map.forceField(ahead, b.bottom()))
      {
        // Fly up until we are above the wall, then let go.
        mJetpackUntilX = ahead;
        in.down = true;
        in.fire = true;
      }
    }
    else if (wall > 0 || gapAhead || hazardAhead)
    {
      startJump(7);
    }

    if (p.x == mLastX)
      ++mStuck;
    else
      mStuck = 0;
    if (mStuck > 8)
    {
      startJump(7);
      mStuck = 0;
    }
  }

  if (mJumpHold > 0)
  {
    in.jump = true;
    if (--mJumpHold == 0)
      mJumpRelease = 1; // let go so the next press registers
  }
  mLastX = p.x;
  return in;
}

} // namespace gr
