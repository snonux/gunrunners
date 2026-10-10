// Planetoids (SPEC 18's bonus level, rules=radial_gravity): six little
// worlds, each pulling the runner toward its centre within 4 blocks of its
// surface. Walking follows the surface all the way round, a jump launches
// along the surface normal as high as a normal jump, and between the fields
// the runner drifts in a straight line. Five parts of the alien's ship lie
// about; goal=collect:N counts them.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kGravity = 0.12f;   // cells a frame, a frame, toward the centre
constexpr float kReach = 8.0f;      // cells beyond the surface a field pulls
constexpr float kMaxSpeed = 1.6f;   // cells a frame
constexpr int kLost = 120;          // frames adrift outside every field before the runner is brought back
constexpr float kPartReach = 2.5f;  // cells from the runner's middle
constexpr float kOrbitDrag = 0.95f; // a frame, on the drift along a field's surface
constexpr int kMaxAir = 450;        // frames in the air before the runner is brought back anyway

} // namespace

bool World::setupOrbitEntity(const EntityDef& e)
{
  auto& o = mOrbit;
  if (e.kind == "planetoid" && e.hasPos)
  {
    Planetoid pl;
    pl.cx = float(e.x * kCellsPerTile + 1);
    pl.cy = float(e.y * kCellsPerTile + 1);
    pl.r = float(e.num("r", 2) * kCellsPerTile);
    pl.look = int(o.planets.size()) % 3;
    o.planets.push_back(pl);
    return true;
  }
  if (e.kind == "part" && e.hasPos)
  {
    OrbitPart pt;
    pt.x = float(e.x * kCellsPerTile + 1);
    pt.y = float(e.y * kCellsPerTile + 1);
    pt.kind = int(o.parts.size()) % 5;
    o.parts.push_back(pt);
    return true;
  }
  return false;
}

void World::linkOrbit()
{
  auto& o = mOrbit;
  o.on = mLevel->rules.find("radial_gravity") != std::string::npos && !o.planets.empty();
  if (!o.on)
    return;
  // The runner starts standing on the planetoid nearest the start, on the
  // side facing it.
  const auto& p = mPlayer;
  const float sx = float(p.x) + 1.5f, sy = float(p.y);
  float best = 1e9f;
  for (std::size_t i = 0; i < o.planets.size(); ++i)
  {
    const float dx = sx - o.planets[i].cx, dy = sy - o.planets[i].cy;
    const float d = std::sqrt(dx * dx + dy * dy) - o.planets[i].r;
    if (d < best)
    {
      best = d;
      o.planet = int(i);
      o.ang = std::atan2(dx, -dy);
    }
  }
  o.lastPlanet = o.planet;
  orbitPlace();
}

// Puts the runner's feet on the surface at the current angle and keeps the
// cell position (the camera, pick-ups) in step.
void World::orbitPlace()
{
  auto& o = mOrbit;
  auto& p = mPlayer;
  if (o.planet >= 0)
  {
    const auto& pl = o.planets[std::size_t(o.planet)];
    o.px = pl.cx + std::sin(o.ang) * pl.r;
    o.py = pl.cy - std::cos(o.ang) * pl.r;
  }
  // The runner's middle, 2.5 cells "up" from the feet, sits in the cell box.
  const float mx = o.px + std::sin(o.ang) * 2.5f, my = o.py - std::cos(o.ang) * 2.5f;
  p.x = int(std::lround(mx - 1.5f));
  p.y = int(std::lround(my + 2.0f));
}

void World::updateOrbit(const PlayerInput& input)
{
  auto& o = mOrbit;
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const int mv = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  if (mv != 0)
    p.facing = mv;
  if (o.planet >= 0)
  {
    // Walking round the surface: a cell a frame, clockwise to the runner's right.
    const auto& pl = o.planets[std::size_t(o.planet)];
    p.state = PlayerState::OnGround;
    if (mv != 0)
    {
      o.ang += float(mv * horizontalSteps()) / pl.r;
      ++p.walkFrame;
      setVisual(PlayerVisual::Walking);
    }
    else
    {
      setVisual(PlayerVisual::Standing);
    }
    if (o.ang > 3.14159265f)
      o.ang -= 6.2831853f;
    else if (o.ang < -3.14159265f)
      o.ang += 6.2831853f;
    orbitPlace();
    if (input.jump.triggered)
    {
      // Off along the normal, as high as a normal jump (v^2 = 2gh), carrying
      // the walk along the surface.
      int h = 0;
      for (int v : jumpArc())
        h += v;
      const float v0 = std::sqrt(2.0f * kGravity * float(h));
      const float nx = std::sin(o.ang), ny = -std::cos(o.ang);
      const float tx = std::cos(o.ang), ty = std::sin(o.ang); // the runner's right
      const float walk = mv != 0 ? float(mv) * 0.6f : 0.0f;
      o.vx = nx * v0 + tx * walk;
      o.vy = ny * v0 + ty * walk;
      o.lastPlanet = o.planet;
      o.planet = -1;
      o.air = 0;
      o.lost = 0;
      p.state = PlayerState::Jumping;
      setVisual(PlayerVisual::Jumping);
      playSound(Sfx::Jump);
    }
  }
  else
  {
    // Adrift: pulled by every field the runner is in, else a straight line.
    ++o.air;
    bool inField = false;
    for (const auto& pl : o.planets)
    {
      const float dx = pl.cx - o.px, dy = pl.cy - o.py;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d > pl.r + kReach || d < 0.01f)
        continue;
      inField = true;
      // Full strength near the surface, fading out over the field's outer
      // half: a jump (7 cells) just gets away, slowly.
      const float out = d - pl.r;
      const float g = out <= kReach * 0.5f ? kGravity : kGravity * (kReach - out) / (kReach * 0.5f);
      o.vx += dx / d * g;
      o.vy += dy / d * g;
    }
    // Within a field the sideways drift dies away (a damped orbit), so a
    // runner always comes down somewhere.
    int nearest = -1;
    float nearestD = 1e9f;
    for (std::size_t i = 0; i < o.planets.size(); ++i)
    {
      const auto& pl = o.planets[i];
      const float d = std::hypot(o.px - pl.cx, o.py - pl.cy) - pl.r;
      if (d <= kReach && d < nearestD)
      {
        nearestD = d;
        nearest = int(i);
      }
    }
    if (nearest >= 0)
    {
      const auto& pl = o.planets[std::size_t(nearest)];
      const float dx = o.px - pl.cx, dy = o.py - pl.cy;
      const float d = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
      const float nx = dx / d, ny = dy / d;
      const float radial = o.vx * nx + o.vy * ny;
      const float tx = o.vx - radial * nx, ty = o.vy - radial * ny;
      o.vx = radial * nx + tx * kOrbitDrag;
      o.vy = radial * ny + ty * kOrbitDrag;
    }
    // A little steering, as in any jump.
    if (mv != 0)
    {
      o.vx += std::cos(o.ang) * 0.03f * float(mv);
      o.vy += std::sin(o.ang) * 0.03f * float(mv);
    }
    const float sp = std::sqrt(o.vx * o.vx + o.vy * o.vy);
    if (sp > kMaxSpeed)
    {
      o.vx *= kMaxSpeed / sp;
      o.vy *= kMaxSpeed / sp;
    }
    o.px += o.vx;
    o.py += o.vy;
    // The body turns its feet toward the pull (or keeps its tilt adrift).
    for (std::size_t i = 0; i < o.planets.size(); ++i)
    {
      const auto& pl = o.planets[i];
      const float dx = o.px - pl.cx, dy = o.py - pl.cy;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d <= pl.r + kReach)
      {
        const float want = std::atan2(dx, -dy);
        float da = want - o.ang;
        while (da > 3.14159265f)
          da -= 6.2831853f;
        while (da < -3.14159265f)
          da += 6.2831853f;
        o.ang += da * 0.2f;
        if (o.ang > 3.14159265f)
          o.ang -= 6.2831853f;
        else if (o.ang < -3.14159265f)
          o.ang += 6.2831853f;
      }
      // Touchdown: falling onto the surface.
      if (d <= pl.r && (o.vx * dx + o.vy * dy) < 0.0f)
      {
        o.planet = int(i);
        o.ang = std::atan2(dx, -dy);
        o.vx = o.vy = 0.0f;
        o.lastPlanet = o.planet;
        p.state = PlayerState::OnGround;
        setVisual(PlayerVisual::Standing);
        playSound(Sfx::Land);
        break;
      }
    }
    orbitPlace();
    // Lost in space: back to the last planetoid.
    const float W = float(mLevel->width * kCellsPerTile), H = float(mLevel->height * kCellsPerTile);
    o.lost = inField ? 0 : o.lost + 1;
    if (o.planet < 0 && (o.lost >= kLost || o.air >= kMaxAir || o.px < -12.0f || o.py < -12.0f || o.px > W + 12.0f || o.py > H + 12.0f))
    {
      o.planet = o.lastPlanet;
      o.ang = 0.0f;
      o.vx = o.vy = 0.0f;
      p.state = PlayerState::OnGround;
      orbitPlace();
      playSound(Sfx::Teleport);
      showMessage("LOST IN SPACE - BEAMED BACK");
    }
  }
  // The ship's parts.
  const float mx = o.px + std::sin(o.ang) * 2.5f, my = o.py - std::cos(o.ang) * 2.5f;
  for (auto& pt : o.parts)
  {
    if (pt.flash > 0)
      --pt.flash;
    if (pt.taken)
      continue;
    const float dx = pt.x - mx, dy = pt.y - my;
    if (dx * dx + dy * dy <= kPartReach * kPartReach * 2.0f)
    {
      pt.taken = true;
      pt.flash = 20;
      ++o.partsTaken;
      addScore(1000, {pt.x * kCellSize, pt.y * kCellSize});
      playSound(Sfx::Item);
      showMessage("SHIP PART " + std::to_string(o.partsTaken) + " OF " + std::to_string(o.parts.size()));
    }
  }
}

} // namespace gr
