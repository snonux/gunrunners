#include "frontend/bot.hpp"

#include "game/world.hpp"

#include <cmath>

namespace td
{

namespace
{

int toTile(float v) { return int(std::floor(v / float(kTileSize))); }

bool isGround(Tile t) { return t == Tile::Solid || t == Tile::Platform || t == Tile::Crate; }

} // namespace

Input Bot::menu(int cursor, int target, int ticksInMenu) const
{
  Input in;
  // Browse through all three dudes first, then settle on the target.
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
  const auto& p = world.player();
  const auto& level = world.level();
  const Rect b = p.box();
  Input in;
  in.right = true;

  const int footY = toTile(b.bottom() - 1.0f);
  const int headY = toTile(b.y + 2.0f);
  const int frontX = toTile(b.right() + 5.0f);
  const bool wallAhead = level.isSolid(frontX, footY) || level.isSolid(frontX, headY);

  const int aheadX = toTile(b.right() + 10.0f);
  bool groundAhead = false;
  for (int dy = 1; dy <= 3 && !groundAhead; ++dy)
    groundAhead = isGround(level.at(aheadX, footY + dy));

  bool spikesAhead = false;
  for (float dx : {6.0f, 14.0f, 22.0f})
    if (level.at(toTile(b.right() + dx), footY) == Tile::Spikes ||
        level.at(toTile(b.right() + dx), footY + 1) == Tile::Spikes)
      spikesAhead = true;

  // Shoot whatever is in front and roughly at gun height.
  bool holdPosition = false;
  for (const auto& e : world.enemies())
  {
    if (!e.alive || !e.active)
      continue;
    const Rect eb = e.box();
    const float dx = eb.cx() - b.cx();
    const float dy = eb.cy() - (p.pos.y + 12.0f);
    if (dx > -4.0f && dx < 150.0f && std::abs(dy) < 18.0f)
    {
      in.fire = true;
      if (e.kind != EnemyKind::Flyer && dx < 46.0f)
        holdPosition = true;
    }
  }
  for (int dx = 0; dx < 6; ++dx)
  {
    const int tx = toTile(b.right()) + dx;
    if (level.at(tx, footY) == Tile::Crate || level.at(tx, footY - 1) == Tile::Crate)
    {
      in.fire = true;
      if (dx <= 1)
        holdPosition = true;
      break;
    }
  }
  if (holdPosition && p.onGround)
    in.right = false;

  // Jumping.
  const bool crateAhead = level.at(frontX, footY) == Tile::Crate;
  const bool needJump = !holdPosition && !crateAhead && (wallAhead || !groundAhead || spikesAhead);
  if (mJumpRelease > 0)
    --mJumpRelease;
  const bool canStartJump = mJumpHold == 0 && mJumpRelease == 0;
  if (p.onGround && canStartJump && needJump)
    mJumpHold = 22;

  if (in.right && p.onGround && std::abs(p.pos.x - mLastX) < 0.05f)
    ++mStuckTicks;
  else
    mStuckTicks = 0;
  if (mStuckTicks > 20 && canStartJump)
  {
    mJumpHold = 22;
    mStuckTicks = 0;
  }
  mLastX = p.pos.x;

  if (mJumpHold > 0)
  {
    in.jump = true;
    if (--mJumpHold == 0)
      mJumpRelease = 2; // let go so the next press registers
  }
  return in;
}

} // namespace td
