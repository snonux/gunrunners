// Level 24's bonus, Turntable (rules=turntable): a microwave interior that
// makes one full turn in 60 seconds about its centre while gravity keeps
// pointing down the screen. The runner and the loose gems slide along
// whatever is "down" at the time; a grille in the middle of each wall lets
// gems out (lost) but not the runner.
//
// Physics runs in the room's own frame, in cells: there gravity turns the
// other way round. The runner is a circle about its middle, the gems small
// circles; the walls are the level map's solid blocks, copied out at load
// (the map itself is cleared, so nothing else collides with them unturned).

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTurnRate = 0.4f * 0.0174533f; // radians a frame
constexpr float kTurnGravity = 0.15f;          // cells a frame, a frame
constexpr float kTurnMaxSpeed = 1.6f;
constexpr float kRunnerR = 2.0f;               // cells, about the runner's middle
constexpr float kGemR = 0.8f;
constexpr float kWalk = 0.8f;                  // cells a frame, along the screen
constexpr float kReach = 2.6f;                 // runner's middle to a gem's centre

} // namespace

void World::linkTurntable()
{
  auto& t = mTurn;
  t.on = mLevel->rules.find("turntable") != std::string::npos;
  if (!t.on)
    return;
  t.w = mLevel->width;
  t.h = mLevel->height;
  t.solid.assign(std::size_t(t.w * t.h), 0);
  for (int by = 0; by < t.h; ++by)
    for (int bx = 0; bx < t.w; ++bx)
      if (mMap.block(bx, by) == Tile::Solid)
      {
        bool grille = false;
        for (const auto& g : t.grilles)
          grille = grille || (bx >= g.x0 && bx <= g.x1 && by >= g.y0 && by <= g.y1);
        t.solid[std::size_t(by * t.w + bx)] = grille ? 2 : 1;
        mMap.setBlock(bx, by, Tile::Empty);
      }
  t.x0 = t.y0 = 1e9f;
  t.x1 = t.y1 = -1e9f;
  for (int by = 0; by < t.h; ++by)
    for (int bx = 0; bx < t.w; ++bx)
      if (t.solid[std::size_t(by * t.w + bx)] != 0)
      {
        t.x0 = std::min(t.x0, float(bx * kCellsPerTile));
        t.y0 = std::min(t.y0, float(by * kCellsPerTile));
        t.x1 = std::max(t.x1, float((bx + 1) * kCellsPerTile));
        t.y1 = std::max(t.y1, float((by + 1) * kCellsPerTile));
      }
  t.cx = (t.x0 + t.x1) / 2.0f; // the room's middle
  t.cy = (t.y0 + t.y1) / 2.0f;
  t.ang = t.prevAng = 0.0f;
  // The gems leave the item list: they slide about in the room's frame.
  for (auto& it : mItems)
    if (it.kind == ItemKind::Gem && !it.taken)
    {
      TurnGem g;
      g.x = float(it.x) + 1.0f;
      g.y = float(it.y) + 1.0f - kGemR;
      g.variant = it.variant;
      t.gems.push_back(g);
      it.taken = true;
    }
  mItems.erase(std::remove_if(mItems.begin(), mItems.end(), [](const Item& i) { return i.taken; }), mItems.end());
  const auto& p = mPlayer;
  t.px = float(p.x) + 1.5f;
  t.py = float(p.y) + 1.0f - kRunnerR;
  t.sx = t.prevSx = t.px;
  t.sy = t.prevSy = t.py;
}

bool World::turnSolid(int bx, int by, bool gem) const
{
  const auto& t = mTurn;
  if (bx < 0 || by < 0 || bx >= t.w || by >= t.h)
    return false;
  const unsigned char s = t.solid[std::size_t(by * t.w + bx)];
  return s == 1 || (s == 2 && !gem);
}

// Pushes a circle out of the walls it overlaps and takes the speed into
// them away. `ground` (if given) is set when one of them is under it.
void World::turnCollide(float& x, float& y, float& vx, float& vy, float r, bool gem, bool* ground) const
{
  const auto& t = mTurn;
  // "Up" on screen, in the room's frame.
  const float ux = -std::sin(t.ang), uy = -std::cos(t.ang);
  for (int pass = 0; pass < 2; ++pass)
  {
    const int bx0 = int(std::floor((x - r) / float(kCellsPerTile))), bx1 = int(std::floor((x + r) / float(kCellsPerTile)));
    const int by0 = int(std::floor((y - r) / float(kCellsPerTile))), by1 = int(std::floor((y + r) / float(kCellsPerTile)));
    for (int by = by0; by <= by1; ++by)
      for (int bx = bx0; bx <= bx1; ++bx)
      {
        if (!turnSolid(bx, by, gem))
          continue;
        const float x0 = float(bx * kCellsPerTile), y0 = float(by * kCellsPerTile);
        const float x1 = x0 + float(kCellsPerTile), y1 = y0 + float(kCellsPerTile);
        const float qx = std::clamp(x, x0, x1), qy = std::clamp(y, y0, y1);
        float dx = x - qx, dy = y - qy;
        float d = std::sqrt(dx * dx + dy * dy);
        float push = r - d;
        if (d < 1e-4f)
        {
          // The centre is inside the block: out the nearest side.
          const float l = x - x0, rr = x1 - x, u = y - y0, dn = y1 - y;
          const float m = std::min({l, rr, u, dn});
          dx = m == l ? -1.0f : (m == rr ? 1.0f : 0.0f);
          dy = m == u ? -1.0f : (m == dn ? 1.0f : 0.0f);
          if (dx != 0.0f && dy != 0.0f)
            dy = 0.0f;
          d = 1.0f;
          push = m + r;
        }
        if (push <= 0.0f)
          continue;
        const float nx = dx / d, ny = dy / d;
        x += nx * push;
        y += ny * push;
        const float vn = vx * nx + vy * ny;
        if (vn < 0.0f)
        {
          vx -= vn * nx;
          vy -= vn * ny;
        }
        if (ground && nx * ux + ny * uy > 0.5f)
          *ground = true;
      }
  }
}

void World::updateTurntable(const PlayerInput& input)
{
  auto& t = mTurn;
  auto& p = mPlayer;
  t.prevAng = t.ang;
  t.ang += kTurnRate;
  if (t.ang > 6.2831853f)
    t.ang -= 6.2831853f;
  const float c = std::cos(t.ang), s = std::sin(t.ang);
  // Screen right and screen down, in the room's frame.
  const float ex = c, ey = -s, dx = s, dy = c;

  // The runner.
  const int mv = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  if (mv != 0)
    p.facing = mv;
  t.vx += dx * kTurnGravity;
  t.vy += dy * kTurnGravity;
  const float h = t.vx * ex + t.vy * ey;
  if (t.ground)
  {
    const float want = float(mv) * kWalk;
    const float k = mv != 0 ? 1.0f : 0.4f; // a little give when let go
    t.vx += ex * (want - h) * k;
    t.vy += ey * (want - h) * k;
    if (input.jump.triggered)
    {
      int hgt = 0;
      for (int v : jumpArc())
        hgt += v;
      const float v0 = std::sqrt(2.0f * kTurnGravity * float(hgt));
      const float down = t.vx * dx + t.vy * dy;
      t.vx -= dx * (down + v0);
      t.vy -= dy * (down + v0);
      t.ground = false;
      playSound(Sfx::Jump);
    }
  }
  else if (mv != 0)
  {
    const float want = std::clamp(h + float(mv) * 0.12f, -kWalk, kWalk);
    t.vx += ex * (want - h);
    t.vy += ey * (want - h);
  }
  float sp = std::hypot(t.vx, t.vy);
  if (sp > kTurnMaxSpeed)
  {
    t.vx *= kTurnMaxSpeed / sp;
    t.vy *= kTurnMaxSpeed / sp;
    sp = kTurnMaxSpeed;
  }
  const int steps = std::max(1, int(std::ceil(sp / 0.5f)));
  bool ground = false;
  for (int i = 0; i < steps; ++i)
  {
    t.px += t.vx / float(steps);
    t.py += t.vy / float(steps);
    turnCollide(t.px, t.py, t.vx, t.vy, kRunnerR, false, &ground);
  }
  if (ground && !t.ground)
    playSound(Sfx::Land);
  t.ground = ground;
  p.state = ground ? PlayerState::OnGround : PlayerState::Jumping;
  if (!ground)
    setVisual(PlayerVisual::Jumping);
  else if (mv != 0)
  {
    ++p.walkFrame;
    setVisual(PlayerVisual::Walking);
  }
  else
    setVisual(PlayerVisual::Standing);
  // Where that is on screen (the camera and the sprite follow it).
  t.prevSx = t.sx;
  t.prevSy = t.sy;
  t.sx = t.cx + c * (t.px - t.cx) - s * (t.py - t.cy);
  t.sy = t.cy + s * (t.px - t.cx) + c * (t.py - t.cy);
  p.prevX = p.x;
  p.prevY = p.y;
  p.x = int(std::lround(t.sx - 1.5f));
  p.y = int(std::lround(t.sy + kRunnerR - 1.0f));

  // The gems: they slide; out through a grille they're lost.
  int left = 0;
  for (auto& g : t.gems)
  {
    if (g.gone)
      continue;
    g.vx += dx * kTurnGravity;
    g.vy += dy * kTurnGravity;
    const float gs = std::hypot(g.vx, g.vy);
    if (gs > kTurnMaxSpeed)
    {
      g.vx *= kTurnMaxSpeed / gs;
      g.vy *= kTurnMaxSpeed / gs;
    }
    const int gsteps = std::max(1, int(std::ceil(std::min(gs, kTurnMaxSpeed) / 0.5f)));
    bool rest = false;
    for (int i = 0; i < gsteps; ++i)
    {
      g.x += g.vx / float(gsteps);
      g.y += g.vy / float(gsteps);
      turnCollide(g.x, g.y, g.vx, g.vy, kGemR, true, &rest);
    }
    if (rest)
    {
      g.vx *= 0.97f; // a little friction on the floor
      g.vy *= 0.97f;
    }
    if (g.x < t.x0 - 1.0f || g.y < t.y0 - 1.0f || g.x > t.x1 + 1.0f || g.y > t.y1 + 1.0f)
    {
      g.gone = true;
      ++t.lost;
      continue;
    }
    if (p.state != PlayerState::Dying && std::hypot(g.x - t.px, g.y - t.py) <= kReach)
    {
      g.gone = true;
      Item it;
      it.kind = ItemKind::Gem;
      it.variant = g.variant;
      it.x = int(std::lround(t.cx + c * (g.x - t.cx) - s * (g.y - t.cy))) - 1;
      it.y = int(std::lround(t.cy + s * (g.x - t.cx) + c * (g.y - t.cy)));
      collectItem(it);
      continue;
    }
    ++left;
  }
  // Every gem taken or lost: the bonus is over.
  if (left == 0 && !t.gems.empty() && mState == WorldState::Playing)
  {
    const auto& goal = mLevel->goal;
    if (goal.rfind("collect:", 0) != 0 || mStats.gems < std::atoi(goal.c_str() + 8))
    {
      mBonusFailed = true;
      mState = WorldState::Done;
      mStateFrames = 0;
      showMessage(t.lost > 0 ? "THE REST WENT DOWN THE GRILLES" : "ALL GEMS TAKEN");
    }
  }
}

} // namespace gr
