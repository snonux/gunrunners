// Boulder Surfing (SPEC 13, the bonus level, rules=boulder_surf): the runner
// stands on a 6 x 6 block boulder and rolls it along the canyon. Left and
// right speed the roll up or down by 1/16 cell a frame each frame (2 at
// most); pushing it, the runner leans back on top, and letting be, finds
// the middle again. More than 2 blocks off the top centre is a fall: back
// to the start of the screen, nothing lost. Jump leaves the boulder, which
// rolls on; landing back on it is fine, landing on the ground is a fall,
// except on the exit ledge past the last gap. Cultists walking at it are
// crushed (100 each).

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kAccel = 1.0f / 16.0f; // cells a frame, a frame
constexpr float kMaxSpeed = 2.0f;
constexpr float kDrag = 1.0f / 64.0f;  // rolling, left alone
constexpr float kLean = 0.125f;        // the runner's drift a frame while pushing
constexpr float kSettle = 0.0625f;     // and back toward the middle, let be
constexpr float kFallDrift = 4.0f;     // two blocks off the top centre
constexpr float kSlope = 0.06f;        // speed a step up costs (down gains), per cell
constexpr float kGravity = 0.25f;
constexpr int kStepUp = 4;
constexpr int kScreen = 40; // cells: a screen's width, where a fall restarts
constexpr int kFallFrames = 30;
constexpr int kLaunchCells = 9;

float sgn(float v)
{
  return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f);
}

} // namespace

void World::setupSurf()
{
  auto& s = mSurf;
  Surf fresh;
  fresh.exitX = s.exitX;
  fresh.restarts = s.restarts;
  s = fresh;
  if (mBoulders.empty())
  {
    mSurfing = false;
    return;
  }
  s.boulder = 0;
  mBoulders[0].state = Boulder::Surf;
  placeSurf(float(mBoulders[0].x));
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
}

// The boulder at `x` (cells, its left edge) on the floor there, at rest,
// with the runner on top in the middle.
void World::placeSurf(float x)
{
  auto& s = mSurf;
  auto& b = mBoulders[std::size_t(s.boulder)];
  auto& p = mPlayer;
  s.x = x;
  s.v = s.vy = s.drift = s.carry = s.carried = 0.0f;
  s.mounted = true;
  s.fall = 0;
  b.x = b.prevX = int(x);
  const int floor = rollFloor(b.x, kCellsPerTile, b.size);
  s.y = float((floor < 0 ? mMap.height() / 2 : floor) - b.size);
  b.y = b.prevY = int(s.y);
  b.state = Boulder::Surf;
  p.x = p.prevX = b.x + b.size / 2 - 1;
  p.y = p.prevY = b.y - 1;
  p.state = PlayerState::OnGround;
  p.hidden = false;
  setVisual(PlayerVisual::Standing);
}

void World::moveSurfBoulder()
{
  auto& s = mSurf;
  auto& b = mBoulders[std::size_t(s.boulder)];
  const int size = b.size;

  // Along: a wall higher than a step stops it dead.
  if (s.v != 0.0f)
  {
    const float nx = s.x + s.v;
    const int lead = s.v > 0.0f ? int(std::floor(nx)) + size - 1 : int(std::floor(nx));
    bool wall = lead < 0 || lead >= mMap.width();
    for (int y = int(s.y); !wall && y < int(s.y) + size - kStepUp - 1; ++y)
      wall = mMap.solid(lead, y);
    if (wall)
    {
      if (std::abs(s.v) > 0.5f)
      {
        playSound(Sfx::Crash);
        mCamera.shake(8, 2.0f);
      }
      s.v = 0.0f;
    }
    else
      s.x = nx;
  }
  b.x = int(std::floor(s.x));

  // Up and down: it rides up steps and falls where the floor drops away.
  const int floor = rollFloor(b.x, int(s.y) + size - kStepUp - 1, size);
  if (floor < 0)
  {
    s.vy = std::min(s.vy + kGravity, 4.0f);
    s.y += s.vy;
  }
  else
  {
    const float want = float(floor - size);
    if (s.y > want)
    {
      const float dy = std::min(2.0f, s.y - want);
      s.y -= dy;
      s.vy = 0.0f;
      const float sp = std::abs(s.v);
      s.v = sgn(s.v) * std::max(sp - kSlope * dy, std::min(sp, 0.25f));
      s.drift -= sgn(s.v) * 0.1f * dy; // the runner rocks back
    }
    else if (s.y < want)
    {
      s.vy = std::min(s.vy + kGravity, 4.0f);
      const float dy = std::min(want - s.y, s.vy);
      s.y += dy;
      s.v = sgn(s.v) * std::min(kMaxSpeed, std::abs(s.v) + kSlope * dy);
      if (s.y >= want)
      {
        if (s.vy >= 2.0f)
        {
          mCamera.shake(8, 2.5f);
          playSound(Sfx::Crash);
          burst({(s.x + float(size) * 0.5f) * kCellSize, float(floor) * kCellSize}, rgb(214, 190, 150),
            rgb(92, 70, 48), 18, 2.0f, false);
          s.drift += sgn(s.v) * 0.75f; // a hard landing throws the runner forward
        }
        s.vy = 0.0f;
      }
    }
  }
  b.y = int(std::lround(s.y));
  b.angle += s.v / (float(size) * 0.5f);
  if (std::abs(s.v) > 0.3f && s.vy == 0.0f && mStats.frames % 12 == 0)
    playSound(Sfx::Rumble);

  // It crushes the cultists walking at it.
  const float r = float(size) * 0.5f - 0.5f;
  const float cx = float(b.x) + float(size) * 0.5f, cy = float(b.y) + float(size) * 0.5f;
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.hidden)
      continue;
    const CellBox eb = e.box();
    const float nx = std::clamp(cx, float(eb.x), float(eb.x + eb.w));
    const float ny = std::clamp(cy, float(eb.y), float(eb.y + eb.h));
    if ((nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) < r * r)
      damageEnemy(e, 1000);
  }
}

void World::updateSurf(const PlayerInput& input)
{
  auto& s = mSurf;
  auto& p = mPlayer;
  auto& b = mBoulders[std::size_t(s.boulder)];
  b.prevX = b.x;
  b.prevY = b.y;
  b.prevAngle = b.angle;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
  {
    updatePlayer(input);
    return;
  }

  const float size = float(b.size);
  auto startFall = [&]() {
    s.mounted = false;
    s.fall = kFallFrames;
    s.carry = 0.0f;
    ++s.falls;
    p.state = PlayerState::Falling;
    setVisual(PlayerVisual::Falling);
    playSound(Sfx::Hurt);
    showMessage("WIPEOUT");
  };

  // A fall: the runner tumbles, the boulder rolls to a stop, and both go
  // back to the start of the screen.
  if (s.fall > 0)
  {
    s.v *= 0.9f;
    moveSurfBoulder();
    if (p.y < mMap.height())
      updatePlayer(PlayerInput{});
    if (--s.fall == 0)
    {
      const int at = std::clamp(int(s.x), 0, mMap.width() - 1);
      int start = std::max(at / kScreen * kScreen, kCellsPerTile);
      if (!s.restarts.empty())
      {
        start = s.restarts.front();
        for (int r : s.restarts)
          if (r <= at)
            start = std::max(start, r);
      }
      placeSurf(float(start));
      playSound(Sfx::Checkpoint);
    }
    return;
  }

  const int dir = s.mounted ? (input.right ? 1 : 0) - (input.left ? 1 : 0) : 0;
  if (dir != 0)
    s.v = std::clamp(s.v + float(dir) * kAccel, -kMaxSpeed, kMaxSpeed);
  else if (s.vy == 0.0f)
    s.v -= sgn(s.v) * std::min(std::abs(s.v), kDrag);
  if (s.mounted)
  {
    if (dir != 0)
      s.drift -= float(dir) * kLean;
    else
      s.drift -= sgn(s.drift) * std::min(std::abs(s.drift), kSettle);
  }
  moveSurfBoulder();

  if (s.mounted)
  {
    if (std::abs(s.drift) > kFallDrift || b.y > mMap.height())
    {
      p.x += s.drift > 0.0f ? 2 : -2;
      startFall();
      return;
    }
    p.x = int(std::lround(s.x + size * 0.5f + s.drift - 1.5f));
    p.y = b.y - 1;
    if (dir != 0)
      p.facing = dir;
    if (input.jump.triggered)
    {
      // Off the top, with the roll's speed.
      s.mounted = false;
      s.carry = s.v;
      s.carried = 0.0f;
      startLaunch(kLaunchCells);
      playSound(Sfx::Jump);
      return;
    }
    p.state = PlayerState::OnGround;
    setVisual(dir != 0 ? PlayerVisual::Walking : PlayerVisual::Standing);
    return;
  }

  // In the air off the boulder: the runner's own jump, plus the roll's speed.
  updatePlayer(input);
  s.carried += s.carry;
  while (std::abs(s.carried) >= 1.0f)
  {
    const int step = s.carried > 0.0f ? 1 : -1;
    s.carried -= float(step);
    CellBox nb = p.box();
    nb.x += step;
    if (mMap.overlapsSolid(nb))
    {
      s.carry = s.carried = 0.0f;
      break;
    }
    p.x += step;
  }
  // Back down on top of it?
  const float off = float(p.x) + 1.5f - (s.x + size * 0.5f);
  if (p.y >= b.y - 1 && p.prevY <= b.y + 1 && std::abs(off) <= kFallDrift &&
      (p.state == PlayerState::Falling || p.state == PlayerState::Jumping || p.state == PlayerState::OnGround) &&
      p.y >= p.prevY)
  {
    s.mounted = true;
    s.drift = off;
    s.carry = s.carried = 0.0f;
    p.y = b.y - 1;
    p.state = PlayerState::OnGround;
    mLaunch = 0;
    playSound(Sfx::Land);
    return;
  }
  if (p.state == PlayerState::OnGround || p.state == PlayerState::Recovering)
  {
    if (s.exitX > 0 && p.x >= s.exitX)
    {
      s.free = true; // on the exit ledge: walk to the gate
      return;
    }
    startFall();
    return;
  }
  if (p.y > mMap.height() - 6)
    startFall();
}

} // namespace gr
