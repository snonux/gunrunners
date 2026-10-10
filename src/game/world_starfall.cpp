// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 43, Starfall: you fly
// the courier ship (world_vehicle.cpp) through open space over the planet
// Vurr. Asteroids drift through it; shots push them along and chip at them
// (big ones split in two, then in two again), so you clear your own lane.
// Void Rays sweep across in waves and dive at you, Rock Leeches ride the
// asteroids and spit. A `pilot=1` ship is where the runner starts and
// respawns, and there is no getting out of it until it lands.

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

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile); // 64
constexpr int kRockMaxSpeed = 24; // sixteenths of a cell a frame
constexpr int kRayDive = 14;      // frames of a Void Ray's dive
constexpr int kRayTurn = 34;      // cells behind you before a Void Ray turns round

int sgn(int v) { return (v > 0) - (v < 0); }

int hpFor(int size) { return size >= 6 ? 10 : (size >= 4 ? 6 : 2); }

CellBox rockBox(const DriftRock& k) { return {k.cx(), k.cy(), k.size, k.size}; }

} // namespace

// --- Setup ---------------------------------------------------------------------------

bool World::setupStarfallEntity(const EntityDef& e)
{
  if (e.kind == "rock" && e.hasPos)
  {
    // `@ rock x y size=1..3 v=vx,vy`: x y the block of its top-left corner,
    // size in blocks, v in sixteenths of a cell a frame.
    DriftRock k;
    k.size = std::clamp(e.num("size", 2), 1, 3) * 2;
    k.hp = hpFor(k.size);
    k.x = e.x * kCellsPerTile * 16;
    k.y = e.y * kCellsPerTile * 16;
    const auto v = e.list("v");
    if (v.size() >= 2)
    {
      k.vx = v[0];
      k.vy = v[1];
    }
    k.look = e.num("look", (e.x * 7 + e.y * 3) % 3);
    mSpace.rocks.push_back(k);
    return true;
  }
  if ((e.kind == "probe" || e.kind == "landingpad") && (e.hasPos || e.has("rect")))
  {
    SpaceProp pr;
    pr.kind = e.kind;
    const auto r = e.list("rect");
    if (r.size() >= 4)
    {
      pr.x = r[0];
      pr.y = r[1];
      pr.w = r[2] - r[0] + 1;
      pr.h = r[3] - r[1] + 1;
    }
    else
    {
      pr.x = e.x;
      pr.y = e.y;
      pr.w = e.num("w", 5);
      pr.h = e.num("h", 3);
    }
    mSpace.props.push_back(pr);
    return true;
  }
  if (e.kind == "clouds")
  {
    // The block x where the ship starts down into Vurr's clouds.
    mSpace.cloudX = e.hasPos ? e.x : e.num("x", -1);
    return true;
  }
  return false;
}

void World::finishStarfallSetup()
{
  mSpace.rocksAtStart = mSpace.rocks;
  boardPilotShip();
}

void World::boardPilotShip()
{
  auto& p = mPlayer;
  if (p.vehicle >= 0 || p.state == PlayerState::Dying)
    return;
  for (std::size_t i = 0; i < mVehicles.size(); ++i)
  {
    const Vehicle& v = mVehicles[i];
    if (!v.pilot || v.occupied || v.wreck > 0)
      continue;
    boardVehicle(int(i));
    showMessage("FLY TO THE LANDING PAD - SHOOT THE ROCKS OUT OF YOUR WAY");
    mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
    return;
  }
}

void World::resetStarfall()
{
  mSpace.rocks = mSpace.rocksAtStart;
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::RockLeech)
      e.platform = -1; // finds its rock again
  boardPilotShip();
}

// --- The rocks -----------------------------------------------------------------------

void World::hitRock(std::size_t i, int damage, int pushX, int pushY)
{
  DriftRock& k = mSpace.rocks[i];
  k.vx = std::clamp(k.vx + pushX, -kRockMaxSpeed, kRockMaxSpeed);
  k.vy = std::clamp(k.vy + pushY, -kRockMaxSpeed, kRockMaxSpeed);
  k.flash = 4;
  k.hp -= damage;
  const CellBox b = rockBox(k);
  const Vec2 c = cellCenter(b);
  burst(c, rgb(200, 186, 170), rgb(120, 104, 96), 5, 1.2f, false);
  if (k.hp > 0)
  {
    playSound(Sfx::Hit);
    return;
  }
  k.alive = false;
  playSound(Sfx::SmallExplosion);
  burst(c, rgb(220, 200, 180), rgb(90, 80, 80), 10 + k.size * 2, 1.6f, false);
  if (k.size <= 2)
  {
    addScore(100, c);
    return;
  }
  // Two halves fly apart, up and down, still going the way it was pushed.
  addScore(k.size >= 6 ? 200 : 150, c);
  const DriftRock parent = k;
  for (int s : {-1, 1})
  {
    DriftRock h;
    h.size = parent.size - 2;
    h.hp = hpFor(h.size);
    h.x = parent.x + (s > 0 ? 2 * 16 : 0);
    h.y = parent.y + (s > 0 ? 2 * 16 : 0);
    h.vx = std::clamp(parent.vx, -kRockMaxSpeed, kRockMaxSpeed);
    h.vy = std::clamp(parent.vy + s * 7, -kRockMaxSpeed, kRockMaxSpeed);
    h.look = (parent.look + 1 + (s > 0)) % 3;
    mSpace.rocks.push_back(h); // (k is not used after this)
  }
}

bool World::shotAtRock(Projectile& pr)
{
  const CellBox b = pr.box();
  for (std::size_t i = 0; i < mSpace.rocks.size(); ++i)
  {
    const DriftRock& k = mSpace.rocks[i];
    if (!k.alive || !rockBox(k).intersects(b))
      continue;
    // Pushed along the shot, and off to the side it was hit from.
    const int midY = k.cy() * 2 + k.size, shotY = b.y * 2 + b.h;
    const int side = midY < shotY ? -1 : (midY > shotY ? 1 : 0);
    hitRock(i, std::max(1, pr.damage), sgn(pr.dx) * 4, side * 3 + sgn(pr.dy) * 4);
    return true;
  }
  return false;
}

void World::updateStarfall()
{
  if (!mSpace.starfall)
    return;
  auto& p = mPlayer;
  const int W = mMap.width(), H = mMap.height();
  for (std::size_t i = 0; i < mSpace.rocks.size(); ++i)
  {
    DriftRock& k = mSpace.rocks[i];
    if (!k.alive)
      continue;
    if (k.flash > 0)
      --k.flash;
    // Drift, one axis at a time; a wall or the map's edge bounces it back.
    auto blocked = [&](int x16, int y16) {
      const CellBox b{x16 / 16, y16 / 16, k.size, k.size};
      return b.x < 0 || b.y < 0 || b.right() >= W || b.bottom() >= H || mMap.overlapsSolid(b);
    };
    if (k.vx != 0)
    {
      if (blocked(k.x + k.vx, k.y))
        k.vx = -k.vx;
      else
        k.x += k.vx;
    }
    if (k.vy != 0)
    {
      if (blocked(k.x, k.y + k.vy))
        k.vy = -k.vy;
      else
        k.y += k.vy;
    }
  }

  // Rock Leeches ride their rocks, awake or not.
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::RockLeech)
      continue;
    if (e.platform == -1)
    {
      // Find its rock: the nearest one within three cells.
      int best = -2, bestD = 4;
      for (std::size_t i = 0; i < mSpace.rocks.size(); ++i)
      {
        const DriftRock& k = mSpace.rocks[i];
        if (!k.alive)
          continue;
        const CellBox kb = rockBox(k), eb = e.box();
        const int gx = std::max({0, kb.x - eb.right(), eb.x - kb.right()});
        const int gy = std::max({0, kb.y - eb.bottom(), eb.y - kb.bottom()});
        if (gx + gy < bestD)
        {
          bestD = gx + gy;
          best = int(i);
        }
      }
      e.platform = best;
      if (best >= 0)
      {
        e.ox = e.x - mSpace.rocks[std::size_t(best)].cx();
        e.oy = e.y - mSpace.rocks[std::size_t(best)].cy();
      }
    }
    if (e.platform < 0)
      continue;
    const DriftRock& k = mSpace.rocks[std::size_t(e.platform)];
    if (!k.alive)
    {
      killEnemy(e); // blown off with its rock
      continue;
    }
    e.x = k.cx() + e.ox;
    e.y = k.cy() + e.oy;
  }

  // Rocks against the ship: a dent, and both bounce apart.
  if (p.vehicle >= 0)
  {
    Vehicle& v = mVehicles[std::size_t(p.vehicle)];
    const CellBox vb = v.box();
    for (auto& k : mSpace.rocks)
    {
      if (!k.alive || !rockBox(k).intersects(vb))
        continue;
      const int dx = (vb.x * 2 + vb.w) - (k.cx() * 2 + k.size);
      const int dy = (vb.y * 2 + vb.h) - (k.cy() * 2 + k.size);
      if (std::abs(dx) * vb.h >= std::abs(dy) * vb.w)
      {
        const int s = dx >= 0 ? 1 : -1;
        v.vx = s * std::max(std::abs(v.vx) / 2, 14);
        k.vx = std::clamp(k.vx - s * 6, -kRockMaxSpeed, kRockMaxSpeed);
        vehicleStep(v, s, 0);
      }
      else
      {
        const int s = dy >= 0 ? 1 : -1;
        v.vy = s * std::max(std::abs(v.vy) / 2, 14);
        k.vy = std::clamp(k.vy - s * 6, -kRockMaxSpeed, kRockMaxSpeed);
        vehicleStep(v, 0, s);
      }
      damageVehicle(v, 1);
      if (p.vehicle < 0)
        break; // that was the last of its armour
    }
    if (p.vehicle >= 0)
      placeDriver();
  }
  else if (p.state != PlayerState::Dying && p.state != PlayerState::Teleporting)
  {
    for (const auto& k : mSpace.rocks)
      if (k.alive && rockBox(k).intersects(p.hitBox()))
      {
        hurtPlayer(1);
        break;
      }
  }
}

// --- Aliens --------------------------------------------------------------------------

// Void Ray: sweeps across in a wave (e.attach 0); when you are close ahead
// its fins light up (1, the tell) and it dives straight at where you were
// (2). It turns round once it is far behind you.
void World::updateVoidRay(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.railX1 == 0)
  {
    e.railX1 = 1;
    e.aimY = e.y; // the wave's middle row
    e.ox = (e.x * 5) % 52; // formations: each ray a little further along the wave
    e.dir = -1;
  }
  const CellBox b = e.box();
  const CellBox pb = riding() ? riding()->box() : p.box();
  const int dx = (pb.x * 2 + pb.w) / 2 - (b.x * 2 + b.w) / 2;
  const int dy = (pb.y * 2 + pb.h) / 2 - (b.y * 2 + b.h) / 2;
  const int W = mMap.width(), H = mMap.height();
  auto moveTo = [&](int nx, int ny) {
    nx = std::clamp(nx, 0, W - e.w);
    ny = std::clamp(ny, e.h, H - 1);
    if (!mMap.overlapsSolid(boxAt(nx, ny, e.w, e.h)))
    {
      e.x = nx;
      e.y = ny;
      return true;
    }
    return false;
  };
  if (e.cool > 0)
    --e.cool;
  switch (e.attach)
  {
    case 1: // fins lit
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.timer = 0;
        // Eight ways, straight at where you are now.
        const double a = std::atan2(double(dy), double(dx));
        const int oct = int(std::lround(a / (3.14159265358979 / 4.0))) & 7;
        static const int kDir[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        e.railX0 = kDir[oct][0];
        e.oy = kDir[oct][1];
        if (e.railX0 != 0)
          e.dir = e.railX0;
        playSound(Sfx::Whoosh);
      }
      return;
    case 2: // diving
      for (int s = 0; s < 2; ++s)
        if (!moveTo(e.x + e.railX0, e.y + e.oy))
          e.timer = kRayDive;
      if (e.timer >= kRayDive)
      {
        e.attach = 0;
        e.aimY = e.y;
        e.cool = def.cooldown;
      }
      return;
    default:
      break;
  }
  // The wave.
  if (dx * e.dir < -kRayTurn)
    e.dir = -e.dir;
  const int wave = int(std::lround(4.0 * std::sin(double(e.timer + e.ox) * 0.12)));
  const int ny = std::clamp(e.aimY + wave, e.h + 1, H - 2);
  if (e.timer % 4 != 0 && !moveTo(e.x + e.dir, e.y))
    e.dir = -e.dir;
  if (ny != e.y)
    moveTo(e.x, e.y + sgn(ny - e.y));
  const bool ahead = dx * e.dir > 0 || std::abs(dx) < 6;
  if (e.cool == 0 && ahead && std::abs(dx) <= def.range && std::abs(dy) <= 12 && p.state != PlayerState::Dying &&
      isOnScreen(b, 0))
  {
    e.attach = 1;
    e.tell = def.tell;
  }
}

// Rock Leech: rides the asteroid it was put on (updateStarfall moves it,
// and it dies with its rock); it swells, then spits a slow glob at you as you pass.
void World::updateRockLeech(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
  const CellBox pb = riding() ? riding()->box() : p.box();
  const int dx = (pb.x * 2 + pb.w) / 2 - (b.x + 1);
  const int dy = (pb.y * 2 + pb.h) / 2 - (b.y + 1);
  if (e.tell == 0)
    e.dir = dx < 0 ? -1 : 1;
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell > 0)
      return;
    // A slow glob, straight at you.
    const float len = std::max(1.0f, std::sqrt(float(dx * dx + dy * dy)));
    Projectile pr;
    pr.kind = ShotKind::Enemy;
    pr.w = pr.h = 1;
    pr.speed = 1;
    pr.damage = 1;
    pr.precise = true;
    pr.range = 90;
    pr.fx = float(b.x + 1 + e.dir);
    pr.fy = float(b.y + 1);
    pr.vx = float(dx) / len * 0.8f;
    pr.vy = float(dy) / len * 0.8f;
    pr.dx = e.dir;
    pr.x = pr.prevX = int(pr.fx);
    pr.y = pr.prevY = int(pr.fy);
    mProjectiles.push_back(pr);
    playSound(Sfx::Spit);
    e.cool = def.cooldown;
    return;
  }
  if (std::abs(dx) <= def.range && std::abs(dy) <= 20 && isOnScreen(b, 0) && p.state != PlayerState::Dying)
    e.tell = def.tell; // it swells
}

// --- Drawing -------------------------------------------------------------------------

void World::drawStarfallSky(Renderer& r, float camX, float camY, int frame) const
{
  if (!mSpace.starfall || mSpace.cloudX < 0)
    return;
  // Down into Vurr's clouds: the black turns violet and cloud banks slide
  // past, slower far away and faster close by.
  const float viewMid = (camX + float(kScreenW) * 0.5f) / kTilePx;
  const float t = std::clamp((viewMid - float(mSpace.cloudX)) / 24.0f, 0.0f, 1.0f);
  if (t <= 0.0f)
    return;
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(110, 50, 160, int(t * 150.0f)));
  r.fillRect(0, float(kScreenH) * 0.55f, float(kScreenW), float(kScreenH) * 0.45f, rgba(190, 120, 220, int(t * 70.0f)));
  for (int layer = 0; layer < 3; ++layer)
  {
    const float par = 0.25f + 0.2f * float(layer);
    const float bandW = 1600.0f;
    const float y = 120.0f + float(layer) * 190.0f - camY * par * 0.3f;
    const float shift = std::fmod(camX * par + float(frame) * 0.3f * float(layer + 1), bandW);
    for (float x = -shift; x < float(kScreenW) + bandW; x += bandW)
    {
      DrawOpts o;
      o.alpha = t * (0.45f + 0.15f * float(layer));
      r.draw(styledEnemySprite(mArt, r, mTheme, "cloud_band", layer, 0, 50, 6).get(1), x + bandW * 0.5f, y + 192.0f, o);
    }
  }
}

void World::drawStarfallBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (!mSpace.starfall)
    return;
  auto visible = [](float x, float y, float w, float h) {
    return x + w > -64.0f && x < float(kScreenW) + 64.0f && y + h > -64.0f && y < float(kScreenH) + 64.0f;
  };
  for (const auto& pr : mSpace.props)
  {
    const float x = float(pr.x) * kTilePx - camX, y = float(pr.y) * kTilePx - camY;
    const float w = float(pr.w) * kTilePx, h = float(pr.h) * kTilePx;
    if (!visible(x, y, w, h))
      continue;
    if (pr.kind == "probe")
    {
      r.draw(styledEnemySprite(mArt, r, mTheme, "derelict_probe", 0, (frame / 20) % 2, pr.w * 2, pr.h * 2).get(1),
        x + w * 0.5f, y + h);
      drawGlow(r, mArt, x + w * 0.5f, y + h * 0.45f, 90.0f, rgb(255, 220, 90), 0.18f);
    }
    else
    {
      r.draw(styledEnemySprite(mArt, r, mTheme, "landing_pad", 0, (frame / 10) % 2, pr.w * 2, pr.h * 2).get(1),
        x + w * 0.5f, y + h);
    }
  }
  for (std::size_t i = 0; i < mSpace.rocks.size(); ++i)
  {
    const DriftRock& k = mSpace.rocks[i];
    if (!k.alive)
      continue;
    const float px = float(k.x) / 16.0f * kCellPx - camX, py = float(k.y) / 16.0f * kCellPx - camY;
    const float s = float(k.size) * kCellPx;
    if (!visible(px, py, s, s))
      continue;
    static const std::string kRock = "drift_rock";
    const Texture& tex = styledEnemySprite(mArt, r, mTheme, kRock, k.look, 0, k.size, k.size).get(1);
    r.draw(tex, px + s * 0.5f, py + s);
    if (k.flash > 0)
    {
      DrawOpts o;
      o.blend = Blend::Add;
      o.alpha = float(k.flash) / 5.0f;
      r.draw(tex, px + s * 0.5f, py + s, o);
    }
  }
}

} // namespace gr
