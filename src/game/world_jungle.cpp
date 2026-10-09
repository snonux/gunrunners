// Level 8, Canopy Road (SPEC 08): Swing Vines. A runner whose hands meet
// the lower three blocks of a vine grabs it; pushing along the swing pumps
// it higher, and letting go at the end of a high swing launches them up and
// across. Rope bridges snap under a heavy runner, a while after you leave
// them, or at a Bridge Cutter's third chop. Ropes over the ravine hold logs
// (shoot the rope and the log drops across as a bridge) and a cage of gems.
// Howlers lob fruit that rolls, Canopy Vipers drop from the branches, and
// the level's prototype is the Boomerang: it flies out, turns and comes
// back, cutting every rope it touches. Also the Bounce House bonus
// (rules=bounce), where every surface is a trampoline.

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

constexpr Color kInk = rgb(10, 8, 20);
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr float kTilePx = float(kTileSize) * kPixelScale;
constexpr float kRad = 3.14159265f / 180.0f;
constexpr int kPumpStep = 5;     // degrees a pumped half-swing adds (or an idle one loses)
constexpr int kMaxAmp = 60;
constexpr int kMinAmp = 15;      // vines always sway
constexpr int kLaunchAmp = 45;   // degrees to launch from
constexpr int kLaunchWindow = 4; // frames either side of a turnaround
constexpr int kYellHalves = 20;  // ten full swings without letting go
constexpr int kGrabReach = 6;    // cells: the lower three blocks of a vine
constexpr int kRegrab = 8;       // frames before a vine you let go of can be grabbed again
constexpr int kCreak = 10;       // frames a bridge creaks before it drops
constexpr int kBridgeDrop = 8;   // frames the planks take to fall away
constexpr int kLogFall = 10;     // frames a cut log takes to land
constexpr int kRollFrames = 60;  // frames a fruit rolls at most
constexpr int kStrike = 6;       // frames a hanging Viper's strike reaches out
constexpr int kViperRetract = 30;
constexpr int kCutterReach = 20; // cells from the near end of its bridge
constexpr int kChopAt[3] = {10, 40, 70};
constexpr int kBoomerangOut = 16; // steps out: 8 frames at speed 2
constexpr int kBounceMin = 4, kBounceMax = 18, kBounceStart = 8;
constexpr int kKick = 6; // frames a wall bounces you back in the Bounce House

int sgn(int v) { return (v > 0) - (v < 0); }

float vineAngleAt(const Vine& v, float t) { return float(v.amp) * std::cos(6.2831853f * t / float(v.period)); }

// Does the line from (x0, y0) to (x1, y1) (cells, as points) cross box b?
bool segmentHits(float x0, float y0, float x1, float y1, const CellBox& b)
{
  const int n = std::max(1, int(std::max(std::abs(x1 - x0), std::abs(y1 - y0)) * 4.0f));
  for (int i = 0; i <= n; ++i)
  {
    const float t = float(i) / float(n);
    const float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
    if (x >= float(b.x) && x < float(b.x + b.w) && y >= float(b.y) && y < float(b.y + b.h))
      return true;
  }
  return false;
}

// Where a load's rope meets it (cells, as a point).
void loadTop(const Load& l, float& x, float& y)
{
  x = float(l.x);
  y = float(l.state == 1 && l.cage ? l.fy : l.y);
}

// A log's box while it hangs or falls (cells).
CellBox logBox(const Load& l)
{
  if (l.state >= 2)
    return {l.landX0 * kCellsPerTile, l.landRow * kCellsPerTile, (l.landX1 - l.landX0 + 1) * kCellsPerTile, 2};
  const float t = float(l.fall) / float(kLogFall);
  const float cx = float(l.x) + (float(l.landX0 + l.landX1 + 1) - float(l.x)) * t;
  const float top = float(l.y) + (float(l.landRow * kCellsPerTile) - float(l.y)) * t;
  return {int(std::lround(cx)) - l.len, int(std::lround(top)), l.len * 2, 2};
}

CellBox cageBox(const Load& l) { return {l.x - 2, l.state == 1 ? l.fy : l.y, 4, 4}; }

} // namespace

float Vine::angle() const { return vineAngleAt(*this, float(t)); }

void Vine::point(int at, float& x, float& y) const
{
  const float a = angle() * kRad;
  x = float(ax) + float(at) * std::sin(a);
  y = float(ay) + float(at) * std::cos(a);
}

bool JungleRope::hits(const CellBox& b) const
{
  return segmentHits(float(x0), float(y0), float(x1), float(y1), b);
}

// --- Level entities ----------------------------------------------------------------

bool World::setupJungleEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "vine" && e.hasPos)
  {
    Vine v;
    v.id = e.id;
    v.ax = e.x * kCellsPerTile + 1;
    v.ay = e.y * kCellsPerTile + 1;
    const int len = std::max(2, e.num("len", 7));
    v.len = len * kCellsPerTile;
    v.period = 12 + 2 * len;
    v.rest = v.amp = std::clamp(e.num("amp", 20), kMinAmp, kMaxAmp);
    v.t = (v.ax * 3) % v.period;
    mVines.push_back(v);
    return true;
  }
  if (e.kind == "bridge" && hasRect)
  {
    Bridge b;
    b.id = e.id;
    b.x0 = x0;
    b.x1 = x1;
    b.y = y0;
    b.snap = e.num("snap", 0);
    b.after = e.num("after", 0);
    b.cut = e.num("cut", 0);
    for (int tx = x0; tx <= x1; ++tx)
    {
      mMap.setBlock(tx, y0, Tile::Platform);
      if (tx >= 0 && y0 >= 0 && tx < mLevel->width && y0 < mLevel->height)
        mLayerMask[std::size_t(y0 * mLevel->width + tx)] = 1; // drawn as planks (drawJungleBack)
    }
    mBridges.push_back(b);
    return true;
  }
  if ((e.kind == "log" || e.kind == "cage") && e.hasPos)
  {
    Load l;
    l.id = e.id;
    JungleRope rope;
    rope.id = e.id;
    rope.hp = std::max(1, e.num("hp", 3));
    if (e.kind == "log")
    {
      // Hangs `hang` blocks under its pulley at (x, y); the rope runs from
      // its tie up over the pulley and down to the log.
      l.len = std::max(1, e.num("len", 6));
      l.x = e.x * kCellsPerTile + 1;
      l.y = (e.y + e.num("hang", 4)) * kCellsPerTile;
      const auto lands = e.list("lands");
      if (lands.size() == 3)
      {
        l.landX0 = lands[0];
        l.landX1 = lands[1];
        l.landRow = lands[2];
      }
      for (int tx = l.landX0; tx <= l.landX1; ++tx)
        if (tx >= 0 && l.landRow >= 0 && tx < mLevel->width && l.landRow < mLevel->height)
          mLayerMask[std::size_t(l.landRow * mLevel->width + tx)] = 1;
      const auto tie = e.list("tie");
      rope.x0 = tie.size() == 2 ? tie[0] * kCellsPerTile + 1 : l.x;
      rope.y0 = tie.size() == 2 ? tie[1] * kCellsPerTile + 1 : e.y * kCellsPerTile + 1;
      rope.x1 = l.x;
      rope.y1 = e.y * kCellsPerTile + 1;
    }
    else
    {
      // A 2 x 2 block cage, top centre at (x, y), on a rope from `rope`.
      l.cage = true;
      l.x = e.x * kCellsPerTile + 1;
      l.y = l.fy = e.y * kCellsPerTile;
      l.gems = e.num("gems", 6);
      mStats.gemsTotal += l.gems;
      const auto from = e.list("rope");
      rope.x0 = rope.x1 = l.x;
      rope.y0 = rope.y1 = from.size() == 2 ? from[1] * kCellsPerTile + 1 : l.y - 6;
    }
    rope.load = int(mLoads.size());
    mLoads.push_back(l);
    mJRopes.push_back(rope);
    return true;
  }
  if (e.kind == "water" && hasRect)
  {
    mWater.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile,
      (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  return false;
}

void World::setupJungleEnemy(Enemy& en, const EntityDef& e)
{
  switch (en.kind)
  {
    case EnemyKind::Howler:
      en.variant = e.str("look") == "dash" ? 1 : 0; // wears a tiny copy of Dash's jacket
      break;
    case EnemyKind::Viper:
      en.aimY = en.y; // coiled on its branch
      en.railX0 = en.x;
      break;
    case EnemyKind::Cutter:
    {
      en.railX0 = en.x;
      en.railX1 = en.y;
      en.attach = -1;
      const std::string bridge = e.str("bridge");
      for (std::size_t i = 0; i < mBridges.size(); ++i)
        if (mBridges[i].id == bridge)
          en.attach = int(i);
      break;
    }
    default:
      break;
  }
}

// --- Queries ------------------------------------------------------------------------

int World::vineLaunchDir() const
{
  const auto& p = mPlayer;
  if (p.state != PlayerState::Swing || p.vine < 0 || std::size_t(p.vine) >= mVines.size())
    return 0;
  const Vine& v = mVines[std::size_t(p.vine)];
  const float a = v.angle();
  if (p.turbo > 0)
    return a > 1.0f ? 1 : (a < -1.0f ? -1 : v.swingDir()); // Turbo: every release is a launch
  const int half = v.period / 2;
  const int d = std::min({v.t, std::abs(v.t - half), v.period - v.t});
  if (v.amp < kLaunchAmp || d > kLaunchWindow)
    return 0;
  return a >= 0.0f ? 1 : -1;
}

bool World::inWater() const
{
  const CellBox feet = boxAt(mPlayer.x, mPlayer.y, Player::kWidth, 1);
  for (const auto& w : mWater)
    if (w.intersects(feet))
      return true;
  return false;
}

// --- Update ----------------------------------------------------------------------------

void World::updateJungle(const PlayerInput& input)
{
  (void)input;
  if (mBridges.empty() && mLoads.empty() && mFruits.empty())
    return;
  updateBridges();
  updateLoads();
  updateFruits();
}

void World::updateVines()
{
  const auto& p = mPlayer;
  for (std::size_t i = 0; i < mVines.size(); ++i)
  {
    Vine& v = mVines[i];
    if (v.cool > 0)
      --v.cool;
    v.t = (v.t + 1) % v.period;
    const int half = v.period / 2;
    if (v.t != 0 && v.t != half)
      continue;
    // A turnaround: pushing along this half-swing raised it, letting it be
    // lowers it back toward its resting swing.
    const bool held = p.state == PlayerState::Swing && p.vine == int(i);
    if (held && v.pump >= std::max(3, half / 3))
      v.amp = std::min(kMaxAmp, v.amp + kPumpStep);
    else
      v.amp = std::max(v.rest, v.amp - kPumpStep);
    v.pump = 0;
    if (!held)
      continue;
    if (++v.halves >= kYellHalves && !v.yelled)
    {
      // Easter egg: ten full swings without letting go.
      v.yelled = true;
      playSound(Sfx::Yell);
      showMessage("AAH-AH-AH-AAAH-AH-AH-AAAH!");
      addScore(4200, cellCenter(mPlayer.box()));
    }
  }
}

bool World::tryGrabVine()
{
  auto& p = mPlayer;
  const float hx = float(p.x) + 1.5f, hy = float(p.y - Player::kHeight + 1);
  for (std::size_t i = 0; i < mVines.size(); ++i)
  {
    Vine& v = mVines[i];
    if (v.cool > 0)
      continue;
    const float a = v.angle() * kRad;
    const float ux = std::sin(a), uy = std::cos(a);
    const float dx = hx - float(v.ax), dy = hy - float(v.ay);
    const float along = dx * ux + dy * uy;
    const float perp = std::abs(dx * uy - dy * ux);
    if (along < float(v.len - kGrabReach) || along > float(v.len) + 1.0f || perp > 2.0f)
      continue;
    const int at = std::clamp(int(std::lround(along)), 2, v.len);
    float px = 0.0f, py = 0.0f;
    v.point(at, px, py);
    const int nx = int(std::lround(px - 1.5f)), ny = int(std::lround(py)) + 5;
    if (mMap.overlapsSolid(boxAt(nx, ny, Player::kWidth, 6)))
      continue;
    p.state = PlayerState::Swing;
    p.vine = int(i);
    p.vineAt = at;
    p.frames = 0;
    p.somersault = -1;
    p.fling = 0;
    p.vineArc = false;
    p.fromLadder = false;
    p.x = nx;
    p.y = ny;
    v.pump = 0;
    v.halves = 0;
    setVisual(PlayerVisual::Hanging);
    playSound(Sfx::AttachClimbable);
    playSound(Sfx::Rustle);
    return true;
  }
  return false;
}

void World::placeOnVine()
{
  auto& p = mPlayer;
  const Vine& v = mVines[std::size_t(p.vine)];
  float hx = 0.0f, hy = 0.0f;
  v.point(p.vineAt, hx, hy);
  const int nx = int(std::lround(hx - 1.5f)), ny = int(std::lround(hy)) + 5;
  if (mMap.overlapsSolid(boxAt(nx, ny, Player::kWidth, 6)))
  {
    letGoOfVine(false, 0); // swung into something solid
    return;
  }
  p.x = nx;
  p.y = ny;
}

void World::updateSwing(int mvX, int mvY, const Button& jump)
{
  auto& p = mPlayer;
  if (p.vine < 0 || std::size_t(p.vine) >= mVines.size())
  {
    p.vine = -1;
    startFallingDelayed();
    return;
  }
  Vine& v = mVines[std::size_t(p.vine)];
  setVisual(PlayerVisual::Hanging);
  if (jump.triggered)
  {
    const int dir = vineLaunchDir();
    letGoOfVine(dir != 0, dir);
    return;
  }
  if (mvY > 0)
  {
    letGoOfVine(false, 0); // down drops off
    return;
  }
  if (mvX != 0)
  {
    p.facing = mvX;
    if (mvX == v.swingDir())
      ++v.pump;
  }
  placeOnVine();
}

void World::letGoOfVine(bool launch, int dir)
{
  auto& p = mPlayer;
  Vine& v = mVines[std::size_t(p.vine)];
  // The hands' speed along the swing right now.
  float x0 = 0.0f, y0 = 0.0f;
  v.point(p.vineAt, x0, y0);
  const float a1 = vineAngleAt(v, float(v.t + 1)) * kRad;
  const float vx = float(v.ax) + float(p.vineAt) * std::sin(a1) - x0;
  v.cool = kRegrab;
  v.halves = 0;
  v.pump = 0;
  p.vine = -1;
  p.y -= 1; // back to a 5-cell box, hands where they were
  p.somersault = -1;
  p.frames = 0;
  if (mMap.overlapsSolid(p.box()))
    ++p.y;
  if (launch)
  {
    // The vine arc: 14 cells up, 2 cells a frame the way you let go.
    p.state = PlayerState::Jumping;
    p.fromLadder = true;
    p.vineArc = true;
    p.fling = 2 * dir;
    p.facing = dir;
    p.jumpRequested = false;
    setVisual(PlayerVisual::Jumping);
    playSound(Sfx::Jump);
    playSound(Sfx::Whoosh);
    burst(cellCenter(p.box()), rgb(120, 200, 80), rgb(220, 255, 160), 10, 1.6f, false);
    return;
  }
  // Let go anywhere else: you keep the speed of the swing, without a lift.
  p.state = PlayerState::Falling;
  p.vineArc = false;
  p.fling = std::abs(vx) < 0.5f ? 0 : std::clamp(int(std::lround(vx)), -2, 2);
  if (p.fling != 0)
    p.facing = sgn(p.fling);
  setVisual(PlayerVisual::Falling);
}

// --- Rope bridges --------------------------------------------------------------------------

int World::bridgeWeight(const Bridge& b, bool& runner) const
{
  runner = false;
  const int top = b.y * kCellsPerTile;
  const int x0 = b.x0 * kCellsPerTile, x1 = (b.x1 + 1) * kCellsPerTile - 1;
  int weight = 0;
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  if ((p.state == PlayerState::OnGround || p.state == PlayerState::Recovering) && pb.bottom() + 1 == top &&
      pb.right() >= x0 && pb.left() <= x1)
  {
    runner = true;
    weight += mCharacterIndex == 1 ? 2 : 1; // Rocco is heavy
  }
  for (const auto& e : mEnemies)
    if (e.alive && e.y + 1 == top && e.x + e.w - 1 >= x0 && e.x <= x1)
      weight += 1;
  return weight;
}

void World::dropBridge(Bridge& b)
{
  b.down = true;
  b.drop = 0;
  b.creak = 0;
  for (int tx = b.x0; tx <= b.x1; ++tx)
    mMap.setBlock(tx, b.y, Tile::Empty);
  playSound(Sfx::Chop);
  const Vec2 c{(float(b.x0 + b.x1 + 1) * 0.5f) * float(kTileSize), float(b.y) * float(kTileSize) + 4.0f};
  burst(c, rgb(170, 120, 70), rgb(110, 74, 40), 24, 2.0f, false);
}

void World::updateBridges()
{
  const auto& p = mPlayer;
  for (auto& b : mBridges)
  {
    if (b.down)
    {
      if (b.drop < kBridgeDrop)
        ++b.drop;
      continue;
    }
    if (b.creak > 0)
    {
      if (--b.creak == 0)
        dropBridge(b);
      continue;
    }
    bool runner = false;
    const int weight = bridgeWeight(b, runner);
    auto creak = [&]() {
      b.creak = kCreak;
      playSound(Sfx::Creak);
    };
    b.sag = runner ? p.x + 1 : -1;
    if (b.snap > 0)
    {
      b.heavy = weight >= 2 ? b.heavy + 1 : 0;
      if (b.heavy >= b.snap)
      {
        creak();
        continue;
      }
    }
    if (runner)
    {
      b.stood = true;
      b.left = -1;
    }
    else if (b.stood && b.after > 0)
    {
      if (b.left < 0)
        b.left = b.after;
      else if (--b.left == 0)
        creak();
    }
  }
}

// --- Ropes, logs and the cage ------------------------------------------------------------------

void World::cutRope(JungleRope& rope)
{
  if (rope.cut)
    return;
  rope.cut = true;
  playSound(Sfx::Chop);
  burst({float(rope.x1) * kCellSize, float(rope.y1) * kCellSize}, rgb(200, 170, 110), rgb(120, 90, 50), 10, 1.4f, false);
  if (rope.load >= 0 && std::size_t(rope.load) < mLoads.size())
  {
    Load& l = mLoads[std::size_t(rope.load)];
    if (l.state == 0)
    {
      l.state = 1;
      l.fall = 0;
      l.fy = l.y;
    }
  }
}

void World::updateLoads()
{
  for (auto& l : mLoads)
  {
    if (l.state != 1)
      continue;
    if (!l.cage)
    {
      if (++l.fall >= kLogFall)
      {
        // Across its notches: a one-row bridge.
        l.state = 2;
        for (int tx = l.landX0; tx <= l.landX1; ++tx)
          mMap.setBlock(tx, l.landRow, Tile::Platform);
        const CellBox b = logBox(l);
        playSound(Sfx::Land);
        mCamera.shake(8, 1.5f);
        burst(cellCenter(b), rgb(170, 130, 80), rgb(120, 90, 50), 20, 2.0f, false);
      }
      continue;
    }
    // The cage drops until it hits something to stand on, and breaks open.
    for (int step = 0; step < 2 && l.state == 1; ++step)
    {
      const CellBox b = cageBox(l);
      bool landed = b.bottom() + 1 >= mMap.height();
      for (int x = b.left(); x <= b.right() && !landed; ++x)
        landed = mMap.solidTop(x, b.bottom() + 1);
      if (!landed)
      {
        ++l.fy;
        continue;
      }
      l.state = 3;
      const Vec2 c = cellCenter(b);
      for (int i = 0; i < l.gems; ++i)
      {
        Item it;
        it.kind = ItemKind::Gem;
        it.x = it.prevX = b.x + 1;
        it.y = it.prevY = b.bottom();
        it.variant = i % 4;
        it.vx = i - l.gems / 2;
        it.pickupDelay = 4;
        mItems.push_back(it);
      }
      burst(c, rgb(255, 240, 120), rgb(160, 120, 70), 26, 2.4f);
      flashAt(c, 110.0f, rgb(255, 230, 120), 18);
      playSound(Sfx::BoxBreak);
      showMessage("THE CAGE BREAKS OPEN!");
    }
  }
}

// --- Fruit ----------------------------------------------------------------------------------

void World::lobFruit(const Enemy& e)
{
  const CellBox b = e.box(), pb = mPlayer.box();
  Fruit f;
  f.x = f.prevX = float(b.x) + float(b.w) * 0.5f + float(e.dir);
  f.y = f.prevY = float(b.y);
  // Aimed so it comes down on the runner: up at 1.2 cells a frame, gravity
  // 0.25, sideways whatever reaches them (at most 1.5).
  const float tx = float(pb.x) + 1.5f, ty = float(pb.bottom()) - 1.0f;
  const float drop = std::max(0.0f, ty - f.y);
  const float t = (1.2f + std::sqrt(1.44f + 0.5f * drop)) / 0.25f;
  f.vx = std::clamp((tx - f.x) / t, -1.5f, 1.5f);
  f.vy = -1.2f;
  f.dir = e.dir;
  f.carrier = e.carrier;
  f.banana = e.variant == 1 && mCharacterIndex == 0; // the Dash Howler, when you are Dash
  mFruits.push_back(f);
}

void World::updateFruits()
{
  auto& p = mPlayer;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& f : mFruits)
  {
    f.prevX = f.x;
    f.prevY = f.y;
    bool gone = false;
    if (f.roll < 0)
    {
      f.vy = std::min(2.0f, f.vy + 0.25f);
      for (int s = 0; s < 4 && !gone && f.roll < 0; ++s)
      {
        f.x += f.vx * 0.25f;
        f.y += f.vy * 0.25f;
        const int cx = int(std::floor(f.x)), cy = int(std::floor(f.y));
        if (mMap.solid(cx, cy))
          gone = true; // splat on a wall
        else if (f.vy > 0.0f && mMap.solidTop(cx, cy + 1))
        {
          // Landed: it rolls on toward the runner.
          f.y = float(cy) + 0.5f;
          f.roll = kRollFrames;
          f.dir = float(p.x) + 1.5f < f.x ? -1 : 1;
          f.vx = f.vy = 0.0f;
        }
      }
      if (f.y > float(mMap.height() + 2))
        gone = true;
    }
    else
    {
      const int cx = int(std::floor(f.x)), cy = int(std::floor(f.y));
      if (--f.roll <= 0 || mMap.solid(cx + f.dir, cy) || !mMap.solidTop(cx + f.dir, cy + 1))
        gone = true; // a wall, an edge, or rolled out
      else
        f.x += float(f.dir);
    }
    if (!gone && vulnerable && f.box().intersects(p.hitBox()))
    {
      gone = true;
      if (f.banana)
      {
        showMessage("A BANANA? HE LIKES YOU");
        playSound(Sfx::Land);
      }
      else if (f.carrier)
      {
        if (p.mercy == 0 && p.virus == 0)
        {
          infect();
          p.mercy = 20;
        }
      }
      else
        hurtPlayer(1);
    }
    if (gone)
    {
      f.roll = -2; // marked for removal
      const Vec2 c{f.x * kCellSize, f.y * kCellSize};
      burst(c, f.carrier ? rgb(140, 255, 80) : (f.banana ? rgb(255, 230, 80) : rgb(255, 120, 60)), rgb(255, 230, 160), 8,
        1.2f, false);
    }
  }
  mFruits.erase(std::remove_if(mFruits.begin(), mFruits.end(), [](const Fruit& f) { return f.roll == -2; }),
    mFruits.end());
}

// --- Enemies -----------------------------------------------------------------------------------

void World::updateHowler(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const int pcx = pb.x + 1, ecx = b.x + 1;
  e.dir = pcx < ecx ? -1 : 1;
  if (e.aimX > 0)
    --e.aimX;
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      lobFruit(e);
      e.aimX = def.cooldown;
    }
    return;
  }
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Throws only down at a runner below it, within twelve blocks.
  const bool below = pb.top() > b.bottom() && pb.top() - b.bottom() < 40;
  if (vulnerable && below && std::abs(pcx - ecx) <= def.range && e.aimX == 0 && isOnScreen(b, 0))
  {
    e.tell = def.tell;
    playSound(Sfx::Hoot);
  }
}

void World::updateViper(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int ecx = e.x + 1, pcx = pb.x + 1;
  switch (e.attach)
  {
    case 0: // coiled on its branch, waiting for a runner to pass below
      if (vulnerable && std::abs(pcx - ecx) <= 4 && pb.top() > e.aimY + 2 && pb.top() - e.aimY < 16)
      {
        e.attach = 1;
        e.tell = def.tell;
        playSound(Sfx::Rustle);
      }
      break;
    case 1: // the leaves rustle
      if (--e.tell > 0)
        break;
      // Drops two blocks to hang under the branch.
      e.attach = 2;
      e.dive = 0;
      e.h = 4;
      e.y = e.aimY + 6;
      e.dir = pcx < ecx ? -1 : 1;
      break;
    case 2: // hanging, and striking a block down and sideways
    {
      ++e.dive;
      if (e.dive >= 2 && e.dive < 2 + kStrike && vulnerable)
      {
        const CellBox strike{e.dir > 0 ? e.x + e.w : e.x - 2, e.y - 1, 2, 4};
        if (strike.intersects(p.hitBox()))
          touchPlayer(e);
      }
      if (e.dive >= kViperRetract)
      {
        e.attach = 3;
        e.h = 2;
        e.y = e.aimY;
        e.aimX = def.cooldown;
      }
      break;
    }
    default: // coiled again, catching its breath
      if (--e.aimX <= 0)
        e.attach = 0;
      break;
  }
}

void World::updateCutter(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.attach < 0 || std::size_t(e.attach) >= mBridges.size())
    return;
  Bridge& br = mBridges[std::size_t(e.attach)];
  e.tell = 0;
  if (br.down || br.creak > 0 || e.dive >= 2)
    return; // nothing left to cut
  // Its post is the end of the bridge on its side; the runner comes from
  // the other end.
  const bool postRight = e.railX0 >= br.x1 * kCellsPerTile;
  const int nearX = postRight ? br.x0 * kCellsPerTile : (br.x1 + 1) * kCellsPerTile - 1;
  const int postX = postRight ? (br.x1 + 1) * kCellsPerTile : br.x0 * kCellsPerTile - e.w;
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  e.dir = postRight ? -1 : 1;
  if (e.dive == 0)
  {
    const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
    if (vulnerable && std::abs(pb.x + 1 - nearX) <= kCutterReach && std::abs(pb.bottom() + 1 - br.y * kCellsPerTile) <= 12)
    {
      e.dive = 1;
      e.aimX = 0;
    }
    return;
  }
  // The script: run to the post, then three chops.
  ++e.aimX;
  if (e.x != postX && e.timer % std::max(1, def.stepEvery) == 0)
    e.x += sgn(postX - e.x);
  for (int k = br.chops; k < 3; ++k)
  {
    const int at = kChopAt[k];
    if (e.aimX >= at - def.tell && e.aimX < at)
    {
      e.tell = at - e.aimX; // the machete goes up
      break;
    }
  }
  if (br.chops < 3 && e.aimX == kChopAt[br.chops])
  {
    ++br.chops;
    playSound(Sfx::Chop);
    const Vec2 c{float(postRight ? br.x1 * kCellsPerTile + 2 : br.x0 * kCellsPerTile) * kCellSize,
      float(br.y * kCellsPerTile) * kCellSize};
    burst(c, rgb(200, 170, 110), rgb(255, 255, 255), 8, 1.4f, false);
    if (br.cut > 0 && br.chops >= br.cut)
    {
      br.creak = kCreak;
      playSound(Sfx::Creak);
      e.dive = 2;
    }
  }
}

// --- Shots ------------------------------------------------------------------------------------

bool World::shotAtJungle(Projectile& pr)
{
  const CellBox b = pr.box();
  const bool boomerang = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::Boomerang);
  for (std::size_t i = 0; i < mJRopes.size(); ++i)
  {
    auto& rope = mJRopes[i];
    if (rope.cut)
      continue;
    bool hit = rope.hits(b);
    if (!hit && rope.load >= 0)
    {
      const Load& l = mLoads[std::size_t(rope.load)];
      float lx = 0.0f, ly = 0.0f;
      loadTop(l, lx, ly);
      hit = segmentHits(float(rope.x1), float(rope.y1), lx, ly, b) || (l.cage && cageBox(l).intersects(b));
    }
    const int tag = -100 - int(i);
    if (!hit || std::find(pr.hit.begin(), pr.hit.end(), tag) != pr.hit.end())
      continue;
    if (boomerang)
    {
      cutRope(rope); // clean through, and it flies on
      continue;
    }
    rope.hp -= std::max(1, pr.damage);
    playSound(Sfx::Hit);
    burst(cellCenter(b), rgb(220, 190, 130), rgb(140, 100, 60), 6, 1.0f, false);
    if (rope.hp <= 0)
      cutRope(rope);
    if (!pr.pierce)
      return true;
    pr.hit.push_back(tag);
  }
  for (auto& f : mFruits)
  {
    if (f.roll == -2 || !f.box().intersects(b))
      continue;
    f.roll = -2;
    const Vec2 c{f.x * kCellSize, f.y * kCellSize};
    burst(c, f.carrier ? rgb(140, 255, 80) : rgb(255, 140, 60), rgb(255, 240, 180), 10, 1.4f, false);
    addScore(50, c);
    playSound(Sfx::SmallExplosion);
    if (!pr.pierce)
      return true;
  }
  return false;
}

// The Boomerang: eight frames out, a block up at the turn, then home to the
// runner. Returns false once it is caught (or gone).
bool World::stepBoomerang(Projectile& pr)
{
  auto& p = mPlayer;
  if (pr.sx == 0)
  {
    pr.x += pr.dx;
    pr.y += pr.dy;
    if (++pr.sy >= kBoomerangOut)
    {
      pr.sx = 1;
      pr.hit.clear(); // hits everything again on the way back
      if (!mMap.overlapsSolid({pr.x, pr.y - 2, pr.w, pr.h}))
        pr.y -= 2;
    }
    return true;
  }
  const CellBox pb = p.box();
  const int tx = pb.x, ty = pb.top() + 1;
  pr.x += sgn(tx - pr.x);
  pr.y += sgn(ty - pr.y);
  const bool catcher = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (catcher && pr.box().intersects(pb))
  {
    // Caught: the throw costs nothing.
    if (p.weapon == Weapon::Proto && p.proto == pr.proto)
      p.ammo = std::min(protoDef(pr.proto).maxAmmo, p.ammo + 1);
    else if (p.weapon == Weapon::Normal && mLevelProto == pr.proto)
    {
      p.weapon = Weapon::Proto;
      p.proto = pr.proto;
      p.ammo = 1;
    }
    playSound(Sfx::Item);
    return false;
  }
  return true;
}

void World::resetJungle()
{
  // Every bridge is back up, and the Cutters go back to their posts.
  for (auto& b : mBridges)
  {
    if (b.down || b.creak > 0)
      for (int tx = b.x0; tx <= b.x1; ++tx)
        mMap.setBlock(tx, b.y, Tile::Platform);
    b.down = false;
    b.creak = b.drop = b.heavy = b.chops = 0;
    b.left = -1;
    b.stood = false;
    b.sag = -1;
  }
  for (auto& e : mEnemies)
    if (e.alive && e.kind == EnemyKind::Cutter)
    {
      e.x = e.prevX = e.railX0;
      e.y = e.prevY = e.railX1;
      e.dive = e.aimX = e.tell = 0;
      e.drawSnap = true;
    }
  mFruits.clear();
  mBounceH = kBounceStart;
  mBounceKick = 0;
}

// --- Bounce House --------------------------------------------------------------------------------

void World::updateBounce(int mvX, int mvY, const PlayerInput& in)
{
  auto& p = mPlayer;
  // Sideways: normal air control, and a wall bounces you back.
  if (mBounceKick != 0)
  {
    const int dir = sgn(mBounceKick);
    mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), dir);
    mBounceKick -= dir;
  }
  else if (mvX != 0)
  {
    p.facing = mvX;
    for (int i = 0; i < horizontalSteps(); ++i)
      if (mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX) != MoveResult::Completed)
      {
        mBounceKick = -mvX * kKick;
        playSound(Sfx::Land);
        break;
      }
  }
  if (p.state == PlayerState::Jumping)
  {
    // p.frames: cells risen so far on this bounce.
    const int left = mBounceH - p.frames;
    const int step = left > 8 ? 3 : (left > 3 ? 2 : 1);
    int moved = 0;
    if (left > 0)
      for (; moved < step; ++moved)
        if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1) != MoveResult::Completed)
          break;
    p.frames += moved;
    if (left <= 0 || moved < step)
    {
      p.state = PlayerState::Falling;
      p.frames = 0;
    }
    setVisual(PlayerVisual::Jumping);
    return;
  }
  p.state = PlayerState::Falling;
  const int fall = std::min(3, 1 + p.frames / 2);
  ++p.frames;
  setVisual(p.frames > 2 ? PlayerVisual::FallingFull : PlayerVisual::Falling);
  for (int i = 0; i < fall; ++i)
  {
    if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), 1) == MoveResult::Completed)
      continue;
    // A trampoline: back up to the last height, higher with jump held,
    // lower with down (or nothing).
    int h = mBounceH + (in.jump.pressed ? 2 : (mvY > 0 ? -3 : -1));
    mBounceH = std::clamp(h, kBounceMin, kBounceMax);
    p.state = PlayerState::Jumping;
    p.frames = 0;
    setVisual(PlayerVisual::Coiling);
    playSound(Sfx::Boing);
    burst({(float(p.x) + 1.5f) * kCellSize, float(p.y + 1) * kCellSize}, rgb(255, 120, 200), rgb(255, 255, 255), 8, 1.2f);
    break;
  }
}

// --- Drawing ----------------------------------------------------------------------------------------

void World::drawJungleProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame,
  bool foreground) const
{
  switch (pr.kind)
  {
    case PropKind::Gate:
      if (foreground)
        break;
      {
        // The temple gate: two carved pillars and a lintel around the exit.
        const Color stone = rgb(150, 140, 108), dark = rgb(90, 84, 64), moss = rgb(80, 130, 60);
        const float pw = std::min(64.0f, w * 0.18f);
        for (const float px : {x, x + w - pw})
        {
          r.fillRect(px, y + 40.0f, pw, h - 40.0f, stone);
          r.fillRect(px + pw - 8.0f, y + 40.0f, 8.0f, h - 40.0f, dark);
          for (float gy = y + 70.0f; gy < y + h - 20.0f; gy += 48.0f)
            r.fillRect(px + 8.0f, gy, pw - 20.0f, 6.0f, dark);
        }
        r.fillRect(x - 16.0f, y, w + 32.0f, 44.0f, stone);
        r.fillRect(x - 16.0f, y + 36.0f, w + 32.0f, 8.0f, dark);
        for (float gx = x; gx < x + w; gx += 40.0f)
          r.fillRect(gx + 8.0f, y + 10.0f, 22.0f, 18.0f, dark);
        for (int k = 0; k < 6; ++k)
        {
          const float vx = x + float(k) * w / 5.0f;
          r.drawLine(vx, y + 30.0f, vx + 6.0f * std::sin(float(frame) * 0.05f + float(k)), y + 90.0f + float(k % 3) * 30.0f,
            5.0f, moss);
        }
        drawGlow(r, mArt, x + w * 0.5f, y + 22.0f, 40.0f, rgb(255, 210, 90), 0.4f + 0.15f * std::sin(float(frame) * 0.1f));
      }
      break;
    case PropKind::Nest:
      if (!foreground)
        break;
      {
        // A ring of woven twigs; the candid camera sits among three eggs.
        const Color twig = rgb(130, 96, 56), twigDark = rgb(90, 64, 34);
        r.fillRect(x, y + h - 26.0f, w, 20.0f, twigDark);
        for (int k = 0; k < 14; ++k)
        {
          const float tx = x + float(k) * w / 13.0f;
          r.drawLine(tx - 18.0f, y + h - 8.0f - float(k % 3) * 6.0f, tx + 18.0f, y + h - 26.0f + float(k % 2) * 8.0f, 4.0f,
            k % 2 ? twig : twigDark);
        }
      }
      break;
    default:
      break;
  }
}

void World::drawJungleBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const float viewW = float(kScreenW), viewH = float(kScreenH);
  // Tree trunks: bark over their solid blocks, and a dark hollow inside.
  for (const auto& pr : mProps)
  {
    if (pr.kind != PropKind::Trunk)
      continue;
    const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY;
    const float w = float(pr.w) * kCellPx, h = float(pr.h) * kCellPx;
    if (x > viewW + 64.0f || x + w < -64.0f || y > viewH + 64.0f || y + h < -64.0f)
      continue;
    const bool hollow = pr.text == "hollow";
    const Color bark = rgb(104, 74, 46), barkLight = rgb(140, 102, 64), barkDark = rgb(64, 44, 26);
    for (int ty = pr.y / kCellsPerTile; ty < (pr.y + pr.h) / kCellsPerTile; ++ty)
      for (int tx = pr.x / kCellsPerTile; tx < (pr.x + pr.w) / kCellsPerTile; ++tx)
      {
        const float bx = float(tx) * kTilePx - camX, by = float(ty) * kTilePx - camY;
        const bool solid = mMap.block(tx, ty) == Tile::Solid;
        if (!solid && !hollow)
        {
          // A trunk behind the canopy: darker, out of the way.
          r.fillRect(bx, by, kTilePx, kTilePx, rgba(70, 50, 32, 220));
          r.fillRect(bx + 10.0f + float(hash2(tx, ty) % 30u), by, 5.0f, kTilePx, rgba(40, 28, 18, 200));
          continue;
        }
        if (!solid)
        {
          r.fillRect(bx, by, kTilePx, kTilePx, rgb(36, 24, 14));
          r.fillRect(bx, by, kTilePx, 6.0f, rgba(20, 12, 6, 200));
          continue;
        }
        r.fillRect(bx, by, kTilePx, kTilePx, bark);
        const unsigned hsh = hash2(tx * 7, ty * 13);
        for (int k = 0; k < 3; ++k)
          r.fillRect(bx + 6.0f + float(k) * 20.0f + float(hsh % 7u), by, 6.0f, kTilePx, k == 1 ? barkLight : barkDark);
        if (hsh % 5u == 0)
          r.fillRect(bx + 18.0f, by + 20.0f, 22.0f, 14.0f, rgb(70, 120, 50)); // moss
      }
  }

  // Rope bridges: planks that sag where you stand, with hand ropes.
  for (const auto& b : mBridges)
  {
    const float x0 = float(b.x0) * kTilePx - camX, x1 = float(b.x1 + 1) * kTilePx - camX;
    const float y = float(b.y) * kTilePx - camY;
    if (x0 > viewW + 64.0f || x1 < -64.0f || y > viewH + 64.0f || y < -128.0f)
      continue;
    const Color rope = rgb(196, 168, 110), plank = rgb(150, 104, 58), plankDark = rgb(96, 64, 34);
    // Posts at both ends.
    for (const float px : {x0 - 6.0f, x1 - 6.0f})
    {
      r.fillRect(px, y - 56.0f, 12.0f, 72.0f, plankDark);
      r.fillRect(px - 2.0f, y - 60.0f, 16.0f, 8.0f, plank);
    }
    const float shake = b.creak > 0 ? ((frame / 2) % 2 ? 3.0f : -3.0f) : 0.0f;
    if (b.down)
    {
      // The planks hang down from both posts.
      const float t = std::min(1.0f, (float(b.drop) + alpha) / float(kBridgeDrop));
      const float len = (x1 - x0) * 0.5f;
      const float ang = t * 1.45f;
      for (int side = 0; side < 2; ++side)
      {
        const float ox = side == 0 ? x0 : x1, dir = side == 0 ? 1.0f : -1.0f;
        const float ex = ox + dir * std::cos(ang) * len, ey = y + 4.0f + std::sin(ang) * len;
        r.drawLine(ox, y + 4.0f, ex, ey, 10.0f, plankDark);
        r.drawLine(ox, y - 40.0f, ex, ey - 40.0f * (1.0f - t), 3.0f, rope);
      }
      continue;
    }
    const float sagX = b.sag >= 0 ? float(b.sag) * kCellPx - camX : -1.0e9f;
    for (int tx = b.x0; tx <= b.x1; ++tx)
      for (int half = 0; half < 2; ++half)
      {
        const float px = float(tx) * kTilePx + float(half) * kTilePx * 0.5f - camX;
        const float d = std::abs(px + kTilePx * 0.25f - sagX) / (4.0f * kCellPx);
        const float sag = std::max(0.0f, 1.0f - d) * kCellPx * 0.5f + shake * float((tx + half) % 2 ? 1 : -1);
        r.fillRect(px + 2.0f, y + 4.0f + sag, kTilePx * 0.5f - 4.0f, 14.0f, plank);
        r.fillRect(px + 2.0f, y + 14.0f + sag, kTilePx * 0.5f - 4.0f, 4.0f, plankDark);
      }
    // The hand ropes droop between the posts (and a little more under weight).
    const float droop = b.sag >= 0 ? 22.0f : 14.0f;
    for (const float ry : {y - 44.0f, y + 6.0f})
    {
      constexpr int n = 8;
      for (int k = 0; k < n; ++k)
      {
        const float u0 = float(k) / n, u1 = float(k + 1) / n;
        const float a0 = ry + std::sin(u0 * 3.14159f) * droop + shake, a1 = ry + std::sin(u1 * 3.14159f) * droop + shake;
        r.drawLine(x0 + (x1 - x0) * u0, a0, x0 + (x1 - x0) * u1, a1, 3.0f, rope);
      }
    }
    for (int tx = b.x0; tx <= b.x1; ++tx)
    {
      const float px = float(tx) * kTilePx + kTilePx * 0.5f - camX;
      const float u = (px - x0) / std::max(1.0f, x1 - x0);
      r.drawLine(px, y - 44.0f + std::sin(u * 3.14159f) * droop, px, y + 8.0f, 2.0f, withAlpha(rope, 180));
    }
  }

  // Ropes and what hangs from them.
  for (const auto& rope : mJRopes)
  {
    const Load* l = rope.load >= 0 ? &mLoads[std::size_t(rope.load)] : nullptr;
    const float tx = float(rope.x0) * kCellPx - camX, ty = float(rope.y0) * kCellPx - camY;
    const float px = float(rope.x1) * kCellPx - camX, py = float(rope.y1) * kCellPx - camY;
    if (std::max(tx, px) < -200.0f || std::min(tx, px) > viewW + 200.0f)
      continue;
    const Color c = rgb(206, 178, 120);
    const float width = rope.cut ? 3.0f : 2.0f + float(rope.hp);
    if (!l || !l->cage)
    {
      // The pulley block on its branch.
      r.fillRect(px - 10.0f, py - 10.0f, 20.0f, 20.0f, rgb(90, 70, 50));
      r.fillRect(px - 5.0f, py - 5.0f, 10.0f, 10.0f, rgb(170, 140, 90));
    }
    if (!rope.cut)
    {
      if (rope.x0 != rope.x1 || rope.y0 != rope.y1)
        r.drawLine(tx, ty, px, py, width, c);
      if (l)
      {
        float lx = 0.0f, ly = 0.0f;
        loadTop(*l, lx, ly);
        r.drawLine(px, py, lx * kCellPx - camX, ly * kCellPx - camY, width, c);
      }
    }
    else
    {
      // Two frayed ends.
      r.drawLine(px, py, px + 4.0f, py + 30.0f, 3.0f, c);
      if (rope.x0 != rope.x1 || rope.y0 != rope.y1)
        r.drawLine(tx, ty, tx + (px - tx) * 0.15f, ty + 10.0f, 3.0f, c);
    }
    if (!l)
      continue;
    if (l->cage)
    {
      if (l->state == 3)
        continue;
      const CellBox b = cageBox(*l);
      const float bx = float(b.x) * kCellPx - camX, by = float(b.y) * kCellPx - camY;
      const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
      drawGlow(r, mArt, bx + bw * 0.5f, by + bh * 0.6f, 60.0f, rgb(255, 220, 90), 0.4f + 0.15f * std::sin(float(frame) * 0.15f));
      for (int g = 0; g < 3; ++g)
        r.fillRect(bx + 22.0f + float(g) * 30.0f, by + bh - 34.0f - float(g % 2) * 14.0f, 16.0f, 16.0f,
          mArt.gemColor[std::size_t(g % 4)]);
      r.fillRect(bx, by, bw, 8.0f, rgb(120, 90, 50));
      r.fillRect(bx, by + bh - 8.0f, bw, 8.0f, rgb(120, 90, 50));
      for (float gx = bx; gx <= bx + bw; gx += bw / 5.0f)
        r.fillRect(gx - 3.0f, by, 6.0f, bh, rgb(150, 116, 70));
    }
    else
    {
      const CellBox b = logBox(*l);
      const float bx = float(b.x) * kCellPx - camX, by = float(b.y) * kCellPx - camY;
      const float bw = float(b.w) * kCellPx, bh = float(b.h) * kCellPx;
      r.fillRect(bx, by + 4.0f, bw, bh - 6.0f, rgb(116, 78, 42));
      r.fillRect(bx, by + 4.0f, bw, 10.0f, rgb(156, 112, 64));
      r.fillRect(bx, by + bh - 12.0f, bw, 6.0f, rgb(80, 52, 26));
      for (const float ex : {bx, bx + bw - 14.0f})
      {
        r.fillRect(ex, by + 6.0f, 14.0f, bh - 10.0f, rgb(206, 166, 106));
        r.fillRect(ex + 4.0f, by + 16.0f, 6.0f, bh - 30.0f, rgb(150, 110, 64));
      }
    }
  }

  // Vines: from the anchor down, swaying (drawn at the in-between time).
  for (const auto& v : mVines)
  {
    const float axp = float(v.ax) * kCellPx - camX, ayp = float(v.ay) * kCellPx - camY;
    const float reach = float(v.len) * kCellPx;
    if (axp + reach < -64.0f || axp - reach > viewW + 64.0f || ayp > viewH + 64.0f || ayp + reach < -64.0f)
      continue;
    float t = float(v.t) - 1.0f + alpha;
    if (t < 0.0f)
      t += float(v.period);
    const float a = vineAngleAt(v, t) * kRad;
    const float ex = axp + std::sin(a) * reach, ey = ayp + std::cos(a) * reach;
    // A slight bow, as if it trails the swing.
    const float bow = -std::sin(6.2831853f * t / float(v.period)) * float(v.amp) * 0.25f;
    constexpr int n = 10;
    float lx = axp, ly = ayp;
    for (int k = 1; k <= n; ++k)
    {
      const float u = float(k) / n;
      const float bx = axp + (ex - axp) * u + bow * std::sin(u * 3.14159f) * std::cos(a);
      const float by = ayp + (ey - ayp) * u - bow * std::sin(u * 3.14159f) * std::sin(a);
      r.drawLine(lx, ly, bx, by, 16.0f - u * 5.0f, rgb(48, 82, 30));
      r.drawLine(lx, ly, bx, by, 7.0f - u * 2.0f, rgb(104, 156, 62));
      if (k % 2 == 0)
      {
        const float side = (k % 4 == 0) ? 1.0f : -1.0f;
        r.drawLine(bx, by, bx + side * 22.0f, by - 10.0f, 11.0f, rgb(70, 140, 52));
        r.drawLine(bx + side * 4.0f, by - 2.0f, bx + side * 18.0f, by - 8.0f, 4.0f, rgb(130, 200, 90));
      }
      lx = bx;
      ly = by;
    }
    r.fillRect(axp - 14.0f, ayp - 12.0f, 28.0f, 22.0f, rgb(70, 54, 34)); // the knot on its branch
    r.fillRect(axp - 10.0f, ayp - 12.0f, 20.0f, 6.0f, rgb(110, 84, 50));
  }

  // Fruit.
  for (const auto& f : mFruits)
  {
    const float fx = (f.prevX + (f.x - f.prevX) * alpha) * kCellPx - camX;
    const float fy = (f.prevY + (f.y - f.prevY) * alpha) * kCellPx - camY;
    if (fx < -64.0f || fx > viewW + 64.0f)
      continue;
    if (f.banana)
    {
      r.drawLine(fx - 16.0f, fy - 6.0f, fx, fy + 6.0f, 9.0f, rgb(250, 220, 70));
      r.drawLine(fx, fy + 6.0f, fx + 16.0f, fy - 6.0f, 9.0f, rgb(250, 220, 70));
      continue;
    }
    const Color c = f.carrier ? rgb(130, 240, 70) : rgb(240, 110, 50);
    if (f.carrier)
      drawGlow(r, mArt, fx, fy, 36.0f, rgb(120, 255, 60), 0.6f);
    r.fillRect(fx - 12.0f, fy - 12.0f, 24.0f, 24.0f, c);
    r.fillRect(fx - 12.0f, fy - 12.0f, 24.0f, 6.0f, lerpColor(c, rgb(255, 255, 255), 0.35f));
    r.drawLine(fx, fy - 12.0f, fx + 4.0f, fy - 20.0f, 3.0f, rgb(80, 60, 30));
  }
  (void)kInk;
}

void World::drawJungleFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  // Shallow water over the runner's feet.
  for (const auto& w : mWater)
  {
    const float x = float(w.x) * kCellPx - camX, y = float(w.y) * kCellPx - camY;
    const float ww = float(w.w) * kCellPx, h = float(w.h) * kCellPx;
    if (x > float(kScreenW) || x + ww < 0.0f || y > float(kScreenH) || y + h < 0.0f)
      continue;
    r.fillRect(x, y + 10.0f, ww, h - 10.0f, rgba(60, 140, 150, 120));
    for (float rx = x; rx < x + ww; rx += 32.0f)
    {
      const float wave = 4.0f * std::sin(float(frame) * 0.12f + rx * 0.05f);
      r.fillRect(rx, y + 8.0f + wave, 20.0f, 4.0f, rgba(210, 240, 240, 150));
    }
  }
}

} // namespace gr
