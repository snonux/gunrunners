// Level 10, Sun Mirrors (SPEC 10): Light Beams. Sunbeams come in through
// roof slits and sun pipes and travel in eight directions; mirror statues
// turn a step when shot and send a beam on, and a sun door opens after 15
// lit frames in a row. Shade Wraiths drift through walls and only light
// hurts them, Mirror Monks send shots and sunbeams back the way they face,
// and Sun Moths swarm onto a beam until three of them block it. The
// prototype is the Sunstone Lance: hold fire for a beam of your own that
// reflects like sunlight, tap it for a glint. Also the Negative Space bonus
// (rules=negative), where sun blocks and moon blocks trade places.

#include "game/world.hpp"

#include "assets/art.hpp"

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
constexpr int kLitToOpen = 15;  // lit frames in a row that open a sun door
constexpr int kHoldOpen = 45;   // frames a latch=0 door stays open after the light goes
constexpr int kTurnFrames = 8;  // a mirror's quarter-turn
constexpr int kMonkFreeze = 45; // frames a Monk stands frozen in a beam
constexpr int kMonkImmune = 60; // and then walks on before a beam can freeze him again
constexpr int kSunSteps = 200;  // blocks a sunbeam can travel
constexpr int kSunBounces = 8;
constexpr int kLanceLen = 20; // blocks
constexpr int kLanceBounces = 4;
constexpr int kLanceHold = 4;   // frames of holding fire before the beam comes out
constexpr int kMothBlock = 3;   // moths in one block that stop a beam
constexpr int kMothScatter = 60;
constexpr int kCameraEgg = 15;  // frames of Lance on the camera's lens
const Color kSun = rgb(255, 214, 120);
const Color kLance = rgb(255, 250, 210);

int sgn(int v) { return (v > 0) - (v < 0); }
int blockOf(int cell) { return cell >= 0 ? cell / kCellsPerTile : -1 - (-cell - 1) / kCellsPerTile; }

// "34..66" -> 34, 66.
bool parseSpan(const std::string& s, int& a, int& b)
{
  const auto dots = s.find("..");
  if (dots == std::string::npos)
    return false;
  a = std::atoi(s.substr(0, dots).c_str());
  b = std::atoi(s.substr(dots + 2).c_str());
  if (a > b)
    std::swap(a, b);
  return true;
}

bool beamPasses(Tile t) { return t != Tile::Solid && t != Tile::ForceField && t != Tile::Spikes; }

// The block a moth counts in: the one under its middle.
int mothBlockX(const Enemy& e) { return blockOf(e.x + 1); }
int mothBlockY(const Enemy& e) { return blockOf(e.y); }

// The Monk's shield: the block row over his feet, the blocks his body covers.
bool shieldAt(const Enemy& e, int bx, int by)
{
  return by == blockOf(e.y) - 1 && bx >= blockOf(e.x) && bx <= blockOf(e.x + e.w - 1);
}

} // namespace

int World::negativePhase() const { return (mStats.frames % 150) < 75 ? 0 : 1; }

// --- Level entities ----------------------------------------------------------------

bool World::setupLightEntity(const EntityDef& e)
{
  if (e.kind == "beam" && e.hasPos)
  {
    mSunBeams.push_back({e.id, e.x, e.y, std::clamp(e.num("dir", 6), 0, 7)});
    return true;
  }
  if (e.kind == "mirror" && e.hasPos)
  {
    Mirror m;
    m.id = e.id;
    m.x = e.x;
    m.y = e.y;
    m.angle = m.to = ((e.num("angle", 0) % 8) + 8) % 8;
    mMirrors.push_back(m);
    return true;
  }
  if (e.kind == "sundoor" && e.hasPos)
  {
    SunDoor d;
    d.id = e.id;
    d.x = e.x;
    d.y = e.y;
    d.crack = e.num("crack", 0) != 0;
    d.w = std::max(1, e.num("w", d.crack ? 3 : 1));
    d.h = std::max(1, e.num("h", d.crack ? 1 : 3));
    d.latch = e.num("latch", 1) != 0;
    d.opensId = e.str("opens");
    if (!d.opensId.empty())
    {
      for (std::size_t i = 0; i < mSunDoors.size(); ++i)
        if (mSunDoors[i].id == d.opensId)
          d.opens = int(i);
      if (d.opens < 0)
        std::fprintf(stderr, "level line %d: no sun door %s (declare it first)\n", e.line, d.opensId.c_str());
    }
    for (int ty = d.y; ty < d.y + d.h; ++ty)
      for (int tx = d.x; tx < d.x + d.w; ++tx)
      {
        d.under.push_back(mMap.block(tx, ty));
        mMap.setBlock(tx, ty, Tile::Solid);
        if (tx >= 0 && ty >= 0 && tx < mLevel->width && ty < mLevel->height)
          mLayerMask[std::size_t(ty * mLevel->width + tx)] = 1; // drawn as a marble slab
      }
    mSunDoors.push_back(d);
    return true;
  }
  if (e.kind == "moths" && e.hasPos)
  {
    const int def = enemyIndex("moth");
    const int count = std::clamp(e.num("count", 5), 1, 12);
    for (int i = 0; i < count && def >= 0; ++i)
    {
      spawnEnemy(def, e.x * kCellsPerTile + (i - count / 2) * 2, e.y * kCellsPerTile + 1 - (i % 2) * 2);
      Enemy& m = mEnemies.back();
      m.railX0 = m.x; // home
      m.railX1 = m.y;
      m.timer = i * 3;
    }
    return true;
  }
  if (e.kind == "cursedmirror" && e.hasPos)
  {
    CursedMirror c;
    c.x = e.x;
    c.y = e.y - 1; // given by the block it stands on the floor with
    c.need = std::max(1, e.num("stare", 30));
    mCursed.push_back(c);
    return true;
  }
  return false;
}

void World::setupLightEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind != EnemyKind::Monk)
    return;
  en.active = true; // on patrol whether you are there or not
  int a = 0, b = 0;
  if (parseSpan(e.str("patrol"), a, b))
  {
    en.railX0 = a * kCellsPerTile;
    en.railX1 = (b + 1) * kCellsPerTile - en.w;
  }
  else
    en.railX0 = en.railX1 = -1;
  if (e.str("dir") == "l")
    en.dir = -1;
}

// --- Beams ---------------------------------------------------------------------------

void World::traceBeam(int x, int y, int dir, int maxSteps, int bounces, bool lance, BeamPath& path)
{
  path.pts.clear();
  path.lance = lance;
  path.pts.push_back({x * kCellsPerTile + 1, y * kCellsPerTile + 1});
  const int W = mMap.widthBlocks(), H = mMap.heightBlocks();
  int bx = x, by = y, d = dir;
  int lastMonk = -1;
  std::vector<int> burnt; // enemies this beam already hurt this frame
  auto centre = [](int b) { return b * kCellsPerTile + 1; };
  auto burn = [&](Enemy& e, int every, int damage) {
    if (std::find(burnt.begin(), burnt.end(), e.id) != burnt.end())
      return;
    burnt.push_back(e.id);
    if (mStats.frames % every == 0)
    {
      damageEnemy(e, damage);
      burst(cellCenter(e.box()), kSun, rgb(90, 70, 110), 4, 1.0f, false);
    }
  };
  for (int step = 0; step <= maxSteps; ++step)
  {
    if (bx < 0 || by < 0 || bx >= W || by >= H)
    {
      path.pts.push_back({centre(std::clamp(bx, 0, W - 1)), centre(std::clamp(by, 0, H - 1))});
      return;
    }
    // A sun door takes the light.
    bool door = false;
    for (auto& sd : mSunDoors)
      if (!sd.open && sd.covers(bx, by))
      {
        sd.litNow = true;
        door = true;
      }
    if (door)
    {
      path.pts.push_back({centre(bx) - kDirX[d], centre(by) - kDirY[d]});
      return;
    }
    // A mirror's head: the face sends it on, the back (or a turning mirror) stops it.
    bool bounced = false;
    for (auto& m : mMirrors)
    {
      if (m.x != bx || m.y != by)
        continue;
      const int out = m.turn > 0 ? -1 : Mirror::reflect(m.angle, d);
      path.pts.push_back({centre(bx), centre(by)});
      if (out < 0 || --bounces < 0)
        return;
      m.lit = true;
      d = out;
      bounced = true;
      lastMonk = -1;
      break;
    }
    if (!bounced)
    {
      const Tile t = mMap.block(bx, by);
      if (!beamPasses(t))
      {
        path.pts.push_back({centre(bx) - kDirX[d], centre(by) - kDirY[d]});
        return;
      }
      // Creatures in the light.
      int moths = 0;
      for (std::size_t i = 0; i < mEnemies.size(); ++i)
      {
        Enemy& e = mEnemies[i];
        if (!e.alive || !e.active)
          continue;
        switch (e.kind)
        {
          case EnemyKind::Moth:
            if (mothBlockX(e) == bx && mothBlockY(e) == by && e.dive == 0)
            {
              if (lance)
                burn(e, 1, 1); // the Lance burns them off
              else
                ++moths;
            }
            break;
          case EnemyKind::Monk:
            if (int(i) != lastMonk && shieldAt(e, bx, by) && !bounced)
            {
              // Freezes, shield up, and sends the beam the way he faces.
              if (e.aimX == 0 && e.aimY == 0)
                e.aimX = kMonkFreeze;
              path.pts.push_back({centre(bx), centre(by)});
              if (--bounces < 0)
                return;
              d = e.dir > 0 ? 0 : 4;
              lastMonk = int(i);
              bounced = true;
            }
            break;
          case EnemyKind::Wraith:
            if (e.box().intersects({bx * kCellsPerTile, by * kCellsPerTile, kCellsPerTile, kCellsPerTile}))
              burn(e, lance ? 2 : 4, 1);
            break;
          case EnemyKind::Camera:
            if (lance && !e.hidden && e.box().intersects({bx * kCellsPerTile, by * kCellsPerTile, 2, 2}) &&
                e.aimX >= 0 && ++e.aimX >= kCameraEgg)
            {
              // Hold the Lance on the lens: the studio lights come up.
              e.aimX = -1;
              const Vec2 c = cellCenter(e.box());
              flashAt(c, 900.0f, rgb(255, 255, 255), 30);
              addScore(4200, c);
              showMessage("LIVE STUDIO AUDIENCE: APPLAUSE");
              playSound(Sfx::LettersComplete);
            }
            break;
          default:
            if (lance && e.box().intersects({bx * kCellsPerTile, by * kCellsPerTile, 2, 2}) &&
                !(enemyDef(e.def).flags & kEnemyHarmless))
              burn(e, 4, 1);
            break;
        }
      }
      if (moths >= kMothBlock)
      {
        path.pts.push_back({centre(bx), centre(by)});
        return;
      }
    }
    if (step == maxSteps)
    {
      path.pts.push_back({centre(bx), centre(by)});
      return;
    }
    bx += kDirX[d];
    by += kDirY[d];
  }
}

int World::traceBeamFor(int x, int y, int dir, const std::vector<int>& angles, const std::vector<char>& fixed,
  int* stop) const
{
  const int W = mMap.widthBlocks(), H = mMap.heightBlocks();
  int bx = x, by = y, d = dir;
  for (int step = 0; step <= kSunSteps; ++step)
  {
    if (bx < 0 || by < 0 || bx >= W || by >= H)
      return -1;
    for (std::size_t i = 0; i < mSunDoors.size(); ++i)
      if (!mSunDoors[i].open && mSunDoors[i].covers(bx, by))
        return int(i);
    bool bounced = false;
    for (std::size_t i = 0; i < mMirrors.size(); ++i)
    {
      const Mirror& m = mMirrors[i];
      if (m.x != bx || m.y != by)
        continue;
      if (i >= fixed.size() || !fixed[i])
      {
        if (stop)
          *stop = int(i);
        return -2;
      }
      const int out = Mirror::reflect(angles[i], d);
      if (out < 0)
        return -1;
      d = out;
      bounced = true;
      break;
    }
    if (!bounced && !beamPasses(mMap.block(bx, by)))
      return -1;
    bx += kDirX[d];
    by += kDirY[d];
  }
  return -1;
}

void World::setSunDoor(SunDoor& d, bool open)
{
  d.open = open;
  d.hold = open ? kHoldOpen : 0;
  d.lit = 0;
  std::size_t k = 0;
  for (int ty = d.y; ty < d.y + d.h; ++ty)
    for (int tx = d.x; tx < d.x + d.w; ++tx, ++k)
    {
      if (tx < 0 || ty < 0 || tx >= mLevel->width || ty >= mLevel->height)
        continue;
      mMap.setBlock(tx, ty, open ? (k < d.under.size() ? d.under[k] : Tile::Empty) : Tile::Solid);
      mLayerMask[std::size_t(ty * mLevel->width + tx)] = open ? 0 : 1;
    }
  const Vec2 c{(float(d.x) + float(d.w) * 0.5f) * kTileSize, (float(d.y) + float(d.h) * 0.5f) * kTileSize};
  if (open)
  {
    burst(c, kSun, rgb(255, 255, 255), 16, 1.6f);
    flashAt(c, 90.0f, kSun, 14);
    playSound(d.crack ? Sfx::BoxBreak : Sfx::Chime);
  }
  else
    playSound(Sfx::Clunk);
}

void World::updateSunDoors()
{
  const CellBox pb = mPlayer.box();
  for (auto& d : mSunDoors)
  {
    if (d.open)
    {
      if (d.latch || d.opens >= 0)
        continue;
      if (d.litNow)
        d.hold = kHoldOpen;
      else if (d.hold > 0)
        --d.hold;
      else if (!CellBox{d.x * kCellsPerTile, d.y * kCellsPerTile, d.w * kCellsPerTile, d.h * kCellsPerTile}
                   .intersects(pb))
        setSunDoor(d, false);
      continue;
    }
    d.lit = d.litNow ? d.lit + 1 : 0;
    if (d.lit < kLitToOpen)
      continue;
    if (d.opens >= 0)
    {
      // The stone sun takes the light and opens its hatch.
      d.open = true;
      SunDoor& t = mSunDoors[std::size_t(d.opens)];
      if (!t.open)
        setSunDoor(t, true);
      showMessage("THE STONE SUN WAKES - THE SUMMIT HATCH OPENS");
      playSound(Sfx::LettersComplete);
    }
    else
    {
      setSunDoor(d, true);
      if (d.crack)
        showMessage("THE CRACKED DISC GIVES WAY");
      else if (d.latch)
        showMessage("SUN DOOR OPEN");
    }
  }
}

bool World::lightHides(const Enemy& e) const
{
  // The candid camera in a mirror's pedestal only shows while the mirror is lit.
  if (e.kind != EnemyKind::Camera)
    return false;
  for (const auto& m : mMirrors)
    if (blockOf(e.x) >= m.x - 1 && blockOf(e.x) <= m.x + 1 && blockOf(e.y) == m.y + 1)
      return !m.lit;
  return false;
}

void World::updateLight(const PlayerInput& /*input*/)
{
  auto& p = mPlayer;
  if (p.weapon != Weapon::Proto || ProtoId(p.proto) != ProtoId::SunstoneLance || p.state == PlayerState::Dying ||
      p.state == PlayerState::Teleporting)
    mLanceOn = false;
  // Shots a Monk sent back last frame.
  for (const auto& r : mReflects)
  {
    spawnProjectile(ShotKind::Enemy, r[0], r[1], r[2], 0);
    mProjectiles.back().speed = 2;
  }
  mReflects.clear();
  if (mSunBeams.empty() && mMirrors.empty() && mSunDoors.empty() && mCursed.empty() && !mLanceOn)
    return;
  for (auto& m : mMirrors)
  {
    m.lit = false;
    if (m.turn > 0 && --m.turn == 0)
      m.angle = m.to;
  }
  for (auto& d : mSunDoors)
    d.litNow = false;
  mBeamPaths.clear();
  for (const auto& b : mSunBeams)
  {
    BeamPath path;
    traceBeam(b.x, b.y, b.dir, kSunSteps, kSunBounces, false, path);
    mBeamPaths.push_back(std::move(path));
  }
  if (mLanceOn)
  {
    // From the muzzle: the way you face, or up.
    const int dir = p.stance == Stance::Up ? 2 : (p.facing > 0 ? 0 : 4);
    const CellBox pb = p.box();
    const int mx = dir == 2 ? pb.x + 1 : (p.facing > 0 ? pb.right() + 1 : pb.left() - 1);
    const int my = dir == 2 ? pb.top() - 1 : pb.top() + 1;
    BeamPath path;
    traceBeam(blockOf(mx), blockOf(my), dir, kLanceLen, kLanceBounces, true, path);
    path.pts.front() = {mx, my};
    mBeamPaths.push_back(std::move(path));
  }
  updateSunDoors();
  // The cursed mirror: stand in front of it and it gives you the virus.
  const CellBox pb = p.box();
  for (auto& c : mCursed)
  {
    if (c.stare < 0)
    {
      ++c.stare;
      continue;
    }
    const bool before = p.state == PlayerState::OnGround && pb.bottom() == (c.y + 2) * kCellsPerTile - 1 &&
      pb.right() >= (c.x - 1) * kCellsPerTile && pb.left() < (c.x + 2) * kCellsPerTile;
    c.stare = before ? c.stare + 1 : 0;
    if (c.stare >= c.need)
    {
      c.stare = -150;
      if (p.virus == 0 && p.turbo == 0)
      {
        infect();
        showMessage("THE MIRROR LOOKS BACK");
      }
    }
  }
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::Camera)
      e.hidden = lightHides(e);
}

// --- Enemies ---------------------------------------------------------------------------

void World::updateWraith(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int dx = (pb.x + 1) - (b.x + 1), dy = pb.y - e.y;
  const int maxX = mMap.width() - e.w, maxY = mMap.height() - 1;
  if (e.dive > 0)
  {
    // Backs off four blocks after a touch, then hangs there a while.
    if (--e.dive > 30 - 8)
    {
      e.x = std::clamp(e.x - (dx >= 0 ? 1 : -1), 0, maxX);
      e.y = std::clamp(e.y - sgn(dy), e.h, maxY);
    }
    e.tell = 0;
    return;
  }
  const bool near = std::abs(dx) <= 8 && std::abs(dy) <= 8;
  if (near && e.tell == 0 && vulnerable)
    playSound(Sfx::Rustle); // the hiss
  e.tell = near && vulnerable ? 1 : 0;
  if (!vulnerable || std::abs(dx) > 80 || std::abs(dy) > 50)
    return;
  // Drifts through walls toward you.
  if (e.timer % std::max(1, def.stepEvery) == 0)
  {
    e.x = std::clamp(e.x + sgn(dx), 0, maxX);
    e.y = std::clamp(e.y + sgn(dy), e.h, maxY);
  }
  if (dx != 0)
    e.dir = sgn(dx);
  if (e.box().intersects(p.hitBox()))
    e.dive = 30; // the touch hurts (updateEnemies), then it backs off
}

void World::updateMonk(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.aimX > 0)
  {
    // Frozen in a sunbeam, shield up.
    if (--e.aimX == 0)
      e.aimY = kMonkImmune;
    e.tell = 0;
    e.dive = 0;
    return;
  }
  if (e.aimY > 0)
    --e.aimY;
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int dx = (pb.x + 1) - (b.x + b.w / 2);
  if (e.dive > 0)
  {
    // The bash: the shield shoved a block forward, then lowered a moment.
    --e.dive;
    const CellBox bash{e.dir > 0 ? b.right() + 1 : b.left() - 2, b.y + 1, 2, b.h - 1};
    if (e.dive == 2 && vulnerable && bash.intersects(p.hitBox()))
      hurtPlayer(1);
    if (e.dive == 0)
      e.attach = 12;
    return;
  }
  if (e.attach > 0)
  {
    --e.attach; // shield down: shots get through
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      e.dive = 4;
      playSound(Sfx::Land);
    }
    return;
  }
  const bool sameFloor = std::abs(pb.bottom() - b.bottom()) <= 2;
  const bool chasing = sameFloor && vulnerable && std::abs(dx) <= 24;
  if (chasing && dx != 0)
    e.dir = sgn(dx);
  const int gap = e.dir > 0 ? pb.left() - b.right() - 1 : b.left() - pb.right() - 1;
  if (chasing && sgn(dx) == e.dir && gap <= 2 && gap >= -2 && e.timer >= e.lastDive)
  {
    e.tell = def.tell; // plants the shield
    e.lastDive = e.timer + def.cooldown;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  const bool post = e.railX0 >= 0 && ((e.dir < 0 && e.x <= e.railX0) || (e.dir > 0 && e.x >= e.railX1));
  if (wall || ledge || post)
  {
    if (!chasing)
      e.dir = -e.dir;
  }
  else if (!(chasing && gap <= 2))
    e.x += e.dir;
}

void World::updateMoth(Enemy& e, const EnemyDef& def)
{
  const int maxX = mMap.width() - e.w, maxY = mMap.height() - 1;
  if (e.dive > 0)
  {
    // Scattered: off and up for a moment, then back to the light.
    if (--e.dive > kMothScatter - 12)
    {
      e.x = std::clamp(e.x + e.dir, 0, maxX);
      e.y = std::clamp(e.y - 1, e.h, maxY);
    }
    return;
  }
  // The nearest sunbeam within reach.
  const int cx = e.x + 1, cy = e.y;
  int best = def.range * def.range + 1, tx = e.railX0, ty = e.railX1;
  for (const auto& path : mBeamPaths)
  {
    if (path.lance)
      continue;
    for (std::size_t i = 1; i < path.pts.size(); ++i)
    {
      const auto [x0, y0] = path.pts[i - 1];
      const auto [x1, y1] = path.pts[i];
      const int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) / kCellsPerTile;
      for (int k = 0; k <= n; ++k)
      {
        const int px = n == 0 ? x0 : x0 + (x1 - x0) * k / n, py = n == 0 ? y0 : y0 + (y1 - y0) * k / n;
        const int d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);
        if (d2 < best)
        {
          best = d2;
          // Into that block, each moth at its own corner of it.
          tx = blockOf(px) * kCellsPerTile - 1 + (e.id % 2);
          ty = blockOf(py) * kCellsPerTile + ((e.id / 2) % 2);
        }
      }
    }
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  if (tx == e.railX0 && ty == e.railX1)
  {
    // No light near: flutter about home.
    tx += ((e.timer / 16 + e.id) % 3) - 1;
    ty += ((e.timer / 12 + e.id) % 3) - 1;
  }
  e.dir = tx > e.x ? 1 : (tx < e.x ? -1 : e.dir);
  e.x = std::clamp(e.x + sgn(tx - e.x), 0, maxX);
  e.y = std::clamp(e.y + sgn(ty - e.y), e.h, maxY);
}

// --- Shots -------------------------------------------------------------------------------

bool World::shotAtLight(Projectile& pr, const CellBox& b)
{
  for (auto& m : mMirrors)
  {
    const CellBox mb{m.x * kCellsPerTile, m.y * kCellsPerTile, kCellsPerTile, 2 * kCellsPerTile};
    if (!mb.intersects(b))
      continue;
    const int dir = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
    if (dir != 0 && pr.kind != ShotKind::Enemy)
    {
      // Eastbound turns it a step anticlockwise, westbound clockwise.
      m.to = ((m.to + dir) % 8 + 8) % 8;
      m.turn = kTurnFrames;
      playSound(Sfx::Click);
    }
    burst(cellCenter(b), rgb(255, 255, 240), kSun, 6, 1.2f);
    return true;
  }
  return false;
}

int World::shotAtLightEnemy(Projectile& pr, Enemy& e)
{
  const int dir = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
  const bool lance = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SunstoneLance);
  switch (e.kind)
  {
    case EnemyKind::Wraith:
      // Only light hurts it: Turbo's shots count, and the Lance's glint.
      if (mPlayer.turbo == 0 && !lance)
        return 2;
      damageEnemy(e, pr.damage);
      burst(cellCenter(e.box()), kSun, rgb(90, 70, 110), 8, 1.4f, false);
      return 1;
    case EnemyKind::Monk:
      if (pr.dy == 0 && e.attach == 0 && (dir == -e.dir || e.aimX > 0))
      {
        // The mirror shield sends it straight back.
        const CellBox b = e.box();
        mReflects.push_back({e.dir > 0 ? b.right() + 1 : b.left() - 2, pr.y, e.dir});
        e.flash = 3;
        burst(cellCenter(pr.box()), rgb(255, 255, 255), kSun, 6, 1.4f);
        playSound(Sfx::Land);
        return 1;
      }
      return 0;
    case EnemyKind::Moth:
    {
      damageEnemy(e, pr.damage);
      // The rest scatter for a moment.
      for (auto& m : mEnemies)
        if (m.alive && m.kind == EnemyKind::Moth && std::abs(m.x - e.x) < 24 && std::abs(m.y - e.y) < 24)
        {
          m.dive = kMothScatter;
          m.dir = m.x < e.x ? -1 : 1;
        }
      return 1;
    }
    default:
      return 0;
  }
}

// --- Drawing --------------------------------------------------------------------------

void World::drawLightBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  if (mSunBeams.empty() && mMirrors.empty() && mSunDoors.empty() && mCursed.empty())
    return;
  auto onScreen = [&](float x, float y, float w, float h) {
    return x + w > -64.0f && x < float(kScreenW) + 64.0f && y + h > -64.0f && y < float(kScreenH) + 64.0f;
  };
  // Sun doors: marble slabs with a sun disc that fills as the light holds.
  for (const auto& d : mSunDoors)
  {
    const float x = float(d.x) * kTilePx - camX, y = float(d.y) * kTilePx - camY;
    const float w = float(d.w) * kTilePx, h = float(d.h) * kTilePx;
    if (!onScreen(x, y, w, h))
      continue;
    if (d.open && d.opens < 0)
    {
      // The empty frame of an open door.
      r.fillRect(x, y, w, 4, rgba(255, 230, 170, 120));
      r.fillRect(x, y + h - 4, w, 4, rgba(255, 230, 170, 120));
      continue;
    }
    const float glow = d.open ? 1.0f : std::min(1.0f, float(d.lit) / float(kLitToOpen));
    r.fillRect(x + 1, y + 1, w - 2, h - 2, d.crack ? rgb(214, 206, 190) : rgb(236, 230, 218));
    r.fillRect(x + 1, y + 1, w - 2, 5, rgb(255, 250, 240));
    r.fillRect(x + 1, y + h - 6, w - 2, 5, rgb(188, 176, 160));
    r.fillRect(x + 4, y + 8, 3, h - 16, rgba(190, 170, 140, 160));
    if (d.crack)
      for (int k = 0; k < 4; ++k)
        r.drawLine(x + w * 0.5f, y + h * 0.5f, x + w * (0.15f + 0.24f * float(k)), y + (k % 2 ? 2.0f : h - 2.0f), 2.0f,
          rgb(110, 96, 80));
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float rad = std::min(w, h) * 0.32f;
    drawGlow(r, mArt, cx, cy, rad * 2.6f, kSun, 0.15f + 0.65f * glow);
    r.fillRect(cx - rad, cy - rad, rad * 2, rad * 2, lerpColor(rgb(200, 170, 110), rgb(255, 230, 120), glow));
    for (int k = 0; k < 8; ++k)
    {
      const float a = float(k) * 0.785398f + float(frame) * 0.01f * glow;
      r.drawLine(cx + std::cos(a) * rad, cy + std::sin(a) * rad, cx + std::cos(a) * rad * 1.6f,
        cy + std::sin(a) * rad * 1.6f, 3.0f, withAlpha(rgb(230, 180, 80), 120 + int(135 * glow)));
    }
  }
  // The cursed mirror: green rim, and your reflection going to static.
  for (const auto& c : mCursed)
  {
    const float x = float(c.x) * kTilePx - camX, y = float(c.y) * kTilePx - camY;
    if (!onScreen(x, y, kTilePx, kTilePx * 2))
      continue;
    r.fillRect(x - 4, y - 4, kTilePx + 8, kTilePx * 2 + 8, rgb(60, 200, 90));
    r.fillRect(x, y, kTilePx, kTilePx * 2, rgb(170, 190, 200));
    r.fillRect(x + 4, y + 4, 6, kTilePx * 2 - 8, rgba(255, 255, 255, 140));
    if (c.stare >= 15)
      for (int k = 0; k < 24; ++k)
      {
        const unsigned hsh = hash2(k + frame * 31, c.x);
        r.fillRect(x + float(hsh % 28u), y + float((hsh >> 5) % 60u), 4, 3,
          (hsh >> 11) % 2 ? rgb(120, 255, 120) : rgb(20, 40, 20));
      }
  }
  // Beams.
  for (const auto& path : mBeamPaths)
  {
    if (path.lance)
      continue;
    for (std::size_t i = 1; i < path.pts.size(); ++i)
    {
      const float x0 = (float(path.pts[i - 1].first) + 0.5f) * kCellPx - camX;
      const float y0 = (float(path.pts[i - 1].second) + 0.5f) * kCellPx - camY;
      const float x1 = (float(path.pts[i].first) + 0.5f) * kCellPx - camX;
      const float y1 = (float(path.pts[i].second) + 0.5f) * kCellPx - camY;
      if (std::max(x0, x1) < -32.0f || std::min(x0, x1) > float(kScreenW) + 32.0f || std::max(y0, y1) < -32.0f ||
          std::min(y0, y1) > float(kScreenH) + 32.0f)
        continue;
      r.drawLine(x0, y0, x1, y1, 22.0f, rgba(255, 200, 110, 40), Blend::Add);
      r.drawLine(x0, y0, x1, y1, 9.0f, rgba(255, 220, 140, 110), Blend::Add);
      r.drawLine(x0, y0, x1, y1, 3.0f, rgba(255, 250, 220, 230), Blend::Add);
      // Dust motes drifting in the light.
      const float len = std::hypot(x1 - x0, y1 - y0);
      for (float t = std::fmod(float(frame) * 0.7f, 37.0f); t < len; t += 37.0f)
      {
        const float u = t / std::max(1.0f, len);
        const unsigned hsh = hash2(int(t), int(i));
        const float off = float(int(hsh % 13u) - 6);
        r.fillRect(x0 + (x1 - x0) * u + off, y0 + (y1 - y0) * u - off * 0.5f, 3, 3, rgba(255, 245, 210, 200));
      }
    }
    if (!path.pts.empty())
    {
      const auto [ex, ey] = path.pts.back();
      drawGlow(r, mArt, (float(ex) + 0.5f) * kCellPx - camX, (float(ey) + 0.5f) * kCellPx - camY, 34, kSun, 0.6f);
    }
  }
  // Mirror statues: a marble pedestal and a disc that is a mirror on one side.
  for (const auto& m : mMirrors)
  {
    const float x = float(m.x) * kTilePx - camX, y = float(m.y) * kTilePx - camY;
    if (!onScreen(x, y, kTilePx, kTilePx * 2))
      continue;
    r.fillRect(x + 6, y + kTilePx * 0.5f, kTilePx - 12, kTilePx * 1.5f, rgb(226, 220, 208));
    r.fillRect(x + 2, y + kTilePx * 2 - 8, kTilePx - 4, 8, rgb(200, 190, 176));
    r.fillRect(x + 6, y + kTilePx * 0.5f, 4, kTilePx * 1.5f, rgb(250, 248, 240));
    // The angle it shows, easing through a turn.
    float ang = float(m.angle);
    if (m.turn > 0)
    {
      int delta = ((m.to - m.angle) % 8 + 8) % 8;
      if (delta > 4)
        delta -= 8;
      ang += float(delta) * float(kTurnFrames - m.turn) / float(kTurnFrames);
    }
    const float a = ang * 0.785398f;
    const float nx = std::cos(a), ny = -std::sin(a); // the face's normal, on screen
    const float cx = x + kTilePx * 0.5f, cy = y + kTilePx * 0.5f;
    const float half = kTilePx * 0.48f;
    // Back (gold), then the face (silver) a little in front of it.
    r.drawLine(cx - ny * half - nx * 4, cy + nx * half - ny * 4, cx + ny * half - nx * 4, cy - nx * half - ny * 4, 7.0f,
      rgb(170, 120, 50));
    r.drawLine(cx - ny * half + nx * 2, cy + nx * half + ny * 2, cx + ny * half + nx * 2, cy - nx * half + ny * 2, 5.0f,
      m.lit ? rgb(255, 250, 220) : rgb(200, 215, 230));
    r.drawLine(cx + nx * 3, cy + ny * 3, cx + nx * 11, cy + ny * 11, 2.0f, rgba(255, 255, 255, 160));
    if (m.lit)
      drawGlow(r, mArt, cx, cy, 40, kSun, 0.7f);
  }
}

void World::drawLightFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  for (const auto& path : mBeamPaths)
  {
    if (!path.lance)
      continue;
    const float flick = 0.85f + 0.15f * std::sin(float(frame) * 1.3f);
    for (std::size_t i = 1; i < path.pts.size(); ++i)
    {
      const float x0 = (float(path.pts[i - 1].first) + 0.5f) * kCellPx - camX;
      const float y0 = (float(path.pts[i - 1].second) + 0.5f) * kCellPx - camY;
      const float x1 = (float(path.pts[i].first) + 0.5f) * kCellPx - camX;
      const float y1 = (float(path.pts[i].second) + 0.5f) * kCellPx - camY;
      r.drawLine(x0, y0, x1, y1, 18.0f * flick, rgba(255, 230, 150, 70), Blend::Add);
      r.drawLine(x0, y0, x1, y1, 6.0f, withAlpha(kLance, 230), Blend::Add);
    }
    if (!path.pts.empty())
    {
      const auto [ex, ey] = path.pts.back();
      drawGlow(r, mArt, (float(ex) + 0.5f) * kCellPx - camX, (float(ey) + 0.5f) * kCellPx - camY, 44, kLance, 0.8f);
    }
  }
}

void World::drawNegativeLayer(Renderer& r, const Layer& l, float x, float y, float w, float h, int frame) const
{
  const bool moon = l.style == 4;
  const int period = std::max(1, l.on + l.off);
  const int f = (((mStats.frames + l.phase) % period) + period) % period;
  if (!l.solid)
  {
    // Not here this phase: a dotted outline, shimmering just before it comes.
    const bool soon = f >= period - 15;
    const Color g = moon ? rgba(150, 160, 255, soon ? 200 : 70) : rgba(255, 220, 120, soon ? 200 : 70);
    const float jit = soon ? float((frame / 2) % 3) - 1.0f : 0.0f;
    for (float dx = x + 4; dx < x + w - 4; dx += 12)
    {
      r.fillRect(dx + jit, y + 3, 6, 3, g);
      r.fillRect(dx - jit, y + h - 6, 6, 3, g);
    }
    for (float dy = y + 4; dy < y + h - 4; dy += 12)
    {
      r.fillRect(x + 3, dy + jit, 3, 6, g);
      r.fillRect(x + w - 6, dy - jit, 3, 6, g);
    }
    return;
  }
  if (moon)
  {
    r.fillRect(x, y, w, h, rgb(28, 30, 64));
    r.fillRect(x, y, w, 4, rgb(120, 130, 220));
    for (int i = 0; i < int(w * h / 900.0f) + 1; ++i)
    {
      const unsigned hsh = hash2(l.x0 * 131 + l.y0, i);
      const float sx = x + float(hsh % unsigned(std::max(1.0f, w - 4))), sy = y + float((hsh >> 9) % unsigned(std::max(1.0f, h - 4)));
      r.fillRect(sx, sy, 2, 2, rgba(230, 235, 255, 160 + int((hsh >> 3) % 90u)));
    }
  }
  else
  {
    r.fillRect(x, y, w, h, rgb(246, 236, 210));
    r.fillRect(x, y, w, 4, rgb(255, 250, 235));
    r.fillRect(x, y + h - 5, w, 5, rgb(220, 190, 130));
    for (float gx = x + kTilePx; gx < x + w; gx += kTilePx)
      r.fillRect(gx - 1, y + 4, 2, h - 9, rgba(200, 170, 110, 140));
  }
  // About to go: the edge flickers.
  if (f >= l.on - 15 && f < l.on && (frame / 2) % 2)
    r.fillRect(x, y, w, h, rgba(255, 255, 255, 50), Blend::Add);
}

void World::drawLightHud(Renderer& r, int frame) const
{
  if (!mNegative)
    return;
  // The sun or the moon, and how long until they swap.
  const int phase = negativePhase();
  const int left = 75 - mStats.frames % 75;
  const float cx = float(kScreenW) * 0.5f + 120.0f, cy = 110.0f; // beside the bonus timer
  const bool turning = left <= 15 && (frame / 3) % 2;
  const Color c = (phase == 0) != turning ? rgb(255, 210, 90) : rgb(170, 180, 255);
  drawGlow(r, mArt, cx, cy, 46, c, 0.6f);
  if ((phase == 1) != turning)
  {
    // A crescent moon.
    for (int k = 0; k < 14; ++k)
    {
      const float a0 = 1.2f + float(k) * 0.28f, a1 = a0 + 0.3f;
      const float rr = 14.0f - std::abs(float(k) - 6.5f) * 0.9f;
      r.drawLine(cx + std::cos(a0) * 12.0f, cy + std::sin(a0) * 12.0f, cx + std::cos(a1) * 12.0f,
        cy + std::sin(a1) * 12.0f, rr * 0.6f, c);
    }
  }
  else
  {
    // The sun and its rays.
    for (int k = 0; k < 8; ++k)
    {
      const float a = float(k) * 0.785398f + float(frame) * 0.02f;
      r.drawLine(cx + std::cos(a) * 12.0f, cy + std::sin(a) * 12.0f, cx + std::cos(a) * 22.0f,
        cy + std::sin(a) * 22.0f, 4.0f, c);
    }
    r.drawLine(cx - 10, cy, cx + 10, cy, 20.0f, c);
  }
  const float frac = float(left) / 75.0f;
  r.fillRect(cx - 30, cy + 24, 60 * frac, 4, withAlpha(c, 220));
}

} // namespace gr
