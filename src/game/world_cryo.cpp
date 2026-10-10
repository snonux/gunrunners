// Level 16, Cryo Labs (SPEC 16): Ice Floors and the Freeze Ray. On ice the
// runner speeds up and slides to a stop; a kicker in the ice launches a
// runner crossing it at speed. The Freeze Ray turns what it hits into a
// block of ice to stand on, and a frozen block on ice that is shot slides
// until a wall shatters it. The labs' staff: Puck Drones, Sleeper Pods and
// their mutants, and Lab Arms on the ceiling rail. Air Hockey, the bonus,
// is the same rink with no friction at all (rules=zero_friction).

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr int kFreeze = 120;        // frames a Freeze Ray hit holds
constexpr float kBlockSlide = 2.0f; // a shot frozen block, cells a frame
constexpr int kKnockdown = 20;      // frames an enemy a block hits lies dazed
constexpr int kLift = 4;            // cells a Lab Arm lifts the runner
constexpr int kHold = 15;
constexpr int kMaxMutants = 3;

int sgn(int v) { return (v > 0) - (v < 0); }

CellBox blockBox(int bx, int by, int w, int h)
{
  // (bx, by) is the bottom-left block; w x h blocks.
  return {bx * kCellsPerTile, (by - h + 1) * kCellsPerTile, w * kCellsPerTile, h * kCellsPerTile};
}

CellBox goalBox(const HockeyGoal& g)
{
  return {g.x0 * kCellsPerTile, g.y0 * kCellsPerTile, (g.x1 - g.x0 + 1) * kCellsPerTile,
    (g.y1 - g.y0 + 1) * kCellsPerTile};
}

} // namespace

// --- Setup ---------------------------------------------------------------------------

bool World::setupCryoEntity(const EntityDef& e)
{
  auto& c = mCryo;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  auto markIce = [&](int ax, int ay, int bx, int by) {
    const int w = mLevel->width, h = mLevel->height;
    if (c.ice.empty())
    {
      c.w = w;
      c.ice.assign(std::size_t(w * h), 0);
    }
    for (int y = std::max(0, ay); y <= std::min(h - 1, by); ++y)
      for (int x = std::max(0, ax); x <= std::min(w - 1, bx); ++x)
        c.ice[std::size_t(y * w + x)] = 1;
  };
  if (e.kind == "ice" && e.rect("rect", x0, y0, x1, y1))
  {
    markIce(x0, y0, x1, y1);
    c.on = true;
    return true;
  }
  if (e.kind == "kicker" && e.hasPos)
  {
    Kicker k;
    k.bx = e.x;
    k.by = e.y;
    k.dir = e.str("dir", "r") == "l" ? -1 : 1;
    k.launch = std::clamp(e.num("launch", 14), 2, 30);
    c.kickers.push_back(k);
    c.on = true;
    return true;
  }
  if (e.kind == "frost" && e.rect("rect", x0, y0, x1, y1))
  {
    c.frost.push_back({x0, y0, x1, y1, std::max(1, e.num("dmg", 1)), std::max(1, e.num("every", 27))});
    c.on = true;
    return true;
  }
  if (e.kind == "goalvent" && e.hasPos)
  {
    GoalVent v;
    v.bx = e.x;
    v.by = e.y;
    v.w = std::max(1, e.num("w", 1));
    v.h = std::max(1, e.num("h", 2));
    c.vents.push_back(v);
    return true;
  }
  if (e.kind == "powerbox" && e.hasPos)
  {
    // A solid block that breaks to 3 hits (or one sliding block).
    Breakable b;
    b.x0 = b.x1 = e.x;
    b.y0 = b.y1 = e.y;
    b.hp = 3;
    b.look = 10;
    PowerBox pb;
    pb.id = e.id;
    pb.arm = e.str("arm");
    pb.breakable = int(mBreakables.size());
    mBreakables.push_back(b);
    c.boxes.push_back(pb);
    c.on = true;
    return true;
  }
  if (e.kind == "armrail")
  {
    c.railY = e.num("y");
    c.railX0 = e.num("x0");
    c.railX1 = e.num("x1");
    return true;
  }
  if (e.kind == "hockeygoal" && e.rect("rect", x0, y0, x1, y1))
  {
    c.goals.push_back({x0, y0, x1, y1});
    return true;
  }
  if (e.kind == "puckspawn" && e.hasPos)
  {
    c.puckX = e.x * kCellsPerTile;
    c.puckY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "deco" && e.hasPos)
  {
    const std::string kind = e.str("kind");
    const int k = kind == "glass" ? 0 : (kind == "crewpod" ? 1 : (kind == "hostpod" ? 2 : -1));
    if (k < 0)
      return false;
    c.decos.push_back({k, e.x, e.y});
    if (k == 2)
    {
      c.hostX = e.x * kCellsPerTile;
      c.hostY = e.y * kCellsPerTile + 1;
    }
    return true;
  }
  return false;
}

void World::setupCryoEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::LabArm)
  {
    // rail=x0,x1 (blocks): the section of the ceiling rail it rides.
    const auto rail = e.list("rail");
    const int r0 = rail.size() == 2 ? rail[0] : en.x / kCellsPerTile - 3;
    const int r1 = rail.size() == 2 ? rail[1] : en.x / kCellsPerTile + 3;
    en.railX0 = r0 * kCellsPerTile;
    en.railX1 = std::max(en.railX0, (r1 + 1) * kCellsPerTile - en.w);
    en.oy = en.y; // home, up against the rail
  }
  if (en.kind == EnemyKind::Puck && e.num("goalie", 0) != 0)
  {
    // Air Hockey's goalie: hops in front of its goal (variant 1).
    en.variant = 1;
    en.active = true;
    en.oy = en.y;
    en.timer = e.num("phase", 0);
  }
  if (en.kind == EnemyKind::Puck || en.kind == EnemyKind::SleeperPod || en.kind == EnemyKind::Mutant ||
      en.kind == EnemyKind::LabArm)
    mCryo.on = true;
  if (!e.id.empty())
    mCryo.ids.push_back({e.id, int(mEnemies.size()) - 1});
}

void World::linkCryo()
{
  auto& c = mCryo;
  c.zeroFriction = mLevel->rules.find("zero_friction") != std::string::npos;
  if (c.zeroFriction)
  {
    c.on = true;
    const std::string& goal = mLevel->goal;
    if (goal.rfind("goals:", 0) == 0)
      c.goalTarget = std::max(1, std::atoi(goal.c_str() + 6));
    // Air Hockey: the Freeze Ray in hand, the pucks frozen for good.
    if (mLevelProto >= 0)
    {
      mPlayer.weapon = Weapon::Proto;
      mPlayer.proto = mLevelProto;
      mPlayer.ammo = protoDef(mLevelProto).maxAmmo;
    }
    for (auto& e : mEnemies)
      if (e.kind == EnemyKind::Puck && e.variant == 0)
      {
        e.frozen = kFreeze;
        e.active = true;
      }
  }
  for (const auto& b : mBreakables)
    c.on = c.on || b.look == 8 || b.look == 9;
  if (!c.on)
    return;
  for (auto& pb : c.boxes)
  {
    const Breakable& b = mBreakables[std::size_t(pb.breakable)];
    mMap.setBlock(b.x0, b.y0, Tile::Solid);
    for (const auto& id : c.ids)
      if (id.first == pb.arm)
        pb.enemy = id.second;
  }
  // The DO NOT OPEN pod and the ice blocks: solid until broken. The pod's
  // back door is the bonus entrance.
  for (std::size_t i = 0; i < mBreakables.size(); ++i)
  {
    const Breakable& b = mBreakables[i];
    if (b.look != 8 && b.look != 9)
      continue;
    for (int y = b.y0; y <= b.y1; ++y)
      for (int x = b.x0; x <= b.x1; ++x)
        mMap.setBlock(x, y, Tile::Solid);
    if (b.look != 8)
      continue;
    c.pod = int(i);
    const CellBox area = blockBox(b.x0, b.y1, b.x1 - b.x0 + 1, b.y1 - b.y0 + 1);
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor && area.intersects(pr.box()))
        pr.dormant = true;
  }
}

void World::resetCryo()
{
  auto& c = mCryo;
  c.slide = c.carry = 0.0f;
  c.fromIce = c.wasAir = false;
  c.frostTick = 0;
  if (c.held >= 0 && c.held < int(mEnemies.size()))
  {
    Enemy& arm = mEnemies[std::size_t(c.held)];
    arm.attach = 4;
    arm.cool = enemyDef(arm.def).cooldown;
  }
  c.held = -1;
}

// --- Ice -----------------------------------------------------------------------------

bool World::iceUnder(const CellBox& b) const
{
  const auto& c = mCryo;
  const int row = b.bottom() + 1;
  bool any = false, ice = false;
  for (int x = b.left(); x <= b.right(); ++x)
  {
    if (!mMap.solidTop(x, row))
      continue;
    any = true;
    ice = ice || c.iceAt(x / kCellsPerTile, row / kCellsPerTile);
  }
  return any && ice;
}

bool World::onIce() const
{
  const auto& p = mPlayer;
  return mCryo.on && (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) &&
    iceUnder(p.box());
}

// The runner on the ground in the labs. Returns false where the usual
// walking rules apply (matte floor, Turbo's grip).
bool World::cryoGround(int mvX, int mvY)
{
  auto& c = mCryo;
  auto& p = mPlayer;
  if (p.turbo > 0 && !c.zeroFriction)
  {
    c.slide = c.carry = 0.0f;
    c.fromIce = false;
    return false;
  }
  const bool ice = iceUnder(p.box());
  const bool heavy = mCharacterIndex == 1;
  if (!ice)
  {
    // Off the ice onto matte: a slide stops within 2 cells; walking takes over.
    if (c.slide == 0.0f || (mvX != 0 && mvY == 0))
    {
      c.slide = c.carry = 0.0f;
      c.fromIce = false;
      return false;
    }
    const float s = std::abs(c.slide) - 0.5f;
    c.slide = s <= 0.0f ? 0.0f : std::copysign(s, c.slide);
    if (c.slide == 0.0f)
    {
      c.carry = 0.0f;
      c.fromIce = false;
    }
  }
  else
  {
    c.fromIce = true;
    const int push = mvY == 0 ? mvX : 0;
    const float accel = heavy ? 0.1f : 0.125f, decel = heavy ? 0.05f : 0.0625f, brake = 0.125f;
    if (push != 0)
    {
      p.facing = push; // no frame lost turning on ice
      if (c.slide * float(push) < 0.0f)
        c.slide += float(push) * brake;
      else
        c.slide = float(push) * std::min(1.0f, std::abs(c.slide) + accel);
    }
    else if (!c.zeroFriction)
    {
      const float s = std::abs(c.slide) - decel;
      c.slide = s <= 0.0f ? 0.0f : std::copysign(s, c.slide);
    }
  }
  if (mvY != 0)
  {
    if (mvX != 0)
      p.facing = mvX; // turning round crouched, no push
    p.stance = mvY < 0 ? Stance::Up : Stance::Crouched;
    setVisual(mvY < 0 ? PlayerVisual::LookingUp : PlayerVisual::Crouching);
  }
  else
    setVisual(PlayerVisual::Standing);
  c.carry += c.slide;
  const int steps = int(c.carry); // toward zero
  c.carry -= float(steps);
  const int dir = sgn(steps);
  for (int i = 0; i < std::abs(steps); ++i)
  {
    if (mMap.moveHorizontallyWithStairStepping(p.x, p.y, Player::kWidth, p.height(), dir) != MoveResult::Completed)
    {
      c.slide = c.carry = 0.0f; // stops dead against a wall
      break;
    }
    if (mvY == 0)
      setVisual(PlayerVisual::Walking);
    ++p.walkFrame;
  }
  // A kicker crossed at speed: a launch that keeps the speed.
  if (ice && std::abs(c.slide) >= 0.75f)
    for (const auto& k : c.kickers)
    {
      if (p.y / kCellsPerTile != k.by || (p.x + 1) / kCellsPerTile != k.bx || c.slide * float(k.dir) <= 0.0f)
        continue;
      c.slide = float(k.dir);
      startLaunch(k.launch + (mCharacterIndex == 2 ? 2 : 0));
      const Vec2 at = cellCenter({p.x, p.y, Player::kWidth, 1});
      burst(at, rgb(230, 250, 255), rgb(150, 210, 255), 14, 2.0f);
      playSound(Sfx::Jump);
      break;
    }
  return true;
}

// In the air off the ice the slide carries on; steering takes over.
bool World::cryoAir(int mvX)
{
  auto& c = mCryo;
  auto& p = mPlayer;
  if (!c.on || (p.turbo > 0 && !c.zeroFriction) || (c.slide == 0.0f && !c.fromIce))
    return false;
  if (mvX != 0)
  {
    c.slide = float(mvX);
    c.carry = 0.0f;
    return false;
  }
  if (c.slide == 0.0f)
    return false;
  c.carry += c.slide;
  const int steps = int(c.carry);
  c.carry -= float(steps);
  for (int i = 0; i < std::abs(steps); ++i)
    if (mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), sgn(steps)) != MoveResult::Completed)
    {
      c.slide = c.carry = 0.0f;
      break;
    }
  return true;
}

// --- The Freeze Ray --------------------------------------------------------------------

void World::freezeEnemy(Enemy& e)
{
  e.frozen = kFreeze;
  e.vx = e.fx = 0.0f;
  e.tell = 0;
  e.dive = 0;
  if (e.kind == EnemyKind::Mutant || e.kind == EnemyKind::Puck)
    e.attach = 0;
  const Vec2 c = cellCenter(e.box());
  burst(c, rgb(240, 252, 255), rgb(140, 220, 255), 16, 1.6f);
  flashAt(c, 70.0f, rgb(150, 230, 255), 10);
  playSound(Sfx::Freeze);
}

void World::shatterFrozen(Enemy& e)
{
  const Vec2 c = cellCenter(e.box());
  burst(c, rgb(240, 252, 255), rgb(120, 200, 255), 26, 2.8f);
  playSound(Sfx::Tink);
  e.frozen = 0;
  e.vx = 0.0f;
  killEnemy(e);
}

// Shots at the HOST (SPARE) pod once its frost has cleared: tink.
bool World::shotAtCryoEarly(Projectile& pr, const CellBox& b)
{
  const auto& c = mCryo;
  if (!c.hostSeen || c.hostX < 0)
    return false;
  (void)pr;
  const CellBox host{c.hostX, c.hostY - 5, 4, 6};
  if (!host.intersects(b))
    return false;
  burst(cellCenter(b), rgb(255, 255, 255), rgb(160, 220, 255), 6, 1.0f);
  playSound(Sfx::Tink);
  return true;
}

// 0: not ours, 1: the shot is spent, 2: it passes through.
int World::shotAtCryo(Projectile& pr, Enemy& e)
{
  if (e.kind == EnemyKind::SleeperPod)
    return 2; // the pods are behind glass in the back wall
  if (e.frozen > 0)
  {
    const int dir = pr.precise ? (pr.vx < 0.0f ? -1 : (pr.vx > 0.0f ? 1 : 0)) : pr.dx;
    if (e.vx != 0.0f)
      return 1;
    if (dir != 0 && iceUnder(e.box()))
    {
      // Sent sliding the way the shot was going.
      e.vx = kBlockSlide * float(dir);
      e.fx = 0.0f;
      e.frozen = std::max(e.frozen, 40);
      burst(cellCenter(pr.box()), rgb(255, 255, 255), rgb(160, 220, 255), 8, 1.4f);
      playSound(Sfx::Tink);
      return 1;
    }
    damageEnemy(e, pr.damage);
    if (!e.alive)
    {
      const Vec2 c = cellCenter(e.box());
      burst(c, rgb(240, 252, 255), rgb(120, 200, 255), 20, 2.4f);
    }
    return 1;
  }
  if (e.kind == EnemyKind::Puck && e.variant == 1)
  {
    burst(cellCenter(pr.box()), rgb(255, 255, 255), rgb(160, 220, 255), 6, 1.0f);
    playSound(Sfx::Tink);
    return 1; // the goalie shrugs shots off
  }
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::FreezeRay) && e.kind != EnemyKind::LabArm)
  {
    damageEnemy(e, pr.damage);
    if (e.alive)
      freezeEnemy(e);
    if (!e.alive)
      ++mStats.protoKills;
    return 1;
  }
  return 0;
}

// A frozen enemy: a block of ice that falls, and slides once it is shot.
void World::updateFrozen(Enemy& e)
{
  auto& c = mCryo;
  if (!c.zeroFriction && --e.frozen <= 0)
  {
    e.frozen = 0;
    e.vx = 0.0f;
    e.cool = std::max(e.cool, 15);
    return;
  }
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
    {
      e.alive = false;
      return;
    }
  }
  if (e.vx == 0.0f)
    return;
  int dir = e.vx > 0.0f ? 1 : -1;
  e.fx += std::abs(e.vx);
  while (e.fx >= 1.0f && e.alive)
  {
    e.fx -= 1.0f;
    const CellBox next = boxAt(e.x + dir, e.y, e.w, e.h);
    if (mMap.overlapsSolid(next))
    {
      hitBreakable(next, 4, 2); // a power box, a pane of ice
      if (c.zeroFriction)
      {
        // Air Hockey: off the boards at full speed.
        e.vx = -e.vx;
        dir = -dir;
        playSound(Sfx::Clunk);
        continue;
      }
      shatterFrozen(e);
      return;
    }
    bool stopped = false, bounced = false;
    for (auto& o : mEnemies)
    {
      if (&o == &e || !o.alive || o.hidden || o.kind == EnemyKind::SleeperPod || !o.box().intersects(next))
        continue;
      if (o.kind == EnemyKind::Puck && o.variant == 1)
      {
        // Saved by the goalie: back the way it came.
        e.vx = -e.vx;
        dir = -dir;
        bounced = true;
        o.flash = 6;
        playSound(Sfx::Clunk);
        break;
      }
      if (o.frozen > 0 && c.zeroFriction)
      {
        // A puck hits a puck: it stops, the other goes on.
        o.vx = e.vx;
        o.fx = 0.0f;
        e.vx = 0.0f;
        stopped = true;
        playSound(Sfx::Clunk);
        break;
      }
      if (o.stun > 0)
        continue; // already knocked down
      damageEnemy(o, 4);
      if (o.alive)
        o.stun = kKnockdown;
    }
    if (stopped)
      break;
    if (bounced)
      continue;
    e.x += dir;
    const CellBox b = e.box();
    for (auto& v : c.vents)
      if (!v.cheered && b.intersects(blockBox(v.bx, v.by, v.w, v.h)))
      {
        v.cheered = true;
        c.cheer = 30;
        playSound(Sfx::Cheer);
      }
    for (const auto& g : c.goals)
      if (b.intersects(goalBox(g)))
      {
        // Air Hockey: a goal. The puck comes back to the middle of the rink.
        ++c.scored;
        c.scoredFlash = 30;
        addScore(1000, cellCenter(b));
        flashAt(cellCenter(b), 140.0f, rgb(150, 230, 255), 18);
        playSound(Sfx::Cheer);
        // Back at the face-off spot, or the nearest free spot beside it.
        int spot = c.puckX;
        for (int k = 0; k < 24; ++k)
        {
          const int x = c.puckX + ((k % 2) ? -1 : 1) * ((k + 1) / 2) * kCellsPerTile;
          const CellBox at = boxAt(x, c.puckY, e.w, e.h);
          bool free = !mMap.overlapsSolid(at);
          for (const auto& o : mEnemies)
            free = free && (&o == &e || !o.alive || !o.box().intersects(at));
          if (free)
          {
            spot = x;
            break;
          }
        }
        e.x = e.prevX = spot;
        e.y = e.prevY = c.puckY;
        e.vx = e.fx = 0.0f;
        e.drawSnap = true;
        stopped = true;
        break;
      }
    if (stopped)
      break;
  }
  if (e.alive && e.vx != 0.0f && !c.zeroFriction && !iceUnder(e.box()) && mMap.onSolidGround(e.box()))
  {
    const float s = std::abs(e.vx) - 1.0f; // off the ice: stops within 2 blocks
    e.vx = s <= 0.0f ? 0.0f : std::copysign(s, e.vx);
  }
}

// --- The labs' staff -----------------------------------------------------------------

void World::updatePuck(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.variant == 1)
  {
    // The goalie: on the ice 30 frames, then a hop of 6 cells (up 6
    // frames, held 6, down 6) that lets a puck under it.
    const int t = e.timer % 48;
    e.y = e.oy - (t < 30 ? 0 : (t < 36 ? t - 29 : (t < 42 ? 6 : 47 - t)));
    e.dir = p.x < e.x ? -1 : 1;
    return;
  }
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const int dx = (p.x + 1) - (e.x + e.w / 2);
  const int dy = p.y - e.y;
  switch (e.attach)
  {
    case 0: // at rest
      if (e.cool == 0 && std::abs(dx) < 28 && std::abs(dy) < 10)
      {
        e.attach = 1;
        e.tell = def.tell;
        e.dir = dx < 0 ? -1 : 1;
        if (isOnScreen(e.box(), 0))
          playSound(Sfx::Whine);
      }
      break;
    case 1: // spinning up
      if (--e.tell <= 0)
      {
        e.tell = 0;
        e.attach = 2;
        e.dive = 0;
        e.fx = 0.0f;
      }
      break;
    default: // sliding: 1 on ice, 1/2 on matte
    {
      e.fx += iceUnder(e.box()) ? 1.0f : 0.5f;
      while (e.fx >= 1.0f)
      {
        e.fx -= 1.0f;
        const CellBox b = e.box();
        const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
        const bool wall = mMap.overlapsSolid(boxAt(e.x + e.dir, e.y, e.w, e.h));
        const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
        if (wall || ledge)
        {
          e.dir = -e.dir;
          if (++e.dive >= 3)
          {
            e.attach = 0;
            e.cool = def.cooldown;
            e.fx = 0.0f;
            break;
          }
          continue;
        }
        e.x += e.dir;
      }
      break;
    }
  }
}

void World::updateSleeperPod(Enemy& e, const EnemyDef& def)
{
  // attach 0: frosted over, 1: thawed (its mutant comes out when it can),
  // 2: empty. tell counts the frost melting.
  if (e.attach != 0)
    return;
  const auto& p = mPlayer;
  const int dx = (p.x + 1) - (e.x + e.w / 2);
  if (std::abs(dx) <= def.range + 2 && std::abs(p.y - e.y) < 12)
    ++e.tell;
  if (e.tell >= def.tell)
  {
    e.tell = 0;
    e.attach = 1;
  }
}

void World::updateMutant(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, e.attach == 2 ? 1 : 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const int dx = (p.x + 1) - (e.x + 1);
  const int dy = p.y - e.y;
  auto step = [&](int dir) {
    const CellBox b = e.box();
    const int aheadX = dir > 0 ? b.right() + 1 : b.left() - 1;
    if (mMap.overlapsSolid(boxAt(e.x + dir, e.y, e.w, e.h)) || !mMap.solidTop(aheadX, b.bottom() + 1))
      return false;
    e.x += dir;
    return true;
  };
  switch (e.attach)
  {
    case 0: // staggering after the runner
      if (std::abs(dy) > 8 || std::abs(dx) > 48)
        break;
      e.dir = dx < 0 ? -1 : 1;
      if (e.cool == 0 && std::abs(dx) <= def.range * 2 + 2 && std::abs(dy) <= 3)
      {
        e.attach = 1;
        e.tell = def.tell;
        break;
      }
      if (std::abs(dx) > 1 && e.timer % std::max(1, def.stepEvery) == 0)
        step(e.dir);
      break;
    case 1: // arms up
      if (--e.tell <= 0)
      {
        e.tell = 0;
        e.attach = 2;
        e.dive = def.range;
      }
      break;
    default: // the lunge, a cell a frame
      if (--e.dive < 0 || !step(e.dir))
      {
        e.dive = 0;
        e.attach = 0;
        e.cool = def.cooldown;
      }
      break;
  }
}

void World::updateLabArm(Enemy& e, const EnemyDef& def)
{
  // attach 0: riding the rail above the runner, 1: lamp red, 2: dropping,
  // 3: holding the runner, 4: going back up.
  auto& c = mCryo;
  auto& p = mPlayer;
  const int index = int(&e - mEnemies.data());
  if (e.cool > 0)
    --e.cool;
  const int target = std::clamp(p.x + 1 - e.w / 2, e.railX0, e.railX1);
  switch (e.attach)
  {
    case 0:
    {
      const bool under = p.x + 1 >= e.railX0 && p.x + 1 <= e.railX1 + e.w && p.y > e.y &&
        p.y - e.y < 24 && p.state != PlayerState::Dying && c.held < 0;
      if (e.x != target && e.timer % std::max(1, def.stepEvery) == 0)
        e.x += sgn(target - e.x);
      if (under && e.x == target && std::abs(p.x + 1 - (e.x + e.w / 2)) <= 1 && e.cool == 0)
      {
        e.attach = 1;
        e.tell = def.tell;
        playSound(Sfx::Beep);
      }
      break;
    }
    case 1:
      if (--e.tell <= 0)
      {
        e.tell = 0;
        e.attach = 2;
      }
      break;
    case 2:
      for (int i = 0; i < 3; ++i)
      {
        if (c.held < 0 && p.state != PlayerState::Dying && e.box().intersects(p.hitBox()))
        {
          // Caught: held 15 frames, lifted 2 blocks, let go.
          c.held = index;
          e.attach = 3;
          e.dive = kHold;
          e.aimY = kLift;
          e.aimX = p.y; // where the lift starts
          p.state = PlayerState::Falling;
          p.frames = 0;
          playSound(Sfx::Clunk);
          break;
        }
        if (mMap.moveVertically(e.x, e.y, e.w, e.h, 1) != MoveResult::Completed)
        {
          e.attach = 4;
          e.cool = def.cooldown;
          playSound(Sfx::Clunk);
          break;
        }
      }
      break;
    case 3:
    {
      // Up a cell a frame with the runner hanging under the claw.
      const int px = e.x + e.w / 2 - 1;
      if (e.aimY > 0 && !mMap.overlapsSolid(boxAt(px, p.y - 1, Player::kWidth, p.height())) &&
          !mMap.overlapsSolid(boxAt(e.x, p.y - 1 - Player::kHeight, e.w, e.h)))
      {
        --e.aimY;
        p.x = px;
        p.y -= 1;
      }
      e.y = p.y - Player::kHeight;
      if (--e.dive <= 0 || c.held != index)
      {
        if (c.held == index)
        {
          c.held = -1;
          startFalling();
        }
        e.attach = 4;
        e.cool = def.cooldown;
      }
      break;
    }
    default:
      if (e.y > e.oy)
        mMap.moveVertically(e.x, e.y, e.w, e.h, -1);
      if (e.y <= e.oy)
      {
        e.y = e.oy;
        e.attach = 0;
      }
      break;
  }
}

// --- Per frame -------------------------------------------------------------------------

void World::updateCryo(const PlayerInput& input)
{
  (void)input;
  auto& c = mCryo;
  if (!c.on)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (c.cheer > 0)
    --c.cheer;
  if (c.scoredFlash > 0)
    --c.scoredFlash;
  if (c.zeroFriction && p.weapon == Weapon::Proto)
    p.ammo = protoDef(p.proto).maxAmmo; // Air Hockey never runs dry

  // Rocco landing on the thin ice goes through it.
  const bool air = p.state == PlayerState::Jumping || p.state == PlayerState::Falling;
  if (c.wasAir && (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && mCharacterIndex == 1)
    hitBreakable({p.x, p.y + 1, Player::kWidth, 1}, 4, 2);
  c.wasAir = air;

  // Cold draughts.
  bool inFrost = false;
  if (alive)
    for (const auto& f : c.frost)
    {
      const CellBox fb{f.x0 * kCellsPerTile, f.y0 * kCellsPerTile, (f.x1 - f.x0 + 1) * kCellsPerTile,
        (f.y1 - f.y0 + 1) * kCellsPerTile};
      if (!fb.intersects(p.hitBox()))
        continue;
      inFrost = true;
      if (++c.frostTick >= f.every)
      {
        c.frostTick = 0;
        hurtPlayer(f.dmg);
      }
      break;
    }
  if (!inFrost)
    c.frostTick = 0;

  // Sleeper Pods let their mutants out while fewer than 3 are about.
  int mutants = 0;
  for (const auto& e : mEnemies)
    mutants += e.alive && e.kind == EnemyKind::Mutant;
  const int mutantDef = enemyIndex("pod_mutant");
  for (std::size_t i = 0; i < mEnemies.size() && mutants < kMaxMutants; ++i)
  {
    if (!mEnemies[i].alive || mEnemies[i].kind != EnemyKind::SleeperPod || mEnemies[i].attach != 1)
      continue;
    mEnemies[i].attach = 2;
    const Enemy pod = mEnemies[i];
    spawnEnemy(mutantDef, pod.x, pod.y);
    Enemy& m = mEnemies.back();
    m.carrier = pod.carrier;
    m.active = true;
    m.dir = p.x < pod.x ? -1 : 1;
    m.cool = 20;
    ++mutants;
    burst(cellCenter(pod.box()), rgb(220, 245, 255), pod.carrier ? rgb(120, 255, 90) : rgb(150, 210, 255), 20, 2.0f);
    playSound(Sfx::Crash);
  }

  // A power box gone takes its Lab Arm with it.
  for (const auto& pb : c.boxes)
  {
    if (pb.enemy < 0 || !mBreakables[std::size_t(pb.breakable)].broken)
      continue;
    Enemy& arm = mEnemies[std::size_t(pb.enemy)];
    if (!arm.alive)
      continue;
    if (c.held == pb.enemy)
    {
      c.held = -1;
      startFalling();
    }
    killEnemy(arm);
    showMessage("LAB ARM POWERED DOWN");
  }
  if (c.held >= 0 && !mEnemies[std::size_t(c.held)].alive)
  {
    c.held = -1;
    startFalling();
  }

  // The DO NOT OPEN pod, opened: its back door.
  if (c.pod >= 0 && mBreakables[std::size_t(c.pod)].broken)
  {
    const Breakable& b = mBreakables[std::size_t(c.pod)];
    const CellBox area = blockBox(b.x0, b.y1, b.x1 - b.x0 + 1, b.y1 - b.y0 + 1);
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor && pr.dormant && area.intersects(pr.box()))
      {
        pr.dormant = false;
        showMessage("IT SAID DO NOT OPEN");
      }
  }

  // Walking past the HOST (SPARE) pod clears its frost.
  if (c.hostX >= 0 && !c.hostSeen && alive && std::abs((p.x + 1) - (c.hostX + 2)) <= 6 &&
      std::abs(p.y - c.hostY) < 8)
  {
    c.hostSeen = true;
    playSound(Sfx::Freeze);
  }

  // Frozen enemies are blocks of ice to stand on.
  syncPlatformCollision();
}

bool World::cryoCanSave() const
{
  if (mCryo.held >= 0)
    return false;
  for (const auto& e : mEnemies)
    if (e.alive && e.frozen > 0 && e.vx != 0.0f)
      return false;
  return true;
}

} // namespace gr
