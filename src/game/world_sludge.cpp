// Level 5, Sludge Line (SPEC 3.3 and 05): the tide. Sludge zones rise and
// drain on one 300-frame clock; standing in sludge costs a heart a second,
// slows you to a wade and takes your jump. Valve Keepers flood their zone on
// purpose, Sludge Gators lunge out of it and Pipe Rats pour out of the
// walls. The Bubble Gun traps them in bubbles that float up and ride the
// surface. Also the Duck Rapids bonus's autorun rule.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr int kTidePeriod = 300;
constexpr int kSludgeHeartFrames = 15;
constexpr int kBubbleLife = 150;
constexpr int kBubbleHoldFrames = 45;
constexpr int kStunFrames = 30;
constexpr int kPadlockFrames = 12;
constexpr int kKeeperTurnFrames = 30;

int sgn(int v) { return (v > 0) - (v < 0); }

// Cells the tide has risen, `ph` frames into its 300-frame cycle: siren
// 0-44, rise 45-89, high 90-179, drain 180-224, low 225-299.
int tideRise(int ph, int span)
{
  if (ph < 45)
    return 0;
  if (ph < 90)
    return span * (ph - 45) / 45;
  if (ph < 180)
    return span;
  if (ph < 225)
    return span - span * (ph - 180) / 45;
  return 0;
}

// A valve's flood, `d` frames after the turn: siren, rise, 300 frames high,
// drain, then back to the clock.
int floodRise(int d, int span)
{
  if (d < 45)
    return 0;
  if (d < 90)
    return span * (d - 45) / 45;
  if (d < 390)
    return span;
  if (d < 435)
    return span - span * (d - 390) / 45;
  return 0;
}

bool overlapsX(const CellBox& b, const Fluid& f) { return b.right() >= f.x0 && b.left() <= f.x1; }

} // namespace

// --- Level entities -------------------------------------------------------------

bool World::setupSludgeEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "fluid" && hasRect)
  {
    Fluid f;
    f.id = e.id;
    f.x0 = x0 * kCellsPerTile;
    f.x1 = (x1 + 1) * kCellsPerTile - 1;
    f.y0 = y0 * kCellsPerTile;
    f.y1 = (y1 + 1) * kCellsPerTile - 1;
    f.tide = e.str("kind", "sludge") == "tide";
    f.low = e.num("low", y1) * kCellsPerTile;
    f.high = e.num("high", y0) * kCellsPerTile;
    f.surface = f.tide ? f.low : f.y0;
    f.current = e.num("current", 0);
    mFluids.push_back(f);
    return true;
  }
  if (e.kind == "valve" && e.hasPos)
  {
    Valve v;
    v.id = e.id;
    v.x = e.x * kCellsPerTile;
    v.y = e.y * kCellsPerTile + 1;
    const std::string zone = e.str("zone");
    for (std::size_t i = 0; i < mFluids.size(); ++i)
      if (mFluids[i].id == zone)
        v.fluid = int(i);
    mValves.push_back(v);
    return true;
  }
  if (e.kind == "ratpipe" && e.hasPos)
  {
    RatPipe rp;
    rp.x = e.x * kCellsPerTile;
    rp.y = e.y * kCellsPerTile + 1;
    rp.dir = e.str("dir", "l") == "r" ? 1 : -1;
    rp.count = std::max(1, e.num("count", 5));
    mRatPipes.push_back(rp);
    return true;
  }
  if (e.kind == "raft" && e.hasPos)
  {
    // A cluster of bubbles that never pops, anchored where it floats.
    Bubble b;
    b.w = e.num("w", 3) * kCellsPerTile;
    b.h = 3;
    b.x = b.prevX = e.x * kCellsPerTile;
    b.y = b.prevY = e.y * kCellsPerTile - b.h + 1;
    b.life = -1;
    b.raft = true;
    mBubbles.push_back(b);
    return true;
  }
  if (e.kind == "bubble" && e.hasPos)
  {
    // A bubble around whatever enemy sits here (the candid camera).
    const int cx = e.x * kCellsPerTile, cy = e.y * kCellsPerTile;
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
    {
      Enemy& en = mEnemies[i];
      if (!en.box().intersects({cx, cy, 2, 2}) || en.trapped)
        continue;
      Bubble b;
      b.w = en.w + 2;
      b.h = en.h + 2;
      b.x = b.prevX = en.x - 1;
      b.y = b.prevY = en.y - en.h;
      b.enemy = int(i);
      b.life = -1;
      b.hp = std::max(1, e.num("hp", 1));
      en.trapped = true;
      mBubbles.push_back(b);
      break;
    }
    return true;
  }
  if (e.kind == "devnull" && e.hasPos)
  {
    Prop pr;
    pr.kind = PropKind::DevNull;
    pr.x = e.x * kCellsPerTile;
    pr.y = e.y * kCellsPerTile;
    pr.w = pr.h = 2 * kCellsPerTile;
    mProps.push_back(pr);
    mDevNull.push_back(pr.box());
    return true;
  }
  return false;
}

void World::setupSludgeEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::Gator)
  {
    en.variant = e.str("look") == "sunglasses" ? 1 : 0;
    // Its home is the smallest sludge it starts in.
    en.attach = -1;
    int best = 1 << 30;
    for (std::size_t i = 0; i < mFluids.size(); ++i)
    {
      const auto& f = mFluids[i];
      const int area = (f.x1 - f.x0) * (f.y1 - f.y0);
      if (f.covers(en.x + en.w / 2) && en.y >= f.y0 && en.y <= f.y1 && area < best)
      {
        best = area;
        en.attach = int(i);
      }
    }
  }
  else if (en.kind == EnemyKind::Keeper)
  {
    en.attach = 0; // waiting
    en.aimX = -1;  // its valve
    const std::string valve = e.str("valve");
    for (std::size_t i = 0; i < mValves.size(); ++i)
      if (mValves[i].id == valve)
        en.aimX = int(i);
  }
}

// --- Queries ------------------------------------------------------------------------

int World::fluidSurface(const Fluid& f) const
{
  if (!f.tide)
    return f.surface;
  const int span = f.low - f.high;
  int rise = tideRise(mStats.frames % kTidePeriod, span);
  if (f.floodAt > -100000)
    rise = std::max(rise, floodRise(mStats.frames - f.floodAt, span));
  return f.low - rise;
}

int World::fluidAt(int cx, int cy) const
{
  for (std::size_t i = 0; i < mFluids.size(); ++i)
    if (mFluids[i].wet(cx, cy))
      return int(i);
  return -1;
}

int World::wadeFluid() const
{
  const auto& p = mPlayer;
  const CellBox b = p.box();
  for (std::size_t i = 0; i < mFluids.size(); ++i)
  {
    const auto& f = mFluids[i];
    if (overlapsX(b, f) && p.y >= f.surface && p.y <= f.y1 && b.top() <= f.y1)
      return int(i);
  }
  return -1;
}

bool World::wading() const { return !mAutorun && mPlayer.turbo == 0 && wadeFluid() >= 0; }

bool World::buoyed() const
{
  const auto& p = mPlayer;
  if (p.turbo > 0 || mDiving || mAutorun || p.state == PlayerState::Ladder)
    return false;
  const int i = wadeFluid();
  return i >= 0 && p.y > mFluids[std::size_t(i)].surface;
}

bool World::wadeStep(int dir)
{
  // Out of sludge you can climb a block onto the bank.
  auto& p = mPlayer;
  const int h = p.height();
  for (int up = 1; up <= kCellsPerTile; ++up)
  {
    if (mMap.overlapsSolid(boxAt(p.x, p.y - up, Player::kWidth, h)))
      return false;
    if (!mMap.overlapsSolid(boxAt(p.x + dir, p.y - up, Player::kWidth, h)))
    {
      p.x += dir;
      p.y -= up;
      return true;
    }
  }
  return false;
}

// --- Update ---------------------------------------------------------------------------

void World::syncFloats()
{
  mMap.clearFloats();
  // Turbo walks under the sludge, and holding down dives into it.
  // (Duck Rapids' river is only skin deep: the duck rides the bed.)
  if (mPlayer.turbo == 0 && !mDiving && !mAutorun)
    for (const auto& f : mFluids)
      mMap.addFloat({f.x0, f.surface + 1, f.x1 - f.x0 + 1, 1});
  for (const auto& b : mBubbles)
    mMap.addFloat(b.box());
  for (const auto& t : mTrailBlocks)
    mMap.addFloat({t.x, t.y, Player::kWidth, 1}); // Light Trail (world_maglev.cpp)
  for (const auto& f : mFootholds)
    mMap.addFloat({f.x, f.y, 2, 1}); // Serpent Spears in the wall (world_lava.cpp)
}

void World::floatItem(Item& it) const
{
  for (const auto& f : mFluids)
    if (overlapsX(it.box(), f) && it.y > f.surface && it.y <= f.y1)
      it.y = f.surface; // gems and drops bob on the surface
}

void World::updateSludge(const PlayerInput& input)
{
  if (mFluids.empty() && mBubbles.empty())
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int clock = mStats.frames;

  // Duck Rapids: the duck paddles right half a cell a frame and never
  // stops; whatever it runs into bumps it back four blocks.
  if (mAutorun && alive)
  {
    const int h = p.height();
    if (mBump > 0)
    {
      --mBump;
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, h, -1);
    }
    else if (clock % 2 == 0 && mMap.moveHorizontally(p.x, p.y, Player::kWidth, h, 1) == MoveResult::Failed)
    {
      mBump = 2 * kCellsPerTile * 2;
      p.facing = 1;
      playSound(Sfx::Quack);
      burst({(float(p.x) + 3.0f) * kCellSize, (float(p.y) - 2.0f) * kCellSize}, rgb(255, 230, 80), rgb(255, 255, 255), 8,
        1.2f);
    }
    if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
      p.state = PlayerState::Falling;
  }

  // The surfaces.
  std::vector<int> old(mFluids.size());
  bool nearTide = false;
  for (std::size_t i = 0; i < mFluids.size(); ++i)
  {
    auto& f = mFluids[i];
    old[i] = f.surface;
    if (!f.tide)
      continue;
    f.surface = fluidSurface(f);
    nearTide = nearTide || (p.x > f.x0 - 60 && p.x < f.x1 + 60);
  }
  if (nearTide && clock % kTidePeriod == 0)
  {
    playSound(Sfx::Siren);
    showMessage("TIDE SIREN - GET TO HIGH GROUND");
  }
  syncFloats();

  // Whatever stood on a surface that moved goes with it.
  const CellBox pb = p.box();
  for (std::size_t i = 0; i < mFluids.size(); ++i)
  {
    const auto& f = mFluids[i];
    const int d = f.surface - old[i];
    if (d == 0)
      continue;
    if (p.state == PlayerState::OnGround && p.y == old[i] && overlapsX(pb, f) && !mDiving && p.turbo == 0)
      mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), d);
    for (auto& e : mEnemies)
      if (e.alive && !e.trapped && (e.kind == EnemyKind::Walker || e.kind == EnemyKind::Keeper) && e.y == old[i] &&
          overlapsX(e.box(), f))
        mMap.moveVertically(e.x, e.y, e.w, e.h, d);
  }
  // Under the surface, sludge pushes you up a cell every other frame.
  if (alive && buoyed() && clock % 2 == 0)
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1);
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.trapped || !(e.kind == EnemyKind::Walker || e.kind == EnemyKind::Keeper))
      continue;
    for (const auto& f : mFluids)
      if (overlapsX(e.box(), f) && e.y > f.surface && e.y <= f.y1)
        mMap.moveVertically(e.x, e.y, e.w, e.h, f.surface - e.y);
  }
  // Boxes float up off the floor and settle back.
  for (auto& b : mBoxes)
  {
    if (!b.alive)
      continue;
    if (b.restY < 0)
      b.restY = b.y;
    int y = b.restY;
    for (const auto& f : mFluids)
      if (overlapsX(b.box(), f) && b.restY >= f.y0 - 1 && b.restY <= f.y1)
        y = std::min(y, f.surface);
    b.y = y;
  }

  // Wading: a heart every second, and a splash on the way in.
  const int wf = wadeFluid();
  if (alive && wf >= 0 && p.turbo == 0 && !mAutorun)
  {
    if (mSludgeTicks <= 0 || mSludgeTicks >= kSludgeHeartFrames)
    {
      if (mSludgeTicks <= 0)
      {
        playSound(Sfx::Splash);
        burst({(float(p.x) + 1.5f) * kCellSize, float(p.y) * kCellSize}, rgb(150, 220, 60), rgb(60, 120, 30), 10, 1.4f,
          false);
      }
      mSludgeTicks = kSludgeHeartFrames;
    }
    if (--mSludgeTicks == 0)
    {
      const int mercy = p.mercy;
      p.mercy = 0;
      hurtPlayer(1);
      if (p.state != PlayerState::Dying)
        p.mercy = mercy;
      mSludgeTicks = kSludgeHeartFrames;
    }
  }
  else if (mSludgeTicks > 0 && ++mSludgeTicks >= kSludgeHeartFrames)
    mSludgeTicks = 0;
  mDiving = alive && wf >= 0 && input.down && !input.left && !input.right && p.state == PlayerState::OnGround;

  // Valves: hold up beside one to padlock it.
  for (auto& v : mValves)
  {
    if (v.locked)
      continue;
    const CellBox near{v.x - 3, v.y - 5, 10, 6};
    if (alive && p.state == PlayerState::OnGround && input.up && near.intersects(p.box()))
    {
      if (++v.hold >= kPadlockFrames)
      {
        v.locked = true;
        showMessage("VALVE PADLOCKED - NO MORE FLOODS HERE");
        playSound(Sfx::Item);
        addScore(500, cellCenter(v.box()));
        burst(cellCenter(v.box()), rgb(255, 230, 120), rgb(255, 255, 255), 12, 1.4f);
      }
    }
    else
      v.hold = 0;
  }

  updateRatPipes();
  updateBubbles();

  // /dev/null: whatever floats or runs into the pipe's mouth is gone.
  for (const auto& m : mDevNull)
    for (auto& e : mEnemies)
      if (e.alive && !e.trapped && e.kind == EnemyKind::Walker && e.box().intersects(m))
      {
        e.alive = false;
        playSound(Sfx::Bloop);
      }
  syncFloats();
}

// --- Rat pipes ------------------------------------------------------------------------

void World::updateRatPipes()
{
  const auto& p = mPlayer;
  const int rat = enemyIndex("pipe_rat");
  for (auto& rp : mRatPipes)
  {
    const CellBox mouth = boxAt(rp.x, rp.y, 2, 2);
    if (!rp.armed)
    {
      // Re-arms once it has been off screen for a while.
      if (rp.left == 0 && rp.rattle == 0)
      {
        rp.idle = isOnScreen(mouth, 2) ? 0 : rp.idle + 1;
        if (rp.idle >= 300)
        {
          rp.armed = true;
          rp.idle = 0;
        }
      }
    }
    else if (p.state != PlayerState::Dying && std::abs(p.x + 1 - rp.x) <= 16 && std::abs(p.y - rp.y) <= 10)
    {
      rp.armed = false;
      rp.rattle = 15;
      playSound(Sfx::Rattle);
    }
    if (rp.rattle > 0 && --rp.rattle == 0)
    {
      rp.left = rp.count;
      rp.next = 0;
    }
    if (rp.left > 0 && --rp.next <= 0)
    {
      spawnEnemy(rat, rp.dir > 0 ? rp.x + 1 : rp.x - 1, rp.y);
      Enemy& e = mEnemies.back();
      e.dir = rp.dir;
      e.active = true;
      e.timer = 0;
      --rp.left;
      rp.next = 4; // two blocks apart at a cell a frame
    }
  }
}

// --- Bubbles ------------------------------------------------------------------------

bool World::trapEnemy(Enemy& e)
{
  if (e.w > 4 || e.h > 3 || e.kind == EnemyKind::Camera || e.trapped)
    return false;
  Bubble b;
  b.w = e.w + 2;
  b.h = e.h + 2;
  b.x = b.prevX = e.x - 1;
  b.y = b.prevY = e.y - e.h;
  b.enemy = int(&e - mEnemies.data());
  b.life = kBubbleLife;
  e.trapped = true;
  e.tell = 0;
  e.dive = 0;
  mBubbles.push_back(b);
  playSound(Sfx::Bubble);
  if (e.kind == EnemyKind::Gator && e.variant == 1)
    showMessage("THE GATOR JUST NODS AT YOU");
  return true;
}

void World::popBubble(std::size_t i, bool stun)
{
  Bubble b = mBubbles[i];
  mBubbles.erase(mBubbles.begin() + std::ptrdiff_t(i));
  if (b.enemy >= 0 && std::size_t(b.enemy) < mEnemies.size())
  {
    Enemy& e = mEnemies[std::size_t(b.enemy)];
    e.trapped = false;
    e.stun = stun ? kStunFrames : 0;
    e.drawSnap = true;
  }
  const Vec2 c = cellCenter(b.box());
  burst(c, rgb(200, 240, 255), rgb(255, 255, 255), 14, 1.6f);
  playSound(Sfx::Bloop);
}

bool World::shotAtBubbles(const CellBox& b)
{
  for (std::size_t i = 0; i < mBubbles.size(); ++i)
  {
    Bubble& bb = mBubbles[i];
    if (bb.hp <= 0 || !bb.box().intersects(b))
      continue;
    if (--bb.hp <= 0)
      popBubble(i, false);
    return true;
  }
  return false;
}

bool World::shotAtSludge(const Projectile& pr)
{
  const CellBox b = pr.box();
  if (fluidAt(b.x + b.w / 2, b.y + b.h / 2) >= 0)
  {
    burst(cellCenter(b), rgb(150, 220, 60), rgb(80, 140, 40), 6, 1.0f, false);
    return true;
  }
  for (const auto& m : mDevNull)
    if (m.intersects(b))
    {
      playSound(Sfx::Bloop);
      return true;
    }
  return false;
}

void World::updateBubbles()
{
  auto& p = mPlayer;
  for (std::size_t i = 0; i < mBubbles.size();)
  {
    Bubble& b = mBubbles[i];
    b.prevX = b.x;
    b.prevY = b.y;
    const CellBox before = b.box();
    const bool riding = p.state == PlayerState::OnGround && p.y == before.top() - 1 &&
      p.x + Player::kWidth - 1 >= before.left() && p.x <= before.right();
    if (riding && b.life >= 0 && ++b.stood >= kBubbleHoldFrames)
    {
      popBubble(i, true);
      continue;
    }
    if (b.life > 0 && --b.life == 0)
    {
      popBubble(i, true);
      continue;
    }
    if (b.hp <= 0)
    {
      // Float to the surface underneath (or up out of the sludge) and ride it.
      const Fluid* under = nullptr;
      for (const auto& f : mFluids)
        if (f.covers(b.x + b.w / 2) && before.bottom() <= f.y1 && (!under || f.surface < under->surface))
          under = &f;
      int dx = 0, dy = 0;
      if (under)
      {
        const int target = under->surface; // its bottom row rides the surface
        dy = sgn(target - before.bottom());
        if (dy == 0 && under->current > 0 && !b.raft)
        {
          b.drift += under->current;
          if (b.drift >= 4)
          {
            b.drift -= 4;
            dx = 1;
          }
        }
      }
      auto blocked = [&](int ddx, int ddy) { return mMap.overlapsSolid({b.x + ddx, b.y + ddy, b.w, b.h}); };
      if (dy != 0 && blocked(0, dy))
        dy = 0;
      if (dx != 0 && blocked(dx, dy))
        dx = 0;
      b.x += dx;
      b.y += dy;
      if (riding && (dx != 0 || dy != 0))
      {
        if (dx != 0)
          mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dx);
        if (dy < 0)
          mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), dy);
        else if (dy > 0 && !mMap.overlapsSolid(boxAt(p.x, p.y + dy, Player::kWidth, p.height())))
          p.y += dy;
      }
    }
    if (b.enemy >= 0 && std::size_t(b.enemy) < mEnemies.size())
    {
      Enemy& e = mEnemies[std::size_t(b.enemy)];
      if (!e.alive)
      {
        mBubbles.erase(mBubbles.begin() + std::ptrdiff_t(i));
        continue;
      }
      e.x = b.x + (b.w - e.w) / 2;
      e.y = b.y + b.h - 2;
    }
    bool gone = false;
    for (const auto& m : mDevNull)
      if (!b.raft && b.hp <= 0 && b.box().intersects(m))
        gone = true;
    if (gone)
    {
      if (b.enemy >= 0 && std::size_t(b.enemy) < mEnemies.size())
        mEnemies[std::size_t(b.enemy)].alive = false; // no score: /dev/null keeps it
      playSound(Sfx::Bloop);
      mBubbles.erase(mBubbles.begin() + std::ptrdiff_t(i));
      continue;
    }
    ++i;
  }
}

// --- Enemies ----------------------------------------------------------------------------

void World::updateGator(Enemy& e, const EnemyDef& def)
{
  if (e.attach < 0 || std::size_t(e.attach) >= mFluids.size())
    return;
  const Fluid& f = mFluids[std::size_t(e.attach)];
  const auto& p = mPlayer;
  // Under the surface: two cells of sludge over its back.
  const int rest = std::min(f.y1, f.surface + 1 + e.h);
  const int peak = f.surface - 6 + e.h - 1; // a lunge's top: three blocks out
  if (e.dive > 0)
  {
    // Lunge: up three cells a frame, hold, back down.
    ++e.dive;
    if (e.dive <= 5)
      e.y = std::max(peak, e.y - 3);
    else if (e.dive > 8)
    {
      e.y = std::min(rest, e.y + 3);
      if (e.y >= rest)
      {
        e.dive = 0;
        e.lastDive = e.timer;
      }
    }
    return;
  }
  e.y = rest;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      e.dive = 1;
      playSound(Sfx::Splash);
      burst({(float(e.x) + float(e.w) * 0.5f) * kCellSize, float(f.surface) * kCellSize},
        e.carrier ? rgb(140, 255, 70) : rgb(150, 220, 60), rgb(60, 120, 30), 16, 2.0f, false);
    }
    return;
  }
  // Swim back and forth along the bottom of its sludge.
  if (e.timer % std::max(1, def.stepEvery) == 0)
  {
    const int nx = e.x + e.dir;
    if (nx < f.x0 || nx + e.w - 1 > f.x1 || mMap.overlapsSolid(boxAt(nx, e.y, e.w, e.h)))
      e.dir = -e.dir;
    else
      e.x = nx;
  }
  // Lunges at a runner close above; the one in sunglasses never bothers.
  const CellBox pb = p.box();
  const int gap = std::max({0, pb.left() - (e.x + e.w - 1), e.x - pb.right()});
  const bool ready = e.lastDive < 0 || e.timer - e.lastDive >= def.cooldown;
  if (e.variant == 0 && ready && gap <= def.range && p.y <= f.surface + 1 && p.y >= f.surface - 16 &&
      p.state != PlayerState::Dying && p.turbo == 0)
    e.tell = def.tell;
}

void World::updateKeeper(Enemy& e, const EnemyDef& def)
{
  Valve* v = e.aimX >= 0 && std::size_t(e.aimX) < mValves.size() ? &mValves[std::size_t(e.aimX)] : nullptr;
  const auto& p = mPlayer;
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    return;
  }
  if (v && v->locked && e.attach < 3)
    e.attach = 3; // padlocked: nothing left but to fight
  switch (e.attach)
  {
    case 0: // waits until a runner comes into its zone
      if (v && v->fluid >= 0)
      {
        const Fluid& f = mFluids[std::size_t(v->fluid)];
        if (p.x + 2 >= f.x0 && p.x <= f.x1 && p.y >= f.high - 12 && p.state != PlayerState::Dying)
          e.attach = 1;
      }
      break;
    case 1: // walks to the wheel
    {
      if (!v)
      {
        e.attach = 3;
        break;
      }
      const int target = v->x + 2 - e.w / 2;
      e.dir = target < e.x ? -1 : 1;
      if (e.x == target)
      {
        e.attach = 2;
        e.tell = kKeeperTurnFrames;
        playSound(Sfx::Klaxon);
      }
      else if (e.timer % std::max(1, def.stepEvery) == 0)
        mMap.moveHorizontallyWithStairStepping(e.x, e.y, e.w, e.h, e.dir);
      break;
    }
    case 2: // turns it: the klaxon, then the flood
      if (e.tell > 0 && --e.tell == 0)
      {
        if (v && v->fluid >= 0)
        {
          mFluids[std::size_t(v->fluid)].floodAt = mStats.frames;
          playSound(Sfx::Siren);
          showMessage("THE KEEPER OPENED THE VALVE - FLOOD COMING");
        }
        e.attach = 3;
      }
      break;
    default: // guards its patch
    {
      if (e.timer % std::max(1, def.stepEvery) != 0)
        break;
      const CellBox b = e.box();
      const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
      const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
      const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
      // Turns toward a runner close by.
      if (std::abs(p.x - e.x) < 16 && std::abs(p.y - e.y) < 6)
        e.dir = p.x < e.x ? -1 : 1;
      if (wall || ledge)
        e.dir = -e.dir;
      else
        e.x += e.dir;
      break;
    }
  }
}

// --- Drawing --------------------------------------------------------------------------

void World::drawSludgeBack(Renderer& r, float camX, float camY, int frame) const
{
  if (mAutorun && !mPlayer.hidden)
  {
    // The runner's ride: a rubber duck, bobbing.
    const auto& p = mPlayer;
    const float x = (float(p.x) + 1.5f) * kCellPx - camX;
    const float y = (float(p.y) + 1.6f) * kCellPx - camY + 3.0f * std::sin(float(frame) * 0.2f);
    r.draw(styledEnemySprite(mArt, r, mTheme, "rubber_duck", 0, 0, 5, 3).get(1), x, y);
  }
  if (mFluids.empty() && mValves.empty() && mRatPipes.empty())
    return;
  // High-water marks along the zones' walls.
  for (const auto& f : mFluids)
  {
    if (!f.tide)
      continue;
    const float y = float(f.high) * kCellPx - camY;
    for (int cx = f.x0; cx <= f.x1; cx += 8)
    {
      const float x = float(cx) * kCellPx - camX;
      if (x < -300.0f || x > float(kScreenW) + 40.0f)
        continue;
      r.fillRect(x, y - 3.0f, 5.0f * kCellPx, 5.0f, rgba(170, 200, 90, 90));
      r.fillRect(x, y + 4.0f, 5.0f * kCellPx, 2.0f, rgba(40, 60, 30, 120));
    }
  }
  for (const auto& v : mValves)
  {
    const float x = (float(v.x) + 2.0f) * kCellPx - camX, y = (float(v.y) + 1.0f) * kCellPx - camY;
    if (x < -160.0f || x > float(kScreenW) + 160.0f)
      continue;
    bool turning = false;
    for (const auto& e : mEnemies)
      turning = turning || (e.alive && e.kind == EnemyKind::Keeper && e.attach == 2 &&
                             &v - mValves.data() == e.aimX);
    const int f = turning ? (frame / 3) % 4 : 0;
    r.draw(styledEnemySprite(mArt, r, mTheme, "valve", v.locked ? 1 : 0, f, 4, 4).get(1), x, y);
    if (v.hold > 0 && !v.locked)
    {
      const float t = float(v.hold) / float(kPadlockFrames);
      r.fillRect(x - 40.0f, y - 150.0f, 80.0f, 10.0f, rgba(0, 0, 0, 160));
      r.fillRect(x - 38.0f, y - 148.0f, 76.0f * t, 6.0f, rgb(255, 230, 120));
    }
  }
  for (const auto& rp : mRatPipes)
  {
    float x = (float(rp.x) + 1.0f) * kCellPx - camX;
    const float y = (float(rp.y) + 1.0f) * kCellPx - camY;
    if (x < -160.0f || x > float(kScreenW) + 160.0f)
      continue;
    if (rp.rattle > 0)
      x += (frame / 2) % 2 ? 4.0f : -4.0f;
    r.draw(styledEnemySprite(mArt, r, mTheme, "rat_pipe", 0, 0, 2, 2).get(rp.dir), x, y);
  }
}

void World::drawSludgeFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  // Bubbles first: the sludge goes over their lower half.
  for (const auto& b : mBubbles)
  {
    const float bx = (float(b.prevX) + (float(b.x - b.prevX)) * alpha + float(b.w) * 0.5f) * kCellPx - camX;
    const float by = (float(b.prevY) + (float(b.y - b.prevY)) * alpha + float(b.h)) * kCellPx - camY;
    if (bx < -200.0f || bx > float(kScreenW) + 200.0f)
      continue;
    const bool popping = b.life >= 0 && (b.life < 30 || b.stood > kBubbleHoldFrames - 15);
    const float wob = popping ? 3.0f * std::sin(float(frame) * 1.3f) : 0.0f;
    const int variant = b.raft ? 1 : 0;
    r.draw(styledEnemySprite(mArt, r, mTheme, b.raft ? "bubble_raft" : "bubble", variant, (frame / 10) % 2, b.w, b.h)
             .get(1),
      bx + wob, by);
  }
  const int tx0 = std::max(0, int(camX / kTilePx) - 1);
  const int tx1 = std::min(mLevel->width - 1, int((camX + float(kScreenW)) / kTilePx) + 1);
  for (const auto& f : mFluids)
  {
    const int bx0 = std::max(tx0, f.x0 / kCellsPerTile), bx1 = std::min(tx1, f.x1 / kCellsPerTile);
    if (bx0 > bx1)
      continue;
    const int s = f.surface;
    const Color body = rgba(96, 150, 34, 196), deep = rgba(50, 82, 22, 220);
    for (int ty = s / kCellsPerTile; ty <= f.y1 / kCellsPerTile; ++ty)
    {
      const float top = std::max(float(s), float(ty * kCellsPerTile)) * kCellPx - camY;
      const float bottom = float((ty + 1) * kCellsPerTile) * kCellPx - camY;
      if (bottom < 0.0f || top > float(kScreenH))
        continue;
      // Runs of blocks the sludge fills: open space and grates, not walls.
      int run = -1;
      for (int tx = bx0; tx <= bx1 + 1; ++tx)
      {
        const bool wet = tx <= bx1 && mMap.block(tx, ty) != Tile::Solid;
        if (wet && run < 0)
          run = tx;
        if (!wet && run >= 0)
        {
          const float x0 = float(run) * kTilePx - camX, x1 = float(tx) * kTilePx - camX;
          r.fillRect(x0, top, x1 - x0, bottom - top, ty * kCellsPerTile >= s + 4 ? deep : body);
          run = -1;
        }
      }
    }
    // The surface: a bright scum line with slow ripples, and pops of gas.
    const float sy = float(s) * kCellPx - camY;
    for (int tx = bx0; tx <= bx1; ++tx)
    {
      const int ty = s / kCellsPerTile;
      if (mMap.block(tx, ty) == Tile::Solid)
        continue;
      const float x = float(tx) * kTilePx - camX;
      for (int k = 0; k < 4; ++k)
      {
        const float wave = 3.0f * std::sin(float(frame) * 0.12f + float(tx * 4 + k) * 0.9f);
        r.fillRect(x + float(k) * 16.0f, sy + wave - 3.0f, 16.0f, 6.0f, rgba(190, 240, 90, 210));
      }
      const unsigned h = hash2(tx, frame / 20);
      if (h % 7u == 0)
      {
        const float gx = x + float(h % 50u) + 6.0f;
        const float g = float(frame % 20) / 20.0f;
        r.fillRect(gx, sy - 6.0f - 10.0f * g, 8.0f, 8.0f, rgba(200, 255, 120, int(200 * (1.0f - g))));
      }
    }
  }
  // Gator tells: a trail of bubbles before the lunge.
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Gator || e.tell <= 0 || e.attach < 0)
      continue;
    const float sy = float(mFluids[std::size_t(e.attach)].surface) * kCellPx - camY;
    for (int k = 0; k < 4; ++k)
    {
      const float x = (float(e.x) + float(k) + 0.5f) * kCellPx - camX;
      const float lift = float((frame + k * 5) % 12);
      r.fillRect(x - 5.0f, sy - lift, 10.0f, 10.0f,
        e.carrier ? rgba(150, 255, 80, 220) : rgba(220, 255, 170, 220));
    }
  }
}

void World::drawTideHud(Renderer& r, int frame) const
{
  const auto& p = mPlayer;
  const Fluid* near = nullptr;
  for (const auto& f : mFluids)
    if (f.tide && p.x > f.x0 - 40 && p.x < f.x1 + 40)
      near = &f;
  if (!near || mState != WorldState::Playing)
    return;
  // A gauge: where the tide is, and the siren light.
  const float x = float(kScreenW) - 70.0f, y = 120.0f, h = 160.0f;
  r.fillRect(x - 6.0f, y - 30.0f, 52.0f, h + 44.0f, rgba(10, 16, 8, 170));
  r.drawText("TIDE", x + 20.0f, y - 26.0f, {18.0f, rgb(190, 240, 90), rgb(10, 16, 8)}, Align::Center);
  const int span = std::max(1, near->low - near->high);
  const float level = float(near->low - near->surface) / float(span);
  r.fillRect(x + 8.0f, y, 24.0f, h, rgba(40, 50, 30, 220));
  r.fillRect(x + 8.0f, y + h * (1.0f - level), 24.0f, h * level, rgb(130, 200, 50));
  const int ph = mStats.frames % kTidePeriod;
  const int flood = near->floodAt > -100000 ? mStats.frames - near->floodAt : -1;
  const bool siren = ph < 45 || (flood >= 0 && flood < 45);
  if (siren && (frame / 8) % 2 == 0)
    drawGlow(r, mArt, x + 20.0f, y + h + 10.0f, 30, rgb(255, 60, 40), 0.9f);
  r.fillRect(x + 14.0f, y + h + 4.0f, 12.0f, 12.0f, siren ? rgb(255, 80, 60) : rgb(90, 40, 30));
}

} // namespace gr
