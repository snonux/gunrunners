#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>

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
  if (world.flight())
    return fly(world);
  // Fight from the deck; anywhere below it (fallen down the mast shaft)
  // the planner climbs back up first.
  if (world.bossFight() && world.player().y <= world.boss().deckY + 1)
  {
    mFighting = true;
    return fightBoss(world);
  }
  if (mFighting)
  {
    mFighting = false;
    mFightQueue.clear();
    mPlanner.reset();
  }
  // Campaign levels (with a header) get the planner; the PoC level keeps
  // its rule-based bot, which knows about the flamer's jetpack.
  if (world.level().episode > 0 || !world.level().rules.empty())
    return mPlanner.next(world);
  return playRules(world);
}

namespace
{

// The fighter's moves: four frames each; fire is a tap on the first frame.
struct Move
{
  bool left, right, up, down, jump, fire;
};
const Move kMoves[] = {
  {false, false, false, false, false, false}, // wait
  {true, false, false, false, false, false},  // left
  {false, true, false, false, false, false},  // right
  {false, false, false, false, true, false},  // jump
  {true, false, false, false, true, false},   // jump left
  {false, true, false, false, true, false},   // jump right
  {false, false, false, true, false, false},  // crouch
  {false, false, true, false, false, true},   // shoot up
  {false, false, false, false, false, true},  // shoot ahead
  {false, false, false, true, false, true},   // crouch and shoot
  {true, false, true, false, false, true},    // shoot up stepping left
  {false, true, true, false, false, true},    // shoot up stepping right
};

Input moveInput(const Move& m, int f)
{
  Input in;
  in.left = m.left;
  in.right = m.right;
  in.up = m.up;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fire && f == 0;
  return in;
}

PlayerInput asPlayerInput(const Input& in, const Input& prev)
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

// What is left to shoot: the gunship's HP plus its searchlight.
int fightHp(const World& w)
{
  return w.bossHp() + (w.boss().on ? w.boss().hp[std::size_t(BossPart::Light)] : 0);
}

// Where the runner wants to be: under a weak spot that can be hit now,
// next to a health box when hurt, else mid-deck.
int fightTargetX(const World& w)
{
  const auto& p = w.player();
  const auto& b = w.boss();
  if (p.hp < p.maxHp)
    for (const auto& it : w.items())
      if (!it.taken && it.kind == ItemKind::Health && b.arena.contains(it.x, it.y))
        return it.x;
  // The searchlight first: without it the salvos only come slow and random.
  static const BossPart kOrder[] = {BossPart::NosePod, BossPart::TailPod, BossPart::Light, BossPart::Hatch,
    BossPart::Rotor};
  for (BossPart part : kOrder)
  {
    CellBox box;
    if (w.bossTarget(part, box))
      return box.x + box.w / 2 - 1;
  }
  // Nothing open right now: wait under the weak spot this phase opens next.
  if (b.phase == BossPhase::Ram)
    return b.arena.x + 8; // where it turns with its tail rotor out
  const BossPart next[] = {BossPart::NosePod, BossPart::TailPod};
  if (b.phase == BossPhase::Strafe)
    for (BossPart part : next)
      if (b.hp[std::size_t(part)] > 0)
      {
        CellBox box;
        w.bossTarget(part, box);
        return box.x + box.w / 2 - 1;
      }
  return b.x + Boss::kW / 2 - 1; // under the belly hatch

}

} // namespace

Input Bot::fightBoss(const World& world)
{
  if (mFightQueue.empty())
  {
    // Try every move, each followed by a few simple ways to carry on, and
    // keep the move whose best follow-up hurts the gunship most while
    // keeping the runner's hearts.
    constexpr int kAhead = 48;
    const int targetX = fightTargetX(world);
    const int hp0 = world.player().hp, boss0 = fightHp(world);
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < int(sizeof(kMoves) / sizeof(kMoves[0])); ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = moveInput(kMoves[m], f);
        first.update(asPlayerInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      // Damage counts for more the sooner it lands, so the fighter does not
      // put off a shot that every follow-up would take anyway.
      const long firstScore = 200L * (boss0 - fightHp(first));
      const int kills0 = world.stats().kills;
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 7 && !dead; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int bossHp = fightHp(sim);
        bool down = false;
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const int dx = targetX - sim.player().x;
          switch (follow)
          {
            case 0: // stand and shoot up
              in.up = true;
              in.fire = f % 2 == 0;
              break;
            case 1: // crouch
              in.down = true;
              break;
            case 2: // hop
              in.jump = sim.player().state == PlayerState::OnGround && f % 8 < 4;
              break;
            case 3: // walk to the spot, shooting up
              in.left = dx < -1;
              in.right = dx > 1;
              in.up = true;
              in.fire = f % 2 == 0;
              break;
            case 6: // turn on the nearest trooper and shoot it
            {
              const Enemy* near = nullptr;
              for (const auto& e : sim.enemies())
                if (e.alive && e.active && std::abs(e.y - sim.player().y) < 4 &&
                    (!near || std::abs(e.x - sim.player().x) < std::abs(near->x - sim.player().x)))
                  near = &e;
              if (near)
              {
                const int dir = near->x > sim.player().x ? 1 : -1;
                in.left = dir < 0 && sim.player().facing > 0;
                in.right = dir > 0 && sim.player().facing < 0;
              }
              in.fire = f % 2 == 0;
              break;
            }
            case 4: // get away to the left or the right (a rocket coming in)
            case 5:
              in.left = follow == 4 && f < 16;
              in.right = follow == 5 && f < 16;
              in.up = f >= 16;
              in.fire = f >= 16 && f % 2 == 0;
              break;
          }
          sim.update(asPlayerInput(in, fp));
          fp = in;
          score += long(bossHp - fightHp(sim)) * (180 - 3 * f);
          bossHp = fightHp(sim);
          if (!down && (sim.boss().phase == BossPhase::Falling || sim.boss().phase == BossPhase::Done))
          {
            down = true;
            score += 100000L - 1000L * f;
          }
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        // A heart costs more the fewer are left.
        const long lost = std::max(0, hp0 - sp.hp);
        score += -lost * (400L + 1200L / std::max(1, sp.hp)) + 40L * (sim.stats().kills - kills0) -
          2L * std::abs(sp.x - targetX);
        if (sp.state == PlayerState::Dying)
          score -= 1000000L;
        moveScore = std::max(moveScore, score);
      }
      if (dead)
        continue;
      if (moveScore > bestScore)
      {
        bestScore = moveScore;
        best = m;
      }
    }
    static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
    if (debug)
      std::fprintf(stderr, "fight f%d phase %d hp %d at %d,%d runner hp %d target %d -> move %d (%ld)\n",
        world.stats().frames, int(world.boss().phase), world.bossHp(), world.player().x, world.player().y,
        world.player().hp, targetX, best, bestScore);
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(moveInput(kMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

Input Bot::fly(const World& world)
{
  // Trucks first (they are worth the most): fly ahead of one and lob
  // rockets so they land on it. Otherwise the nearest runner: level with it,
  // ten cells to the side, facing it, chaingun.
  const auto& p = world.player();
  Input in;
  const Enemy* truck = nullptr;
  const Enemy* runner = nullptr;
  int truckD = 1 << 30, runnerD = 1 << 30;
  for (const auto& e : world.enemies())
  {
    if (!e.alive)
      continue;
    const int d = std::abs(e.x - p.x) + std::abs(e.y - p.y);
    if (e.w >= 8)
    {
      if (d < truckD)
      {
        truckD = d;
        truck = &e;
      }
    }
    else if (d < runnerD)
    {
      runnerD = d;
      runner = &e;
    }
  }
  const int shipC = p.x + 3;
  auto steer = [&](int tx, int ty) {
    in.left = shipC > tx + 1;
    in.right = shipC < tx - 1;
    in.up = p.y > ty;
    in.down = p.y < ty;
  };
  if (truck && (!runner || truckD < runnerD + 40))
  {
    // A rocket falls about 6 frames from the ship's lowest row and drifts 2
    // cells forward; the truck drives a cell a frame meanwhile.
    const int lead = truck->x + truck->w / 2 + truck->dir * 6;
    const int tx = lead - 2 * p.facing;
    steer(tx, 39);
    if (std::abs(shipC - tx) <= 2 && p.y >= 37)
    {
      in.down = true;
      in.fire = true;
    }
    return in;
  }
  if (!runner)
  {
    // Nothing up: cruise over the middle of the skyline.
    steer(world.map().width() / 2, 24);
    return in;
  }
  const int rc = runner->x + runner->w / 2;
  const int ty = std::min(39, runner->y - 2);
  const int side = rc < shipC ? 1 : -1; // stay on this side of it
  steer(rc + side * 10, ty);
  const bool facing = (rc < shipC) == (p.facing < 0);
  if (std::abs(p.y - ty) <= 1 && std::abs(shipC - rc) <= 16)
  {
    if (!facing)
    {
      in.left = side > 0;
      in.right = side < 0;
    }
    else
      in.fire = true;
  }
  return in;
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
