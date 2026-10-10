// Level 26's bonus, Top-Down Rampage (rules=topdown): the camera looks
// straight down on a grid city. The runner walks in eight directions (no
// gravity, no jumping) and every model building breaks when walked into,
// its rating going on the score; Model Tank squads drive the streets and
// fire shells at the runner (a 10-frame turret flash first). Fire shoots
// the way the runner last walked.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

void World::linkTopDown()
{
  mTop.on = mLevel->rules.find("topdown") != std::string::npos;
  if (!mTop.on)
    return;
  mTop.fx = 1;
  mTop.fy = 0;
  mPlayer.facing = 1;
}

void World::updateTopDown(int mvX, int mvY, const PlayerInput& in)
{
  auto& t = mTop;
  auto& p = mPlayer;
  if (p.shotCooldown > 0)
    --p.shotCooldown;
  if (mvX != 0 || mvY != 0)
  {
    t.fx = mvX;
    t.fy = mvY;
    if (mvX != 0)
      p.facing = mvX;
  }
  // One cell a frame along each axis; whatever building is in the way breaks.
  auto step = [&](int dx, int dy) {
    if (dx == 0 && dy == 0)
      return;
    const CellBox to = boxAt(p.x + dx, p.y + dy, Player::kWidth, p.height());
    for (std::size_t i = 0; i < mCity.buildings.size(); ++i)
    {
      const Building& b = mCity.buildings[i];
      if (b.standing() &&
          to.intersects({b.x0 * kCellsPerTile, b.y0 * kCellsPerTile, (b.x1 - b.x0 + 1) * kCellsPerTile,
            (b.y1 - b.y0 + 1) * kCellsPerTile}))
      {
        breakBuilding(int(i));
        ++t.broken;
      }
    }
    if (!mMap.overlapsSolid(to))
    {
      p.x += dx;
      p.y += dy;
    }
  };
  step(mvX, 0);
  step(0, mvY);
  p.state = PlayerState::OnGround;
  p.stance = Stance::Regular;
  setVisual(mvX != 0 || mvY != 0 ? PlayerVisual::Walking : PlayerVisual::Standing);
  if (in.fire.pressed && (p.shotCooldown == 0 || p.turbo > 0) && (in.fire.triggered || p.shotCooldown == 0))
  {
    const CellBox b = p.box();
    const int cx = b.x + 1, cy = b.y + b.h / 2;
    const int ax = t.fx > 0 ? b.right() + 1 : (t.fx < 0 ? b.left() - 1 : cx);
    const int ay = t.fy > 0 ? b.bottom() + 1 : (t.fy < 0 ? b.top() - 1 : cy);
    spawnProjectile(ShotKind::Normal, ax, ay, t.fx, t.fy);
    p.shotCooldown = 5;
    p.muzzleTicks = 4;
    playSound(Sfx::Shot);
  }
}

// A tank on the street grid: drives straight, turns at anything in the
// way (now and then toward the runner at a crossing), and fires when its
// squad says so.
void World::updateTopTank(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
  if (e.aimX == 0 && e.aimY == 0)
    e.aimX = e.dir;
  if (e.tell > 0)
  {
    if (--e.tell == 0 && p.state != PlayerState::Dying)
    {
      const int dx = (p.x + 1) - (b.x + 1), dy = (p.y - 2) - (b.y + 1);
      int sx = dx == 0 ? 0 : (dx < 0 ? -1 : 1), sy = dy == 0 ? 0 : (dy < 0 ? -1 : 1);
      if (std::abs(dy) * 2 < std::abs(dx))
        sy = 0;
      if (std::abs(dx) * 2 < std::abs(dy))
        sx = 0;
      if (sx == 0 && sy == 0)
        sx = e.dir;
      spawnProjectile(ShotKind::Enemy, b.x + 1 + sx * 2, b.y + 1 + sy * 2, sx, sy);
      playSound(Sfx::Cannon);
    }
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  auto open = [&](int dx, int dy) {
    const CellBox to = boxAt(e.x + dx, e.y + dy, e.w, e.h);
    return !mMap.overlapsSolid(to);
  };
  // At a crossing, now and then turn toward the runner.
  if ((e.timer / std::max(1, def.stepEvery)) % 12 == 0)
  {
    const int tx = e.aimY != 0 ? (p.x < e.x ? -1 : 1) : 0;
    const int ty = e.aimX != 0 ? (p.y < e.y ? -1 : 1) : 0;
    if ((tx != 0 || ty != 0) && open(tx * 3, ty * 3))
    {
      e.aimX = tx;
      e.aimY = ty;
    }
  }
  if (!open(e.aimX, e.aimY))
  {
    const bool left = ((e.timer / std::max(1, def.stepEvery)) + e.id) % 2 == 0;
    const int px = left ? e.aimY : -e.aimY, py = left ? -e.aimX : e.aimX;
    if (open(px, py))
    {
      e.aimX = px;
      e.aimY = py;
    }
    else if (open(-px, -py))
    {
      e.aimX = -px;
      e.aimY = -py;
    }
    else
    {
      e.aimX = -e.aimX;
      e.aimY = -e.aimY;
    }
    return;
  }
  e.x += e.aimX;
  e.y += e.aimY;
  if (e.aimX != 0)
    e.dir = e.aimX;
}

} // namespace gr
