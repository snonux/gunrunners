// Level 25's bonus, Dandelion Drift (rules=glide): no walking or jumping.
// The runner hangs from a seed head drifting right at 1/2 cell a frame; Up
// and Down steer at 1/2 cell a frame; `@ wind rect= dir=u|d push=` gusts
// lift or drop it by push/2 cells a frame; a thorn (spikes or a solid block)
// or a dragonfly bounces it back 4 blocks, no damage. Reaching the exit's
// column ends the bonus.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

// `@ dragonfly x y range= speed=`: hovers up and down range blocks about (x, y).
bool World::setupGlideEntity(const EntityDef& e)
{
  if (e.kind != "dragonfly" || !e.hasPos)
    return false;
  Dragonfly d;
  d.x = d.prevX = float(e.x * kCellsPerTile + 1);
  d.y = d.prevY = float(e.y * kCellsPerTile + 1);
  const float range = float(std::max(0, e.num("range", 3)) * kCellsPerTile);
  d.y0 = d.y - range;
  d.y1 = d.y + range;
  d.speed = e.real("speed", 0.25f);
  mGlide.flies.push_back(d);
  return true;
}

void World::linkGlide()
{
  auto& gl = mGlide;
  gl.on = mLevel->rules.find("glide") != std::string::npos;
  if (!gl.on)
    return;
  // The level's up and down winds are the glide's gusts (updatePlayer, which
  // would push with them, never runs here).
  for (const auto& z : mZones)
    if (z.kind == ZoneKind::Wind && z.dy != 0)
      gl.winds.push_back({z.box.left(), z.box.top(), z.box.right(), z.box.bottom(), z.dy, std::max(1, z.num / z.den)});
  gl.fx = float(mPlayer.x);
  gl.fy = float(mPlayer.y);
  mPlayer.facing = 1;
}

void World::updateGlide(const PlayerInput& input)
{
  auto& gl = mGlide;
  auto& p = mPlayer;
  for (auto& d : gl.flies)
  {
    d.prevX = d.x;
    d.prevY = d.y;
    d.y += d.speed * float(d.dir);
    if (d.y <= d.y0 || d.y >= d.y1)
      d.dir = -d.dir;
  }
  const auto blocked = [&](float fx, float fy) {
    const CellBox b = boxAt(int(std::lround(fx)), int(std::lround(fy)), Player::kWidth, Player::kHeight);
    if (mMap.overlapsSolid(b) || mMap.overlapsHazard(b))
      return true;
    for (const auto& d : gl.flies)
      if (CellBox{int(d.x) - 3, int(d.y) - 1, 6, 2}.intersects(b))
        return true;
    return false;
  };
  if (gl.bounce > 0)
  {
    // Bouncing back: eased out over kGlideBounce frames, stopped by anything in the way.
    --gl.bounce;
    const float t = 1.0f - float(gl.bounce) / float(kGlideBounce);
    const float nx = gl.bounceFrom + (gl.bounceTo - gl.bounceFrom) * (1.0f - (1.0f - t) * (1.0f - t));
    if (!mMap.overlapsSolid(boxAt(int(std::lround(nx)), int(std::lround(gl.fy)), Player::kWidth, Player::kHeight)))
      gl.fx = nx;
    else
      gl.bounce = 0;
  }
  else
  {
    float nx = gl.fx + kGlideDrift;
    float ny = gl.fy + kGlideSteer * float((input.down ? 1 : 0) - (input.up ? 1 : 0));
    for (const auto& w : gl.winds)
      if (CellBox{w.x0, w.y0, w.x1 - w.x0 + 1, w.y1 - w.y0 + 1}.intersects(p.box()))
        ny += 0.5f * float(w.push * w.dir);
    ny = std::clamp(ny, float(Player::kHeight), float(mMap.height() - 1));
    if (blocked(nx, ny))
    {
      // A thorn or a dragonfly: back 4 blocks.
      gl.bounce = kGlideBounce;
      gl.bounceFrom = gl.fx;
      gl.bounceTo = std::max(0.0f, gl.fx - float(4 * kCellsPerTile));
      ++gl.bounces;
      playSound(Sfx::Boing);
      burst(cellCenter(p.box()), rgb(255, 255, 255), rgb(240, 240, 220), 10, 1.2f);
    }
    else
    {
      gl.fx = nx;
      gl.fy = ny;
    }
  }
  p.x = int(std::lround(gl.fx));
  p.y = int(std::lround(gl.fy));
  p.facing = 1;
  p.state = PlayerState::OnGround;
  setVisual(PlayerVisual::Hanging);
  // The exit: past the fence.
  if (mLevel->hasExit && p.box().right() >= mLevel->exitTx * kCellsPerTile)
  {
    p.state = PlayerState::Teleporting;
    setVisual(PlayerVisual::Standing);
    mState = WorldState::Exiting;
    mStateFrames = 0;
    playSound(Sfx::Teleport);
  }
}

} // namespace gr
