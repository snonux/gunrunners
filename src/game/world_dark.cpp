// Level 4, Blackout (SPEC 04): power cuts. The district is split into dark
// sectors; throwing a sector's breaker relights it as a wave and opens its
// shutters. The Flare Gun sticks lights to walls and enemies. Also the
// dark's residents (Night Stalker, Looter, Grid Leech) and the Echo Room
// bonus's sonar.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr int kFlareRadius = 12; // cells: a flare lights 6 blocks around it
constexpr int kMaxFlares = 4;

int sgn(int v) { return (v > 0) - (v < 0); }

float dist(float ax, float ay, float bx, float by) { return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }

// Total length of a cable in cells, and the point `at` cells along it.
int cableLength(const Cable& c)
{
  int n = 0;
  for (std::size_t i = 1; i < c.path.size(); ++i)
    n += std::max(std::abs(c.path[i].first - c.path[i - 1].first), std::abs(c.path[i].second - c.path[i - 1].second));
  return n;
}

std::pair<int, int> cablePoint(const Cable& c, int at)
{
  for (std::size_t i = 1; i < c.path.size(); ++i)
  {
    const auto [x0, y0] = c.path[i - 1];
    const auto [x1, y1] = c.path[i];
    const int len = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    if (at <= len)
      return {x0 + sgn(x1 - x0) * std::min(at, std::abs(x1 - x0)), y0 + sgn(y1 - y0) * std::min(at, std::abs(y1 - y0))};
    at -= len;
  }
  return c.path.empty() ? std::pair<int, int>{0, 0} : c.path.back();
}

} // namespace

// --- Level entities -------------------------------------------------------------

bool World::setupDarkEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  auto breakerIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < mBreakers.size(); ++i)
      if (mBreakers[i].id == id)
        return int(i);
    return -1;
  };
  if (e.kind == "dark" && hasRect)
  {
    mSectors.push_back({e.id, x0, y0, x1, y1, -1});
    return true;
  }
  if (e.kind == "light" && e.hasPos)
  {
    mLights.push_back({e.x * kCellsPerTile + 1, e.y * kCellsPerTile + 1, e.num("r", 6) * kCellsPerTile});
    return true;
  }
  if (e.kind == "breaker" && e.hasPos)
  {
    Breaker b;
    b.id = e.id;
    b.x = e.x * kCellsPerTile;
    b.y = e.y * kCellsPerTile + 1;
    const std::string sector = e.str("sector");
    for (std::size_t i = 0; i < mSectors.size(); ++i)
      if (mSectors[i].id == sector)
      {
        b.sector = int(i);
        mSectors[i].breaker = int(mBreakers.size());
      }
    mBreakers.push_back(b);
    return true;
  }
  if (e.kind == "door" && e.hasPos)
  {
    Door d;
    d.id = e.id;
    d.x0 = e.x;
    d.y0 = e.y;
    d.w = std::max(1, e.num("w", 1));
    d.h = std::max(1, e.num("h", 4));
    d.breaker = breakerIndex(e.str("open"));
    d.opentime = std::max(1, e.num("opentime", 15));
    applyDoor(d, true);
    mDoors.push_back(d);
    return true;
  }
  if (e.kind == "cable")
  {
    Cable c;
    c.id = e.id;
    for (const auto& [px, py] : e.path("path"))
      c.path.emplace_back(px * kCellsPerTile, py * kCellsPerTile);
    c.breaker = breakerIndex(e.str("breaker"));
    if (c.path.size() >= 2)
      mCables.push_back(c);
    return true;
  }
  if (e.kind == "spikes" && hasRect)
  {
    // hidden=dark: these spikes only draw once their sector is lit.
    mHiddenSpikes.push_back({{x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile,
                               (y1 - y0 + 1) * kCellsPerTile},
      0});
    return true;
  }
  if (e.kind == "stash" && hasRect)
  {
    mStash = {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
    const auto m = e.list("manhole");
    if (m.size() == 2)
    {
      mManholeX = m[0] * kCellsPerTile;
      mManholeY = m[1] * kCellsPerTile;
    }
    return true;
  }
  return false;
}

void World::applyDoor(Door& d, bool solid)
{
  d.solid = solid;
  for (int ty = d.y0; ty < d.y0 + d.h; ++ty)
    for (int tx = d.x0; tx < d.x0 + d.w; ++tx)
      mMap.setBlock(tx, ty, solid ? Tile::Solid : Tile::Empty);
}

// --- Light ------------------------------------------------------------------------

bool World::darkAt(int cx, int cy) const
{
  for (const auto& s : mSectors)
  {
    if (!s.contains(cx, cy))
      continue;
    if (s.breaker < 0)
      return true; // only flares ever light it
    const Breaker& b = mBreakers[std::size_t(s.breaker)];
    if (!b.on)
      return true;
    // The relight spreads from the breaker a block a frame.
    const int radius = (mStats.frames - b.thrownAt) * kCellsPerTile;
    const int bx = b.x + 1, by = b.y - 2;
    const bool reached = std::max(std::abs(cx - bx), std::abs(cy - by)) <= radius;
    const bool global = mAllLitAt >= 0 && mStats.frames - mAllLitAt >= 60;
    if (!reached && !global)
      return true;
  }
  return false;
}

bool World::litAt(int cx, int cy) const
{
  if (!darkAt(cx, cy))
    return true;
  for (const auto& l : mLights)
    if (std::max(std::abs(cx - l.x), std::abs(cy - l.y)) <= l.r)
      return true;
  for (const auto& f : mFlares)
    if (dist(float(cx) + 0.5f, float(cy) + 0.5f, f.x, f.y) <= float(kFlareRadius))
      return true;
  return false;
}

float World::lightLevel(int cx, int cy) const
{
  if (mSonar)
    return 0.0f;
  if (!darkAt(cx, cy))
    return 1.0f;
  float best = 0.0f;
  auto pool = [&](float lx, float ly, float r, float strength) {
    const float d = dist(float(cx) + 0.5f, float(cy) + 0.5f, lx, ly);
    if (d < r)
      best = std::max(best, strength * (1.0f - (d / r) * (d / r)));
  };
  for (const auto& l : mLights)
    pool(float(l.x) + 0.5f, float(l.y) + 0.5f, float(l.r) * 1.25f, 1.0f);
  for (const auto& f : mFlares)
    pool(f.x, f.y, float(kFlareRadius) * 1.2f, f.life > 20 ? 1.0f : float(f.life) / 20.0f);
  // Your own eyes adjust: a faint pool around the runner, for drawing only.
  const auto& p = mPlayer;
  pool(float(p.x) + 1.5f, float(p.y) - 2.0f, 7.0f, 0.55f);
  return best;
}

bool World::exitPowered() const
{
  if (mBoss.on && mBoss.exitT < 15)
    return false; // the exit drops from the crane cab once Black Halo is down
  if (mGolem.on && mGolem.phase != GolemPhase::Done)
    return false; // the idol's mouth opens once Kaan-Tolok falls
  if (mSpace.mother.on && mSpace.mother.phase != MotherPhase::Done)
    return false; // the hatch in her throne opens once the Hive Mother falls
  if (mZero.boss.on && !mZero.boss.exitOpen)
    return false; // the stage door shows once the wall has fallen
  if (mManor.on && (mManor.split.on || offSide(mManor.exitSide)))
    return false; // the front door is the way out in the reflection only; Both Sides exits by itself
  if (mWest.noon.on && mWest.noon.phase != DuelPhase::Done)
    return false; // High Noon: the way out opens after the twentieth duel
  return !darkAt(mLevel->exitTx * kCellsPerTile, (mLevel->exitTy + 1) * kCellsPerTile - 1) || mSectors.empty();
}

// --- Breakers, doors, Leeches -------------------------------------------------------

void World::throwBreaker(Breaker& b)
{
  const bool first = b.thrownAt < -1000;
  b.on = true;
  b.thrownAt = mStats.frames;
  playSound(Sfx::ForceFieldOff);
  const std::string name = b.sector >= 0 ? mSectors[std::size_t(b.sector)].id : b.id;
  showMessage("SECTOR " + name + " - POWER RESTORED");
  flashAt({(float(b.x) + 1.0f) * kCellSize, (float(b.y) - 2.0f) * kCellSize}, 140.0f, rgb(120, 230, 255), 24);
  for (const auto& c : mCables)
    if (c.breaker == int(&b - mBreakers.data()) && !b.leechKilled)
      b.leechIn = first ? 0 : 60;
  bool all = true;
  for (const auto& o : mBreakers)
    all = all && o.on;
  if (all && mAllLitAt < 0)
  {
    mAllLitAt = mStats.frames;
    showMessage("THE WHOLE DISTRICT IS BACK ON THE GRID");
  }
}

void World::cutBreaker(Breaker& b)
{
  b.on = false;
  const int bi = int(&b - mBreakers.data());
  for (auto& d : mDoors)
    if (d.breaker == bi && !d.solid)
    {
      d.open = 0;
      applyDoor(d, true);
    }
  for (auto& d : mDoors)
    if (d.breaker == bi)
      d.open = 0;
  playSound(Sfx::ForceFieldOff);
  showMessage("A GRID LEECH PULLED THE BREAKER!");
}

void World::updateDark(const PlayerInput& input)
{
  if (mSectors.empty() && !mSonar && mFlares.empty())
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;

  // Breakers: press up next to one; the lever takes 12 frames.
  for (auto& b : mBreakers)
  {
    if (b.throwing > 0)
    {
      if (--b.throwing == 0)
        throwBreaker(b);
      continue;
    }
    if (!b.on && alive && input.up && p.state == PlayerState::OnGround && b.box().intersects(p.box()))
    {
      b.throwing = 12;
      playSound(Sfx::Land); // the clunk
    }
  }

  // Shutters roll up while their breaker is on and slam shut when it goes.
  for (auto& d : mDoors)
  {
    const bool power = d.breaker >= 0 && mBreakers[std::size_t(d.breaker)].on;
    if (power && d.solid && ++d.open >= d.opentime)
      applyDoor(d, false);
    else if (!power && !d.solid)
    {
      d.open = 0;
      applyDoor(d, true);
    }
  }

  // A new Grid Leech sets off along the cable.
  for (std::size_t bi = 0; bi < mBreakers.size(); ++bi)
  {
    auto& b = mBreakers[bi];
    if (b.leechIn < 0 || !b.on)
      continue;
    if (b.leechIn-- > 0)
      continue;
    for (std::size_t ci = 0; ci < mCables.size(); ++ci)
      if (mCables[ci].breaker == int(bi))
      {
        const int def = enemyIndex("grid_leech");
        const auto [cx, cy] = mCables[ci].path.front();
        spawnEnemy(def, cx, cy + 1);
        Enemy& e = mEnemies.back();
        e.attach = int(ci);
        e.aimX = 0;
        e.aimY = 1;
        e.active = true;
        showMessage("SOMETHING IS CRAWLING ALONG THE CABLE...");
      }
  }

  updateFlares();

  // Each relit sector brings an instrument back into the score.
  if (!mSectors.empty() && !mMusicOverride.empty())
  {
    int lit = 0;
    for (const auto& b : mBreakers)
      lit += b.on;
    mMusicOverride = mLevel->music + "@" + std::to_string(std::min(lit, 4));
  }

  // Props: the graffiti glows once flared; the cat leaves when shot at.
  for (auto& pr : mProps)
  {
    if (pr.kind == PropKind::Graffiti && !pr.used)
    {
      const float cx = float(pr.x) + float(pr.w) * 0.5f, cy = float(pr.y) + float(pr.h) * 0.5f;
      for (const auto& f : mFlares)
        if (dist(cx, cy, f.x, f.y) <= float(kFlareRadius))
        {
          pr.used = true;
          addScore(4200, cellCenter(pr.box()));
          showMessage("GLOW-IN-THE-DARK CREDITS!");
          break;
        }
    }
    if (pr.kind == PropKind::Cat)
    {
      if (pr.timer >= 0 && ++pr.timer > 24)
        pr.timer = -2; // gone
      if (pr.timer == -1)
        for (const auto& s : mProjectiles)
          if (s.kind != ShotKind::Enemy && s.alive &&
              std::abs(s.x - (pr.x + 1)) <= 6 && std::abs(s.y - (pr.y + 1)) <= 6)
          {
            pr.timer = 0;
            playSound(Sfx::MenuMove);
            mTexts.push_back({cellCenter(pr.box()), "MEOW", rgb(255, 230, 120), 50});
            break;
          }
    }
  }

  // Sonar: rings spread 2 cells a frame and outline what they pass.
  if (mSonar)
  {
    const int W = mLevel->width, H = mLevel->height;
    if (mPinged.empty())
      mPinged.assign(std::size_t(W * H), -1000);
    for (auto& pg : mPings)
    {
      ++pg.age;
      const float r0 = float(pg.age - 1) * 2.0f, r1 = float(pg.age) * 2.0f;
      const int bx0 = std::max(0, int((pg.x - r1) / 2) - 1), bx1 = std::min(W - 1, int((pg.x + r1) / 2) + 1);
      const int by0 = std::max(0, int((pg.y - r1) / 2) - 1), by1 = std::min(H - 1, int((pg.y + r1) / 2) + 1);
      for (int by = by0; by <= by1; ++by)
        for (int bx = bx0; bx <= bx1; ++bx)
        {
          const float d = dist(float(bx * 2 + 1), float(by * 2 + 1), pg.x, pg.y);
          if (d >= r0 - 1.0f && d <= r1 + 1.0f)
            mPinged[std::size_t(by * W + bx)] = mStats.frames;
        }
    }
    mPings.erase(std::remove_if(mPings.begin(), mPings.end(), [](const Ping& pg) { return pg.age > 40; }), mPings.end());
    // Gems chime as you come within 4 blocks.
    if (mChimed.size() != mItems.size())
      mChimed.assign(mItems.size(), 0);
    for (std::size_t i = 0; i < mItems.size(); ++i)
    {
      const auto& it = mItems[i];
      const bool near = !it.taken && it.kind == ItemKind::Gem && std::abs(it.x - p.x) <= 8 && std::abs(it.y - p.y) <= 8;
      if (near && !mChimed[i])
        playSound(Sfx::Gem);
      mChimed[i] = near;
    }
  }
}

void World::sonarPing(int x, int y)
{
  if (mSonar)
    mPings.push_back({float(x) + 0.5f, float(y) + 0.5f, 0});
}

// --- Flares -----------------------------------------------------------------------------

void World::stickFlare(const Projectile& pr, int enemy)
{
  Flare f;
  // Back off a step so a flare on a wall sits on its face.
  f.x = float(pr.x - (enemy < 0 ? pr.dx : 0)) + 1.0f;
  f.y = float(pr.y - (enemy < 0 ? pr.dy : 0)) + 0.5f;
  f.enemy = enemy;
  if (enemy >= 0)
  {
    const Enemy& e = mEnemies[std::size_t(enemy)];
    f.ox = f.x - float(e.x);
    f.oy = f.y - float(e.y);
  }
  mFlares.push_back(f);
  if (int(mFlares.size()) > kMaxFlares)
    mFlares.erase(mFlares.begin()); // the oldest goes out
  flashAt({f.x * kCellSize, f.y * kCellSize}, 70.0f, rgb(255, 140, 60), 10);
}

void World::updateFlares()
{
  for (auto& f : mFlares)
  {
    --f.life;
    if (f.enemy < 0)
      continue;
    Enemy& e = mEnemies[std::size_t(f.enemy)];
    if (!e.alive)
    {
      f.enemy = -1; // drops where it was
      continue;
    }
    f.x = float(e.x) + f.ox;
    f.y = float(e.y) + f.oy;
    // Burns: a point of damage every 15 frames, three in all.
    if (f.burnLeft > 0 && (120 - f.life) % 15 == 14)
    {
      --f.burnLeft;
      damageEnemy(e, 1);
    }
  }
  mFlares.erase(std::remove_if(mFlares.begin(), mFlares.end(), [](const Flare& f) { return f.life <= 0; }), mFlares.end());
}

// --- The dark's residents ------------------------------------------------------------------

void World::updateStalker(Enemy& e, const EnemyDef& def)
{
  const CellBox b = e.box();
  const bool lit = litAt(b.x + 1, b.y + 1) || litAt(b.x + 1, b.bottom() - 1);
  e.attach = lit ? 1 : 0; // frozen, flinching
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (lit)
  {
    e.tell = 0;
    e.dive = 0;
    return;
  }
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  auto step = [&](int dir) {
    const CellBox nb = e.box();
    const int aheadX = dir > 0 ? nb.right() + 1 : nb.left() - 1;
    if ((dir > 0 ? mMap.touchingRightWall(nb) : mMap.touchingLeftWall(nb)) || !mMap.solidTop(aheadX, nb.bottom() + 1))
      return false;
    e.x += dir;
    return true;
  };
  if (e.dive > 0)
  {
    // The lunge: two blocks in two frames.
    --e.dive;
    step(e.dir);
    step(e.dir);
    if (e.dive == 0)
      e.lastDive = e.timer + def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      e.dive = 2;
    return;
  }
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int dx = (pb.x + 1) - (b.x + 1);
  const bool level = pb.bottom() >= b.top() && pb.top() <= b.bottom();
  if (!vulnerable || std::abs(dx) > 40 || std::abs(pb.bottom() - b.bottom()) > 12)
    return;
  e.dir = dx < 0 ? -1 : 1;
  if (level && std::abs(dx) <= def.range + 3 && e.timer >= e.lastDive)
  {
    e.tell = def.tell; // the eyes widen
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) == 0 && std::abs(dx) > 1)
    step(e.dir);
}

void World::updateLooter(Enemy& e, const EnemyDef& def)
{
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
  }
  // What it carries rides on its back.
  bool carrying = false;
  for (auto& it : mItems)
    if (it.heldBy == e.id && !it.taken)
    {
      it.x = it.prevX = e.x;
      it.y = it.prevY = e.y - e.h + 1;
      carrying = true;
    }
  auto step = [&](int dir) {
    const CellBox nb = e.box();
    if (dir > 0 ? mMap.touchingRightWall(nb) : mMap.touchingLeftWall(nb))
      return false;
    e.x += dir;
    return true;
  };
  if (!carrying)
  {
    // The nearest loose gem or popped pick-up in reach.
    Item* best = nullptr;
    int bestD = def.range + 1;
    for (auto& it : mItems)
    {
      if (it.taken || it.heldBy >= 0 || it.kind == ItemKind::Virus || it.kind == ItemKind::Proto)
        continue;
      const int d = std::abs(it.x - e.x);
      if (d < bestD && std::abs(it.y - e.y) <= 6)
      {
        bestD = d;
        best = &it;
      }
    }
    if (!best)
      return;
    if (best->box().intersects(e.box()))
    {
      best->heldBy = e.id;
      e.dive = 1;
      return;
    }
    e.dir = best->x < e.x ? -1 : 1;
    if (!step(e.dir))
      e.dir = -e.dir;
    return;
  }
  // Off to the manhole with it. A Looter that cannot get there slips away
  // through a gap all the same.
  e.dir = mManholeX < e.x ? -1 : 1;
  const bool there = std::abs(e.x - mManholeX) <= 1;
  if (!there && step(e.dir))
    return;
  if (!there && ++e.tell < 20)
    return;
  int k = 0;
  for (auto& it : mItems)
    if (it.heldBy == e.id)
    {
      it.heldBy = -1;
      it.floating = true;
      it.vx = 0;
      if (mStash.w > 0)
      {
        it.x = it.prevX = mStash.x + (k * 3) % std::max(2, mStash.w - 2);
        it.y = it.prevY = mStash.bottom() - (k * 3) / std::max(2, mStash.w - 2) * 2;
      }
      ++k;
    }
  burst(cellCenter(e.box()), rgb(90, 90, 110), rgb(40, 40, 50), 10, 1.0f, false);
  e.alive = false; // gone down the manhole, not a kill
}

void World::dropLoot(const Enemy& e)
{
  for (auto& it : mItems)
    if (it.heldBy == e.id)
    {
      it.heldBy = -1;
      it.floating = false;
      it.frames = 3;
      it.x = it.prevX = e.x;
      it.y = it.prevY = e.y;
    }
}

void World::updateLeech(Enemy& e, const EnemyDef& def)
{
  if (e.attach < 0 || e.attach >= int(mCables.size()))
    return;
  const Cable& c = mCables[std::size_t(e.attach)];
  const int len = cableLength(c);
  if (e.aimY == 0)
  {
    // Restored from a save: find how far along the cable it was.
    int best = 0, bestD = 1 << 30;
    for (int at = 0; at <= len; ++at)
    {
      const auto [px, py] = cablePoint(c, at);
      const int d = std::abs(px - e.x) + std::abs(py + 1 - e.y);
      if (d < bestD)
      {
        bestD = d;
        best = at;
      }
    }
    e.aimX = best * 2;
    e.aimY = 1;
  }
  // Half a cell a frame.
  if (e.timer % std::max(1, def.stepEvery) == 0)
    ++e.aimX;
  const int at = e.aimX / 2;
  const auto [px, py] = cablePoint(c, at);
  e.dir = px >= e.x ? 1 : -1;
  e.x = px;
  e.y = py + 1;
  // Green sparks just before it touches you.
  const CellBox reach{e.x - 2, e.y - 3, e.w + 4, e.h + 4};
  e.tell = reach.intersects(mPlayer.hitBox()) ? def.tell : std::max(0, e.tell - 1);
  if (at >= len)
  {
    if (c.breaker >= 0 && mBreakers[std::size_t(c.breaker)].on)
    {
      cutBreaker(mBreakers[std::size_t(c.breaker)]);
      mBreakers[std::size_t(c.breaker)].leechIn = -1;
    }
    burst(cellCenter(e.box()), rgb(120, 255, 90), rgb(255, 255, 255), 14, 1.6f);
    e.alive = false;
  }
}

// --- Drawing --------------------------------------------------------------------------------

void World::drawDark(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (mSectors.empty() && !mSonar)
    return;
  // Cables and roller shutters, under the darkness like the rest of the street.
  for (const auto& c : mCables)
    for (std::size_t i = 1; i < c.path.size(); ++i)
    {
      const float x0 = (float(c.path[i - 1].first) + 0.5f) * kCellPx - camX, y0 = (float(c.path[i - 1].second) + 1.0f) * kCellPx - camY;
      const float x1 = (float(c.path[i].first) + 0.5f) * kCellPx - camX, y1 = (float(c.path[i].second) + 1.0f) * kCellPx - camY;
      r.drawLine(x0, y0, x1, y1, 8.0f, rgb(22, 22, 28));
      r.drawLine(x0, y0 - 2.0f, x1, y1 - 2.0f, 2.0f, rgb(70, 70, 84));
    }
  for (const auto& d : mDoors)
  {
    const float x = float(d.x0) * kTilePx - camX, y = float(d.y0) * kTilePx - camY;
    const float w = float(d.w) * kTilePx, h = float(d.h) * kTilePx;
    if (x > float(kScreenW) || x + w < 0.0f)
      continue;
    const float shut = d.solid ? 1.0f - float(d.open) / float(std::max(1, d.opentime)) : 0.0f;
    if (d.w > d.h)
    {
      // A floor hatch: a plate with hazard stripes, sliding aside.
      if (shut <= 0.0f)
        continue;
      r.fillRect(x, y, w * shut, h, rgb(70, 74, 84));
      for (float sx = x; sx < x + w * shut - 12.0f; sx += 28.0f)
        r.drawLine(sx, y + h - 4.0f, sx + 14.0f, y + 4.0f, 7.0f, rgb(220, 180, 40));
      continue;
    }
    // A roller shutter: slats from the top down to how far it is still shut,
    // the dark doorway showing beneath while it rolls up; the box on top.
    if (d.solid)
      r.fillRect(x, y, w, h, rgb(8, 8, 14));
    const float sh = h * shut;
    r.fillRect(x + 4.0f, y, w - 8.0f, sh, rgb(96, 102, 116));
    for (float sy = y + 10.0f; sy < y + sh - 4.0f; sy += 12.0f)
      r.fillRect(x + 4.0f, sy, w - 8.0f, 3.0f, rgb(62, 66, 78));
    if (sh > 8.0f)
      r.fillRect(x + 2.0f, y + sh - 8.0f, w - 4.0f, 8.0f, rgb(150, 150, 160));
    r.fillRect(x - 4.0f, y - 14.0f, w + 8.0f, 16.0f, rgb(50, 54, 64));
    r.fillRect(x, y, 4.0f, h, rgb(40, 42, 50));
    r.fillRect(x + w - 4.0f, y, 4.0f, h, rgb(40, 42, 50));
  }

  // The darkness: a grid of cells shaded by how much light reaches them.
  const int cx0 = int(std::floor(camX / kCellPx)), cy0 = int(std::floor(camY / kCellPx));
  const int cols = int(kScreenW / kCellPx) + 2, rows = int(kScreenH / kCellPx) + 2;
  bool anyDark = false;
  std::vector<float> shade(std::size_t(cols * rows), 0.0f);
  for (int j = 0; j < rows; ++j)
    for (int i = 0; i < cols; ++i)
    {
      const float l = lightLevel(cx0 + i, cy0 + j);
      const float a = mSonar ? 1.0f : (1.0f - l) * 0.94f;
      shade[std::size_t(j * cols + i)] = a;
      anyDark = anyDark || a > 0.01f;
    }
  if (!anyDark)
    return;
  // Stretched with smooth filtering, so the light pools have soft edges.
  r.drawAlphaGrid(shade, cols, rows, float(cx0) * kCellPx - camX, float(cy0) * kCellPx - camY, kCellPx, kCellPx,
    rgb(2, 3, 10));

  // Cyan outlines on the edges of solid blocks in the dark (sonar: only the
  // ones an echo just passed, fading over 15 frames; spikes ping red).
  const int tx0 = std::max(0, cx0 / kCellsPerTile), tx1 = std::min(mLevel->width - 1, (cx0 + cols) / kCellsPerTile);
  const int ty0 = std::max(0, cy0 / kCellsPerTile), ty1 = std::min(mLevel->height - 1, (cy0 + rows) / kCellsPerTile);
  auto solidBlock = [&](int tx, int ty) {
    const Tile t = mMap.block(tx, ty);
    return t == Tile::Solid || t == Tile::ForceField;
  };
  for (int ty = ty0; ty <= ty1; ++ty)
    for (int tx = tx0; tx <= tx1; ++tx)
    {
      const Tile t = mMap.block(tx, ty);
      const bool spikes = t == Tile::Spikes;
      if (!solidBlock(tx, ty) && t != Tile::Platform && !(mSonar && spikes))
        continue;
      float a = 0.0f;
      if (mSonar)
      {
        if (mPinged.empty())
          continue;
        const int age = mStats.frames - mPinged[std::size_t(ty * mLevel->width + tx)];
        if (age >= 15)
          continue;
        a = 1.0f - float(age) / 15.0f;
      }
      else
      {
        const int i = tx * kCellsPerTile - cx0, j = ty * kCellsPerTile - cy0;
        if (i < 0 || j < 0 || i >= cols || j >= rows)
          continue;
        a = shade[std::size_t(j * cols + i)];
        if (a < 0.3f)
          continue;
      }
      const Color c = spikes ? rgb(255, 60, 60) : rgb(70, 230, 255);
      const Color col = withAlpha(c, int(220 * a));
      const float x = float(tx) * kTilePx - camX, y = float(ty) * kTilePx - camY;
      if (t == Tile::Platform || spikes)
      {
        r.fillRect(x, y + (spikes ? 30.0f : 0.0f), kTilePx, 3.0f, col, Blend::Add);
        continue;
      }
      if (!solidBlock(tx, ty - 1))
        r.fillRect(x, y, kTilePx, 3.0f, col, Blend::Add);
      if (!solidBlock(tx, ty + 1))
        r.fillRect(x, y + kTilePx - 3.0f, kTilePx, 3.0f, col, Blend::Add);
      if (!solidBlock(tx - 1, ty))
        r.fillRect(x, y, 3.0f, kTilePx, col, Blend::Add);
      if (!solidBlock(tx + 1, ty))
        r.fillRect(x + kTilePx - 3.0f, y, 3.0f, kTilePx, col, Blend::Add);
    }

  if (mSonar)
  {
    // The echoes themselves, and the gems you can hear.
    for (const auto& pg : mPings)
    {
      const float rad = float(pg.age) * 2.0f * kCellPx;
      const float px = pg.x * kCellPx - camX, py = pg.y * kCellPx - camY;
      for (int k = 0; k < 72; ++k)
      {
        const float a0 = float(k) * 0.08727f, a1 = a0 + 0.08727f;
        r.drawLine(px + std::cos(a0) * rad, py + std::sin(a0) * rad, px + std::cos(a1) * rad, py + std::sin(a1) * rad, 2.0f,
          rgba(120, 230, 255, std::max(0, 160 - pg.age * 4)), Blend::Add);
      }
    }
    for (std::size_t i = 0; i < mItems.size() && i < mChimed.size(); ++i)
      if (mChimed[i])
      {
        const auto& it = mItems[i];
        drawGlow(r, mArt, (float(it.x) + 1.0f) * kCellPx - camX, (float(it.y) - 0.5f) * kCellPx - camY, 26,
          mArt.gemColor[std::size_t(it.variant % 4)], 0.5f + 0.3f * float(frame % 20 < 10));
      }
    return;
  }

  // Things that still show in the dark: enemy eyes, the Leech's crackle,
  // faint glints of pick-ups, the lights themselves and the cat.
  auto dark = [&](float wx, float wy) {
    const int i = int(std::floor(wx)) - cx0, j = int(std::floor(wy)) - cy0;
    return i >= 0 && j >= 0 && i < cols && j < rows && shade[std::size_t(j * cols + i)] > 0.4f;
  };
  for (const auto& e : mEnemies)
  {
    if (!e.alive || e.kind == EnemyKind::Camera)
      continue;
    const float ex = e.drawX + float(e.w) * 0.5f, ey = e.drawY - float(e.h) + 1.0f;
    if (!dark(ex, ey + 1.0f))
      continue;
    const float sx = ex * kCellPx - camX, sy = (ey + 0.8f) * kCellPx - camY;
    if (e.kind == EnemyKind::Leech)
    {
      drawGlow(r, mArt, sx, sy + 20.0f, 40, rgb(120, 255, 90), 0.5f + 0.4f * float((frame / 2) % 2));
      continue;
    }
    const float wide = (e.kind == EnemyKind::Stalker && e.tell > 0) ? 1.8f : 1.0f;
    const Color eye = e.kind == EnemyKind::Stalker ? rgb(255, 230, 90) : mTheme.enemyEye;
    for (int s : {-1, 1})
    {
      r.fillRect(sx + float(s) * 9.0f - 4.0f * wide, sy - 2.0f * wide, 8.0f * wide, 4.0f * wide, eye, Blend::Add);
      drawGlow(r, mArt, sx + float(s) * 9.0f, sy, 12.0f * wide, eye, 0.6f);
    }
  }
  for (const auto& it : mItems)
    if (!it.taken && it.heldBy < 0 && dark(float(it.x) + 1.0f, float(it.y) - 0.5f) && (frame / 6 + it.x) % 5 == 0)
      drawGlow(r, mArt, (float(it.x) + 1.0f) * kCellPx - camX, (float(it.y) - 0.5f) * kCellPx - camY, 14,
        rgb(255, 255, 255), 0.2f);
  for (const auto& b : mBoxes)
    if (b.alive && dark(float(b.x) + 1.0f, float(b.y) - 0.5f) && (frame / 6 + b.x) % 7 == 0)
      drawGlow(r, mArt, (float(b.x) + 1.0f) * kCellPx - camX, (float(b.y) - 0.5f) * kCellPx - camY, 16,
        rgb(200, 230, 255), 0.2f);
  for (const auto& l : mLights)
  {
    const float lx = (float(l.x) + 0.5f) * kCellPx - camX, ly = (float(l.y) + 0.5f) * kCellPx - camY;
    if (lx < -300.0f || lx > float(kScreenW) + 300.0f)
      continue;
    drawGlow(r, mArt, lx, ly, float(l.r) * kCellPx * 0.9f, rgb(255, 210, 140), 0.18f);
    drawGlow(r, mArt, lx, ly, 18, rgb(255, 240, 200), 0.9f);
  }
  for (const auto& b : mBreakers)
  {
    // Breaker boxes: a red lamp while off, green once thrown.
    const float bx = float(b.x) * kCellPx - camX, by = float(b.y - 3) * kCellPx - camY;
    if (bx < -100.0f || bx > float(kScreenW) + 100.0f)
      continue;
    r.fillRect(bx + 4.0f, by, 56.0f, 96.0f, rgb(46, 50, 60));
    r.fillRect(bx + 10.0f, by + 8.0f, 44.0f, 70.0f, rgb(28, 30, 36));
    const float lever = b.on ? 0.0f : (b.throwing > 0 ? float(b.throwing) / 12.0f : 1.0f);
    r.drawLine(bx + 32.0f, by + 44.0f, bx + 32.0f + 14.0f, by + 44.0f - 26.0f * (lever * 2.0f - 1.0f), 5.0f, rgb(220, 220, 230));
    const Color lamp = b.on ? rgb(80, 255, 120) : rgb(255, 50, 50);
    drawGlow(r, mArt, bx + 32.0f, by + 88.0f, 18, lamp, 0.9f);
    if (!b.on && b.throwing == 0)
      r.drawText("UP", bx + 32.0f, by - 30.0f, {18.0f, rgb(255, 230, 120), rgb(0, 0, 0)}, Align::Center,
        0.5f + 0.5f * float((frame / 10) % 2));
  }
  for (const auto& f : mFlares)
  {
    const float fx = f.x * kCellPx - camX, fy = f.y * kCellPx - camY;
    const float flick = 0.8f + 0.2f * std::sin(float(frame) * 0.9f + f.x);
    drawGlow(r, mArt, fx, fy, 70.0f * flick, rgb(255, 120, 50), 0.8f);
    drawGlow(r, mArt, fx, fy, 16.0f, rgb(255, 255, 220), 1.0f);
  }
  for (const auto& pr : mProps)
  {
    const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
    if (x < -400.0f || x > float(kScreenW) + 100.0f)
      continue;
    if (pr.kind == PropKind::Cat && pr.timer != -2)
    {
      const bool blink = (frame / 8) % 37 == 0 || pr.timer >= 0;
      if (!blink)
        for (int s : {-1, 1})
        {
          r.fillRect(x + 32.0f + float(s) * 10.0f - 4.0f, y + 20.0f, 8.0f, 5.0f, rgb(190, 255, 90), Blend::Add);
          drawGlow(r, mArt, x + 32.0f + float(s) * 10.0f, y + 22.0f, 10, rgb(190, 255, 90), 0.5f);
        }
    }
    if (pr.kind == PropKind::Graffiti && pr.used)
    {
      // The credits, in glow paint.
      static const char* const kLines[] = {"GUNRUNNERS", "DASH  ROCCO  NOVA", "AND MAX", "MADE BY CODE"};
      for (int k = 0; k < 4; ++k)
        r.drawText(kLines[k], x + float(pr.w) * kCellPx * 0.5f, y + float(k) * 26.0f,
          {k == 0 ? 26.0f : 18.0f, rgb(150, 255, 120), rgb(10, 40, 10), true}, Align::Center, 0.9f);
      drawGlow(r, mArt, x + float(pr.w) * kCellPx * 0.5f, y + 50.0f, 140, rgb(120, 255, 90), 0.2f);
    }
  }
}

} // namespace gr
