#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

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

bool Bot::reactor(const World& world, Input& in)
{
  const auto& rs = world.reactor();
  const auto& p = world.player();
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return false;
  const int t = world.framesToRing();
  const bool bracer = p.weapon == Weapon::Proto && p.proto == int(ProtoId::DeflectorBracer) && (p.ammo > 0 || rs.raise > 0);
  bool hold = false;
  // A ring on its way and no booth: raise the Bracer (it goes up after
  // kBracerTap frames held) and keep it up till the ring has passed.
  if (bracer && !world.inBooth() && p.turbo == 0 && t <= kBracerTap + 4)
  {
    in.fire = true;
    hold = true;
  }
  // In a booth with nothing to raise: wait there for the ring.
  if (!bracer && world.inBooth() && p.turbo == 0 && t < 75 && t > 0)
    hold = true;
  const int valve = mPlanner.valveToHold(world);
  if (valve >= 0)
  {
    in.up = true;
    hold = true;
  }
  return hold;
}

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
  if (world.trapmaster())
    return trapmaster(world);
  if (world.pinball())
    return pinball(world);
  if (world.floorLava())
    return floorLava(world);
  if (world.surfing() && !world.surf().free)
    return surf(world);
  if (world.recoilOnly())
    return drift(world);
  if (world.orbit().on)
    return orbit(world);
  if (world.cryo().zeroFriction)
    return hockey(world);
  if (world.grav().gun)
    return gunGravity(world);
  if (world.west().noon.on && world.west().noon.phase != DuelPhase::Done)
    return highNoon(world);
  if (world.manor().split.on)
    return split(world);
  // Level 13: the bonus patch shows on the boulder that rolls over the
  // runner sheltering in its alcove and teeters at the chute: wait there.
  if (mPlanner.wantsBonus())
    for (const auto& b : world.boulders())
      if (b.teeter >= 0 && b.state >= Boulder::Dust && b.state <= Boulder::Lip && world.inShelter(b.teeter))
      {
        mPlanner.reset();
        return Input{};
      }
  // Level 14: beside the altar the planner went to, hold up (gems go, or
  // the false altar wakes the bonus entrance).
  const int altar = mPlanner.altarToHold(world);
  if (altar >= 0 && world.besideAltar(altar))
  {
    mPlanner.reset();
    Input in;
    in.up = true;
    return in;
  }
  // Level 20: hold up at a valve till it shuts; sit out the Core Pulse.
  if (world.reactor().on && !world.reactor().stopMotion)
  {
    Input in;
    if (reactor(world, in))
    {
      mPlanner.reset();
      return in;
    }
  }
  // Level 22: barrels about to blow, burning fuses, duels, Tumble Mines.
  if (world.west().on)
  {
    Input in;
    if (west(world, in))
    {
      mPlanner.reset();
      return in;
    }
  }
  // Level 23: pin ghosts over the gallery hole for a bridge.
  if (world.manor().on)
  {
    Input in;
    if (manor(world, in))
    {
      mPlanner.reset();
      return in;
    }
  }
  // Fight from the deck; anywhere below it (fallen down the mast shaft)
  // the planner climbs back up first.
  if (world.bossFight() && world.player().y <= world.boss().deckY + 1)
  {
    mFighting = true;
    return fightBoss(world);
  }
  // Level 14: Kaan-Tolok, once the arena door is shut behind the runner.
  if (world.golemFight())
  {
    mFighting = true;
    return fightGolem(world);
  }
  // Level 21: ZERO, once the arena door is shut behind the runner.
  if (world.zeroFight())
  {
    mFighting = true;
    return fightZero(world);
  }
  // Level 49: the Hive Mother, once the egg chamber's door is shut.
  if (world.motherFight())
  {
    mFighting = true;
    return fightMother(world);
  }
  if (mFighting)
  {
    mFighting = false;
    mFightQueue.clear();
    mPlanner.reset();
  }
  // Level 11: in a cart (or climbing into one going our way), ride.
  if (world.player().cart >= 0 || !mRideQueue.empty() || boardable(world) >= 0)
  {
    mRiding = true;
    return ride(world);
  }
  if (mRiding)
  {
    mRiding = false;
    mPlanner.reset();
  }
  // A vehicle marked for the bot (bot_drive.cpp). A lost life puts them
  // all back where they were: they can be driven again.
  if (world.player().state == PlayerState::Dying)
  {
    mVehDone.clear();
    mVehField = -1; // and back at a checkpoint: how far to go starts over
  }
  const int botV = world.riding() ? -1 : botVehicle(world);
  if (world.riding() || botV >= 0)
  {
    mDriving = true;
    return world.riding() ? drive(world) : approach(world, botV);
  }
  if (mDriving)
  {
    mDriving = false;
    mApproachBest = 1 << 30;
    mApproachStall = 0;
    mDrivePrev = Input{};
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
  p.use.pressed = in.use;
  p.use.triggered = in.use && !prev.use;
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

namespace
{

// Kaan-Tolok's fighter: moves of four frames; fireAt is the frame the
// trigger goes down (-1: not at all), so a jump can shoot from its top.
struct GolemMove
{
  int dir;
  bool down, jump;
  int fireAt;
};
const GolemMove kGolemMoves[] = {
  {0, false, false, -1}, // wait
  {-1, false, false, -1},
  {1, false, false, -1},
  {0, false, true, -1},
  {-1, false, true, -1},
  {1, false, true, -1},
  {0, true, false, -1},  // crouch
  {0, false, false, 0},  // shoot
  {0, false, true, 3},   // jump, shoot from the top
  {0, true, false, 0},   // crouch and shoot
  {-1, false, false, 0}, // turn left and shoot
  {1, false, false, 0},
  {-1, false, true, 3},  // jump left, shoot
  {1, false, true, 3},
};

Input golemInput(const GolemMove& m, int f)
{
  Input in;
  in.left = m.dir < 0;
  in.right = m.dir > 0;
  in.down = m.down;
  in.jump = m.jump;
  in.fire = m.fireAt == f;
  return in;
}

// A shockwave coming at the runner, close enough to jump now.
bool waveNear(const World& w)
{
  const auto& p = w.player();
  for (const auto& wv : w.golem().waves)
  {
    const float d = (float(p.x) + 1.5f - wv.x) * float(-wv.dir);
    if (d > -2.0f && d < 9.0f)
      return true;
  }
  return false;
}

// The nearest of the bouncing heads, or null.
const Golem::Head* nearestHead(const World& w)
{
  const auto& p = w.player();
  const Golem::Head* best = nullptr;
  for (const auto& h : w.golem().heads)
    if (h.alive && (!best || std::abs(h.x + 3.0f - float(p.x)) < std::abs(best->x + 3.0f - float(p.x))))
      best = &h;
  return best;
}

} // namespace

Input Bot::fightGolem(const World& world)
{
  if (mFightQueue.empty())
  {
    constexpr int kAhead = 48;
    const int hp0 = world.player().hp, golem0 = world.golemHp();
    int best = 0;
    long bestScore = std::numeric_limits<long>::min();
    for (int m = 0; m < int(sizeof(kGolemMoves) / sizeof(kGolemMoves[0])); ++m)
    {
      const auto firstPtr = world.cloneForSim();
      World& first = *firstPtr;
      Input prev = mFightPrev;
      bool dead = false;
      for (int f = 0; f < 4 && !dead; ++f)
      {
        const Input in = golemInput(kGolemMoves[m], f);
        first.update(asPlayerInput(in, prev));
        prev = in;
        dead = first.player().state == PlayerState::Dying;
      }
      const long firstScore = 200L * (golem0 - first.golemHp());
      long moveScore = std::numeric_limits<long>::min();
      for (int follow = 0; follow < 8 && !dead; ++follow)
      {
        World sim(first);
        Input fp = prev;
        long score = firstScore;
        int golemHp = sim.golemHp();
        bool down = false;
        for (int f = 0; f < kAhead; ++f)
        {
          Input in;
          const auto& sp = sim.player();
          const auto& g = sim.golem();
          const float gcx = g.x + float(Golem::kW) * 0.5f, pcx = float(sp.x) + 1.5f;
          const int toGolem = gcx < pcx ? -1 : 1;
          const bool ground = sp.state == PlayerState::OnGround;
          const Golem::Head* head = nearestHead(sim);
          auto face = [&](int dir) {
            in.left = dir < 0 && sp.facing > 0;
            in.right = dir > 0 && sp.facing < 0;
          };
          switch (follow)
          {
            case 0: // duel: face it, jump-shoot while the gem shows
            case 1: // back off first
            case 2: // close in first
              if (follow == 1 && f < 12)
              {
                in.left = toGolem > 0;
                in.right = toGolem < 0;
              }
              else if (follow == 2 && f < 8)
              {
                in.left = toGolem < 0;
                in.right = toGolem > 0;
              }
              else
                face(toGolem);
              in.jump = ground && (waveNear(sim) || (g.open > 0 && f % 8 == 0));
              in.fire = f % 2 == 1;
              break;
            case 3: // crouch (a high sweep)
              in.down = true;
              face(toGolem);
              in.fire = f % 2 == 1;
              break;
            case 4: // hop
              in.jump = ground && f % 6 == 0;
              face(toGolem);
              in.fire = f % 2 == 1;
              break;
            case 5: // shoot the nearest head, jumping it when it comes low
            case 6: // run from it
              if (head)
              {
                const int dir = head->x + 3.0f < pcx ? -1 : 1;
                const float dist = std::abs(head->x + 3.0f - pcx);
                if (follow == 5)
                  face(dir);
                else
                {
                  in.left = dir > 0;
                  in.right = dir < 0;
                }
                in.jump = ground && dist < 10.0f && head->y > float(sim.golem().floor - 5);
              }
              in.fire = f % 2 == 1;
              break;
            case 7: // stand and shoot
              face(head ? (head->x + 3.0f < pcx ? -1 : 1) : toGolem);
              in.fire = f % 2 == 1;
              break;
          }
          sim.update(asPlayerInput(in, fp));
          fp = in;
          score += long(golemHp - sim.golemHp()) * (180 - 3 * f);
          golemHp = sim.golemHp();
          if (!down && (sim.golem().phase == GolemPhase::Crumble || sim.golem().phase == GolemPhase::Done))
          {
            down = true;
            score += 100000L - 1000L * f;
          }
          if (sim.player().state == PlayerState::Dying || sim.state() != WorldState::Playing)
            break;
        }
        const auto& sp = sim.player();
        const long lost = std::max(0, hp0 - sp.hp);
        score += -lost * (400L + 1200L / std::max(1, sp.hp));
        // Keep a little way off the body, where the gem is in reach.
        const auto& g = sim.golem();
        if (g.phase == GolemPhase::Stomp || g.phase == GolemPhase::Sweep)
        {
          const float gap = std::abs(float(sp.x) + 1.5f - (g.x + float(Golem::kW) * 0.5f)) - float(Golem::kW) * 0.5f;
          score -= long(std::abs(gap - 9.0f) * 3.0f);
        }
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
      std::fprintf(stderr, "golem f%d phase %d hp %d at %d,%d runner hp %d -> move %d (%ld)\n", world.stats().frames,
        int(world.golem().phase), world.golemHp(), world.player().x, world.player().y, world.player().hp, best,
        bestScore);
    for (int f = 0; f < 4; ++f)
      mFightQueue.push_back(golemInput(kGolemMoves[best], f));
  }
  Input in = mFightQueue.front();
  mFightQueue.pop_front();
  mFightPrev = in;
  return in;
}

Input Bot::trapmaster(const World& world)
{
  // Every few frames, try each armed trap in a copy of the world: walk the
  // cursor to its plate, fire, and see how much score it brings in over
  // the next 50 frames compared with doing nothing.
  if (mTrapQueue.empty())
  {
    const int n = int(world.plates().size());
    auto steps = [&](int to) {
      std::vector<Input> seq;
      int at = world.trapCursor().at;
      while (at != to)
      {
        const int fwd = (to - at + n) % n, back = (at - to + n) % n;
        Input in;
        in.down = fwd <= back;
        in.up = !in.down;
        seq.push_back(in);
        seq.push_back(Input{});
        at = in.down ? (at + 1) % n : (at + n - 1) % n;
      }
      Input fire;
      fire.fire = true;
      seq.push_back(fire);
      seq.push_back(Input{});
      return seq;
    };
    auto run = [&](const std::vector<Input>& seq) {
      auto sim = world.cloneForSim();
      Input prev;
      for (int f = 0; f < 60; ++f)
      {
        const Input in = f < int(seq.size()) ? seq[std::size_t(f)] : Input{};
        sim->update(asPlayerInput(in, prev));
        prev = in;
        if (sim->state() != WorldState::Playing)
          break;
      }
      return sim->stats().score;
    };
    const int base = run({});
    int best = -1, bestGain = 0;
    std::vector<Input> bestSeq;
    for (int i = 0; i < n; ++i)
    {
      bool armed = false;
      for (const auto& t : world.traps())
        armed = armed || (t.plate == i && t.state == 0);
      if (!armed)
        continue;
      const auto seq = steps(i);
      const int gain = run(seq) - base;
      // Nearer plates win ties (they cost fewer frames).
      if (gain > bestGain || (gain == bestGain && gain > 0 && seq.size() < bestSeq.size()))
      {
        best = i;
        bestGain = gain;
        bestSeq = seq;
      }
    }
    if (best >= 0)
      mTrapQueue.assign(bestSeq.begin(), bestSeq.end());
    else
      mTrapQueue.assign(3, Input{});
  }
  const Input in = mTrapQueue.front();
  mTrapQueue.pop_front();
  return in;
}

Input Bot::pinball(const World& world)
{
  if (!mPinQueue.empty())
  {
    const Input in = mPinQueue.front();
    mPinQueue.pop_front();
    return in;
  }
  const Pinball& pin = world.pin();
  static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
  if (debug && world.clock() % 5 == 0)
  {
    std::string lamps;
    for (const auto& l : pin.lamps)
      lamps += l.lit ? '*' : '.';
    std::fprintf(stderr, "pin f%d ball %.1f,%.1f v %.2f,%.2f lamps %s drains %d\n", world.clock(), pin.x, pin.y, pin.vx,
      pin.vy, lamps.c_str(), pin.drains);
  }
  // Only on the plunger and near the flippers is there anything to decide.
  const float reach = pin.flipLen + 4.0f;
  const bool near = pin.vy > -1.0f && pin.y > std::min(pin.ly, pin.ry) - 6.0f &&
    (std::hypot(pin.x - pin.lx, pin.y - pin.ly) < reach || std::hypot(pin.x - pin.rx, pin.y - pin.ry) < reach);
  if (!near && !pin.inPlunger)
    return Input{};
  // Try each flipper after a few frames' delay (and leaving them be) in a
  // copy of the world, and see which sends the ball best: a lantern lit,
  // the gate reached, close to the next unlit lantern, not down the drain.
  auto target = [&](const Pinball& b, float& tx, float& ty) {
    float best = 1e9f;
    tx = b.gateX;
    ty = b.gateY;
    if (b.gateOpen)
      return;
    for (const auto& l : b.lamps)
      if (!l.lit)
      {
        const float d = std::hypot(l.x - b.x, l.y - b.y);
        if (d < best)
        {
          best = d;
          tx = l.x;
          ty = l.y;
        }
      }
  };
  int litNow = 0;
  for (const auto& l : pin.lamps)
    litNow += l.lit;
  auto run = [&](const std::vector<Input>& seq) {
    auto sim = world.cloneForSim();
    Input prev;
    float closest = 1e9f, top = sim->pin().y;
    int f = 0;
    const int drains = sim->pin().drains;
    for (; f < 110; ++f)
    {
      const Input in = f < int(seq.size()) ? seq[std::size_t(f)] : Input{};
      sim->update(asPlayerInput(in, prev));
      prev = in;
      if (sim->state() != WorldState::Playing)
        return 100000.0f - float(f);
      const Pinball& b = sim->pin();
      if (b.drains > drains || b.inPlunger)
        break;
      float tx = 0.0f, ty = 0.0f;
      target(b, tx, ty);
      closest = std::min(closest, std::hypot(b.x - tx, b.y - ty));
      top = std::min(top, b.y);
    }
    int lit = 0;
    for (const auto& l : sim->pin().lamps)
      lit += l.lit;
    return float(lit - litNow) * 1000.0f - closest * 30.0f + (pin.y - top) * 4.0f + float(f) * 2.0f;
  };
  std::vector<Input> best;
  if (pin.inPlunger)
  {
    // How far to pull the plunger back.
    float bestScore = -1e9f;
    for (int pull = 1; pull <= 15; ++pull)
    {
      Input hold;
      hold.jump = true;
      std::vector<Input> seq(std::size_t(pull), hold);
      seq.push_back(Input{});
      const float sc = run(seq);
      if (sc > bestScore)
      {
        bestScore = sc;
        best = seq;
      }
    }
    mPinQueue.assign(best.begin(), best.end());
    mPinQueue.push_back(Input{});
    return Input{};
  }
  float bestScore = run({});
  for (int side = 0; side < 2; ++side)
    for (int delay : {0, 1, 2, 3, 4, 5, 6, 8, 10})
      for (int hold : {2, 6})
    {
      std::vector<Input> seq(std::size_t(delay), Input{});
      Input flip;
      flip.jump = side == 0;
      flip.fire = side == 1;
      seq.insert(seq.end(), std::size_t(hold), flip);
      seq.push_back(Input{});
      const float sc = run(seq);
      if (sc > bestScore + 1.0f)
      {
        bestScore = sc;
        best = seq;
      }
    }
  if (best.empty())
    best.assign(2, Input{});
  mPinQueue.assign(best.begin(), best.end());
  const Input in = mPinQueue.front();
  mPinQueue.pop_front();
  return in;
}

int Bot::boardable(const World& world) const
{
  // A cart standing on a long rail that runs on toward the exit, right
  // where we stand: jump in.
  const auto& p = world.player();
  if (p.state != PlayerState::OnGround || p.cart >= 0)
    return -1;
  const int exitX = world.level().exitTx * kCellsPerTile;
  for (std::size_t i = 0; i < world.carts().size(); ++i)
  {
    const Cart& c = world.carts()[i];
    if (c.rail < 0 || c.speed != 0 || c.falling || c.lost > 0)
      continue;
    const Rail& r = world.rails()[std::size_t(c.rail)];
    if (r.len < 40 * 8 || r.pts.back().first < p.x + 40 || exitX < p.x || !r.leverId.empty())
      continue;
    const CellBox box = c.box();
    if (p.x + 1 >= box.x && p.x + 1 < box.x + box.w && std::abs(box.y + box.h - 1 - p.y) <= 1)
      return int(i);
  }
  return -1;
}

Input Bot::ride(const World& world)
{
  if (!mRideQueue.empty())
  {
    const Input in = mRideQueue.front();
    mRideQueue.pop_front();
    return in;
  }
  const auto& p = world.player();
  if (p.cart < 0)
  {
    // Climbing in: straight up, then let it carry us down into the cart.
    Input jump;
    jump.jump = true;
    mRideQueue.assign(3, jump);
    mRideQueue.push_back(Input{});
    for (int f = 0; f < 12; ++f)
      mRideQueue.push_back(Input{});
    const Input in = mRideQueue.front();
    mRideQueue.pop_front();
    return in;
  }
  const Cart& cart = world.carts()[std::size_t(p.cart)];
  if (cart.speed == 0 && !cart.falling && cart.hop == 0)
  {
    // Stopped at a bumper: climb out.
    Input jump;
    jump.jump = true;
    mRideQueue.assign(2, jump);
    mRideQueue.push_back(Input{});
    return Input{};
  }
  // Ride on, hop or duck: whichever keeps cart and rider whole over the
  // next couple of seconds.
  // Options: 0 ride on, 1 hop now, 2 duck, 3+k hop k frames from now.
  auto run = [&](int option) {
    auto sim = world.cloneForSim();
    Input prev;
    const int hp = sim->player().hp;
    const int hopAt = option == 1 ? 0 : (option >= 3 ? option - 2 : -1);
    int f = 0;
    for (; f < 40; ++f)
    {
      Input in;
      in.jump = f == hopAt;
      in.down = option == 2 && f < 8;
      sim->update(asPlayerInput(in, prev));
      prev = in;
      if (sim->state() != WorldState::Playing || sim->player().state == PlayerState::Dying)
        break;
      const auto& sp = sim->player();
      if (sp.cart < 0)
        break;
      const Cart& sc = sim->carts()[std::size_t(sp.cart)];
      if (sc.falling || sc.lost > 0)
        break;
    }
    const auto& sp = sim->player();
    float score = float(sp.hp - hp) * 200.0f + float(f) * 5.0f;
    if (sp.virus > 0 && p.virus == 0)
      score -= 300.0f;
    if (sp.state == PlayerState::Dying)
      score -= 5000.0f;
    score += float(sp.x) * 0.5f;
    return score;
  };
  int best = 0;
  float bestScore = run(0);
  for (int option = 1; option <= 14; ++option)
  {
    const float sc = run(option);
    if (sc > bestScore + 1.0f)
    {
      bestScore = sc;
      best = option;
    }
  }
  if (best >= 3)
    best = 0; // a hop later on is better: not yet
  Input in;
  if (best == 1)
  {
    in.jump = true;
    mRideQueue.push_back(Input{});
  }
  else if (best == 2)
  {
    in.down = true;
    mRideQueue.assign(3, in);
  }
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

// Boulder Surfing: try a handful of ways to push (right for so many frames
// of every 16, with a jump somewhere or none) a short while ahead, play the
// first frames of the one that gets furthest without a fall, and look again.
Input Bot::surf(const World& world)
{
  if (!mSurfQueue.empty())
  {
    mSurfPrev = mSurfQueue.front();
    mSurfQueue.pop_front();
    return mSurfPrev;
  }
  const Surf& s0 = world.surf();
  if (!s0.mounted)
  {
    // In the air (or falling): lean for the ledge.
    Input in;
    in.right = s0.fall == 0;
    mSurfPrev = in;
    return in;
  }
  struct Plan
  {
    int push, jumpAt;
  };
  constexpr int kHorizon = 48;
  auto inputAt = [](const Plan& pl, int f) {
    Input in;
    in.right = f % 16 < pl.push && (pl.jumpAt < 0 || f < pl.jumpAt + 2);
    in.jump = f == pl.jumpAt;
    if (pl.jumpAt >= 0 && f > pl.jumpAt)
      in.right = true; // steer for the ledge
    return in;
  };
  auto score = [&](const Plan& pl) {
    auto sim = world.cloneForSim();
    Input prev = mSurfPrev;
    const int falls = sim->surf().falls;
    for (int f = 0; f < kHorizon; ++f)
    {
      const Input in = inputAt(pl, f);
      sim->update(asPlayerInput(in, prev));
      prev = in;
      if (sim->state() != WorldState::Playing || sim->surf().free)
        return 2000000000 - f;
      if (sim->surf().falls > falls)
        return -2000000000 + f;
    }
    const Surf& s = sim->surf();
    if (!s.mounted)
      return -1000000000; // still in the air: no better than a fall
    return int(s.x * 100.0f) - int(std::abs(s.drift) * 40.0f);
  };
  Plan best{0, -1};
  int bestScore = std::numeric_limits<int>::min();
  for (int push = 0; push <= 12; ++push)
    for (int jumpAt = -1; jumpAt < kHorizon - 16; jumpAt += (jumpAt < 0 ? 1 : 3))
    {
      const Plan pl{push, jumpAt};
      const int v = score(pl);
      if (v > bestScore)
      {
        bestScore = v;
        best = pl;
      }
    }
  for (int f = 0; f < 4; ++f)
    mSurfQueue.push_back(inputAt(best, f));
  mSurfPrev = mSurfQueue.front();
  mSurfQueue.pop_front();
  return mSurfPrev;
}

// Asteroid Belt (recoil_only): head for the nearest gem left (the beacon
// once none are). Tries a shot (8 ways or none) now and another 3 frames
// on, over 18 frames, and keeps the pair that ends nearest, slowest there.
// Air Hockey (level 16's bonus): with no friction, glide to a spot a few
// blocks short of the nearest resting puck (braking by pushing the other
// way), turn to it crouched and tap the Freeze Ray: every puck kicked
// slides into the far goal (or knocks the one in front of it on).
Input Bot::hockey(const World& world)
{
  Input in;
  const auto& p = world.player();
  const float slide = world.cryo().slide;
  const Enemy* best = nullptr;
  bool moving = false;
  for (const auto& e : world.enemies())
  {
    if (!e.alive || e.kind != EnemyKind::Puck || e.variant == 1)
      continue; // (not the goalies)
    moving = moving || e.vx != 0.0f;
    if (e.vx == 0.0f && (!best || std::abs(e.x - p.x) < std::abs(best->x - p.x)))
      best = &e;
  }
  if (!best)
    return in;
  // Shoot it from the runner's side, on into the goal beyond it.
  const int dir = best->x > p.x ? 1 : -1;
  const int spot = best->x + (dir > 0 ? -12 : best->w + 10);
  const float want = std::clamp(float(spot - p.x) / 10.0f, -1.0f, 1.0f);
  const bool there = std::abs(spot - p.x) <= 3;
  if (!there || std::abs(slide) > 0.07f)
  {
    const float target = there ? 0.0f : want;
    if (slide < target - 0.07f)
      in.right = true;
    else if (slide > target + 0.07f)
      in.left = true;
    return in;
  }
  in.down = true;
  if (dir != p.facing)
  {
    (dir > 0 ? in.right : in.left) = true;
    return in;
  }
  if (moving || (world.stats().frames % 2) != 0)
    return in;
  // Past the goalie? Try the shot in a copy of the rink first.
  World sim(world);
  Input shot = in, crouch = in;
  shot.fire = true;
  sim.update(asPlayerInput(shot, Input{}));
  for (int f = 0; f < 50 && sim.cryo().scored == world.cryo().scored; ++f)
    sim.update(asPlayerInput(crouch, shot));
  in.fire = sim.cryo().scored > world.cryo().scored;
  return in;
}

Input Bot::drift(const World& world)
{
  if (!mDriftQueue.empty())
  {
    mDriftPrev = mDriftQueue.front();
    mDriftQueue.pop_front();
    return mDriftPrev;
  }
  constexpr int kHorizon = 18, kSecond = 3, kCommit = 3;
  static const int kDirs[9][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  const auto& p0 = world.player();
  auto mid = [](const Player& p) { return std::pair<float, float>{float(p.x) + 1.5f, float(p.y) - 2.0f}; };
  float tx = 0, ty = 0;
  {
    const auto [px, py] = mid(p0);
    float best = 1e9f;
    for (const auto& it : world.items())
      if (!it.taken && it.kind == ItemKind::Gem)
      {
        const float dd = std::hypot(float(it.x) + 1.0f - px, float(it.y) + 1.0f - py);
        if (dd < best)
        {
          best = dd;
          tx = float(it.x) + 1.0f;
          ty = float(it.y) + 1.0f;
        }
      }
    if (best >= 1e9f)
    {
      tx = float(world.level().exitTx * kCellsPerTile) + 1.0f;
      ty = float(world.level().exitTy * kCellsPerTile) + 1.0f;
    }
  }
  auto inputAt = [&](int a, int b, int f) {
    const int k = f == 0 ? a : (f == kSecond ? b : 0);
    Input in;
    if (k != 0)
    {
      in.right = kDirs[k][0] > 0;
      in.left = kDirs[k][0] < 0;
      in.down = kDirs[k][1] > 0;
      in.up = kDirs[k][1] < 0;
      in.fire = true;
    }
    return in;
  };
  double bestScore = -1e18;
  int bestA = 0, bestB = 0;
  const int gems0 = world.stats().gems;
  int gemsLeft = 0;
  for (const auto& it : world.items())
    gemsLeft += !it.taken && it.kind == ItemKind::Gem;
  for (int a = 0; a < 9; ++a)
    for (int b = 0; b < 9; ++b)
    {
      auto sim = world.cloneForSim();
      Input prev = mDriftPrev;
      double closest = 1e9;
      int gotAt = -1;
      for (int f = 0; f < kHorizon; ++f)
      {
        const Input in = inputAt(a, b, f);
        sim->update(asPlayerInput(in, prev));
        prev = in;
        if (sim->state() != WorldState::Playing)
          break;
        const auto [px, py] = mid(sim->player());
        closest = std::min(closest, double(std::hypot(px - tx, py - ty)));
        if (gotAt < 0 && sim->stats().gems > gems0)
          gotAt = f;
      }
      const auto& d = sim->station().drift;
      const auto [px, py] = mid(sim->player());
      const double end = std::hypot(px - tx, py - ty);
      // Moving toward the target is good; arriving fast is not.
      const double toward = ((tx - px) * d.vx + (ty - py) * d.vy) / std::max(1.0, end);
      double score = -end * 4.0 - closest * 2.0 + toward * 6.0 - std::hypot(d.vx, d.vy) * (end < 12.0 ? 8.0 : 0.0);
      if (gotAt >= 0)
        score += 10000.0 - gotAt * 50.0 + (sim->stats().gems - gems0) * 2000.0;
      if (sim->state() == WorldState::Exiting)
        score += gemsLeft > sim->stats().gems - gems0 ? -1e7 : 5000.0;
      if (score > bestScore)
      {
        bestScore = score;
        bestA = a;
        bestB = b;
      }
    }
  for (int f = 0; f < kCommit; ++f)
    mDriftQueue.push_back(inputAt(bestA, bestB, f));
  mDriftPrev = mDriftQueue.front();
  mDriftQueue.pop_front();
  return mDriftPrev;
}

Input Bot::floorLava(const World& world)
{
  if (!mLavaQueue.empty())
  {
    mLavaPrev = mLavaQueue.front();
    mLavaQueue.pop_front();
    return mLavaPrev;
  }
  // A move: wait, then (from a ledge) jump, steering one way for a while,
  // jump held or not for the next head.
  struct Move
  {
    int wait, dir, steer;
    bool hold;
  };
  struct Out
  {
    int kind = 0; // 0 lava or worse, 1 nothing yet, 2 a head, 3 out
    int frames = 0, x = 0;
    std::unique_ptr<World> sim;
    Input prev;
  };
  constexpr int kHorizon = 70;
  auto inputAt = [](const Move& m, bool grounded, int f) {
    Input in;
    if (f < m.wait)
      return in;
    const int t = f - m.wait;
    in.jump = m.hold || (grounded && t < 2);
    in.right = m.dir > 0 && t < m.steer;
    in.left = m.dir < 0 && t < m.steer;
    return in;
  };
  auto run = [&](const World& from, const Input& prev0, const Move& m) {
    Out o;
    o.sim = from.cloneForSim();
    World& sim = *o.sim;
    const bool grounded = sim.player().state == PlayerState::OnGround;
    const int bounces = sim.headBounces(), pops = sim.lavaPops();
    Input prev = prev0;
    for (int f = 0; f < m.wait + kHorizon; ++f)
    {
      const Input in = inputAt(m, grounded, f);
      sim.update(asPlayerInput(in, prev));
      prev = in;
      o.frames = f + 1;
      o.x = sim.player().x;
      if (sim.state() != WorldState::Playing)
      {
        o.kind = sim.player().state == PlayerState::Dying ? 0 : 3;
        break;
      }
      if (sim.lavaPops() > pops)
        break;
      if (sim.headBounces() > bounces)
      {
        o.kind = 2;
        break;
      }
      if (f > m.wait + 2 && sim.player().state == PlayerState::OnGround)
      {
        o.kind = 1;
        break;
      }
      o.kind = 1;
    }
    o.prev = prev;
    return o;
  };
  auto moves = [](bool grounded) {
    std::vector<Move> ms;
    for (int wait = 0; wait <= (grounded ? 44 : 0); wait += 3)
      for (bool hold : {true, false})
      {
        for (int steer = 0; steer <= 36; steer += 2)
          ms.push_back({wait, 1, steer, hold});
        for (int steer = 2; steer <= 16; steer += 2)
          ms.push_back({wait, -1, steer, hold});
      }
    return ms;
  };
  // What a head is worth: how far on it is, and how far the best move from
  // it gets.
  auto value = [](const Out& o) {
    if (o.kind == 3)
      return 1000000 - o.frames;
    if (o.kind == 2)
      return o.x * 100 - o.frames;
    if (o.kind == 1)
      return o.x * 50 - o.frames * 4;
    return -1000000;
  };
  const bool grounded = world.player().state == PlayerState::OnGround;
  std::vector<Move> ms = moves(grounded);
  std::vector<Out> outs;
  outs.reserve(ms.size());
  for (const Move& m : ms)
    outs.push_back(run(world, mLavaPrev, m));
  std::vector<std::size_t> order(ms.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return value(outs[a]) > value(outs[b]); });
  // The best few heads: only one that leads somewhere.
  const std::vector<Move> next = moves(false);
  int best = -1;
  int bestScore = std::numeric_limits<int>::min();
  int looked = 0;
  for (std::size_t i : order)
  {
    const Out& o = outs[i];
    if (o.kind == 3)
    {
      best = int(i);
      break;
    }
    if (o.kind == 0)
      break;
    int score = value(o);
    if (o.kind == 2 && looked < 8)
    {
      ++looked;
      int further = -1000000;
      for (const Move& m : next)
        further = std::max(further, value(run(*o.sim, o.prev, m)));
      score = further <= -1000000 ? -500000 + score : further + score / 10;
    }
    else if (o.kind == 2)
      continue;
    if (score > bestScore)
    {
      bestScore = score;
      best = int(i);
    }
  }
  static const bool debug = std::getenv("GR_FIGHT_DEBUG") != nullptr;
  if (best < 0)
  {
    if (debug)
      std::fprintf(stderr, "lava f%d at %d,%d: no move\n", world.clock(), world.player().x, world.player().y);
    return Input{};
  }
  const Move& m = ms[std::size_t(best)];
  const Out& o = outs[std::size_t(best)];
  if (debug)
    std::fprintf(stderr, "lava f%d at %d,%d: wait %d dir %d steer %d hold %d -> kind %d x %d in %d\n", world.clock(),
      world.player().x, world.player().y, m.wait, m.dir, m.steer, int(m.hold), o.kind, o.x, o.frames);
  // Up to the next head (then think again), or a few frames of a wait.
  const int frames = o.kind == 1 ? std::min(o.frames, 3) : o.frames;
  for (int f = 0; f < frames; ++f)
    mLavaQueue.push_back(inputAt(m, grounded, f));
  mLavaPrev = mLavaQueue.front();
  mLavaQueue.pop_front();
  return mLavaPrev;
}

Input Bot::orbit(const World& world)
{
  if (!mOrbitQueue.empty())
  {
    mOrbitPrev = mOrbitQueue.front();
    mOrbitQueue.pop_front();
    return mOrbitPrev;
  }
  const auto& o0 = world.orbit();
  Input none;
  if (o0.planet < 0)
  {
    mOrbitPrev = none;
    return none; // adrift: wait to land
  }
  // The part to fetch: the nearest one still out there.
  const auto& here = o0.planets[std::size_t(o0.planet)];
  float tx = 0.0f, ty = 0.0f, best = 1e9f;
  for (const auto& pt : o0.parts)
    if (!pt.taken)
    {
      const float d = std::hypot(pt.x - here.cx, pt.y - here.cy);
      if (d < best)
      {
        best = d;
        tx = pt.x;
        ty = pt.y;
      }
    }
  if (best >= 1e9f)
  {
    mOrbitPrev = none;
    return none;
  }
  // The planetoid the part lies on.
  int goal = -1;
  float goalDist = 1e9f;
  for (std::size_t i = 0; i < o0.planets.size(); ++i)
  {
    const float d = std::hypot(tx - o0.planets[i].cx, ty - o0.planets[i].cy) - o0.planets[i].r;
    if (d < goalDist)
    {
      goalDist = d;
      goal = int(i);
    }
  }
  const int parts0 = o0.partsTaken;
  auto planIn = [](int walk, int steer, int f) {
    Input in;
    const int dir = walk > 0 ? 1 : -1;
    const int n = std::abs(walk);
    if (f < n)
    {
      in.right = dir > 0;
      in.left = dir < 0;
    }
    else
    {
      in.jump = f == n;
      in.right = steer > 0;
      in.left = steer < 0;
    }
    return in;
  };
  if (goal == o0.planet)
  {
    // On the part's planetoid: walk round to it, the short way.
    const float want = std::atan2(tx - here.cx, -(ty - here.cy));
    float da = want - o0.ang;
    while (da > 3.14159265f)
      da -= 6.2831853f;
    while (da < -3.14159265f)
      da += 6.2831853f;
    Input in;
    in.right = da > 0.0f;
    in.left = da <= 0.0f;
    mOrbitPrev = in;
    return in;
  }
  // Every walk (both ways, up to half round) then a jump, steered either
  // way or not at all.
  const int maxWalk = int(3.14159f * here.r) + 2;
  double bestScore = -1e18;
  int bestWalk = 0, bestSteer = 0, bestLen = 0;
  for (int walk = -maxWalk; walk <= maxWalk; walk += 2)
    for (int steer = -1; steer <= 1; ++steer)
    {
      auto sim = world.cloneForSim();
      Input prev = mOrbitPrev;
      int f = 0;
      bool left = false;
      for (; f < std::abs(walk) + 150; ++f)
      {
        const Input in = planIn(walk, steer, f);
        sim->update(asPlayerInput(in, prev));
        prev = in;
        if (sim->state() != WorldState::Playing || sim->orbit().partsTaken > parts0)
          break;
        if (f > std::abs(walk))
        {
          left = left || sim->orbit().planet != o0.planet;
          if (left && sim->orbit().planet >= 0)
            break; // landed
        }
      }
      const auto& o = sim->orbit();
      double score;
      if (o.partsTaken > parts0 || sim->state() == WorldState::Exiting)
        score = 1e6 - f;
      else if (o.planet < 0 || o.planet == o0.planet || o.lost > 0)
        score = -1e9; // lost, or back where it started
      else
      {
        const auto& pl = o.planets[std::size_t(o.planet)];
        score = -double(std::hypot(tx - pl.cx, ty - pl.cy)) * 100.0 - f;
        if (o.planet == goal)
          score += 1e5;
      }
      if (score > bestScore)
      {
        bestScore = score;
        bestWalk = walk;
        bestSteer = steer;
        bestLen = f + 1;
      }
    }
  if (bestScore <= -1e9)
  {
    // Nothing lands anywhere new: walk on a bit and look again.
    Input in;
    in.right = true;
    mOrbitPrev = in;
    return in;
  }
  for (int f = 0; f < bestLen; ++f)
    mOrbitQueue.push_back(planIn(bestWalk, bestSteer, f));
  mOrbitPrev = mOrbitQueue.front();
  mOrbitQueue.pop_front();
  return mOrbitPrev;
}

Input Bot::gunGravity(const World& world)
{
  if (!mGunQueue.empty())
  {
    mGunPrev = mGunQueue.front();
    mGunQueue.pop_front();
    return mGunPrev;
  }
  // A plan: a shot (none, facing right, facing left, up), then a walk
  // (with or without a jump) for a while, then let it settle.
  auto planIn = [](int shot, int walk, int len, bool jump, int f) {
    Input in;
    if (shot != 0 && f < 2)
    {
      if (shot == 1 || shot == 2)
      {
        in.right = shot == 1 && f == 0;
        in.left = shot == 2 && f == 0;
        in.fire = f == 1;
      }
      else
      {
        in.up = true;
        in.fire = f == 1;
      }
      return in;
    }
    const int g = f - (shot != 0 ? 2 : 0);
    if (g < len)
    {
      in.right = walk > 0;
      in.left = walk < 0;
      in.jump = jump && g < 4;
    }
    return in;
  };
  const int gems0 = world.stats().gems;
  auto nearest = [](const World& w) {
    const CellBox b = w.player().box();
    const int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    int best = 1 << 20;
    for (const auto& it : w.items())
      if (!it.taken && it.kind == ItemKind::Gem)
        best = std::min(best, std::abs(it.x + 1 - cx) + std::abs(it.y - cy));
    return best;
  };
  double bestScore = -1e18;
  int bestShot = 0, bestWalk = 0, bestLen = 0, bestFrames = 0;
  bool bestJump = false;
  for (int shot = 0; shot <= 3; ++shot)
    for (int walk = -1; walk <= 1; ++walk)
      for (int len = 0; len <= 36; len += 6)
        for (int jump = 0; jump <= 1; ++jump)
        {
          if ((walk == 0) != (len == 0) || (len == 0 && jump))
            continue;
          auto sim = world.cloneForSim();
          Input prev = mGunPrev;
          const int total = (shot != 0 ? 2 : 0) + len + 24;
          int f = 0, closest = nearest(*sim);
          bool got = false;
          for (; f < total; ++f)
          {
            const Input in = planIn(shot, walk, len, jump != 0, f);
            sim->update(asPlayerInput(in, prev));
            prev = in;
            closest = std::min(closest, nearest(*sim));
            if (sim->stats().gems > gems0 || sim->state() != WorldState::Playing)
            {
              got = true;
              ++f;
              break;
            }
          }
          const bool settled = sim->player().state == PlayerState::OnGround;
          const double score = (got ? 10000.0 - f * 10.0 : 0.0) - nearest(*sim) * 4.0 - closest - (settled ? 0.0 : 20.0) -
            f * 0.2;
          if (score > bestScore)
          {
            bestScore = score;
            bestShot = shot;
            bestWalk = walk;
            bestLen = len;
            bestJump = jump != 0;
            bestFrames = got ? f : std::min(total, (shot != 0 ? 2 : 0) + std::max(len, 6) + 8);
          }
        }
  for (int f = 0; f < std::max(1, bestFrames); ++f)
    mGunQueue.push_back(planIn(bestShot, bestWalk, bestLen, bestJump, f));
  mGunPrev = mGunQueue.front();
  mGunQueue.pop_front();
  return mGunPrev;
}

} // namespace gr
