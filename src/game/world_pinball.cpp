// Pinball Mine (SPEC 11, the bonus level, rules=pinball): the runner is a
// one-block ball on a mine-themed pinball table. Jump works the left
// flipper, fire the right one; jump or fire launches the ball from the
// plunger. Bumpers kick, the six lanterns light when hit, and with all six
// lit the gate at the top opens: the way out. The drain sends the ball back
// to the plunger with nothing lost.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kBallR = 1.0f;     // cells
constexpr float kGravity = 0.125f; // cells a frame, every frame
constexpr float kMaxSpeed = 4.0f;
constexpr float kLaunch = 4.0f;
constexpr float kKick = 3.6f;      // a flipper's tip, cells a frame
constexpr float kBumperKick = 2.0f;
constexpr float kRestAngle = 0.52f; // flippers at rest point 30 degrees down
constexpr float kUpAngle = -0.42f;  // and swing up to 24 degrees above level
constexpr int kSubsteps = 8;
constexpr int kMaxPull = 15;

// The flipper's angle (radians, screen y down) for its step.
float flipAngle(int step)
{
  const float t = float(step) / float(Pinball::kFlipSteps);
  return kRestAngle + (kUpAngle - kRestAngle) * t;
}

struct Hit
{
  float nx = 0.0f, ny = 0.0f, depth = 0.0f;
};

// Ball against a segment: the push out and its normal.
bool hitSegment(float bx, float by, float x0, float y0, float x1, float y1, float r, Hit& h)
{
  const float dx = x1 - x0, dy = y1 - y0;
  const float len2 = std::max(1e-6f, dx * dx + dy * dy);
  const float t = std::clamp(((bx - x0) * dx + (by - y0) * dy) / len2, 0.0f, 1.0f);
  const float qx = x0 + dx * t, qy = y0 + dy * t;
  const float ex = bx - qx, ey = by - qy;
  const float d = std::sqrt(ex * ex + ey * ey);
  if (d >= r || d < 1e-5f)
    return false;
  h.nx = ex / d;
  h.ny = ey / d;
  h.depth = r - d;
  return true;
}

} // namespace

void World::setupPinball()
{
  auto& pin = mPin;
  pin.inPlunger = true;
  pin.x = pin.prevX = pin.plungerX;
  pin.y = pin.prevY = pin.plungerY - kBallR;
  auto& p = mPlayer;
  p.hidden = true;
  p.state = PlayerState::OnGround;
  p.x = int(pin.x) - 1;
  p.y = int(pin.y) + 1;
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
}

void World::updatePinball(const PlayerInput& input)
{
  auto& pin = mPin;
  auto& p = mPlayer;
  pin.prevX = pin.x;
  pin.prevY = pin.y;
  p.prevX = p.x;
  p.prevY = p.y;
  for (auto& b : pin.bumpers)
    if (b.flash > 0)
      --b.flash;
  for (auto& l : pin.lamps)
    if (l.flash > 0)
      --l.flash;

  // Flippers: one step a frame toward up while held, back down when let go.
  const int prevL = pin.flipL, prevR = pin.flipR;
  pin.holdL = input.jump.pressed ? pin.holdL + 1 : 0;
  pin.holdR = input.fire.pressed ? pin.holdR + 1 : 0;
  pin.flipL = std::clamp(pin.flipL + (input.jump.pressed ? 1 : -1), 0, Pinball::kFlipSteps);
  pin.flipR = std::clamp(pin.flipR + (input.fire.pressed ? 1 : -1), 0, Pinball::kFlipSteps);
  if ((pin.flipL > prevL && prevL == 0) || (pin.flipR > prevR && prevR == 0))
    playSound(Sfx::Click);

  if (pin.inPlunger)
  {
    pin.x = pin.plungerX;
    pin.y = pin.plungerY - kBallR;
    pin.vx = pin.vy = 0.0f;
    // Pull back with jump or fire and let go: the longer the pull, the
    // harder the launch. Left alone it goes by itself after a while, at
    // whatever strength the spring happens to have.
    const bool held = input.jump.pressed || input.fire.pressed;
    if (held)
      pin.pull = std::min(pin.pull + 1, kMaxPull);
    int pull = -1;
    if (!held && pin.pull > 0)
      pull = pin.pull;
    else if (!held && ++pin.plungeWait >= 45)
      pull = 4 + (mStats.frames * 7) % (kMaxPull - 3);
    if (pull >= 0)
    {
      pin.inPlunger = false;
      pin.plungeWait = 0;
      pin.pull = 0;
      pin.vy = -kLaunch * (0.6f + 0.4f * float(pull) / float(kMaxPull));
      playSound(Sfx::Boing);
    }
  }
  else
  {
    pin.vy += kGravity;
    const float lAng0 = flipAngle(prevL), lAng1 = flipAngle(pin.flipL);
    const float rAng0 = flipAngle(prevR), rAng1 = flipAngle(pin.flipR);
    for (int k = 0; k < kSubsteps; ++k)
    {
      const float u = float(k + 1) / float(kSubsteps);
      pin.x += pin.vx / float(kSubsteps);
      pin.y += pin.vy / float(kSubsteps);
      auto bounce = [&](const Hit& h, float e) {
        pin.x += h.nx * h.depth;
        pin.y += h.ny * h.depth;
        const float vn = pin.vx * h.nx + pin.vy * h.ny;
        if (vn < 0.0f)
        {
          pin.vx -= (1.0f + e) * vn * h.nx;
          pin.vy -= (1.0f + e) * vn * h.ny;
        }
      };
      // Rails and guides.
      Hit h;
      for (const auto& s : pin.segs)
        if (hitSegment(pin.x, pin.y, s.x0, s.y0, s.x1, s.y1, kBallR, h))
          bounce(h, 0.5f);
      // The rock: every solid cell around the ball.
      for (int cy = int(std::floor(pin.y - kBallR)) - 1; cy <= int(std::floor(pin.y + kBallR)) + 1; ++cy)
        for (int cx = int(std::floor(pin.x - kBallR)) - 1; cx <= int(std::floor(pin.x + kBallR)) + 1; ++cx)
        {
          if (!mMap.solid(cx, cy))
            continue;
          const float qx = std::clamp(pin.x, float(cx), float(cx + 1)), qy = std::clamp(pin.y, float(cy), float(cy + 1));
          const float ex = pin.x - qx, ey = pin.y - qy;
          const float d = std::sqrt(ex * ex + ey * ey);
          if (d >= kBallR || d < 1e-5f)
            continue;
          h.nx = ex / d;
          h.ny = ey / d;
          h.depth = kBallR - d;
          bounce(h, 0.45f);
        }
      // Flippers: a swing up kicks the ball off, harder toward the tip.
      for (int side = 0; side < 2; ++side)
      {
        const float px = side == 0 ? pin.lx : pin.rx, py = side == 0 ? pin.ly : pin.ry;
        const float a0 = side == 0 ? lAng0 : rAng0, a1 = side == 0 ? lAng1 : rAng1;
        const float a = a0 + (a1 - a0) * u;
        const float dir = side == 0 ? 1.0f : -1.0f;
        const float tx = px + dir * std::cos(a) * pin.flipLen, ty = py + std::sin(a) * pin.flipLen;
        if (!hitSegment(pin.x, pin.y, px, py, tx, ty, kBallR + 0.5f, h))
          continue;
        bounce(h, 0.3f);
        if (a1 < a0 && h.ny < 0.0f)
        {
          const float along = std::clamp(std::hypot(pin.x - px, pin.y - py) / pin.flipLen, 0.25f, 1.0f);
          pin.vx = h.nx * kKick * along;
          pin.vy = std::min(pin.vy, h.ny * kKick * along - 0.6f);
        }
      }
      // Bumpers.
      for (auto& b : pin.bumpers)
      {
        const float ex = pin.x - b.x, ey = pin.y - b.y;
        const float d = std::sqrt(ex * ex + ey * ey);
        if (d >= b.r + kBallR || d < 1e-5f)
          continue;
        pin.x = b.x + ex / d * (b.r + kBallR);
        pin.y = b.y + ey / d * (b.r + kBallR);
        pin.vx = ex / d * kBumperKick + pin.vx * 0.3f;
        pin.vy = ey / d * kBumperKick + pin.vy * 0.3f;
        if (b.flash == 0)
        {
          b.flash = 6;
          addScore(500, {b.x * float(kCellSize), b.y * float(kCellSize)});
          playSound(Sfx::Boing);
        }
      }
    }
    const float sp = std::hypot(pin.vx, pin.vy);
    if (sp > kMaxSpeed)
    {
      pin.vx *= kMaxSpeed / sp;
      pin.vy *= kMaxSpeed / sp;
    }
    // Lanterns light up when the ball passes.
    int lit = 0;
    for (auto& l : pin.lamps)
    {
      if (!l.lit && std::hypot(pin.x - l.x, pin.y - l.y) < kBallR + 1.6f)
      {
        l.lit = true;
        l.flash = 12;
        addScore(1000, {l.x * float(kCellSize), l.y * float(kCellSize)});
        playSound(Sfx::Chime);
        flashAt({l.x * float(kCellSize), l.y * float(kCellSize)}, 70.0f, rgb(255, 200, 80), 14);
      }
      lit += l.lit;
    }
    if (!pin.gateOpen && lit == int(pin.lamps.size()) && !pin.lamps.empty())
    {
      pin.gateOpen = true;
      showMessage("ALL SIX LANTERNS LIT - THE GATE IS OPEN");
      playSound(Sfx::LettersComplete);
    }
    // Through the open gate: out.
    if (pin.gateOpen && std::hypot(pin.x - pin.gateX, pin.y - pin.gateY) < 3.0f)
    {
      playSound(Sfx::Teleport);
      mState = WorldState::Exiting;
      mStateFrames = 0;
    }
    // Down the drain: back to the plunger.
    if (pin.drain.w > 0 && pin.drain.intersects({int(std::floor(pin.x)), int(std::floor(pin.y)), 1, 1}))
    {
      ++pin.drains;
      pin.inPlunger = true;
      pin.plungeWait = 0;
      playSound(Sfx::Bloop);
      showMessage("DRAINED - BACK TO THE PLUNGER");
    }
    // Back down the plunger lane and settled on the plunger: ready to go again.
    if (std::hypot(pin.x - pin.plungerX, pin.y - (pin.plungerY - kBallR)) < 1.5f && std::hypot(pin.vx, pin.vy) < 0.6f)
    {
      pin.inPlunger = true;
      pin.plungeWait = 0;
    }
    if (pin.y > float(mMap.height() + 2) || pin.x < -2.0f || pin.x > float(mMap.width() + 2))
    {
      pin.inPlunger = true; // out of the table somehow: back in play
      pin.plungeWait = 0;
    }
  }
  // The runner (for the camera and the gems) goes where the ball is.
  p.hidden = true;
  p.state = PlayerState::OnGround;
  p.x = int(std::floor(pin.x - 1.5f));
  p.y = int(std::floor(pin.y)) + 1;
}

} // namespace gr
