// Level 12, Lava Heart (SPEC 12): lava pools that burn and pop you back to
// solid ground, basalt stones that sink under a load and rise back when
// free, crab ferries that cross a lava river once boarded, the bridge that
// comes up out of the lava, the Serpent Spear's footholds, Magma Toads,
// Ember Wisps, Basalt Crabs, the forge's marshmallow, and the bonus rule
// The Floor Is Lava (bounce from head to head).

#include "game/world.hpp"

#include "assets/art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kSpearLife = 150;
constexpr int kSpearFlash = 30;
constexpr int kMaxFootholds = 3;
constexpr int kWadeFrames = 20;
constexpr int kLavaMercy = 30;
constexpr int kRiseRate = 4;      // a free stone rises 4 cells per 15 frames (2 blocks/s)
constexpr int kRumbleFrames = 15; // a ferry's rumble before it sets off
constexpr int kFerryWait = 60;    // and how long it waits empty before it goes back
constexpr int kBubbleFrames = 10; // a bridge segment's bubbles before it rises
constexpr int kRiseFrames = 15;
constexpr int kToadLeap = 12;     // frames in the air
constexpr int kToadSit = 45;
constexpr int kToadDive = 8;
constexpr int kToadReach = 20;    // cells from home a toad will leap to
constexpr int kFlipFrames = 90;
constexpr int kWispSwell = 15;
constexpr int kRoastFrames = 45;
const Color kLavaHot = rgb(255, 210, 90);
const Color kLavaMid = rgb(255, 110, 30);
const Color kLavaDeep = rgb(170, 30, 10);
const Color kBasalt = rgb(58, 52, 56);
const Color kBasaltLight = rgb(104, 96, 100);

int sgn(int v) { return (v > 0) - (v < 0); }

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// Where a parabolic hop from (x0, y0) to (x1, y1), `peak` cells above the
// higher end, is after fraction t.
void arcAt(int x0, int y0, int x1, int y1, int peak, float t, int& x, int& y)
{
  const float top = float(std::min(y0, y1) - peak);
  const float a = float(y0), b = float(y1);
  // Quadratic through (0, a), (0.5, top-ish) and (1, b).
  const float m = 2.0f * (a + b) - 4.0f * top;
  const float yy = a + (b - a) * t - m * t * (1.0f - t) * 0.5f;
  x = int(std::lround(float(x0) + float(x1 - x0) * t));
  y = int(std::lround(yy));
}

} // namespace

// --- Setup ----------------------------------------------------------------------------

bool World::setupLavaEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "fluid" && hasRect && e.str("kind") == "lava")
  {
    Lava l;
    l.x0 = x0 * kCellsPerTile;
    l.x1 = (x1 + 1) * kCellsPerTile - 1;
    l.y0 = y0 * kCellsPerTile;
    l.y1 = (y1 + 1) * kCellsPerTile - 1;
    l.shallow = e.num("shallow", 0) != 0;
    l.wade = e.num("wade", 0) != 0;
    l.hearth = e.num("hearth", 0) != 0;
    mLavas.push_back(l);
    return true;
  }
  if ((e.kind == "serpent" || e.kind == "marshmallow") && e.hasPos)
  {
    Prop pr;
    pr.kind = e.kind == "serpent" ? PropKind::Serpent : PropKind::Marshmallow;
    pr.x = e.x * kCellsPerTile;
    pr.y = e.y * kCellsPerTile;
    pr.w = pr.h = pr.kind == PropKind::Serpent ? 6 : 2;
    if (pr.kind == PropKind::Serpent)
    {
      // Given by the head's middle; it faces the way dir= says.
      pr.x -= 2;
      pr.y -= 2;
      pr.hold = e.str("dir", "r") == "l" ? -1 : 1;
    }
    mProps.push_back(pr);
    return true;
  }
  return false;
}

void World::setupLavaPlatform(Platform& pl, const EntityDef& e)
{
  const std::string mode = e.str("mode");
  if (mode == "sink")
  {
    pl.mode = PlatformMode::Sink;
    pl.homeY = e.y * kCellsPerTile;
    pl.y = pl.prevY = pl.startY = pl.homeY;
    pl.floorY = -1; // worked out in linkLava, from the lava under it
    if (e.num("dock", 0) != 0 && pl.path.size() >= 2)
    {
      pl.ferry = 1;
      pl.target = 0;
      pl.x = pl.prevX = pl.path[0].first;
    }
  }
  else if (mode == "rise")
  {
    pl.mode = PlatformMode::Rise;
    pl.homeY = e.y * kCellsPerTile;
    // start= is where it waits, under the lava.
    int t0 = 0, t1 = 0;
    const std::string trig = e.str("trigger");
    const auto dots = trig.find("..");
    if (dots != std::string::npos)
    {
      t0 = std::atoi(trig.substr(0, dots).c_str());
      t1 = std::atoi(trig.substr(dots + 2).c_str());
      pl.riseX0 = t0 * kCellsPerTile;
      pl.riseX1 = (t1 + 1) * kCellsPerTile - 1;
    }
    pl.riseDelay = e.num("delay", 0);
  }
  if (pl.mode == PlatformMode::Sink || pl.mode == PlatformMode::Rise)
    pl.target = pl.ferry > 0 ? pl.target : 0; // no path to follow
}

void World::setupLavaEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::Toad)
  {
    // Waits under the lava at home; cycle= toads (the bonus) leap straight
    // up out of it on a fixed 45-frame clock, arc= blocks high.
    en.hidden = true;
    en.railX0 = en.x;
    en.railX1 = en.y; // the lava's surface, set in linkLava
    en.attach = 0;
    en.active = true;
    en.aimX = -1;
    en.aimY = e.num("arc", 0) * kCellsPerTile;
    en.ox = e.num("phase", 0);
    en.oy = e.num("cycle", 0);
  }
  if (en.kind == EnemyKind::Wisp)
  {
    en.attach = 0;
    en.railX0 = en.x;
    en.railX1 = en.y;
    en.oy = e.num("fixed", 0); // the bonus's wisps hang where they are
  }
  if (en.kind == EnemyKind::Crab)
  {
    en.variant = e.num("hat", 0) != 0 ? 1 : 0;
    en.platform = -1; // linkLava finds the stone it stands on
    en.dir = 1;
  }
}

void World::linkLava()
{
  // Each sinking stone stops a block under the lava below it.
  for (auto& pl : mPlatforms)
  {
    if (pl.mode != PlatformMode::Sink || pl.floorY >= 0)
      continue;
    int x0 = pl.x, x1 = pl.x + pl.w - 1;
    for (const auto& [px, py] : pl.path)
    {
      x0 = std::min(x0, px);
      x1 = std::max(x1, px + pl.w - 1);
    }
    pl.floorY = pl.homeY + 4 * kCellsPerTile;
    for (const auto& l : mLavas)
      if (l.x0 <= x1 && x0 <= l.x1 && l.y0 >= pl.homeY)
      {
        pl.floorY = l.y0 + kCellsPerTile;
        break;
      }
  }
  for (auto& e : mEnemies)
  {
    if (e.kind == EnemyKind::Toad)
    {
      for (const auto& l : mLavas)
        if (e.x + 1 >= l.x0 && e.x + 1 <= l.x1 && e.y <= l.y1 + 1)
        {
          e.railX1 = l.y0;
          break;
        }
      e.y = e.prevY = e.railX1 + 2;
    }
    if (e.kind == EnemyKind::Crab)
      for (std::size_t i = 0; i < mPlatforms.size(); ++i)
        if (standsOn(e.box(), mPlatforms[i]))
        {
          e.platform = int(i);
          e.aimX = e.x - mPlatforms[i].x;
        }
  }
  mLavaSafeX = mPlayer.x;
  mLavaSafeY = mPlayer.y;
  mHeadX = mPlayer.x;
  mHeadY = mPlayer.y;
}

// --- Queries --------------------------------------------------------------------------

int World::lavaAt(int cx, int cy) const
{
  for (std::size_t i = 0; i < mLavas.size(); ++i)
    if (mLavas[i].wet(cx, cy))
      return int(i);
  return -1;
}

bool World::inLava(const CellBox& b) const
{
  for (const auto& l : mLavas)
    if (l.box().intersects(b))
      return true;
  return false;
}

int World::sinkRate(const Platform& pl) const
{
  // A docked ferry, and one rumbling to set off, holds its load up.
  if (pl.mode != PlatformMode::Sink || pl.ferry == 1 || pl.ferry == 2)
    return 0;
  int rate = 0;
  const auto& p = mPlayer;
  if ((p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl))
    rate += mCharacterIndex == 1 ? 3 : 2; // Rocco is heavy
  const int idx = int(&pl - mPlatforms.data());
  bool crab = false;
  int toads = 0;
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.platform != idx)
      continue;
    crab = crab || e.kind == EnemyKind::Crab;
    toads += e.kind == EnemyKind::Toad && e.attach == 3;
  }
  if (crab)
    rate *= 2;
  return rate + toads * 2;
}

// --- Platforms --------------------------------------------------------------------------

void World::updateSinkPlatform(Platform& pl)
{
  const auto& p = mPlayer;
  const bool rider = (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && standsOn(p.box(), pl);
  if (pl.ferry > 0)
  {
    // bell: the runner was aboard last frame; parked: it crossed with
    // someone and goes back empty once left alone.
    if (pl.ferry == 1)
    {
      if (rider && pl.bell == 0)
      {
        pl.ferry = 2;
        pl.ferryWait = kRumbleFrames;
        pl.shudder = kRumbleFrames;
        if (isOnScreen(pl.box(), 4))
          playSound(Sfx::Rumble);
      }
      else if (!rider && pl.parked && ++pl.ferryWait >= kFerryWait)
      {
        pl.ferry = 3;
        pl.parked = false;
        pl.target = 1 - pl.target;
      }
      else if (rider)
        pl.ferryWait = 0;
    }
    else if (pl.ferry == 2)
    {
      if (--pl.ferryWait <= 0)
      {
        pl.ferry = 3;
        pl.target = 1 - pl.target;
        pl.parked = true;
        if (isOnScreen(pl.box(), 4))
          playSound(Sfx::Clunk);
      }
    }
    else if (pl.ferry == 3 && ++pl.moveTick % pl.speedDen == 0)
    {
      const int tx = pl.path[std::size_t(pl.target)].first;
      for (int s = 0; s < pl.speedNum && pl.x != tx; ++s)
        movePlatform(pl, sgn(tx - pl.x), 0);
      if (pl.x == tx)
      {
        pl.ferry = 1;
        pl.ferryWait = 0;
        if (isOnScreen(pl.box(), 4))
          playSound(Sfx::Land);
      }
    }
    pl.bell = rider ? 1 : 0;
  }
  const int rate = sinkRate(pl);
  if (rate > 0)
  {
    if (pl.acc == 0 && pl.y == pl.homeY && isOnScreen(pl.box(), 2))
      playSound(Sfx::Sink);
    pl.acc += rate;
    while (pl.acc >= 15)
    {
      pl.acc -= 15;
      if (pl.y >= pl.floorY)
      {
        pl.acc = 0;
        break;
      }
      movePlatform(pl, 0, 1);
    }
  }
  else if (pl.y > pl.homeY)
  {
    pl.acc = std::max(0, pl.acc) + kRiseRate;
    while (pl.acc >= 15 && pl.y > pl.homeY)
    {
      pl.acc -= 15;
      if (!movePlatform(pl, 0, -1))
        break;
    }
    if (pl.y == pl.homeY)
      pl.acc = 0;
  }
  else
    pl.acc = 0;
}

void World::updateRisePlatform(Platform& pl)
{
  const auto& p = mPlayer;
  if (pl.riseAt < 0)
  {
    if (p.state != PlayerState::OnGround || p.x + 1 < pl.riseX0 || p.x + 1 > pl.riseX1)
      return;
    // Every segment on this trigger starts, each after its delay.
    for (auto& o : mPlatforms)
      if (o.mode == PlatformMode::Rise && o.riseAt < 0 && o.riseX0 == pl.riseX0 && o.riseX1 == pl.riseX1)
        o.riseAt = mStats.frames + o.riseDelay;
    playSound(Sfx::Rumble);
    return;
  }
  const int t = mStats.frames - pl.riseAt;
  if (t == 0 && isOnScreen(pl.box(), 6))
    playSound(Sfx::Magma);
  if (t < kBubbleFrames || pl.y <= pl.homeY)
    return;
  const int span = pl.startY - pl.homeY;
  const int want = pl.startY - span * std::min(kRiseFrames, t - kBubbleFrames) / kRiseFrames;
  while (pl.y > want)
    if (!movePlatform(pl, 0, -1))
      break;
}

// --- Per frame --------------------------------------------------------------------------

void World::lavaPop(int hearts)
{
  auto& p = mPlayer;
  const Vec2 at{(float(p.x) + 1.5f) * kCellSize, float(p.y) * kCellSize};
  burst(at, kLavaHot, kLavaMid, 16, 1.8f);
  playSound(Sfx::Sizzle);
  ++mLavaPops;
  if (!mFloorLava && p.mercy == 0 && p.turbo == 0 && !mGod && hearts > 0)
  {
    // Never a death: the last heart stays.
    p.hp = std::max(1, p.hp - hearts);
    mStats.tookDamage = true;
    playSound(Sfx::Hurt);
    if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
      std::fprintf(stderr, "lava at %d,%d frame %d hp %d\n", p.x, p.y, mStats.frames, p.hp);
  }
  p.x = p.prevX = mFloorLava ? mHeadX : mLavaSafeX;
  p.y = p.prevY = mFloorLava ? mHeadY : mLavaSafeY;
  p.state = mFloorLava ? PlayerState::Falling : PlayerState::OnGround;
  p.frames = 0;
  p.somersault = -1;
  p.fling = 0;
  mLaunch = 0;
  if (!mFloorLava)
  {
    p.mercy = kLavaMercy;
    setVisual(PlayerVisual::Standing);
  }
  burst({(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize}, rgb(255, 255, 255), kLavaHot, 10, 1.2f);
}

void World::syncFootholds() { syncFloats(); }

void World::updateLava(const PlayerInput& input)
{
  if (mLavas.empty() && mFootholds.empty())
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;

  // Spears in the wall crumble after 10 seconds.
  bool changed = false;
  for (auto& f : mFootholds)
    if (--f.life <= 0)
    {
      changed = true;
      burst({(float(f.x) + 1.0f) * kCellSize, float(f.y) * kCellSize}, kBasaltLight, rgb(200, 160, 120), 6, 1.0f);
    }
  if (changed)
  {
    mFootholds.erase(std::remove_if(mFootholds.begin(), mFootholds.end(), [](const Foothold& f) { return f.life <= 0; }),
      mFootholds.end());
    syncFootholds();
  }

  if (mFloorLava)
  {
    updateFloorLava(input);
    return;
  }

  // Lava: a wading shelf burns slowly; anything else pops you out.
  const CellBox pb = p.box();
  int touching = -1;
  for (std::size_t i = 0; i < mLavas.size(); ++i)
    if (mLavas[i].box().intersects(pb))
      touching = int(i);
  if (alive && touching >= 0 && p.turbo == 0)
  {
    const Lava& l = mLavas[std::size_t(touching)];
    if (l.wade)
    {
      if (mLavaTicks == 0)
        playSound(Sfx::Sizzle);
      if (++mLavaTicks >= kWadeFrames)
      {
        mLavaTicks = 1;
        const int mercy = p.mercy;
        p.mercy = 0;
        if (p.hp > 1)
          hurtPlayer(1);
        p.mercy = mercy;
      }
      if (mStats.frames % 3 == 0)
        burst({(float(p.x) + 1.5f) * kCellSize, float(l.y0) * kCellSize}, kLavaHot, kLavaMid, 2, 0.8f);
    }
    else
      lavaPop(l.shallow ? 1 : 2);
  }
  else
    mLavaTicks = 0;

  // Where lava sends you back to: the last solid ground (not a stone, a
  // spear or a ferry) you stood on.
  if (alive && p.state == PlayerState::OnGround && touching < 0)
  {
    int firm = 0;
    for (int x = p.x; x < p.x + Player::kWidth; ++x)
      firm += mMap.solidTop(x, p.y + 1) && mMap.platformAt(x, p.y + 1) < 0 && !mMap.floatTop(x, p.y + 1);
    if (firm >= 2)
    {
      mLavaSafeX = p.x;
      mLavaSafeY = p.y;
    }
  }

  // The marshmallow: up beside it takes it on a stick; held facing the
  // hearth for three seconds it roasts into a heart.
  for (auto& pr : mProps)
  {
    if (pr.kind != PropKind::Marshmallow || pr.used)
      continue;
    const CellBox near{pr.x - 3, pr.y - 4, pr.w + 6, pr.h + 6};
    if (alive && p.state == PlayerState::OnGround && input.up && near.intersects(pb))
    {
      pr.used = true;
      mOnStick = true;
      mRoast = 0;
      playSound(Sfx::Item);
      showMessage("A MARSHMALLOW ON A STICK. THE FORGE IS STILL HOT...");
    }
  }
  if (mOnStick && alive)
  {
    bool roasting = false;
    for (const auto& l : mLavas)
    {
      if (!l.hearth || p.state != PlayerState::OnGround)
        continue;
      const int front = p.facing > 0 ? p.x + Player::kWidth : p.x - 1;
      const int gap = p.facing > 0 ? l.x0 - front : front - l.x1;
      roasting = roasting || (gap >= 0 && gap <= 5 && p.y >= l.y0 - 6 && p.y <= l.y1 + 1);
    }
    if (roasting)
    {
      if (++mRoast % 6 == 0)
        burst({(float(p.x) + 1.5f + float(p.facing) * 3.0f) * kCellSize, (float(p.y) - 3.0f) * kCellSize}, kLavaHot,
          rgb(255, 255, 255), 2, 0.6f);
      if (mRoast >= kRoastFrames)
      {
        mOnStick = false;
        p.hp = std::min(p.maxHp, p.hp + 1);
        playSound(Sfx::Health);
        showMessage("PERFECTLY TOASTED: +1 HEART");
        addScore(420, {(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 5.0f) * kCellSize});
      }
    }
  }
}

// The platforms' part, run with the other platforms (world_platforms.cpp).
void World::updateLavaPlatforms()
{
  for (auto& pl : mPlatforms)
  {
    if (pl.mode == PlatformMode::Sink)
      updateSinkPlatform(pl);
    else if (pl.mode == PlatformMode::Rise)
      updateRisePlatform(pl);
  }
}

// --- Spears -----------------------------------------------------------------------------

void World::stickSpear(Projectile& pr)
{
  // Into the first wall face it reached; the foothold's top is the row
  // under the thrower's feet at the throw.
  const int dir = pr.dx;
  if (dir == 0 || pr.footRow < 0)
    return;
  int face = -1;
  if (dir > 0)
  {
    for (int c = pr.x; c < pr.x + pr.w && face < 0; ++c)
      if (mMap.solid(c, pr.y))
        face = c;
  }
  else
  {
    for (int c = pr.x + pr.w - 1; c >= pr.x && face < 0; --c)
      if (mMap.solid(c, pr.y))
        face = c;
  }
  if (face < 0)
    return;
  const int fx = dir > 0 ? face - 2 : face + 1;
  const int fy = pr.footRow;
  bool room = true;
  for (int y = fy; y <= fy + 1; ++y)
    for (int x = fx; x <= fx + 1; ++x)
      room = room && !mMap.solid(x, y);
  const bool wall = mMap.solid(face, fy) || mMap.solid(face, fy + 1);
  if (!room || !wall || lavaAt(fx, fy) >= 0)
    return;
  if (int(mFootholds.size()) >= kMaxFootholds)
  {
    const Foothold& old = mFootholds.front();
    burst({(float(old.x) + 1.0f) * kCellSize, float(old.y) * kCellSize}, kBasaltLight, rgb(200, 160, 120), 6, 1.0f);
    mFootholds.erase(mFootholds.begin());
  }
  mFootholds.push_back({fx, fy, kSpearLife, dir});
  syncFootholds();
  playSound(Sfx::Clunk);
}

bool World::shotAtLava(Projectile& pr)
{
  if (pr.kind == ShotKind::Enemy || !inLava(pr.box()))
    return false;
  burst(cellCenter(pr.box()), kLavaHot, kLavaMid, 6, 1.0f);
  if (isOnScreen(pr.box(), 2))
    playSound(Sfx::Sizzle);
  return true;
}

int World::shotAtCrab(Projectile& pr, Enemy& e)
{
  const bool spear = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SerpentSpear);
  if (e.dive > 0)
  {
    // Belly up: everything hurts double.
    damageEnemy(e, pr.damage * 2);
    return 1;
  }
  const int sdx = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
  const bool fromBelow = spear && pr.footRow - 1 > e.y;
  const bool fromBehind = pr.dy == 0 && sdx == e.dir;
  if (fromBelow || fromBehind)
  {
    e.dive = kFlipFrames;
    e.tell = 0;
    burst(cellCenter(e.box()), rgb(255, 255, 255), kLavaHot, 8, 1.4f);
    damageEnemy(e, pr.damage);
    return 1;
  }
  // Armored top and front: the shot sparks off.
  e.flash = 3;
  burst(cellCenter(pr.box()), rgb(255, 255, 255), rgb(255, 180, 90), 6, 1.4f);
  playSound(Sfx::Land);
  return 1;
}

// --- Enemies ------------------------------------------------------------------------------

void World::updateToad(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const int surface = e.railX1;
  // attach: 0 under the lava, 1 the tell (bubbles where it will come out),
  // 2 leaping, 3 sitting on a stone, 4 diving back, 5 the bonus's fixed hop.
  if (e.oy > 0)
  {
    // The Floor Is Lava: straight up and down on a 45-frame clock, harmless.
    const int t = (mStats.frames + e.ox) % 45;
    const int air = 30;
    if (t >= air)
    {
      e.hidden = true;
      e.attach = 0;
      e.y = surface + 2;
      if (t == air && isOnScreen(e.box(), 2))
        burst({(float(e.x) + 1.5f) * kCellSize, float(surface) * kCellSize}, kLavaHot, kLavaMid, 6, 1.2f);
      return;
    }
    e.hidden = false;
    e.attach = 5;
    const float f = float(t) / float(air);
    e.y = surface + 1 - int(std::lround(4.0f * float(e.aimY) * f * (1.0f - f)));
    if (t == 0 && isOnScreen(e.box(), 2))
      playSound(Sfx::Magma);
    return;
  }
  auto platformOf = [&](int idx) -> const Platform* {
    return idx >= 0 && idx < int(mPlatforms.size()) ? &mPlatforms[std::size_t(idx)] : nullptr;
  };
  if (e.attach == 0)
  {
    e.hidden = true;
    e.platform = -1;
    if (e.dive > 0)
    {
      --e.dive;
      return;
    }
    if (p.state == PlayerState::Dying || p.turbo > 0)
      return;
    // The stone the runner is on, or the nearest one beside them.
    int best = -1, bestD = 1 << 30;
    for (std::size_t i = 0; i < mPlatforms.size(); ++i)
    {
      const Platform& pl = mPlatforms[i];
      if (pl.mode != PlatformMode::Sink || pl.ferry > 0)
        continue;
      const int cx = pl.x + pl.w / 2;
      if (std::abs(cx - e.railX0) > kToadReach || std::abs(pl.y - (p.y + 1)) > 6)
        continue;
      const int d = std::abs(cx - (p.x + 1));
      if (d > 8 || d >= bestD)
        continue;
      // Never on a stone within 3 blocks of another toad's.
      bool taken = false;
      for (const auto& o : mEnemies)
        if (&o != &e && o.alive && o.kind == EnemyKind::Toad && o.attach >= 1 && o.attach <= 3 && o.aimX >= 0 &&
            o.aimX < int(mPlatforms.size()))
          taken = taken || std::abs(mPlatforms[std::size_t(o.aimX)].x - pl.x) < 6;
      if (taken)
        continue;
      best = int(i);
      bestD = d;
    }
    if (best < 0)
      return;
    const Platform& pl = mPlatforms[std::size_t(best)];
    e.aimX = best;
    e.dir = e.railX0 < pl.x ? 1 : -1;
    e.x = e.prevX = std::clamp(pl.x + (e.dir > 0 ? -5 : pl.w + 2), e.railX0 - kToadReach, e.railX0 + kToadReach);
    e.y = e.prevY = surface + 2;
    e.drawSnap = true;
    e.attach = 1;
    e.tell = def.tell;
    if (isOnScreen(e.box(), 4))
      playSound(Sfx::Magma);
    return;
  }
  if (e.attach == 1)
  {
    if (e.tell % 4 == 0)
      burst({(float(e.x) + 1.5f) * kCellSize, float(surface) * kCellSize}, kLavaHot, kLavaMid, 3, 0.9f);
    if (--e.tell > 0)
      return;
    e.attach = 2;
    e.hidden = false;
    e.timer = 0;
    e.ox = e.x;
    e.oy = 0;
    e.y = surface + 1;
    burst({(float(e.x) + 1.5f) * kCellSize, float(surface) * kCellSize}, kLavaHot, kLavaMid, 10, 1.6f);
    return;
  }
  const Platform* pl = platformOf(e.aimX);
  if (e.attach == 2)
  {
    if (!pl)
    {
      e.attach = 4;
      e.timer = 0;
      return;
    }
    const int tx = pl->x + pl->w / 2 - 1, ty = pl->y - 1;
    int x = 0, y = 0;
    arcAt(e.ox, surface + 1, tx, ty, 12, std::min(1.0f, float(e.timer) / float(kToadLeap)), x, y);
    e.x = x;
    e.y = y;
    if (e.timer >= kToadLeap)
    {
      e.attach = 3;
      e.timer = 0;
      e.platform = e.aimX;
      e.aimY = e.x - pl->x;
      if (isOnScreen(e.box(), 2))
        playSound(Sfx::Croak);
    }
    return;
  }
  if (e.attach == 3)
  {
    if (!pl)
    {
      e.attach = 4;
      e.timer = 0;
      return;
    }
    e.x = pl->x + e.aimY;
    e.y = pl->y - 1;
    e.dir = p.x < e.x ? -1 : 1;
    if (e.timer >= kToadSit)
    {
      e.attach = 4;
      e.timer = 0;
      e.ox = e.x;
      e.oy = e.y;
      e.platform = -1;
    }
    return;
  }
  // Diving back in beside the stone.
  int x = 0, y = 0;
  arcAt(e.ox, e.oy, e.ox + e.dir * 6, surface + 2, 4, std::min(1.0f, float(e.timer) / float(kToadDive)), x, y);
  e.x = x;
  e.y = y;
  if (e.timer >= kToadDive)
  {
    e.attach = 0;
    e.hidden = true;
    e.aimX = -1;
    e.oy = 0;
    e.dive = def.cooldown;
    burst({(float(e.x) + 1.5f) * kCellSize, float(surface) * kCellSize}, kLavaHot, kLavaMid, 8, 1.4f);
    if (isOnScreen(e.box(), 2))
      playSound(Sfx::Magma);
  }
}

void World::updateWisp(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  if (e.oy != 0)
  {
    // The bonus's wisps just hang there, bobbing.
    e.y = e.railX1 + ((mStats.frames / 6 + e.railX0) % 4 < 2 ? 0 : 1);
    return;
  }
  const int cx = e.x + 1, cy = e.y - 1;
  const int px = p.x + 1, py = p.y - 2;
  const int dist = std::max(std::abs(px - cx), std::abs(py - cy));
  if (e.cool > 0)
    --e.cool;
  if (e.attach == 0)
  {
    if (p.state == PlayerState::Dying)
      return;
    if (dist <= 4 && e.cool == 0)
    {
      e.attach = 1;
      e.tell = kWispSwell;
      e.oy = 0;
      playSound(Sfx::Fuse);
      return;
    }
    // Drifts toward the runner a quarter cell a frame.
    if (e.timer % def.stepEvery == 0)
    {
      const int dx = sgn(px - cx), dy = sgn(py - cy);
      if (dx != 0 && !mMap.overlapsSolid(boxAt(e.x + dx, e.y, e.w, e.h)))
        e.x += dx;
      if (dy != 0 && !mMap.overlapsSolid(boxAt(e.x, e.y + dy, e.w, e.h)))
        e.y += dy;
      e.dir = dx != 0 ? dx : e.dir;
    }
    return;
  }
  // Swelling: back off three blocks and it fizzles back to drifting.
  if (dist > 10)
  {
    e.attach = 0;
    e.tell = 0;
    e.cool = def.cooldown;
    burst(cellCenter(e.box()), rgb(255, 200, 120), rgb(120, 60, 30), 5, 0.8f);
    return;
  }
  if (--e.tell > 0)
    return;
  // Burst: 2 blocks around it.
  const Vec2 c = cellCenter(e.box());
  burst(c, kLavaHot, e.carrier ? rgb(140, 255, 70) : kLavaMid, 22, 2.2f);
  flashAt(c, 140.0f, e.carrier ? rgb(140, 255, 70) : rgb(255, 150, 60), 14);
  playSound(Sfx::SmallExplosion);
  e.alive = false;
  const CellBox reach{cx - 4, cy - 4, 9, 9};
  if (reach.intersects(p.hitBox()) && p.state != PlayerState::Dying)
  {
    if (e.carrier)
    {
      if (p.virus == 0 && p.mercy == 0)
        infect();
    }
    else
      hurtPlayer(1);
  }
}

void World::updateCrab(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const Platform* pl =
    e.platform >= 0 && e.platform < int(mPlatforms.size()) ? &mPlatforms[std::size_t(e.platform)] : nullptr;
  if (pl)
  {
    e.x = pl->x + e.aimX;
    e.y = pl->y - 1;
  }
  if (e.cool > 0)
    --e.cool;
  if (e.dive > 0)
  {
    --e.dive; // belly up, legs waving
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      // Snap: a block and a half ahead.
      const CellBox claw{e.dir > 0 ? e.x + e.w : e.x - 3, e.y - 2, 3, 3};
      if (claw.intersects(p.hitBox()) && p.state != PlayerState::Dying)
        hurtPlayer(1);
      e.cool = def.cooldown;
      if (isOnScreen(e.box(), 2))
        playSound(Sfx::Snap);
    }
    return;
  }
  // Claws open when the runner is right in front (the party crab only pinches).
  const CellBox ahead{e.dir > 0 ? e.x + e.w : e.x - 3, e.y - 3, 3, 4};
  if (e.variant == 0 && e.cool == 0 && ahead.intersects(p.hitBox()) && p.state != PlayerState::Dying)
  {
    e.tell = def.tell;
    return;
  }
  if (e.timer % def.stepEvery != 0)
    return;
  if (pl)
  {
    const int nx = e.aimX + e.dir;
    if (nx < 0 || nx + e.w > pl->w)
      e.dir = -e.dir;
    else
      e.aimX = nx;
    e.x = pl->x + e.aimX;
    return;
  }
  const int nx = e.x + e.dir;
  const CellBox next = boxAt(nx, e.y, e.w, e.h);
  const int footX = e.dir > 0 ? nx + e.w - 1 : nx;
  if (mMap.overlapsSolid(next) || !mMap.solidTop(footX, e.y + 1) || inLava(next))
    e.dir = -e.dir;
  else
    e.x = nx;
}

// --- The Floor Is Lava -------------------------------------------------------------------

void World::updateFloorLava(const PlayerInput& input)
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  // Coming down onto a head: bounce 10 cells up, 14 with jump held.
  const bool falling = p.y > p.prevY || p.state == PlayerState::Falling;
  if (falling && mLaunch == 0)
    for (const auto& e : mEnemies)
    {
      if (!e.alive || e.hidden)
        continue;
      const CellBox b = e.box();
      const int top = b.top();
      if (p.y < top - 2 || p.y > top + 1 || p.x + Player::kWidth <= b.x || p.x >= b.x + b.w)
        continue;
      if (p.prevY > top)
        continue; // came up from under it
      p.y = top - 1;
      startLaunch(input.jump.pressed ? 14 : 10);
      mHeadX = std::clamp(b.x + b.w / 2 - 1, 0, mMap.width() - Player::kWidth);
      mHeadY = top - 1;
      ++mHeadBounces;
      playSound(Sfx::Boing);
      addScore(100, {(float(b.x) + float(b.w) * 0.5f) * kCellSize, float(top) * kCellSize});
      burst({(float(b.x) + float(b.w) * 0.5f) * kCellSize, float(top) * kCellSize}, rgb(255, 255, 255), kLavaHot, 8,
        1.2f);
      return;
    }
  // Solid ledges are safe; lava returns you to the last head.
  if (p.state == PlayerState::OnGround && !inLava(p.box()))
  {
    mHeadX = p.x;
    mHeadY = p.y;
  }
  if (inLava(p.box()))
    lavaPop(0);
}

// --- Drawing ----------------------------------------------------------------------------------

void World::drawLavaBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  // The lava's heat on the walls around it.
  for (const auto& l : mLavas)
  {
    const float x = float(l.x0) * kCellPx - camX, y = float(l.y0) * kCellPx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kCellPx;
    if (!visible(x, y - 200.0f, w, 260.0f))
      continue;
    for (float gx = x + 40.0f; gx < x + w; gx += 160.0f)
      drawGlow(r, mArt, gx, y, 150.0f, kLavaMid, 0.18f + 0.04f * std::sin(float(frame) * 0.05f + gx * 0.01f));
  }
}

void World::drawLavaFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  if (mLavas.empty() && mFootholds.empty())
    return;
  // Sinking stones and the bridge: basalt slabs whose rims glow as they sink.
  for (const auto& pl : mPlatforms)
  {
    if (pl.mode != PlatformMode::Sink && pl.mode != PlatformMode::Rise)
      continue;
    const float fx = float(pl.prevX) + float(pl.x - pl.prevX) * alpha;
    const float fy = float(pl.prevY) + float(pl.y - pl.prevY) * alpha;
    float x = fx * kCellPx - camX, y = fy * kCellPx - camY;
    const float w = float(pl.w) * kCellPx, h = float(pl.h) * kCellPx + 10.0f;
    if (!visible(x, y, w, h))
      continue;
    if (pl.shudder > 0)
      x += (frame / 2) % 2 ? 3.0f : -3.0f;
    const float sunk = std::clamp(float(pl.y - pl.homeY) / float(std::max(1, pl.floorY - pl.homeY)), 0.0f, 1.0f);
    const bool ferry = pl.ferry > 0;
    r.fillRect(x, y + 6.0f, w, h - 6.0f, kBasalt);
    r.fillRect(x + 4.0f, y, w - 8.0f, 10.0f, kBasaltLight);
    for (float cx = x + 18.0f; cx < x + w - 10.0f; cx += 34.0f)
      r.fillRect(cx, y + 16.0f, 3.0f, h - 24.0f, rgb(40, 36, 40));
    if (ferry)
    {
      // A raft of basalt on chains, with the serpent's eye on its prow.
      r.fillRect(x - 6.0f, y + 18.0f, w + 12.0f, 10.0f, rgb(90, 70, 50));
      r.fillRect(x + w * 0.5f - 6.0f, y + 26.0f, 12.0f, 12.0f, kLavaMid);
    }
    if (pl.mode == PlatformMode::Sink && sunk > 0.0f)
    {
      const Color rim = lerpColor(kLavaMid, kLavaHot, sunk);
      r.fillRect(x, y + 6.0f, w, 4.0f, withAlpha(rim, int(120 + 135 * sunk)), Blend::Add);
      r.fillRect(x, y + 6.0f, 4.0f, h - 6.0f, withAlpha(rim, int(160 * sunk)), Blend::Add);
      r.fillRect(x + w - 4.0f, y + 6.0f, 4.0f, h - 6.0f, withAlpha(rim, int(160 * sunk)), Blend::Add);
      drawGlow(r, mArt, x + w * 0.5f, y + 8.0f, w * 0.7f, kLavaHot, 0.25f * sunk);
    }
    if (pl.mode == PlatformMode::Rise && pl.riseAt >= 0 && pl.y > pl.homeY)
    {
      const int t = mStats.frames - pl.riseAt;
      if (t >= 0)
        drawGlow(r, mArt, x + w * 0.5f, y, w, kLavaHot, 0.5f);
    }
  }
  // Lava: a glowing body with a rolling surface and bubbles.
  for (const auto& l : mLavas)
  {
    const float x = float(l.x0) * kCellPx - camX, y = float(l.y0) * kCellPx - camY;
    const float w = float(l.x1 - l.x0 + 1) * kCellPx, h = float(l.y1 - l.y0 + 1) * kCellPx;
    if (!visible(x, y - 20.0f, w, h + 20.0f))
      continue;
    const float x0 = std::max(x, -40.0f), x1 = std::min(x + w, float(kScreenW) + 40.0f);
    r.fillRect(x0, y + 10.0f, x1 - x0, h - 10.0f, kLavaDeep);
    r.fillRect(x0, y + 10.0f, x1 - x0, std::min(h - 10.0f, 30.0f), kLavaMid);
    for (float sx = x0; sx < x1; sx += 8.0f)
    {
      const float wave = 4.0f * std::sin((sx + camX) * 0.03f + float(frame) * 0.09f) +
        2.0f * std::sin((sx + camX) * 0.11f - float(frame) * 0.13f);
      r.fillRect(sx, y + 4.0f + wave, 8.0f, 10.0f - wave * 0.5f, kLavaHot);
    }
    // Crust plates drifting on the surface.
    for (float sx = x0 - std::fmod(x0 + camX + float(frame) * 0.4f, 120.0f); sx < x1; sx += 120.0f)
      if (sx > x0 && sx + 34.0f < x1)
        r.fillRect(sx, y + 10.0f, 34.0f, 6.0f, rgba(80, 30, 20, 160));
    // Bubbles.
    for (int i = 0; i < 6; ++i)
    {
      const int t = (frame + i * 37) % 70;
      const float bx = x0 + std::fmod(float(i * 211 + l.x0 * 7), std::max(1.0f, x1 - x0));
      if (t < 20)
      {
        const float rad = 3.0f + float(t) * 0.5f;
        r.fillRect(bx - rad, y + 6.0f - rad * 0.6f, rad * 2.0f, rad * 1.2f, kLavaHot);
      }
    }
    for (float gx = x0 + 60.0f; gx < x1; gx += 200.0f)
      drawGlow(r, mArt, gx, y + 10.0f, 110.0f, kLavaHot, 0.25f);
  }
  // Toads about to come out: bubbles where they will, and a glowing dot on
  // the stone they are aiming for.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Toad || e.attach != 1)
      continue;
    const float bx = (float(e.x) + 1.5f) * kCellPx - camX, by = float(e.railX1) * kCellPx - camY;
    drawGlow(r, mArt, bx, by, 50.0f, kLavaHot, 0.6f);
    if (e.aimX >= 0 && e.aimX < int(mPlatforms.size()))
    {
      const Platform& pl = mPlatforms[std::size_t(e.aimX)];
      const float tx = (float(pl.x) + float(pl.w) * 0.5f) * kCellPx - camX, ty = float(pl.y) * kCellPx - camY;
      drawGlow(r, mArt, tx, ty, 22.0f + 6.0f * float((frame / 3) % 2), rgb(255, 80, 40), 0.9f);
    }
  }
  // Wisps swelling up.
  for (const auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Wisp && e.attach == 1)
    {
      const Vec2 c = cellCenter(e.box());
      const float grow = 1.0f - float(e.tell) / float(kWispSwell);
      drawGlow(r, mArt, c.x * kPixelScale - camX, c.y * kPixelScale - camY, 40.0f + 90.0f * grow,
        e.carrier ? rgb(140, 255, 70) : kLavaHot, 0.4f + 0.5f * grow);
    }
  // Spears in the wall.
  for (const auto& f : mFootholds)
  {
    if (f.life <= kSpearFlash && (frame / 4) % 2 == 0)
      continue;
    const float x = float(f.x) * kCellPx - camX, y = float(f.y) * kCellPx - camY;
    if (!visible(x, y, 64.0f, 64.0f))
      continue;
    const float wallX = f.dir > 0 ? x + 2.0f * kCellPx : x;
    const float tipX = f.dir > 0 ? x - 6.0f : x + 2.0f * kCellPx + 6.0f;
    r.drawLine(tipX, y + 6.0f, wallX, y + 6.0f, 9.0f, rgb(110, 76, 40));
    r.drawLine(tipX, y + 2.0f, wallX, y + 2.0f, 3.0f, rgb(170, 130, 80));
    // The serpent-head butt.
    r.fillRect(f.dir > 0 ? tipX - 10.0f : tipX - 6.0f, y - 4.0f, 16.0f, 18.0f, rgb(90, 200, 120));
    r.fillRect(f.dir > 0 ? tipX - 6.0f : tipX + 2.0f, y, 4.0f, 4.0f, rgb(255, 230, 90));
  }
  // The marshmallow on its stick.
  if (mOnStick && mPlayer.state != PlayerState::Dying)
  {
    const auto& p = mPlayer;
    const float px = (float(p.prevX) + float(p.x - p.prevX) * alpha + 1.5f) * kCellPx - camX;
    const float py = (float(p.prevY) + float(p.y - p.prevY) * alpha - 2.0f) * kCellPx - camY;
    const float tip = px + float(p.facing) * 92.0f;
    r.drawLine(px + float(p.facing) * 20.0f, py + 10.0f, tip, py - 18.0f, 4.0f, rgb(150, 110, 60));
    const float toast = std::min(1.0f, float(mRoast) / float(kRoastFrames));
    r.fillRect(tip - 10.0f, py - 30.0f, 20.0f, 20.0f, lerpColor(rgb(255, 250, 240), rgb(200, 120, 50), toast));
  }
}

void World::drawLavaProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame) const
{
  if (pr.kind == PropKind::Serpent)
  {
    // A carved serpent head in the basalt, smoke curling from its nostrils.
    const float dir = float(pr.hold);
    const float cy = y + h * 0.5f;
    r.fillRect(x + w * 0.1f, y + h * 0.15f, w * 0.8f, h * 0.7f, rgb(76, 68, 70));
    r.fillRect(x + w * 0.1f, y + h * 0.15f, w * 0.8f, 8.0f, rgb(120, 110, 112));
    r.fillRect(dir > 0 ? x + w * 0.62f : x + w * 0.02f, y + h * 0.4f, w * 0.36f, h * 0.38f, rgb(66, 58, 60));
    // The jaw line and the fangs.
    r.fillRect(dir > 0 ? x + w * 0.6f : x + w * 0.04f, y + h * 0.6f, w * 0.36f, 4.0f, rgb(30, 24, 26));
    for (int k = 0; k < 3; ++k)
      r.fillRect((dir > 0 ? x + w * 0.66f : x + w * 0.1f) + float(k) * 14.0f, y + h * 0.62f, 5.0f, 12.0f,
        rgb(230, 220, 200));
    // The eye glows.
    const float ex = dir > 0 ? x + w * 0.55f : x + w * 0.38f;
    r.fillRect(ex, y + h * 0.3f, 18.0f, 12.0f, kLavaHot);
    drawGlow(r, mArt, ex + 9.0f, y + h * 0.3f + 6.0f, 34.0f, kLavaMid, 0.6f);
    // Smoke.
    for (int i = 0; i < 4; ++i)
    {
      const int t = (frame + i * 15) % 60;
      const float sx = (dir > 0 ? x + w * 0.95f : x + w * 0.05f) + dir * float(t) * 0.5f;
      const float sy = cy - float(t) * 1.6f;
      const float rad = 6.0f + float(t) * 0.25f;
      r.fillRect(sx - rad, sy - rad, rad * 2.0f, rad * 2.0f, rgba(120, 110, 110, 150 - t * 2));
    }
    return;
  }
  if (pr.kind == PropKind::Marshmallow && !pr.used)
  {
    // On a stick leaning on the wall.
    r.drawLine(x + w * 0.2f, y + h, x + w * 0.8f, y - 10.0f, 4.0f, rgb(150, 110, 60));
    r.fillRect(x + w * 0.6f, y - 22.0f, 20.0f, 20.0f, rgb(255, 250, 240));
    r.fillRect(x + w * 0.6f, y - 22.0f, 20.0f, 4.0f, rgb(255, 255, 255));
  }
}

} // namespace gr
