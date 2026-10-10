// Level 18, Hull Walk (SPEC 18): Low Gravity. A walk along the outside of
// the station's ring under 0.6 gravity (higher, floatier jumps, a slower
// fall, less steering in the air; player.cpp). The Recoil Cannon kicks the
// runner back with every shot, and a shot straight down in the air is a
// second jump. Rivet Mites unbolt the hull plates they crawl on until the
// plates drift off; Space Barnacles burst into rings of spikes; EVA Rams
// line up with the runner and ram. The candid camera drifts round a loop.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kStill = 450;      // frames standing still before the runner plants a flag
constexpr int kAlien = 30;       // the UFO alien's thumbs-up
constexpr int kPlateLife = 120;  // a loose plate drifts off for this long
constexpr int kMites = 5;        // to a swarm
constexpr int kRide = 30;        // frames a mite clings to the runner
constexpr int kRamCoast = 30;
constexpr int kSpikeRange = 16;  // cells (8 blocks)
constexpr int kLookUfo = 13;     // Breakable::look of the UFO's hatch

int sgn(int v) { return (v > 0) - (v < 0); }

bool hullDeco(const std::string& k)
{
  return k == "mast" || k == "dish" || k == "truss" || k == "wing" || k == "airlock" || k == "ufo" ||
    k == "girder";
}

CellBox miteBox(const RivetMite& m) { return {int(std::floor(m.x)), int(std::floor(m.y)), 2, 1}; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupHullEntity(const EntityDef& e)
{
  auto& h = mHull;
  if (e.kind == "hullplate")
  {
    HullPlate pl;
    pl.id = e.id;
    pl.x0 = e.num("x0");
    pl.x1 = e.num("x1", pl.x0);
    pl.y = e.num("y");
    // The rivets sit evenly along the plate, inset a cell from its ends.
    const int n = std::max(1, e.num("rivets", 4));
    const int c0 = pl.x0 * kCellsPerTile + 1, c1 = (pl.x1 + 1) * kCellsPerTile - 2;
    for (int i = 0; i < n; ++i)
      pl.rivets.push_back(n == 1 ? (c0 + c1) / 2 : c0 + (c1 - c0) * i / (n - 1));
    pl.unbolt.assign(std::size_t(n), 0);
    h.plates.push_back(pl);
    h.on = true;
    return true;
  }
  if (e.kind == "drifter")
  {
    HullDrifter d;
    d.id = e.id;
    for (const auto& [px, py] : e.path("path"))
      d.path.emplace_back(px * kCellsPerTile, py * kCellsPerTile + 1); // the block's bottom cell
    if (d.path.empty())
      return true;
    d.fx = float(d.path[0].first);
    d.fy = float(d.path[0].second);
    d.speed = e.real("speed", 0.25f);
    h.drifters.push_back(d);
    h.on = true;
    return true;
  }
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  // Level 7's girders are decos too: only the hull's levels draw these.
  if (e.kind == "deco" && mLevel->themeKey == "station_hull" && hullDeco(e.str("kind")) &&
      e.rect("rect", x0, y0, x1, y1))
  {
    h.decos.push_back({e.str("kind"), x0, y0, x1, y1});
    h.on = true;
    return true;
  }
  return false;
}

void World::setupHullEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind != EnemyKind::Barnacle && en.kind != EnemyKind::EvaRam && en.kind != EnemyKind::Mites)
    return;
  mHull.on = true;
  const std::string face = e.str("face");
  if (en.kind == EnemyKind::Barnacle && !face.empty())
  {
    en.variant = face == "ceiling" ? 1 : (face == "left" ? 2 : (face == "right" ? 3 : 0));
    en.aimY = 1;
  }
}

void World::linkHull()
{
  auto& h = mHull;
  h.lowgrav = mLevel->flags.count("lowgrav") != 0;
  h.on = h.on || h.lowgrav || mLevel->weapon == "recoil_cannon";
  if (!h.on)
    return;
  for (std::size_t i = 0; i < mEnemies.size(); ++i)
  {
    auto& e = mEnemies[i];
    if (e.kind == EnemyKind::Barnacle)
    {
      e.dir = 1; // its variant says which way it faces: never mirrored
      e.cool = 10 + (e.x % 3) * 5;
      if (e.aimY == 1)
        continue;
      const CellBox b = e.box();
      e.variant = mMap.solid(b.x + 1, b.bottom() + 1) ? 0
        : mMap.solid(b.x + 1, b.top() - 1)            ? 1
        : mMap.solid(b.left() - 1, b.y + 1)           ? 2
        : mMap.solid(b.right() + 1, b.y + 1)          ? 3
                                                      : 0;
    }
    else if (e.kind == EnemyKind::Mites)
    {
      // The swarm is five mites (below); the enemy only keeps count.
      e.hidden = true;
      e.hp = kMites;
      int plate = -1;
      for (std::size_t k = 0; k < h.plates.size(); ++k)
      {
        const auto& pl = h.plates[k];
        if (e.y + 1 == pl.y * kCellsPerTile && e.x >= pl.x0 * kCellsPerTile &&
            e.x <= (pl.x1 + 1) * kCellsPerTile - 2)
          plate = int(k);
      }
      for (int m = 0; m < kMites; ++m)
      {
        RivetMite mite;
        mite.swarm = int(i);
        mite.plate = plate;
        float x = float(e.x + (m - kMites / 2) * 3);
        if (plate >= 0)
          x = std::clamp(x, float(h.plates[std::size_t(plate)].x0 * kCellsPerTile),
            float((h.plates[std::size_t(plate)].x1 + 1) * kCellsPerTile - 2));
        mite.x = x;
        mite.y = float(e.y);
        mite.dir = m % 2 ? 1 : -1;
        mite.cool = 10 + m * 4;
        h.mites.push_back(mite);
      }
    }
  }
  // Each drifter carries the candid camera nearest its loop's start.
  for (auto& d : h.drifters)
  {
    int best = -1, bestDist = 1 << 30;
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
      if (mEnemies[i].kind == EnemyKind::Camera)
      {
        const int dist = std::abs(mEnemies[i].x - int(d.fx)) + std::abs(mEnemies[i].y - int(d.fy));
        if (dist < bestDist)
        {
          best = int(i);
          bestDist = dist;
        }
      }
    d.enemy = best;
    if (best >= 0)
    {
      mEnemies[std::size_t(best)].x = int(std::lround(d.fx));
      mEnemies[std::size_t(best)].y = int(std::lround(d.fy));
    }
  }
}

void World::resetHull()
{
  auto& h = mHull;
  h.shove = h.shoveFrames = 0;
  h.boostUsed = false;
  h.kick = 0;
  h.still = 0;
  h.fallTick = 0;
  for (auto& m : h.mites)
    if (m.state == MiteState::Ride || m.state == MiteState::Hop || m.state == MiteState::Tell)
    {
      m.state = MiteState::Fall;
      m.vx = m.vy = 0.0f;
      m.cool = 30;
    }
}

bool World::hullCanSave() const
{
  const auto& h = mHull;
  if (h.shove > 0)
    return false;
  for (const auto& pl : h.plates)
    if (pl.drifting && pl.life < kPlateLife)
      return false;
  for (const auto& m : h.mites)
    if (m.state != MiteState::Crawl && m.state != MiteState::Unbolt && m.state != MiteState::Dead)
      return false;
  return true;
}

// --- The runner pushed about -----------------------------------------------------------------

void World::shoveRunner(int dir, int cells, int frames)
{
  auto& h = mHull;
  h.shove = cells;
  h.shoveDir = dir;
  h.shoveFrames = std::max(1, frames);
}

void World::recoilKick(int dx, int dy)
{
  auto& h = mHull;
  auto& p = mPlayer;
  h.kick = 6;
  h.kickDir = dy > 0 ? 2 : (dy < 0 ? 0 : dx);
  playSound(Sfx::Recoil);
  const bool air = p.state == PlayerState::Jumping || p.state == PlayerState::Falling;
  if (dy > 0)
  {
    // Straight down in the air: a fresh jump, once per jump.
    if (air && !h.boostUsed && !mMap.touchingCeiling(p.box()))
    {
      h.boostUsed = true;
      jump();
      setVisual(PlayerVisual::Jumping);
    }
    return;
  }
  if (dx == 0)
    return;
  // Two blocks back over 6 frames (Rocco, on the ground, one).
  shoveRunner(-dx, !air && mCharacterIndex == 1 ? 2 : 4, 6);
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtHull(Projectile& pr, const CellBox& b)
{
  auto& h = mHull;
  for (auto& m : h.mites)
  {
    if (m.state == MiteState::Dead || !miteBox(m).intersects(b))
      continue;
    m.state = MiteState::Dead;
    const Vec2 c = cellCenter(miteBox(m));
    burst(c, rgb(200, 210, 220), rgb(140, 255, 90), 10, 1.4f);
    auto& swarm = mEnemies[std::size_t(m.swarm)];
    if (--swarm.hp <= 0 && swarm.alive)
    {
      // The last of the swarm: it counts as the kill (and its 100 points).
      swarm.x = int(m.x);
      swarm.y = int(m.y);
      killEnemy(swarm);
    }
    else
    {
      addScore(enemyDef(swarm.def).score, c);
      playSound(Sfx::SmallExplosion);
    }
    if (!pr.pierce)
      pr.alive = false;
    return true;
  }
  return false;
}

// --- Plates and rivets ---------------------------------------------------------------------

void World::looseRivet(int plate, int rivet)
{
  auto& pl = mHull.plates[std::size_t(plate)];
  const int rx = pl.rivets[std::size_t(rivet)];
  pl.rivets.erase(pl.rivets.begin() + rivet);
  pl.unbolt.erase(pl.unbolt.begin() + rivet);
  const Vec2 at{(float(rx) + 0.5f) * kCellSize, float(pl.y * kCellsPerTile) * kCellSize};
  burst(at, rgb(255, 230, 160), rgb(200, 200, 210), 8, 1.6f);
  if (isOnScreen({rx, pl.y * kCellsPerTile, 1, 1}, 4))
    playSound(Sfx::Rivet);
  for (auto& m : mHull.mites)
    if (m.plate == plate && m.rivet >= rivet)
    {
      if (m.rivet == rivet && m.state == MiteState::Unbolt)
        m.state = MiteState::Crawl;
      m.rivet = m.rivet == rivet ? -1 : m.rivet - 1;
    }
  if (!pl.rivets.empty())
    return;
  // The last rivet: the plate comes loose and drifts off, up a quarter of
  // a cell and east an eighth a frame, taking whatever stands on it.
  pl.drifting = true;
  pl.life = 0;
  for (int tx = pl.x0; tx <= pl.x1; ++tx)
    mMap.setBlock(tx, pl.y, Tile::Empty);
  Platform plat;
  plat.id = "plate:" + pl.id;
  plat.mode = PlatformMode::Path;
  plat.x = plat.prevX = pl.x0 * kCellsPerTile;
  plat.y = plat.prevY = plat.startY = plat.homeY = pl.y * kCellsPerTile;
  plat.w = (pl.x1 - pl.x0 + 1) * kCellsPerTile;
  plat.h = kCellsPerTile;
  // Steps of a cell east-and-up then a cell up, one every 4 frames.
  for (int s = 0; s <= kPlateLife / 8; ++s)
    plat.path.emplace_back(plat.x + s, plat.y - 2 * s);
  plat.speedNum = 1;
  plat.speedDen = 4;
  plat.once = true;
  plat.running = true;
  plat.target = 1;
  pl.platform = int(mPlatforms.size());
  mPlatforms.push_back(plat);
  syncPlatformCollision();
  playSound(Sfx::Clunk);
}

void World::updateHullPlates()
{
  auto& h = mHull;
  for (std::size_t i = 0; i < h.plates.size(); ++i)
  {
    auto& pl = h.plates[i];
    if (!pl.drifting || pl.life >= kPlateLife)
      continue;
    if (++pl.life >= kPlateLife && pl.platform >= 0)
    {
      // Gone into space.
      auto& plat = mPlatforms[std::size_t(pl.platform)];
      plat.hidden = true;
      plat.running = false;
      plat.y = -100;
      syncPlatformCollision();
    }
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateBarnacle(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box();
  const CellBox wake{b.x - def.range, b.y - def.range, b.w + 2 * def.range, b.h + 2 * def.range};
  const bool near = p.state != PlayerState::Dying && wake.intersects(p.hitBox());
  switch (e.attach)
  {
    case 0: // shut
      if (near && e.cool == 0 && isOnScreen(b, 0))
      {
        e.attach = 1;
        e.tell = def.tell;
      }
      break;
    case 1: // the crack glows
      if (--e.tell <= 0)
      {
        // Six spikes in a ring, 60 degrees apart, one along each side of
        // the walkway.
        const float cx = float(b.x) + float(b.w) * 0.5f - 0.5f, cy = float(b.y) + float(b.h) * 0.5f - 0.5f;
        for (int k = 0; k < 6; ++k)
        {
          const float a = float(k) * 1.0471976f;
          Projectile pr;
          pr.kind = ShotKind::Enemy;
          pr.w = pr.h = 1;
          pr.speed = 1;
          pr.damage = 1;
          pr.range = kSpikeRange;
          pr.carrier = e.carrier;
          pr.spike = true;
          pr.precise = true;
          pr.vx = std::cos(a);
          pr.vy = std::sin(a);
          pr.fx = cx + pr.vx * 2.0f;
          pr.fy = cy + pr.vy * 1.5f;
          pr.dx = sgn(int(std::lround(pr.vx * 2.0f)));
          pr.dy = sgn(int(std::lround(pr.vy * 2.0f)));
          pr.x = pr.prevX = int(std::floor(pr.fx));
          pr.y = pr.prevY = int(std::floor(pr.fy));
          if (!mMap.solid(pr.x, pr.y))
            mProjectiles.push_back(pr);
        }
        playSound(Sfx::Spikes);
        e.attach = 2;
        e.dive = 8;
        e.cool = def.cooldown;
      }
      break;
    default: // open, spent
      if (--e.dive <= 0)
        e.attach = 0;
      break;
  }
}

void World::updateEvaRam(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox pb = p.hitBox();
  const int rowY = p.y - 1; // its bottom lines up with the runner's middle
  switch (e.attach)
  {
    case 0: // hovering over to the runner's row at half a cell a frame
    {
      if (!isOnScreen(e.box(), 0) || p.state == PlayerState::Dying)
        break;
      e.dir = pb.x + 1 < e.x + 1 ? -1 : 1;
      if (++e.timer % std::max(1, def.stepEvery) == 0 && e.y != rowY)
        mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(rowY - e.y));
      const int gap = std::abs((pb.x + 1) - (e.x + 1));
      // It never rams a runner in the air: it waits for them to land.
      if (std::abs(e.y - rowY) <= 1 && gap <= def.range && e.cool == 0 && p.state == PlayerState::OnGround)
      {
        e.attach = 1;
        e.tell = def.tell;
      }
      break;
    }
    case 1: // lined up, the thruster flashing
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.dive = 0;
        if (isOnScreen(e.box(), 2))
          playSound(Sfx::Thrust);
      }
      break;
    case 2: // the ram: 3 cells a frame, up to `range` cells
    {
      bool stop = false;
      for (int s = 0; s < 3 && !stop; ++s)
      {
        if (mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir) != MoveResult::Completed || ++e.dive >= def.range)
          stop = true;
        const bool hurtable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && p.tube < 0;
        if (hurtable && e.box().intersects(p.hitBox()))
        {
          const int hp = p.hp;
          hurtPlayer(1);
          // Three blocks along its way (Rocco stands his ground).
          if (p.hp < hp && mCharacterIndex != 1)
            shoveRunner(e.dir, 6, 4);
          stop = true;
        }
      }
      if (stop)
      {
        e.attach = 3;
        e.ox = kRamCoast;
      }
      break;
    }
    default: // coasting, the thruster sputtering
      if (e.ox > kRamCoast - 8 && e.ox % 2 == 0)
        mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir);
      if (--e.ox <= 0)
      {
        e.attach = 0;
        e.cool = def.cooldown;
      }
      break;
  }
}

void World::updateMites(Enemy& e, const EnemyDef& def)
{
  auto& h = mHull;
  auto& p = mPlayer;
  const int swarm = int(&e - mEnemies.data());
  const CellBox pb = p.hitBox();
  const bool hurtable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting && p.tube < 0;
  const CellBox reach{pb.x - def.range, pb.y - def.range, pb.w + 2 * def.range, pb.h + 2 * def.range};
  int ridden = 0;
  for (auto& m : h.mites)
  {
    if (m.swarm != swarm || m.state == MiteState::Dead)
      continue;
    ++m.timer;
    if (m.cool > 0)
      --m.cool;
    HullPlate* pl = m.plate >= 0 ? &h.plates[std::size_t(m.plate)] : nullptr;
    // On a plate that came loose: carried off with it, lost when it goes.
    if (pl && pl->drifting && (m.state == MiteState::Crawl || m.state == MiteState::Unbolt))
    {
      if (pl->life >= kPlateLife || pl->platform < 0)
      {
        m.state = MiteState::Dead;
        if (--e.hp <= 0 && e.alive)
          e.alive = false; // drifted off into space: no score
        continue;
      }
      const auto& plat = mPlatforms[std::size_t(pl->platform)];
      m.x += float(plat.x - plat.prevX);
      m.y += float(plat.y - plat.prevY);
      continue;
    }
    const bool near = hurtable && m.cool == 0 && reach.intersects(miteBox(m));
    switch (m.state)
    {
      case MiteState::Crawl:
      {
        if (near)
        {
          m.state = MiteState::Tell;
          m.timer = 0;
          break;
        }
        // To the nearest rivet still in, half a cell a frame.
        int target = -1;
        float best = 1e9f;
        if (pl)
          for (std::size_t r = 0; r < pl->rivets.size(); ++r)
          {
            const float d = std::abs(float(pl->rivets[r]) - 0.5f - m.x);
            if (d < best)
            {
              best = d;
              target = int(r);
            }
          }
        if (target < 0)
        {
          // Nothing to unbolt: pace about.
          const float nx = m.x + 0.5f * float(m.dir);
          if (mMap.solid(int(std::floor(nx)) + (m.dir > 0 ? 2 : 0), int(m.y)) ||
              !mMap.solid(int(std::floor(nx)) + (m.dir > 0 ? 1 : 0), int(m.y) + 1))
            m.dir = -m.dir;
          else
            m.x = nx;
          break;
        }
        const float tx = float(pl->rivets[std::size_t(target)]) - 0.5f;
        if (std::abs(tx - m.x) <= 0.5f)
        {
          m.x = tx;
          m.state = MiteState::Unbolt;
          m.rivet = target;
          m.timer = 0;
        }
        else
        {
          m.dir = tx > m.x ? 1 : -1;
          m.x += 0.5f * float(m.dir);
        }
        break;
      }
      case MiteState::Unbolt:
        if (near)
        {
          m.state = MiteState::Tell;
          m.timer = 0;
          break;
        }
        if (!pl || m.rivet < 0 || m.rivet >= int(pl->rivets.size()))
        {
          m.state = MiteState::Crawl;
          break;
        }
        if (++pl->unbolt[std::size_t(m.rivet)] >= kRivetFrames)
          looseRivet(m.plate, m.rivet);
        break;
      case MiteState::Tell: // glowing green
        if (m.timer >= def.tell)
        {
          m.state = MiteState::Hop;
          m.timer = 0;
          m.vx = std::clamp(float(pb.x + 1 - m.x) / 6.0f, -1.0f, 1.0f);
          m.vy = -1.2f;
          if (isOnScreen(miteBox(m), 0))
            playSound(Sfx::Hop);
        }
        break;
      case MiteState::Hop:
      case MiteState::Fall:
      {
        m.vy = std::min(1.5f, m.vy + 0.15f);
        m.x += m.vx;
        const float ny = m.y + m.vy;
        const int cx = int(std::floor(m.x));
        if (m.vy > 0.0f && (mMap.solid(cx, int(std::floor(ny)) + 1) || mMap.solid(cx + 1, int(std::floor(ny)) + 1) ||
              mMap.solidTop(cx, int(std::floor(ny)) + 1)))
        {
          m.y = std::floor(ny);
          m.vx = m.vy = 0.0f;
          m.state = MiteState::Crawl;
          m.cool = std::max(m.cool, def.cooldown);
          // It lands on whatever plate is there now.
          m.plate = -1;
          for (std::size_t k = 0; k < h.plates.size(); ++k)
          {
            const auto& q = h.plates[k];
            if (!q.drifting && int(m.y) + 1 == q.y * kCellsPerTile && cx >= q.x0 * kCellsPerTile - 1 &&
                cx <= (q.x1 + 1) * kCellsPerTile - 1)
              m.plate = int(k);
          }
          break;
        }
        m.y = ny;
        if (m.y > float(mLevel->height * kCellsPerTile + 4))
        {
          m.state = MiteState::Dead; // lost in space
          if (--e.hp <= 0 && e.alive)
            e.alive = false;
          break;
        }
        if (m.state == MiteState::Hop && hurtable && miteBox(m).intersects(pb))
        {
          m.state = MiteState::Ride;
          m.timer = 0;
          if (e.carrier || m.swarm < 0)
          {
            if (p.virus == 0)
              infect();
          }
          else
          {
            hurtPlayer(1);
          }
        }
        break;
      }
      case MiteState::Ride:
        // Clinging to the runner's suit, then it lets go.
        m.x = float(p.x + (ridden % 2 ? 2 : 0));
        m.y = float(p.y - 1 - (ridden / 2) * 2);
        ++ridden;
        if (m.timer >= kRide || !hurtable)
        {
          m.state = MiteState::Fall;
          m.vx = float(-p.facing) * 0.5f;
          m.vy = -0.5f;
          m.cool = def.cooldown;
        }
        break;
      case MiteState::Dead:
        break;
    }
    if (m.flash > 0)
      --m.flash;
  }
  // Keep the swarm's box on its mites, for the screen checks and the bot.
  for (const auto& m : h.mites)
    if (m.swarm == swarm && m.state != MiteState::Dead)
    {
      e.x = int(m.x);
      e.y = int(m.y);
      break;
    }
}

// --- Every frame ---------------------------------------------------------------------------

void World::updateHull(const PlayerInput& input)
{
  auto& h = mHull;
  if (!h.on)
    return;
  auto& p = mPlayer;
  if (h.kick > 0)
    --h.kick;
  if (h.alien > 0)
    --h.alien;
  for (auto& f : h.flags)
    f.age = std::min(f.age + 1, 1000);
  if (p.state == PlayerState::OnGround || p.state == PlayerState::Ladder || p.state == PlayerState::Pipe)
    h.boostUsed = false;

  // Pushed along by a shot's kick or a ram.
  if (h.shove > 0 && p.state != PlayerState::Dying)
  {
    const int step = (h.shove + h.shoveFrames - 1) / std::max(1, h.shoveFrames);
    for (int i = 0; i < step && h.shove > 0; ++i)
    {
      if (mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), h.shoveDir) != MoveResult::Completed)
      {
        h.shove = 0;
        break;
      }
      --h.shove;
    }
    if (--h.shoveFrames <= 0)
      h.shove = 0;
  }
  else if (p.state == PlayerState::Dying)
  {
    h.shove = 0;
  }

  updateHullPlates();

  // The candid camera's loop.
  for (auto& d : h.drifters)
  {
    d.spin += 0.06f;
    if (d.enemy < 0 || !mEnemies[std::size_t(d.enemy)].alive || d.path.size() < 2)
      continue;
    const auto [tx, ty] = d.path[std::size_t(d.target)];
    const float dx = float(tx) - d.fx, dy = float(ty) - d.fy;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= d.speed)
    {
      d.fx = float(tx);
      d.fy = float(ty);
      d.target = (d.target + 1) % int(d.path.size());
    }
    else
    {
      d.fx += dx / len * d.speed;
      d.fy += dy / len * d.speed;
    }
    auto& cam = mEnemies[std::size_t(d.enemy)];
    cam.x = int(std::lround(d.fx));
    cam.y = int(std::lround(d.fy));
  }

  // The UFO's hatch shot open: its tiny alien gives a thumbs-up.
  if (!h.ufoOpen)
    for (const auto& b : mBreakables)
      if (b.look == kLookUfo && b.broken)
      {
        h.ufoOpen = true;
        h.alien = kAlien;
        h.alienX = (b.x0 + b.x1 + 1) * kCellsPerTile / 2;
        h.alienY = (b.y1 + 1) * kCellsPerTile - 1;
        playSound(Sfx::Cheer);
        break;
      }

  // Stand still on the hull for 30 s and the runner plants a flag.
  const bool idle = p.state == PlayerState::OnGround && !input.left && !input.right && !input.up && !input.down &&
    !input.jump.pressed && !input.fire.pressed;
  if (h.lowgrav && idle)
  {
    if (++h.still == kStill)
    {
      bool there = false;
      for (const auto& f : h.flags)
        there = there || (std::abs(f.x - (p.x + 1)) < 4 && f.y == p.y);
      if (!there)
      {
        h.flags.push_back({p.facing > 0 ? p.x - 1 : p.x + 4, p.y, 0});
        playSound(Sfx::FlagUp);
        showMessage("ONE SMALL STEP");
      }
    }
  }
  else
  {
    h.still = 0;
  }
}

} // namespace gr
