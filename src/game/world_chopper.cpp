// Level 7, Chopper Down (SPEC 07): The Hunter. A gunship flies in the
// backdrop and its searchlight spot drifts after the runner; keep it off you
// for 15 frames or a salvo of rockets comes down where it last saw you.
// Awnings, plastic sheets, the office roof and the girders are cover. The
// gunship also drops Rappel Troopers on the girders. Hover Bikers charge
// along the roofs and Shield Troopers block shots from the front. At the top
// of the crane the gunship itself, Black Halo, is the episode's boss.
// Also: Lock-On Rockets (paint up to three targets, release to fire), the
// hook and its latch, and the Pilot Seat bonus (rules=flight).

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(10, 8, 20);
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
const std::string kHaloKey = "black_halo"; // a named key keeps the cached sprite reference warning-free
constexpr int kStrikeFlight = 8;  // frames from launch to impact
constexpr int kRocketGap = 4;     // frames between the rockets of a salvo
constexpr int kStrikeRadius = 4;  // cells: two blocks
constexpr int kRandomLead = 30;   // Black Halo without its light: crosshair frames
constexpr int kRandomEvery = 90;
constexpr int kPaintFrames = 8;   // Lock-On Rockets: frames to paint a target
constexpr int kPaintMax = 3;
constexpr int kPaintReach = 32;   // cells: sixteen blocks in front
constexpr int kRopeFrames = 15;   // a Rappel Trooper's slide down
// Black Halo.
constexpr int kStrafeCycle = 105;
constexpr int kTracer = 15;
constexpr int kSweepSpeed = 3;
constexpr int kOpen = 45;
constexpr int kHatchOpen = 60; // the belly hatch (SPEC: 45; a little longer for a first boss)
constexpr int kPodTroopers = 2; // alive at once from the pods (SPEC: 3)
constexpr int kDropCycle = 150;
constexpr int kPodShadow = 15;
constexpr int kRamCycle = 150;
constexpr int kLineUp = 22;
constexpr int kRamSpeed = 4;
constexpr int kTurn = 45;
constexpr int kRotorPass = 8;
constexpr int kFallFrames = 60;
constexpr int kSplashHold = 45;
constexpr int kExitDrop = 15;
constexpr int kRadioFrames = 75;
constexpr int kBossScore = 50000;
// Pilot Seat.
constexpr int kFlightSpeed = 2;
constexpr int kGunEvery = 2;
constexpr int kRocketEvery = 15;
constexpr int kPopEvery = 45; // all 30 inside the 90 s timer (the spec says 60)
constexpr int kPopMax = 30;
constexpr int kTruckEvery = 135;
constexpr int kTruckMax = 10;
constexpr int kGemPoints = 2000;

int sgn(int v) { return (v > 0) - (v < 0); }

// Where a part sits on the body (nose to the left), cells from the body's
// top-left; mirrored when the gunship faces right.
CellBox partBox(const Boss& b, BossPart part)
{
  CellBox r{};
  switch (part)
  {
    case BossPart::NosePod: r = {1, 6, 3, 2}; break;
    case BossPart::TailPod: r = {11, 6, 3, 2}; break;
    case BossPart::Hatch: r = {6, 5, 4, 2}; break;
    case BossPart::Light: r = {-2, 3, 2, 2}; break;
    case BossPart::Rotor: r = {Boss::kW, 0, 4, 6}; break;
    case BossPart::Count: break;
  }
  if (b.facing > 0)
    r.x = Boss::kW - (r.x + r.w);
  r.x += b.x;
  r.y += b.y;
  return r;
}

// Nearest point of box b to (x, y), squared distance in cells.
int distSq(const CellBox& b, int x, int y)
{
  const int dx = x < b.left() ? b.left() - x : (x > b.right() ? x - b.right() : 0);
  const int dy = y < b.top() ? b.top() - y : (y > b.bottom() ? y - b.bottom() : 0);
  return dx * dx + dy * dy;
}

void ring(Renderer& r, float cx, float cy, float rad, float squash, float width, Color c)
{
  constexpr int n = 24;
  for (int i = 0; i < n; ++i)
  {
    const float a0 = float(i) * 6.2832f / float(n), a1 = float(i + 1) * 6.2832f / float(n);
    r.drawLine(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad * squash, cx + std::cos(a1) * rad,
      cy + std::sin(a1) * rad * squash, width, c);
  }
}

// Corner brackets around a box (px).
void brackets(Renderer& r, float x, float y, float w, float h, float len, Color c)
{
  r.fillRect(x, y, len, 3, c);
  r.fillRect(x, y, 3, len, c);
  r.fillRect(x + w - len, y, len, 3, c);
  r.fillRect(x + w - 3, y, 3, len, c);
  r.fillRect(x, y + h - 3, len, 3, c);
  r.fillRect(x, y + h - len, 3, len, c);
  r.fillRect(x + w - len, y + h - 3, len, 3, c);
  r.fillRect(x + w - 3, y + h - len, 3, len, c);
}

} // namespace

int Boss::total() const
{
  // The light is optional: the fight is the pods, the hatch and the rotor.
  return std::max(0, hp[0]) + std::max(0, hp[1]) + std::max(0, hp[2]) + std::max(0, hp[4]);
}

// --- Level entities ----------------------------------------------------------------

bool World::setupChopperEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  const CellBox rectCells{x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile,
    (y1 - y0 + 1) * kCellsPerTile};
  if (e.kind == "hunter")
  {
    auto& h = mHunter;
    h.on = true;
    const auto zone = e.list("zone");
    if (zone.size() == 2)
    {
      h.zoneX0 = zone[0] * kCellsPerTile;
      h.zoneX1 = (zone[1] + 1) * kCellsPerTile - 1;
    }
    h.r = e.num("r", 3) * kCellsPerTile;
    h.lock = e.num("lock", 15);
    h.salvo = e.num("salvo", 22);
    h.rockets = e.num("rockets", 3);
    h.damage = e.num("damage", 1);
    h.cooldown = e.num("cooldown", 60);
    h.speed = std::max(1, e.num("speed", 1));
    const auto demo = e.list("demo");
    if (demo.size() == 2)
    {
      h.demoX = demo[0] * kCellsPerTile + 1;
      h.demoY = demo[1] * kCellsPerTile + 1;
      h.demoTrigger = e.num("demotrigger", 12) * kCellsPerTile;
      h.demoDone = false;
    }
    h.sx = h.prevSx = float(mPlayer.x + 20);
    h.sy = h.prevSy = float(mPlayer.y - 6);
    return true;
  }
  if (e.kind == "cover" && hasRect)
  {
    mCovers.push_back(rectCells);
    return true;
  }
  if (e.kind == "rappel")
  {
    const auto zone = e.list("zone");
    if (zone.size() != 4)
      return true;
    RappelZone z;
    z.zone = {zone[0] * kCellsPerTile, zone[1] * kCellsPerTile, (zone[2] - zone[0] + 1) * kCellsPerTile,
      (zone[3] - zone[1] + 1) * kCellsPerTile};
    z.max = e.num("max", 2);
    z.period = std::max(30, e.num("period", 150));
    z.next = z.period;
    mRappels.push_back(z);
    return true;
  }
  if (e.kind == "switch" && e.hasPos)
  {
    // Only shootable switches so far: the hook's latch.
    Latch l;
    l.id = e.id;
    l.x = e.x * kCellsPerTile;
    l.y = e.y * kCellsPerTile;
    mLatches.push_back(l);
    for (auto& pl : mPlatforms)
      if (!pl.latchId.empty() && pl.latchId == l.id)
        pl.latch = int(mLatches.size()) - 1;
    return true;
  }
  if (e.kind == "boss" && hasRect)
  {
    auto& b = mBoss;
    b.on = true;
    b.arena = rectCells;
    b.deckY = e.num("deck", y1 - 1) * kCellsPerTile - 1;
    const auto ex = e.list("exit");
    if (ex.size() == 2)
    {
      b.exitX = ex[0];
      b.exitY = ex[1];
    }
    b.x = b.prevX = b.arena.right() - Boss::kW - 2;
    b.y = b.prevY = 6;
    return true;
  }
  if (e.kind == "health" && e.hasPos)
  {
    // full=1: a box that refills every heart.
    ItemBox b;
    b.content = ItemKind::Health;
    b.x = e.x * kCellsPerTile;
    b.y = e.y * kCellsPerTile + 1;
    b.variant = e.num("full", 0) != 0 ? 1 : 0;
    mBoxes.push_back(b);
    return true;
  }
  if (e.kind == "cab" && hasRect)
  {
    mCab = rectCells;
    return true;
  }
  if (e.kind == "popup" && e.hasPos)
  {
    mPopups.emplace_back(e.x * kCellsPerTile, e.y * kCellsPerTile + 1);
    return true;
  }
  if (e.kind == "street" && e.hasPos)
  {
    mStreetY = e.y * kCellsPerTile + 1;
    return true;
  }
  return false;
}

void World::setupChopperEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  if (en.kind == EnemyKind::Shield)
    en.railX0 = en.x; // his post
}

// --- Queries ------------------------------------------------------------------------

bool World::covered() const
{
  const CellBox pb = mPlayer.box();
  const int cx = pb.x + 1;
  for (const auto& c : mCovers)
    if (c.contains(cx, pb.top()) || c.contains(cx, pb.top() + 1))
      return true;
  return false;
}

bool World::bossFight() const
{
  const auto& b = mBoss;
  if (!b.on || (b.phase != BossPhase::Strafe && b.phase != BossPhase::Drop && b.phase != BossPhase::Ram))
    return false;
  return mPlayer.x + 1 >= b.arena.x - 6;
}

bool World::bossTarget(BossPart part, CellBox& box) const
{
  const auto& b = mBoss;
  box = partBox(b, part);
  if (!b.on || b.hp[std::size_t(part)] <= 0)
    return false;
  switch (part)
  {
    case BossPart::NosePod:
    case BossPart::TailPod:
      return b.phase == BossPhase::Strafe && b.open > 0;
    case BossPart::Hatch:
      return b.phase == BossPhase::Drop && b.open > 0;
    case BossPart::Light:
      return b.phase == BossPhase::Drop;
    case BossPart::Rotor:
      return b.phase == BossPhase::Ram && b.ram == 3 && b.passDamage < kRotorPass;
    case BossPart::Count:
      break;
  }
  return false;
}

bool World::latchedPlatform(const Platform& pl) const
{
  return pl.latch >= 0 && std::size_t(pl.latch) < mLatches.size() && !mLatches[std::size_t(pl.latch)].open;
}

bool World::targetBox(int target, CellBox& box) const
{
  if (target >= 0)
  {
    if (std::size_t(target) >= mEnemies.size())
      return false;
    const auto& e = mEnemies[std::size_t(target)];
    box = e.box();
    return e.alive;
  }
  if (target <= kBossTarget && target > kBossTarget - int(BossPart::Count))
  {
    const BossPart part = BossPart(kBossTarget - target);
    bossTarget(part, box);
    return mBoss.on && mBoss.hp[std::size_t(part)] > 0 &&
      (mBoss.phase == BossPhase::Strafe || mBoss.phase == BossPhase::Drop || mBoss.phase == BossPhase::Ram);
  }
  return false;
}

// --- Update ----------------------------------------------------------------------------

void World::updateChopper(const PlayerInput& input)
{
  (void)input;
  if (mFlight)
  {
    updateCardboard();
    return;
  }
  if (!mHunter.on && !mBoss.on && mRappels.empty() && mCab.w == 0)
    return;
  updateHunter();
  updateStrikes();
  updateRappel();
  updateBoss();
  updateRadio();
}

void World::startSalvo(bool demo)
{
  auto& h = mHunter;
  h.beep = h.salvo;
  h.demo = demo;
  h.locking = 0;
  if (demo)
  {
    h.seenX = h.demoX;
    h.seenY = h.demoY;
    showMessage("THE GUNSHIP! STAY OUT OF ITS SEARCHLIGHT");
  }
  else
  {
    showMessage("LOCKED ON - GET UNDER COVER!");
  }
  playSound(Sfx::Beep);
}

void World::fireStrike(int tx, int ty)
{
  Strike s;
  s.x = tx;
  s.y = ty;
  s.t = kStrikeFlight;
  s.r = kStrikeRadius;
  s.damage = mHunter.damage;
  s.fromX = mHunter.sx;
  s.fromY = float(mCamera.y() - 4);
  if (mBoss.on && bossFight())
  {
    // From the boss's nose.
    const CellBox lb = partBox(mBoss, BossPart::Light);
    s.fromX = float(lb.x + 1);
    s.fromY = float(lb.y + 1);
  }
  mStrikes.push_back(s);
  playSound(Sfx::Whistle);
}

void World::updateHunter()
{
  auto& h = mHunter;
  const auto& p = mPlayer;
  h.prevSx = h.sx;
  h.prevSy = h.sy;
  if (!h.on && !(mBoss.on && mBoss.phase == BossPhase::Drop))
    return;
  const CellBox pb = p.box();
  const int pcx = pb.x + 1, pcy = pb.y + pb.h / 2;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Inside the arena the boss takes over: during the Drop phase its own
  // searchlight hunts (or, shot out, it fires at random).
  const bool arena = mBoss.on && pcx >= mBoss.arena.x - 6;
  const bool bossLight = arena && mBoss.phase == BossPhase::Drop && bossFight();
  const bool inZone = bossLight || (h.on && !arena && pcx >= h.zoneX0 && pcx <= h.zoneX1);

  if (bossLight && mBoss.hp[std::size_t(BossPart::Light)] <= 0)
  {
    // Random, slow salvos: a crosshair on a random deck spot.
    h.locking = 0;
    h.beep = 0;
    h.left = 0;
    if (++h.cool >= kRandomEvery)
    {
      h.cool = 0;
      const int x = mBoss.arena.x + 4 + mLogicRng.irange(0, std::max(1, mBoss.arena.w - 8) - 1);
      Strike s;
      s.x = x;
      s.y = mBoss.deckY;
      s.t = kRandomLead;
      s.r = kStrikeRadius;
      s.damage = 1;
      const CellBox lb = partBox(mBoss, BossPart::Light);
      s.fromX = float(lb.x);
      s.fromY = float(lb.y);
      mStrikes.push_back(s);
      playSound(Sfx::Beep);
    }
    return;
  }

  // The yard's demonstration: one salvo at an empty spot, with all the trimmings.
  if (h.on && !h.demoDone && h.demoTrigger >= 0 && pcx >= h.demoTrigger && h.beep == 0 && h.left == 0)
  {
    h.demoDone = true;
    startSalvo(true);
  }

  // The spot drifts toward the runner (or the demonstration's spot).
  float tx = float(pcx) + 0.5f, ty = float(pcy);
  if (h.demo)
  {
    tx = float(h.demoX);
    ty = float(h.demoY);
  }
  else if (bossLight)
  {
    // It starts from the nose.
  }
  else if (!inZone)
  {
    tx = h.sx;
    ty = h.sy;
  }
  const float sp = float(h.speed);
  h.sx += std::clamp(tx - h.sx, -sp, sp);
  h.sy += std::clamp(ty - h.sy, -sp, sp);

  if (h.left > 0)
  {
    if (--h.next <= 0)
    {
      if (!h.demo && alive && !covered())
      {
        h.seenX = pcx;
        h.seenY = pb.bottom();
      }
      fireStrike(h.seenX, h.seenY);
      h.next = kRocketGap;
      if (--h.left == 0)
      {
        h.cool = h.cooldown;
        h.demo = false;
      }
    }
    return;
  }
  if (h.beep > 0)
  {
    if (h.beep % 4 == 0)
      playSound(Sfx::Beep);
    if (--h.beep == 0)
    {
      h.left = h.rockets;
      h.next = 0;
    }
    return;
  }
  if (h.cool > 0)
  {
    --h.cool;
    h.locking = 0;
    return;
  }
  if (h.on && !h.demoDone && h.demoTrigger >= 0)
    return; // fire=0 until the demonstration
  if (!inZone || !alive)
  {
    h.locking = 0;
    return;
  }
  const float dx = h.sx - (float(pcx) + 0.5f), dy = h.sy - float(pcy);
  if (!covered() && dx * dx + dy * dy <= float(h.r * h.r))
  {
    h.seenX = pcx;
    h.seenY = pb.bottom();
    if (++h.locking >= h.lock)
      startSalvo(false);
  }
  else
  {
    h.locking = 0;
  }
}

void World::updateStrikes()
{
  auto& p = mPlayer;
  for (auto& s : mStrikes)
  {
    if (--s.t > 0)
      continue;
    const Vec2 c{(float(s.x) + 0.5f) * kCellSize, (float(s.y) + 0.5f) * kCellSize};
    burst(c, rgb(255, 220, 90), rgb(255, 90, 30), 26, 2.8f);
    burst(c, rgb(255, 255, 255), rgb(120, 120, 130), 10, 1.4f, false);
    flashAt(c, 130.0f, rgb(255, 150, 50), 18);
    mCamera.shake(8, 1.2f);
    playSound(Sfx::Explosion);
    const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
    if (alive && distSq(p.box(), s.x, s.y) <= s.r * s.r)
      hurtPlayer(s.damage);
    for (auto& e : mEnemies)
      if (e.alive && e.active && distSq(e.box(), s.x, s.y) <= s.r * s.r && !(enemyDef(e.def).flags & kEnemyHarmless))
        damageEnemy(e, 2);
  }
  mStrikes.erase(std::remove_if(mStrikes.begin(), mStrikes.end(), [](const Strike& s) { return s.t <= 0; }),
    mStrikes.end());
}

bool World::dropTrooper(int x, int feetY, int fall)
{
  for (int i = 0; i < 3; ++i)
    if (!mMap.solidTop(x + i, feetY + 1))
      return false;
  if (mMap.overlapsSolid(boxAt(x, feetY, 3, 5)))
    return false;
  const int def = enemyIndex("rappel_trooper");
  if (def < 0)
    return false;
  spawnEnemy(def, x, feetY);
  Enemy& e = mEnemies.back();
  e.attach = 1; // on the rope
  e.aimY = feetY;
  e.tell = fall;
  e.y = e.prevY = feetY - fall;
  e.active = true;
  e.variant = 1; // dropped by the gunship
  e.dir = mPlayer.x < x ? -1 : 1;
  e.lastDive = 0;
  return true;
}

void World::updateRappel()
{
  if (mRappels.empty())
    return;
  const auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const CellBox pb = p.box();
  for (auto& z : mRappels)
  {
    if (!z.zone.contains(pb.x + 1, pb.bottom()) || mStats.frames < z.next)
      continue;
    int alive = 0;
    for (const auto& e : mEnemies)
      alive += e.alive && e.kind == EnemyKind::Trooper && e.variant == 1;
    if (alive >= z.max || p.state != PlayerState::OnGround)
    {
      z.next = mStats.frames + 10;
      continue;
    }
    z.next = mStats.frames + z.period;
    for (int d : {6, 8, 10, 4})
      if (dropTrooper(p.x - p.facing * d, pb.bottom(), kRopeFrames))
      {
        playSound(Sfx::Whistle);
        break;
      }
  }
}

// --- Enemies ---------------------------------------------------------------------------

void World::updateTrooper(Enemy& e, const EnemyDef& def)
{
  if (e.attach == 1)
  {
    // Sliding down the rope; it lands after `tell` frames.
    if (e.tell > 0)
      --e.tell;
    e.y = e.aimY - e.tell;
    if (e.tell == 0)
    {
      e.attach = 0;
      playSound(Sfx::Land);
    }
    return;
  }
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const bool level = pb.bottom() >= b.top() - 2 && pb.top() <= b.bottom() + 2;
  const int dist = std::abs((pb.x + 1) - (b.x + 1));
  if (e.tell > 0)
  {
    // Aiming.
    if (--e.tell == 0)
    {
      shootAt(e, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top() + 2, 1, 40);
      e.lastDive = e.timer + def.cooldown;
    }
    return;
  }
  if (level && dist <= def.range && vulnerable && e.timer >= e.lastDive && isOnScreen(b, 0))
  {
    e.dir = pb.x + 1 < b.x + 1 ? -1 : 1;
    e.tell = def.tell;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  if (wall || ledge)
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::updateBiker(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  switch (e.attach)
  {
    case 0: // charging, two cells a frame
      for (int i = 0; i < 2; ++i)
      {
        const CellBox b = e.box();
        const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
        const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
        if (wall || !mMap.solidTop(aheadX, b.bottom() + 1))
        {
          // The end of its run: brake and turn.
          e.attach = 1;
          e.dive = std::max(1, def.cooldown - def.tell);
          break;
        }
        e.x += e.dir;
      }
      break;
    case 1: // turning
      if (--e.dive <= 0)
      {
        e.dir = -e.dir;
        e.attach = 2;
        e.tell = def.tell;
        if (isOnScreen(e.box(), 2))
          playSound(Sfx::Rev);
      }
      break;
    default: // revving, headlight on
      if (--e.tell <= 0)
      {
        e.tell = 0;
        e.attach = 0;
      }
      break;
  }
}

void World::updateShield(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const bool level = pb.bottom() >= b.top() - 2 && pb.top() <= b.bottom() + 2;
  const int pcx = pb.x + 1, ecx = b.x + 1;
  const int dist = std::abs(pcx - ecx);
  if (e.tell > 0)
  {
    // The gun comes up over the shield.
    if (--e.tell == 0)
    {
      shootAt(e, e.dir > 0 ? b.right() + 1 : b.left() - 1, b.top() + 1, 1, 40);
      e.lastDive = e.timer + def.cooldown;
    }
    return;
  }
  const bool near = level && dist <= def.range && vulnerable;
  if (near)
    e.dir = pcx < ecx ? -1 : 1; // the shield toward you
  if (near && e.timer >= e.lastDive && isOnScreen(b, 0))
  {
    e.tell = def.tell;
    return;
  }
  if (near || e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int nx = e.x + e.dir;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool blocked = nx < e.railX0 - 8 || nx > e.railX0 + 8 ||
    (e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) || !mMap.solidTop(aheadX, b.bottom() + 1);
  if (blocked)
    e.dir = -e.dir;
  else
    e.x = nx;
}

// --- Black Halo --------------------------------------------------------------------------

void World::bossPhase(BossPhase phase)
{
  auto& b = mBoss;
  b.phase = phase;
  if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
    std::fprintf(stderr, "boss phase %d at frame %d\n", int(phase), mStats.frames);
  b.t = 0;
  b.open = 0;
  b.sweepX = b.prevSweepX = -1;
  b.ram = 0;
  b.passDamage = 0;
  b.podX = -1;
  b.facing = -1;
  const auto drop = [&](ItemKind kind, int bx) {
    Item it;
    it.kind = kind;
    it.x = it.prevX = bx * kCellsPerTile;
    it.y = it.prevY = 4;
    it.pickupDelay = 4;
    mItems.push_back(it);
  };
  switch (phase)
  {
    case BossPhase::Strafe:
      b.t = -30; // a breath before the first sweep
      showMessage("BLACK HALO!");
      mMusicOverride = "boss_black_halo";
      break;
    case BossPhase::Drop:
      drop(ItemKind::Health, 175);
      showMessage("THE GUN PODS ARE GONE - HIT THE BELLY HATCH WHEN IT OPENS");
      break;
    case BossPhase::Ram:
      drop(ItemKind::Health, 177);
      drop(ItemKind::Turbo, 180);
      showMessage("ITS TAIL ROTOR IS SMOKING - IT'S GOING TO RAM!");
      break;
    case BossPhase::Falling:
      b.fallT = 0;
      for (auto& e : mEnemies)
        if (e.alive && e.kind == EnemyKind::Trooper)
          killEnemy(e);
      mStrikes.clear();
      addScore(kBossScore, cellCenter(b.body()));
      showMessage("BLACK HALO IS GOING DOWN!");
      playSound(Sfx::Explosion);
      break;
    case BossPhase::Done:
      b.exitT = 0;
      if (mLevel)
        mMusicOverride = mLevel->music;
      break;
    case BossPhase::Waiting:
      break;
  }
}

void World::resetBossCycle()
{
  auto& b = mBoss;
  mStrikes.clear();
  mHunter.beep = mHunter.left = mHunter.locking = 0;
  mHunter.cool = mHunter.cooldown;
  if (!b.on || (b.phase != BossPhase::Strafe && b.phase != BossPhase::Drop && b.phase != BossPhase::Ram))
    return;
  b.t = -30;
  b.open = 0;
  b.sweepX = b.prevSweepX = -1;
  b.ram = 0;
  b.passDamage = 0;
  b.podX = -1;
  b.facing = -1;
}

void World::damageBoss(BossPart part, int damage, Vec2 at)
{
  auto& b = mBoss;
  CellBox box;
  if (!bossTarget(part, box))
    return;
  if (part == BossPart::Rotor)
  {
    damage = std::min(damage, kRotorPass - b.passDamage);
    b.passDamage += damage;
    burst(at, rgb(90, 90, 100), rgb(40, 40, 46), 14, 1.2f, false); // a smoke burst
  }
  int& hp = b.hp[std::size_t(part)];
  hp -= damage;
  b.flash = 6;
  burst(at, rgb(255, 255, 255), rgb(255, 160, 60), 8, 1.6f);
  if (hp > 0)
  {
    playSound(Sfx::Hit);
    return;
  }
  hp = 0;
  const Vec2 c = cellCenter(box);
  burst(c, rgb(255, 220, 90), rgb(255, 90, 30), 30, 3.0f);
  flashAt(c, 140.0f, rgb(255, 150, 50), 20);
  mCamera.shake(10, 1.5f);
  playSound(Sfx::Explosion);
  addScore(part == BossPart::Light ? 2000 : 5000, c);
  switch (part)
  {
    case BossPart::NosePod:
    case BossPart::TailPod:
      if (b.hp[0] <= 0 && b.hp[1] <= 0)
        bossPhase(BossPhase::Drop);
      break;
    case BossPart::Hatch:
      bossPhase(BossPhase::Ram);
      break;
    case BossPart::Light:
      showMessage("SEARCHLIGHT OUT - IT'S FIRING BLIND");
      mHunter.cool = 0;
      break;
    case BossPart::Rotor:
      bossPhase(BossPhase::Falling);
      break;
    case BossPart::Count:
      break;
  }
}

bool World::shotAtBoss(Projectile& pr)
{
  const CellBox s = pr.box();
  // The hook's latch.
  for (auto& l : mLatches)
    if (!l.open && l.box().intersects(s))
    {
      l.open = true;
      const Vec2 c = cellCenter(l.box());
      burst(c, rgb(255, 230, 120), rgb(200, 200, 210), 12, 1.6f);
      playSound(Sfx::Clunk);
      showMessage("THE HOOK'S LATCH IS OPEN");
      return true;
    }
  auto& b = mBoss;
  if (!b.on || (b.phase != BossPhase::Strafe && b.phase != BossPhase::Drop && b.phase != BossPhase::Ram))
    return false;
  for (int i = 0; i < int(BossPart::Count); ++i)
  {
    CellBox box;
    if (!bossTarget(BossPart(i), box) || !box.intersects(s))
      continue;
    if (pr.kind == ShotKind::Rocket || pr.radius > 0)
      explodeAt(s.x + s.w / 2, s.y, pr.radius > 0 ? pr.radius : 3, pr.damage);
    else
      damageBoss(BossPart(i), pr.damage, cellCenter(s));
    return true;
  }
  CellBox hull = b.body();
  CellBox rotor = partBox(b, BossPart::Rotor);
  if (hull.intersects(s) || rotor.intersects(s))
  {
    // Armour: sparks and a clang.
    burst(cellCenter(s), rgb(255, 255, 210), rgb(150, 150, 170), 5, 1.0f);
    if (pr.kind == ShotKind::Rocket)
      explodeAt(s.x + s.w / 2, s.y, 3, 0);
    return true;
  }
  return false;
}

void World::explodeAtBoss(int cx, int cy, int radius, int damage)
{
  if (!mBoss.on || damage <= 0)
    return;
  const CellBox area{cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1};
  for (int i = 0; i < int(BossPart::Count); ++i)
  {
    CellBox box;
    if (bossTarget(BossPart(i), box) && box.intersects(area))
      damageBoss(BossPart(i), damage, cellCenter(box));
  }
}

void World::updateBoss()
{
  auto& b = mBoss;
  if (!b.on)
    return;
  b.prevX = b.x;
  b.prevY = b.y;
  b.prevSweepX = b.sweepX;
  if (b.flash > 0)
    --b.flash;
  auto& p = mPlayer;
  const CellBox pb = p.box();
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int deckLeft = b.arena.x - 6, deckRight = b.arena.right();
  auto glide = [&](int tx, int ty, int sp) {
    b.x += std::clamp(tx - b.x, -sp, sp);
    b.y += std::clamp(ty - b.y, -sp, sp);
  };

  switch (b.phase)
  {
    case BossPhase::Waiting:
      // Hovers off the jib's end until the runner tops the mast.
      if (alive && p.state == PlayerState::OnGround && pb.x + 1 >= b.arena.x - 4 && pb.bottom() <= b.deckY + 1)
        bossPhase(BossPhase::Strafe);
      return;
    case BossPhase::Falling:
    {
      ++b.fallT;
      // Spins away into the bay (drawn in the backdrop).
      if (b.fallT <= kFallFrames)
      {
        b.x += 1;
        b.y += 1;
      }
      if (b.fallT == kFallFrames)
      {
        flashAt(cellCenter(b.body()), 300.0f, rgb(200, 230, 255), 30);
        playSound(Sfx::Splash);
        mCamera.shake(16, 2.0f);
      }
      if (b.fallT >= kFallFrames + kSplashHold)
        bossPhase(BossPhase::Done);
      return;
    }
    case BossPhase::Done:
      if (b.exitT >= 0 && b.exitT < kExitDrop)
        ++b.exitT;
      return;
    default:
      break;
  }
  // The fight waits while the runner is down the mast.
  if (pb.x + 1 < deckLeft || !alive)
    return;
  ++b.t;
  if (b.open > 0)
    --b.open;

  switch (b.phase)
  {
    case BossPhase::Strafe:
    {
      // Hover at the right; tracer, chaingun sweep, then the pods are open.
      glide(b.arena.right() - Boss::kW - 3, 6 + ((b.t / 8) % 2), 1);
      if (b.t == 0)
        playSound(Sfx::Warn);
      if (b.t == kTracer)
      {
        b.sweepX = deckLeft;
        playSound(Sfx::Scream);
      }
      if (b.sweepX >= 0)
      {
        const int from = b.sweepX;
        b.sweepX += kSweepSpeed;
        // The sweep runs a row above the deck: jump it.
        const CellBox swept{from, b.deckY - 1, b.sweepX - from + 1, 2};
        if (alive && swept.intersects(p.hitBox()))
          hurtPlayer(1);
        if (b.sweepX > deckRight)
        {
          b.sweepX = -1;
          b.open = kOpen;
          showMessage("GUN PODS EXPOSED!");
        }
      }
      if (b.t >= kStrafeCycle)
        b.t = 0;
      break;
    }
    case BossPhase::Drop:
    {
      glide(b.arena.x + b.arena.w / 2 - Boss::kW / 2, 6 + ((b.t / 8) % 2), 1);
      if (b.t == 1)
      {
        // A trooper pod: its shadow first, then it lands and opens.
        b.podX = b.arena.x + 12 + mLogicRng.irange(0, std::max(1, b.arena.w - 28) - 1);
        b.podY = b.y + 6;
        b.podT = kPodShadow;
        b.open = kHatchOpen;
        playSound(Sfx::Clunk);
      }
      if (b.podX >= 0 && --b.podT <= 0)
      {
        int alive3 = 0;
        for (const auto& e : mEnemies)
          alive3 += e.alive && e.kind == EnemyKind::Trooper;
        for (int k = 0; k < 2 && alive3 < kPodTroopers; ++k)
          if (dropTrooper(b.podX + (k == 0 ? -4 : 3), b.deckY, 0))
            ++alive3;
        burst({(float(b.podX) + 1.0f) * kCellSize, float(b.deckY) * kCellSize}, rgb(200, 200, 210), rgb(90, 90, 100), 16,
          2.0f, false);
        playSound(Sfx::Land);
        b.podX = -1;
      }
      if (b.t >= kDropCycle)
        b.t = 0;
      break;
    }
    case BossPhase::Ram:
    {
      const int hoverX = b.arena.right() - Boss::kW - 3;
      const int turnX = b.arena.x + 12; // the nose clips the cab's corner here
      switch (b.ram)
      {
        case 0:
          b.facing = -1;
          glide(hoverX, 4 + ((b.t / 8) % 2), 2);
          if (b.t >= 40)
          {
            b.ram = 1;
            playSound(Sfx::Scream);
            showMessage("RAM! CROUCH!");
          }
          break;
        case 1: // lining up past the jib's end
          glide(b.arena.right() + 3, b.deckY - 10, 2);
          if (b.t >= 40 + kLineUp)
          {
            b.x = b.arena.right() + 3;
            b.y = b.deckY - 10;
            b.ram = 2;
          }
          break;
        case 2:
        {
          // Rows 4-7 of the jib: a crouched runner (rows 8-9) is under it.
          b.x -= kRamSpeed;
          const CellBox hit{b.x, b.y - 1, Boss::kW + 4, 8};
          if (alive && hit.intersects(p.hitBox()))
            hurtPlayer(1);
          if (b.x <= turnX)
          {
            b.x = turnX;
            b.ram = 3;
            b.facing = 1; // turns over the cab: the tail rotor is open
            b.y = 6;
            b.passDamage = 0;
            const Vec2 c{float(turnX) * kCellSize, float(b.deckY - 6) * kCellSize};
            burst(c, rgb(255, 230, 120), rgb(255, 255, 255), 20, 2.4f);
            playSound(Sfx::Clunk);
            b.open = kTurn;
          }
          break;
        }
        default:
          if (b.open <= 0)
          {
            b.ram = 0;
            b.facing = -1;
          }
          break;
      }
      if (b.t >= kRamCycle && b.ram == 0)
        b.t = 0;
      break;
    }
    default:
      break;
  }
}

// --- The crane cab's radio, and the Pilot Seat bonus -----------------------------------------

void World::updateRadio()
{
  if (mCab.w == 0 || !mLevel)
    return;
  const auto& p = mPlayer;
  const bool inside = p.state == PlayerState::OnGround && mCab.intersects(p.box()) && !bossFight() &&
    (!mBoss.on || mBoss.phase == BossPhase::Waiting || mBoss.phase == BossPhase::Done);
  if (inside)
  {
    if (++mRadio == kRadioFrames)
    {
      mMusicOverride = "elevator_" + mLevel->music;
      showMessage("THE CAB RADIO: THE THEME, AS ELEVATOR MUSIC");
    }
  }
  else
  {
    if (mRadio >= kRadioFrames)
      mMusicOverride = mLevel->music;
    mRadio = 0;
  }
}

void World::updateFlight(const PlayerInput& input)
{
  auto& p = mPlayer;
  const int dx = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  const int dy = (input.down ? 1 : 0) - (input.up ? 1 : 0);
  if (dx != 0)
    p.facing = dx;
  p.x = std::clamp(p.x + dx * kFlightSpeed, 0, mMap.width() - 6);
  p.y = std::clamp(p.y + dy * kFlightSpeed, 3, 39);
  p.state = PlayerState::OnGround;
  setVisual(PlayerVisual::Standing);
  if (mFlightGun > 0)
    --mFlightGun;
  if (mFlightRocket > 0)
    --mFlightRocket;
  if (!input.fire.pressed)
    return;
  const int muzzleX = p.facing > 0 ? p.x + 6 : p.x - 2;
  if (input.down)
  {
    if (mFlightRocket > 0)
      return;
    mFlightRocket = kRocketEvery;
    Projectile pr;
    pr.kind = ShotKind::Rocket;
    pr.precise = true;
    pr.fx = float(p.x + 2);
    pr.fy = float(p.y + 1);
    pr.vx = 0.35f * float(p.facing);
    pr.vy = 0.5f;
    pr.gy = 0.15f;
    pr.speed = 2;
    pr.damage = 4;
    pr.radius = 4;
    pr.w = 2;
    pr.h = 2;
    pr.x = pr.prevX = p.x + 2;
    pr.y = pr.prevY = p.y + 1;
    pr.lob = true;
    pr.dx = p.facing;
    pr.dy = 1;
    mProjectiles.push_back(pr);
    playSound(Sfx::RocketShot);
    return;
  }
  if (mFlightGun > 0)
    return;
  mFlightGun = kGunEvery;
  spawnProjectile(ShotKind::Normal, muzzleX, p.y, p.facing, 0);
  mProjectiles.back().speed = 3;
  mProjectiles.back().damage = 1;
  playSound(Sfx::Shot);
}

void World::updateCardboard()
{
  // A gem for every 2000 points.
  while (mStats.score - mGemScore >= kGemPoints)
  {
    mGemScore += kGemPoints;
    ++mStats.gems;
    ++mStats.gemsTotal;
    playSound(Sfx::Gem);
  }
  const int camL = mCamera.x(), camR = mCamera.x() + int(kViewCellsW);
  if (mPops < kPopMax && !mPopups.empty() && mStats.frames >= mPopNext)
  {
    mPopNext = mStats.frames + kPopEvery;
    // The next spot on screen, or near it.
    for (std::size_t k = 0; k < mPopups.size(); ++k)
    {
      const auto [x, y] = mPopups[(std::size_t(mPops) + k) % mPopups.size()];
      if (x < camL - 8 || x > camR + 8)
        continue;
      spawnEnemy(enemyIndex("cardboard_runner"), x, y);
      Enemy& e = mEnemies.back();
      e.active = true;
      e.dir = (mPops % 2) ? 1 : -1;
      burst(cellCenter(e.box()), rgb(210, 170, 110), rgb(150, 110, 60), 10, 1.4f, false);
      playSound(Sfx::Clunk);
      ++mPops;
      break;
    }
  }
  if (mTrucks < kTruckMax && mStreetY >= 0 && mStats.frames >= mTruckNext)
  {
    mTruckNext = mStats.frames + kTruckEvery;
    const bool fromLeft = mTrucks % 2 == 0;
    const int x = std::clamp(fromLeft ? camL - 6 : camR - 2, 1, mMap.width() - 9);
    spawnEnemy(enemyIndex("cardboard_truck"), x, mStreetY);
    Enemy& e = mEnemies.back();
    e.active = true;
    e.dir = fromLeft ? 1 : -1;
    ++mTrucks;
  }
}

// --- Lock-On Rockets ---------------------------------------------------------------------------

void World::paintTarget()
{
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  const int pcx = pb.x + 1, pcy = pb.y + 2;
  int best = kNoTarget, bestD = 1 << 30;
  auto consider = [&](int code, const CellBox& b) {
    if (std::find(mPaint.begin(), mPaint.end(), code) != mPaint.end())
      return;
    const int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    const int dx = cx - pcx, dy = cy - pcy;
    if (dx * p.facing < -2 || std::abs(dx) > kPaintReach || std::abs(dy) > 20)
      return;
    const int d = dx * dx + dy * dy;
    if (d < bestD)
    {
      bestD = d;
      best = code;
    }
  };
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    const auto& e = mEnemies[i];
    if (e.alive && e.active && !e.trapped && e.y >= 0)
      consider(int(i), e.box());
  }
  for (int i = 0; i < int(BossPart::Count); ++i)
  {
    CellBox b;
    if (bossTarget(BossPart(i), b))
      consider(kBossTarget - i, b);
  }
  if (best == kNoTarget)
    return;
  mPaint.push_back(best);
  playSound(Sfx::Beep);
}

void World::launchRockets()
{
  auto& p = mPlayer;
  const int proto = p.proto;
  if (mPaint.empty())
  {
    mNextTarget = kNoTarget;
    fireShot();
  }
  for (const int t : mPaint)
  {
    if (p.weapon != Weapon::Proto || p.proto != proto)
      break;
    mNextTarget = t;
    fireShot();
  }
  mNextTarget = kNoTarget;
  mPaint.clear();
}

void World::steerRocket(Projectile& pr)
{
  CellBox b;
  if (pr.target == kNoTarget)
    return;
  if (!targetBox(pr.target, b))
  {
    pr.target = kNoTarget; // gone: fly on straight
    return;
  }
  const float tx = float(b.x) + float(b.w) * 0.5f, ty = float(b.y) + float(b.h) * 0.5f;
  float dx = tx - pr.fx, dy = ty - pr.fy;
  const float len = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
  dx /= len;
  dy /= len;
  // Climbs first, then curves in from above.
  const float turn = pr.age < 4 ? 0.15f : 0.45f;
  float vx = pr.vx + dx * turn, vy = pr.vy + dy * turn;
  const float vl = std::max(0.01f, std::sqrt(vx * vx + vy * vy));
  pr.vx = vx / vl;
  pr.vy = vy / vl;
  pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
  pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
}

// --- Drawing ----------------------------------------------------------------------------------------

void World::drawChopperProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame,
  bool foreground) const
{
  switch (pr.kind)
  {
    case PropKind::Girder:
      if (foreground)
      {
        // A red I-beam over its solid row: flanges, a web with lightening
        // holes, rivets, and any stamp on it ("BEAM 42").
        r.fillRect(x, y, w, h, rgb(150, 44, 30));
        r.fillRect(x, y, w, 8.0f, rgb(220, 80, 50));
        r.fillRect(x, y + h - 8.0f, w, 8.0f, rgb(110, 30, 20));
        for (float hx = x + 24.0f; hx < x + w - 24.0f; hx += 64.0f)
          r.fillRect(hx, y + h * 0.35f, 24.0f, h * 0.3f, rgb(70, 22, 16));
        for (float rx = x + 8.0f; rx < x + w - 4.0f; rx += 16.0f)
        {
          r.fillRect(rx, y + 2.0f, 4.0f, 4.0f, rgb(250, 150, 110));
          r.fillRect(rx, y + h - 6.0f, 4.0f, 4.0f, rgb(170, 60, 40));
        }
        if (!pr.text.empty())
          r.drawText(pr.text, x + w * 0.5f, y + h * 0.5f - 11.0f, {20.0f, rgb(255, 230, 200), kInk}, Align::Center);
      }
      break;
    case PropKind::Sheet:
      if (foreground)
      {
        // Hanging plastic sheeting: sways, and the light can't get through.
        const float sway = 6.0f * std::sin(float(frame) * 0.07f + float(pr.x));
        r.fillRect(x - 4.0f, y, w + 8.0f, 6.0f, rgb(120, 120, 130));
        for (float sx = x; sx < x + w; sx += 16.0f)
        {
          const float s = sway * (0.5f + 0.5f * std::sin(sx * 0.1f));
          r.fillRect(sx + s * 0.3f, y + 6.0f, 16.0f, h - 6.0f, rgba(220, 230, 240, 70));
          r.drawLine(sx + 2.0f, y + 6.0f, sx + 2.0f + s, y + h, 2.0f, rgba(255, 255, 255, 60));
        }
      }
      break;
    case PropKind::Awning:
      if (!foreground)
      {
        for (float sx = x; sx < x + w; sx += 32.0f)
          r.fillRect(sx, y, std::min(32.0f, x + w - sx), 22.0f,
            int((sx - x) / 32.0f) % 2 ? rgb(240, 230, 210) : rgb(40, 130, 90));
        for (float sx = x; sx < x + w; sx += 32.0f)
          r.fillRect(sx + 4.0f, y + 22.0f, 24.0f, 8.0f, int((sx - x) / 32.0f) % 2 ? rgb(220, 210, 190) : rgb(30, 110, 76));
        r.fillRect(x, y + 30.0f, 4.0f, h - 30.0f, rgb(80, 80, 90));
        r.fillRect(x + w - 4.0f, y + 30.0f, 4.0f, h - 30.0f, rgb(80, 80, 90));
      }
      break;
    case PropKind::Office:
      if (!foreground)
      {
        // The site office: a cabin's back wall, a window, a plan on the wall.
        r.fillRect(x, y, w, h, rgb(58, 66, 74));
        for (float sx = x; sx < x + w; sx += 24.0f)
          r.fillRect(sx, y, 3.0f, h, rgb(46, 52, 60));
        r.fillRect(x + w * 0.15f, y + h * 0.15f, w * 0.25f, h * 0.35f, rgb(30, 50, 80));
        r.fillRect(x + w * 0.15f, y + h * 0.15f, w * 0.25f, 4.0f, rgb(150, 170, 200));
        r.fillRect(x + w * 0.55f, y + h * 0.18f, w * 0.3f, h * 0.3f, rgb(220, 220, 200));
        r.drawLine(x + w * 0.58f, y + h * 0.4f, x + w * 0.82f, y + h * 0.22f, 2.0f, rgb(60, 80, 160));
        r.drawText("SAFETY FIRST", x + w * 0.7f, y + h * 0.55f, {16.0f, rgb(255, 210, 60), kInk}, Align::Center);
      }
      break;
    case PropKind::Lattice:
      if (!foreground)
      {
        const Color c = rgb(200, 150, 40);
        r.fillRect(x, y, w, 6.0f, c);
        r.fillRect(x, y + h - 6.0f, w, 6.0f, c);
        for (float sx = x; sx < x + w; sx += 64.0f)
        {
          r.drawLine(sx, y + 3.0f, sx + 32.0f, y + h - 3.0f, 4.0f, withAlpha(c, 200));
          r.drawLine(sx + 32.0f, y + h - 3.0f, sx + 64.0f, y + 3.0f, 4.0f, withAlpha(c, 200));
        }
      }
      break;
    case PropKind::SpareShip:
      if (!foreground)
      {
        // The spare gunship parked on its deck: the same airframe, dark.
        const Sprite& spr = styledEnemySprite(mArt, r, mTheme, kHaloKey, 0, 0, Boss::kW, Boss::kH);
        DrawOpts o;
        o.tint = rgb(150, 150, 170);
        o.scale = w / (float(Boss::kW) * kCellPx);
        r.draw(spr.get(1), x + w * 0.5f, y + h, o);
        r.drawText("SPARE", x + w * 0.5f, y - 24.0f, {16.0f, rgb(255, 200, 60), kInk}, Align::Center, 0.8f);
      }
      break;
    case PropKind::Radio:
      if (!foreground)
      {
        r.fillRect(x + 8.0f, y + h - 30.0f, w - 16.0f, 30.0f, rgb(60, 50, 44));
        r.fillRect(x + 14.0f, y + h - 24.0f, 20.0f, 18.0f, rgb(30, 30, 30));
        r.fillRect(x + w - 30.0f, y + h - 22.0f, 12.0f, 6.0f, rgb(255, 180, 60));
        r.drawLine(x + w - 14.0f, y + h - 30.0f, x + w - 4.0f, y + h - 60.0f, 2.0f, rgb(180, 180, 190));
        if (mRadio >= kRadioFrames)
          for (int k = 0; k < 3; ++k)
          {
            const float t = std::fmod(float(frame) * 0.02f + float(k) / 3.0f, 1.0f);
            r.drawText(k % 2 ? "~" : "*", x + w * 0.5f + std::sin(t * 6.0f) * 14.0f, y + h - 40.0f - t * 60.0f,
              {22.0f, rgb(255, 230, 140), kInk}, Align::Center, 1.0f - t);
          }
      }
      break;
    default:
      break;
  }
}

void World::drawChopperBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& h = mHunter;
  const auto lerp = [&](float a, float b) { return a + (b - a) * alpha; };
  // The hunting gunship, small in the backdrop, its searchlight on the runner.
  const bool arena = mBoss.on && mPlayer.x + 1 >= mBoss.arena.x - 6;
  if (h.on && !arena)
  {
    const float sx = lerp(h.prevSx, h.sx) * kCellPx - camX, sy = lerp(h.prevSy, h.sy) * kCellPx - camY;
    const float shipX = sx + 40.0f * std::sin(float(frame) * 0.01f), shipY = 70.0f + 6.0f * std::sin(float(frame) * 0.05f);
    const Sprite& spr = styledEnemySprite(mArt, r, mTheme, kHaloKey, 0, (frame / 3) % 2, Boss::kW, Boss::kH);
    DrawOpts o;
    o.scale = 0.42f;
    o.tint = rgb(120, 130, 160);
    r.draw(spr.get(sx < float(kScreenW) * 0.5f ? -1 : 1), shipX, shipY + 30.0f, o);
    // The cone: a fan of soft beams to the spot.
    const float rad = float(h.r) * kCellPx;
    for (int k = -4; k <= 4; ++k)
    {
      const float ex = sx + float(k) * rad / 4.0f;
      r.drawLine(shipX, shipY + 20.0f, ex, sy, 18.0f, rgba(220, 235, 255, 10), Blend::Add);
    }
    const bool locking = h.locking > 0 || h.beep > 0;
    drawGlow(r, mArt, sx, sy, rad * 1.3f, locking ? rgb(255, 160, 140) : rgb(230, 240, 255), 0.30f);
    ring(r, sx, sy, rad, 0.8f, 2.0f, locking ? rgba(255, 90, 70, 150) : rgba(255, 255, 255, 70));
  }
  // The boss's own searchlight in the Drop phase.
  if (mBoss.on && mBoss.phase == BossPhase::Drop && mBoss.hp[std::size_t(BossPart::Light)] > 0 && arena)
  {
    const CellBox lb = partBox(mBoss, BossPart::Light);
    const float lx = float(lb.x) * kCellPx - camX, ly = float(lb.y + 1) * kCellPx - camY;
    const float sx = lerp(h.prevSx, h.sx) * kCellPx - camX, sy = lerp(h.prevSy, h.sy) * kCellPx - camY;
    const float rad = float(h.r) * kCellPx;
    for (int k = -4; k <= 4; ++k)
      r.drawLine(lx, ly, sx + float(k) * rad / 4.0f, sy, 16.0f, rgba(220, 235, 255, 12), Blend::Add);
    drawGlow(r, mArt, sx, sy, rad * 1.3f, h.locking > 0 || h.beep > 0 ? rgb(255, 160, 140) : rgb(230, 240, 255), 0.3f);
  }
  // Black Halo going down into the bay, far behind.
  if (mBoss.on && mBoss.phase == BossPhase::Falling)
  {
    const auto& b = mBoss;
    const float t = std::min(1.0f, float(b.fallT) / float(kFallFrames));
    const float bx = float(b.x) * kCellPx - camX, by = float(b.y) * kCellPx - camY;
    const Sprite& spr = styledEnemySprite(mArt, r, mTheme, kHaloKey, 2, (frame / 2) % 2, Boss::kW, Boss::kH);
    DrawOpts o;
    o.scale = 1.0f - 0.7f * t;
    o.angle = float(b.fallT) * 14.0f;
    r.draw(spr.get(b.facing), bx, by, o);
    for (int k = 0; k < 3; ++k)
      drawGlow(r, mArt, bx - float(k) * 20.0f, by - float(k) * 30.0f, 40.0f + float(k) * 10.0f, rgb(90, 90, 100), 0.3f);
    if (b.fallT >= kFallFrames)
    {
      // The splash in the bay.
      const float s = float(b.fallT - kFallFrames) / float(kSplashHold);
      for (int k = 0; k < 9; ++k)
      {
        const float a = -1.57f + (float(k) - 4.0f) * 0.25f;
        const float len = 160.0f * std::sin(std::min(1.0f, s * 2.0f) * 3.14f) + 20.0f;
        r.drawLine(bx, by, bx + std::cos(a) * len * 0.6f, by + std::sin(a) * len, 8.0f, rgba(220, 240, 255, 180));
      }
    }
  }
}

void World::drawBoss(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& b = mBoss;
  if (!b.on || b.phase == BossPhase::Falling || b.phase == BossPhase::Done)
    return;
  const float bx = (float(b.prevX) + float(b.x - b.prevX) * alpha) * kCellPx - camX;
  const float by = (float(b.prevY) + float(b.y - b.prevY) * alpha) * kCellPx - camY;
  const float w = float(Boss::kW) * kCellPx, hgt = float(Boss::kH) * kCellPx;
  if (bx > float(kScreenW) + 300.0f || bx + w < -300.0f)
    return;
  const int variant = (b.phase == BossPhase::Drop && b.open > 0) ? 1 : 0;
  const Sprite& spr = styledEnemySprite(mArt, r, mTheme, kHaloKey, variant, (frame / 2) % 2, Boss::kW, Boss::kH);
  DrawOpts o;
  if (b.flash > 0 && (frame / 2) % 2)
    o.tint = rgb(255, 200, 200);
  r.draw(spr.get(b.facing), bx + w * 0.5f, by + hgt, o);
  // Main rotor: a blur of blades.
  for (int k = 0; k < 2; ++k)
  {
    const float a = float(frame) * 0.9f + float(k) * 1.57f;
    r.drawLine(bx + w * 0.5f - std::cos(a) * w * 0.6f, by - 10.0f, bx + w * 0.5f + std::cos(a) * w * 0.6f, by - 10.0f,
      5.0f, rgba(40, 40, 50, 200));
  }
  // The parts: weak spots blink when they can be hit; dead ones smoke.
  for (int i = 0; i < int(BossPart::Count); ++i)
  {
    const BossPart part = BossPart(i);
    CellBox pb;
    const bool hittable = bossTarget(part, pb);
    const float px = float(pb.x - b.x) * kCellPx + bx, py = float(pb.y - b.y) * kCellPx + by;
    const float pw = float(pb.w) * kCellPx, ph = float(pb.h) * kCellPx;
    if (b.hp[std::size_t(i)] <= 0)
    {
      // Shot off: a charred stub (the pods, the lamp) and a trail of smoke.
      if (part == BossPart::NosePod || part == BossPart::TailPod || part == BossPart::Light)
      {
        static const std::string kPod = "halo_pod", kLight = "halo_light";
        DrawOpts dark;
        dark.tint = rgb(70, 56, 52);
        dark.scale = 0.8f;
        const Sprite& stub =
          styledEnemySprite(mArt, r, mTheme, part == BossPart::Light ? kLight : kPod, 0, 0, pb.w, pb.h);
        r.draw(stub.get(-b.facing), px + pw * 0.5f, py + ph * 0.8f, dark);
      }
      if (part != BossPart::Rotor)
        drawGlow(r, mArt, px + pw * 0.5f, py - 10.0f - float(frame % 20), 30.0f, rgb(80, 80, 90), 0.35f);
      continue;
    }
    switch (part)
    {
      case BossPart::NosePod:
      case BossPart::TailPod:
      {
        static const std::string kPod = "halo_pod";
        const Sprite& pod = styledEnemySprite(mArt, r, mTheme, kPod, hittable ? 1 : 0, 0, pb.w, pb.h);
        // The pod art points left; it points the gunship's way.
        r.draw(pod.get(-b.facing), px + pw * 0.5f, py + ph);
        break;
      }
      case BossPart::Hatch:
        if (b.phase == BossPhase::Drop && b.open > 0)
          drawGlow(r, mArt, px + pw * 0.5f, py + ph * 0.4f, 60.0f, rgb(255, 120, 60), 0.5f);
        break;
      case BossPart::Light:
      {
        static const std::string kLight = "halo_light";
        r.draw(styledEnemySprite(mArt, r, mTheme, kLight, 0, 0, pb.w, pb.h).get(1), px + pw * 0.5f, py + ph);
        break;
      }
      case BossPart::Rotor:
      {
        const float cx = px + pw * 0.5f, cy = py + ph * 0.5f;
        const float a = float(frame) * (b.passDamage >= kRotorPass ? 0.2f : 0.8f);
        for (int k = 0; k < 2; ++k)
          r.drawLine(cx, cy - std::cos(a + float(k) * 1.57f) * ph * 0.5f, cx, cy + std::cos(a + float(k) * 1.57f) * ph * 0.5f,
            6.0f, rgba(50, 50, 60, 220));
        r.fillRect(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, rgb(90, 90, 100));
        if (b.hp[std::size_t(BossPart::Rotor)] < 24)
          drawGlow(r, mArt, cx, cy - 20.0f - float(frame % 16) * 2.0f, 30.0f, rgb(90, 90, 100), 0.4f);
        break;
      }
      case BossPart::Count:
        break;
    }
    if (hittable && (frame / 6) % 2 == 0)
      brackets(r, px - 6.0f, py - 6.0f, pw + 12.0f, ph + 12.0f, 14.0f, rgb(255, 230, 60));
  }
}

void World::drawChopperFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  // The hook's latch and the hook itself.
  for (const auto& l : mLatches)
  {
    const float x = float(l.x) * kCellPx - camX, y = float(l.y) * kCellPx - camY;
    r.fillRect(x + 8.0f, y + 8.0f, 48.0f, 48.0f, l.open ? rgb(80, 80, 90) : rgb(200, 170, 60));
    r.fillRect(x + 20.0f, y + 20.0f, 24.0f, 24.0f, l.open ? rgb(40, 40, 46) : rgb(255, 230, 120));
  }
  for (const auto& pl : mPlatforms)
  {
    if (pl.latch < 0)
      continue;
    const float x = (float(pl.prevX) + float(pl.x - pl.prevX) * alpha) * kCellPx - camX;
    const float y = (float(pl.prevY) + float(pl.y - pl.prevY) * alpha) * kCellPx - camY;
    const float w = float(pl.w) * kCellPx;
    r.drawLine(x + w * 0.5f, y - 2000.0f, x + w * 0.5f, y - 40.0f, 4.0f, rgb(60, 60, 70));
    r.fillRect(x, y, w, 18.0f, rgb(240, 190, 40));
    r.fillRect(x, y, w, 4.0f, rgb(255, 240, 150));
    r.fillRect(x + w * 0.5f - 6.0f, y - 40.0f, 12.0f, 40.0f, rgb(90, 90, 100));
    r.drawLine(x + w * 0.5f, y + 18.0f, x + w * 0.5f + 16.0f, y + 40.0f, 6.0f, rgb(90, 90, 100));
  }

  drawBoss(r, camX, camY, frame, alpha);
  const auto& b = mBoss;
  if (b.on)
  {
    // Tracer dots, then the chaingun's sweep along the deck.
    if (b.phase == BossPhase::Strafe && b.t >= 0 && b.t < kTracer)
    {
      const float t = float(b.t) / float(kTracer);
      for (int k = 0; k < 12; ++k)
      {
        const float x = (float(b.arena.right()) - t * float(b.arena.w + 6) + float(k) * 6.0f) * kCellPx - camX;
        r.fillRect(x, float(b.deckY - 1) * kCellPx - camY + 10.0f, 10.0f, 10.0f, rgba(255, 80, 60, 200));
      }
    }
    if (b.sweepX >= 0)
    {
      const float x0 = float(b.prevSweepX < 0 ? b.sweepX : b.prevSweepX), x1 = float(b.sweepX);
      const float x = (x0 + (x1 - x0) * alpha) * kCellPx - camX, y = float(b.deckY - 1) * kCellPx - camY;
      r.fillRect(x - 120.0f, y + 14.0f, 120.0f, 6.0f, rgba(255, 220, 120, 160), Blend::Add);
      drawGlow(r, mArt, x, y + 16.0f, 50.0f, rgb(255, 200, 80), 0.7f);
      const CellBox nose = partBox(b, BossPart::NosePod);
      r.drawLine(float(nose.x) * kCellPx - camX, float(nose.y + 1) * kCellPx - camY, x, y + 16.0f, 2.0f,
        rgba(255, 230, 150, 120), Blend::Add);
    }
    // A trooper pod falling: its shadow, then the pod.
    if (b.podX >= 0)
    {
      const float t = 1.0f - float(b.podT) / float(kPodShadow);
      const float x = float(b.podX) * kCellPx - camX, gy = float(b.deckY + 1) * kCellPx - camY;
      r.fillRect(x - 30.0f * t, gy - 6.0f, 60.0f + 60.0f * t, 8.0f, rgba(0, 0, 0, int(80 + 100 * t)));
      const float py = float(b.podY) * kCellPx - camY + (gy - float(b.podY) * kCellPx + camY) * t * t;
      r.fillRect(x - 10.0f, py - 70.0f, 84.0f, 70.0f, rgb(70, 76, 70));
      r.fillRect(x - 10.0f, py - 70.0f, 84.0f, 8.0f, rgb(255, 170, 40));
    }
    // The exit dropping from the cab.
    if (b.phase == BossPhase::Falling)
    {
      // The pilot ejects and drifts off to the left under a canopy, waving.
      const float t = float(b.fallT) / float(kFallFrames + kSplashHold);
      const float x0 = float(b.arena.x + 30) * kCellPx - camX, y0 = float(b.arena.y + 2) * kCellPx - camY;
      const float x = x0 - t * 520.0f, y = y0 + t * 140.0f + 8.0f * std::sin(t * 12.0f);
      r.fillRect(x - 50.0f, y - 70.0f, 100.0f, 20.0f, rgb(240, 240, 230));
      r.fillRect(x - 40.0f, y - 80.0f, 80.0f, 12.0f, rgb(255, 120, 40));
      r.drawLine(x - 46.0f, y - 52.0f, x - 4.0f, y, 2.0f, rgb(200, 200, 200));
      r.drawLine(x + 46.0f, y - 52.0f, x + 4.0f, y, 2.0f, rgb(200, 200, 200));
      r.fillRect(x - 8.0f, y, 16.0f, 30.0f, rgb(40, 50, 40));
      r.fillRect(x - 7.0f, y - 12.0f, 14.0f, 14.0f, rgb(230, 190, 150));
      const float wave = std::sin(float(frame) * 0.5f) * 10.0f;
      r.drawLine(x + 6.0f, y + 6.0f, x + 22.0f, y - 12.0f + wave, 4.0f, rgb(40, 50, 40));
    }
  }

  // Rockets on the way down and where they will land.
  for (const auto& s : mStrikes)
  {
    const float tx = (float(s.x) + 0.5f) * kCellPx - camX, ty = (float(s.y) + 1.0f) * kCellPx - camY;
    const float rad = float(s.r) * kCellPx;
    const bool blink = (frame / 4) % 2 == 0;
    ring(r, tx, ty, rad, 0.35f, 3.0f, blink ? rgba(255, 60, 40, 230) : rgba(255, 200, 60, 200));
    r.fillRect(tx - 14.0f, ty - 2.0f, 28.0f, 4.0f, rgba(255, 60, 40, 200));
    r.fillRect(tx - 2.0f, ty - 14.0f * 0.35f, 4.0f, 28.0f * 0.35f, rgba(255, 60, 40, 200));
    const int flight = s.t > kStrikeFlight ? kRandomLead : kStrikeFlight;
    const float u = 1.0f - (float(s.t) - alpha) / float(flight);
    if (s.t <= kStrikeFlight)
    {
      const float fx = s.fromX * kCellPx - camX, fy = s.fromY * kCellPx - camY;
      const float rx = fx + (tx - fx) * u, ry = fy + (ty - fy) * u;
      r.drawLine(rx, ry, rx - (tx - fx) * 0.08f, ry - (ty - fy) * 0.08f, 6.0f, rgba(255, 200, 100, 200), Blend::Add);
      drawGlow(r, mArt, rx, ry, 26.0f, rgb(255, 170, 60), 0.8f);
    }
  }

  // Lock-On Rockets: brackets on what is painted.
  for (const int t : mPaint)
  {
    CellBox tb;
    if (!targetBox(t, tb))
      continue;
    brackets(r, float(tb.x) * kCellPx - camX - 8.0f, float(tb.y) * kCellPx - camY - 8.0f, float(tb.w) * kCellPx + 16.0f,
      float(tb.h) * kCellPx + 16.0f, 16.0f, rgb(255, 90, 60));
  }
  // The hunter's lock on the runner: a red bracket while it beeps.
  if ((mHunter.beep > 0 && !mHunter.demo) || (mHunter.locking > 0 && (frame / 3) % 2))
  {
    const float x = (float(p.prevX) + float(p.x - p.prevX) * alpha) * kCellPx - camX;
    const float y = (float(p.prevY) + float(p.y - p.prevY) * alpha + 1.0f) * kCellPx - camY;
    const CellBox pb = p.box();
    const float hgt = float(pb.h) * kCellPx;
    brackets(r, x - 10.0f, y - hgt - 10.0f, 3.0f * kCellPx + 20.0f, hgt + 20.0f, 18.0f,
      mHunter.beep > 0 ? rgb(255, 40, 40) : rgba(255, 255, 255, 160));
  }
  // Ropes for the troopers sliding down.
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Trooper && e.attach == 1)
    {
      const float x = (float(e.x) + 1.5f) * kCellPx - camX;
      const float y = float(e.y - e.h) * kCellPx - camY;
      r.drawLine(x, y - 2000.0f, x, y + 20.0f, 3.0f, rgb(60, 56, 50));
      const float gy = float(e.aimY + 1) * kCellPx - camY;
      r.fillRect(x - 40.0f, gy - 4.0f, 80.0f, 6.0f, rgba(0, 0, 0, 110));
    }
}

void World::drawFlightShip(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& p = mPlayer;
  const float x = (float(p.prevX) + float(p.x - p.prevX) * alpha) * kCellPx - camX;
  const float y = (float(p.prevY) + float(p.y - p.prevY) * alpha + 1.0f) * kCellPx - camY;
  const Sprite& spr = styledEnemySprite(mArt, r, mTheme, kHaloKey, 0, (frame / 2) % 2, Boss::kW, Boss::kH);
  DrawOpts o;
  o.scale = 0.4f;
  r.draw(spr.get(p.facing), x + 3.0f * kCellPx, y, o);
  const float a = float(frame) * 0.9f;
  r.drawLine(x + 3.0f * kCellPx - std::cos(a) * 110.0f, y - 2.6f * kCellPx, x + 3.0f * kCellPx + std::cos(a) * 110.0f,
    y - 2.6f * kCellPx, 4.0f, rgba(40, 40, 50, 200));
}

} // namespace gr
