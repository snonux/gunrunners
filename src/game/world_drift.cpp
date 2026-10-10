// Asteroid Belt (SPEC 15's bonus level, rules=recoil_only): no gravity, no
// ground, no jump. Every shot pushes the runner half a cell a frame the
// other way (8 directions, 2 cells a frame at most) and drag takes 1/32 of
// the speed each frame. Asteroids (`@ asteroid`) tumble along slow loops;
// touching one, or the belt's edge, bounces the runner back at half speed
// and never hurts. The return beacon (the level's exit) ends the stage.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kKick = 0.5f;     // cells a frame, a shot
constexpr float kMaxSpeed = 2.0f; // cells a frame
constexpr float kDrag = 1.0f / 32.0f;
constexpr int kFireEvery = 3;     // frames between shots, fire held
constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr float kTilePx = kCellPx * float(kCellsPerTile);  // 64

// The runner's body at (fx, fy): 3 x 5 cells over its feet.
CellBox bodyAt(float fx, float fy)
{
  return {int(std::floor(fx)), int(std::floor(fy)) - 4, 3, 5};
}

CellBox rockBox(const Asteroid& a)
{
  return {int(std::floor(a.fx)), int(std::floor(a.fy)), a.w * kCellsPerTile, a.h * kCellsPerTile};
}

} // namespace

void World::setupDrift()
{
  auto& d = mStation.drift;
  d = DriftState{};
  d.fx = float(mPlayer.x);
  d.fy = float(mPlayer.y);
  for (auto& a : mStation.asteroids)
  {
    a.fx = float(a.path[0].first * kCellsPerTile);
    a.fy = float(a.path[0].second * kCellsPerTile);
    a.seg = a.path.size() > 1 ? 1 : 0;
  }
}

// Whether the runner's body at `b` hits the belt's edge or a rock (but a
// rock it is already inside, pinned against the edge, lets it out).
bool World::driftBlocked(const CellBox& b) const
{
  for (int y = b.top(); y <= b.bottom(); ++y)
    for (int x = b.left(); x <= b.right(); ++x)
      if (mMap.solid(x, y) || y >= mMap.height())
        return true;
  const CellBox now = bodyAt(mStation.drift.fx, mStation.drift.fy);
  for (const auto& a : mStation.asteroids)
    if (rockBox(a).intersects(b) && !rockBox(a).intersects(now))
      return true;
  return false;
}

void World::moveAsteroids()
{
  auto& d = mStation.drift;
  for (auto& a : mStation.asteroids)
  {
    a.angle = std::fmod(a.angle + a.spin, 360.0f);
    if (a.path.size() < 2 || a.speed <= 0.0f)
      continue;
    const float tx = float(a.path[std::size_t(a.seg)].first * kCellsPerTile);
    const float ty = float(a.path[std::size_t(a.seg)].second * kCellsPerTile);
    const float dx = tx - a.fx, dy = ty - a.fy, dist = std::hypot(dx, dy);
    const float ox = a.fx, oy = a.fy;
    if (dist <= a.speed)
    {
      a.fx = tx;
      a.fy = ty;
      a.seg = (a.seg + 1) % int(a.path.size());
    }
    else
    {
      a.fx += dx / dist * a.speed;
      a.fy += dy / dist * a.speed;
    }
    // Drifting into the runner: a shove the way it goes.
    const float mx = a.fx - ox, my = a.fy - oy;
    if (rockBox(a).intersects(bodyAt(d.fx, d.fy)))
    {
      for (int k = 0; k < 8 && rockBox(a).intersects(bodyAt(d.fx, d.fy)); ++k)
      {
        const float nx = d.fx + (mx == 0.0f ? 0.0f : (mx > 0.0f ? 0.5f : -0.5f));
        const float ny = d.fy + (my == 0.0f ? 0.0f : (my > 0.0f ? 0.5f : -0.5f));
        const CellBox b = bodyAt(nx, ny);
        bool wall = false;
        for (int y = b.top(); y <= b.bottom() && !wall; ++y)
          for (int x = b.left(); x <= b.right() && !wall; ++x)
            wall = mMap.solid(x, y) || y >= mMap.height();
        if (wall)
          break; // pinned against the edge: it slides on through
        d.fx = nx;
        d.fy = ny;
      }
      d.vx = mx * 2.0f + (mx > 0.0f ? 0.25f : (mx < 0.0f ? -0.25f : 0.0f));
      d.vy = my * 2.0f + (my > 0.0f ? 0.25f : (my < 0.0f ? -0.25f : 0.0f));
      if (d.bump > 6)
        playSound(Sfx::Clunk);
      d.bump = 0;
    }
  }
}

void World::updateDrift(const PlayerInput& input)
{
  auto& p = mPlayer;
  auto& d = mStation.drift;
  ++d.bump;
  moveAsteroids();

  // Aim: the arrows held (8 ways), or the way the runner faces.
  int ax = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  const int ay = (input.down ? 1 : 0) - (input.up ? 1 : 0);
  if (ax != 0)
    p.facing = ax;
  if (ax == 0 && ay == 0)
    ax = p.facing;
  p.stance = ay < 0 && ax == 0 ? Stance::Up : Stance::Regular;
  if (d.cool > 0)
    --d.cool;
  if ((input.fire.triggered || input.fire.pressed) && d.cool == 0)
  {
    // The shot one way, the runner the other.
    const int mx = p.x + (ax > 0 ? 3 : (ax < 0 ? -2 : 1)), my = p.y - 3 + ay * 3;
    // The runner's own gun (it never runs dry out here).
    const ShotKind kind = p.weapon == Weapon::Laser ? ShotKind::Laser
      : p.weapon == Weapon::Rocket                  ? ShotKind::Rocket
      : p.weapon == Weapon::Flame                   ? ShotKind::Flame
                                                    : ShotKind::Normal;
    spawnProjectile(kind, mx, my, ax, ay);
    playSound(kind == ShotKind::Laser ? Sfx::LaserShot
        : kind == ShotKind::Rocket    ? Sfx::RocketShot
        : kind == ShotKind::Flame     ? Sfx::FlameShot
                                      : Sfx::Shot);
    p.muzzleTicks = 6;
    p.muzzleStance = p.stance;
    const float n = (ax != 0 && ay != 0) ? 0.7071f : 1.0f;
    d.vx -= float(ax) * kKick * n;
    d.vy -= float(ay) * kKick * n;
    const float sp = std::hypot(d.vx, d.vy);
    if (sp > kMaxSpeed)
    {
      d.vx *= kMaxSpeed / sp;
      d.vy *= kMaxSpeed / sp;
    }
    d.cool = kFireEvery;
    ++d.shots;
  }
  d.vx -= d.vx * kDrag;
  d.vy -= d.vy * kDrag;
  if (std::abs(d.vx) < 0.01f)
    d.vx = 0.0f;
  if (std::abs(d.vy) < 0.01f)
    d.vy = 0.0f;

  // Move in half-cell steps; whatever it hits bounces it back at half speed.
  bool bumped = false;
  const int steps = std::max(1, int(std::ceil(std::max(std::abs(d.vx), std::abs(d.vy)) / 0.5f)));
  for (int s = 0; s < steps; ++s)
  {
    const float nx = d.fx + d.vx / float(steps);
    if (driftBlocked(bodyAt(nx, d.fy)))
    {
      d.vx = -d.vx * 0.5f;
      bumped = true;
    }
    else
      d.fx = nx;
    const float ny = d.fy + d.vy / float(steps);
    if (driftBlocked(bodyAt(d.fx, ny)))
    {
      d.vy = -d.vy * 0.5f;
      bumped = true;
    }
    else
      d.fy = ny;
  }
  if (bumped)
  {
    if (d.bump > 6)
      playSound(Sfx::Clunk);
    d.bump = 0;
  }
  p.x = int(std::floor(d.fx));
  p.y = int(std::floor(d.fy));
  p.state = PlayerState::Falling;
  setVisual(d.bump < 4 ? PlayerVisual::Falling : PlayerVisual::Jumping);

  // Shots into rock: a puff of dust.
  for (auto& pr : mProjectiles)
    if (pr.alive && pr.kind != ShotKind::Enemy)
      for (const auto& a : mStation.asteroids)
        if (rockBox(a).intersects({pr.x, pr.y, pr.w, pr.h}))
        {
          pr.alive = false;
          burst({(float(pr.x) + 1.0f) * kCellSize, (float(pr.y) + 0.5f) * kCellSize}, rgb(170, 150, 130),
            rgb(110, 100, 90), 5, 1.2f);
          break;
        }

  // The return beacon.
  const CellBox beacon{mLevel->exitTx * kCellsPerTile - 2, mLevel->exitTy * kCellsPerTile - 2, 6, 6};
  if (beacon.intersects(p.box()))
  {
    p.state = PlayerState::Teleporting;
    setVisual(PlayerVisual::Standing);
    mState = WorldState::Exiting;
    mStateFrames = 0;
    playSound(Sfx::Teleport);
    showMessage("BACK TO THE STATION");
  }
}

void World::drawAsteroids(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  static const std::string kRock = "belt_asteroid";
  for (const auto& a : mStation.asteroids)
  {
    const float w = float(a.w) * kTilePx, h = float(a.h) * kTilePx;
    const float x = a.fx * kCellPx - camX, y = a.fy * kCellPx - camY;
    if (x > float(kScreenW) + 64.0f || x + w < -64.0f || y > float(kScreenH) + 64.0f || y + h < -64.0f)
      continue;
    // Tumbling: the sprite turns about its middle (drawn from its foot,
    // so the anchor moves round with it).
    const float rad = a.angle * 0.0174533f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    DrawOpts o;
    o.angle = a.angle;
    const float ax = cx - std::sin(rad) * h * 0.5f, ay = cy + std::cos(rad) * h * 0.5f;
    r.draw(styledEnemySprite(mArt, r, mTheme, kRock, a.look, 0, a.w * kCellsPerTile, a.h * kCellsPerTile).get(1), ax,
      ay, o);
  }
  // The return beacon: a ring of light that pulses.
  const float bx = (float(mLevel->exitTx) + 0.5f) * kTilePx - camX, by = (float(mLevel->exitTy) + 0.5f) * kTilePx - camY;
  const float pulse = 0.5f + 0.5f * std::sin(float(frame) * 0.15f);
  drawGlow(r, mArt, bx, by, 70.0f + 20.0f * pulse, rgb(80, 220, 255), 0.5f + 0.3f * pulse);
  drawGlow(r, mArt, bx, by, 24.0f, rgb(255, 255, 255), 0.9f);
}

} // namespace gr
