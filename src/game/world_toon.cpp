// Level 28, Toon Town Throwdown: the air-run and its "uh-oh", painted
// tunnels (a passage, or a bonk), the Portable Hole in walls and floors,
// Grinning Bombs, Piano Birds and their pianos, Rubber Goons, the ink
// under the big top, Buddy Boom in three phases, the hat tipped to the
// canyon sign and the secrets. Also the Inkwell bonus (rules=ink): fire
// draws a one-way line. Drawing is in world_toon_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kTossGravity = 0.15f;
constexpr int kBombReach = 4;      // cells: a Grinning Bomb lunges this close
constexpr int kBombTurn = 15;      // frames a Bomb takes to turn after it runs past
constexpr int kBombBlast = 2;      // cells
constexpr int kLungeLen = 6;       // frames of a lunge
constexpr int kGoonReach = 6;      // cells: a Rubber Goon's punch
constexpr float kBirdSpeed = 1.0f;
constexpr int kBirdRow = 8;        // blocks
constexpr int kPianoW = 3, kPianoH = 2; // blocks
constexpr int kWaveSpeed = 2;
constexpr int kNoseFlash = 30;
constexpr int kSpin = 30;
constexpr int kHopLen = 24;
constexpr int kPieTell = 12;
constexpr int kPieWhite = 45;
constexpr int kBirdEvery = 150;
constexpr int kPencilLen = 24;
constexpr int kOutline = 30;
constexpr int kErased = 120;
constexpr int kDizzy = 150;
constexpr int kCrackHits = 10;
constexpr int kDeflate = 45;

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupToonEntity(const EntityDef& e)
{
  const bool ink = mLevel->rules.find("ink") != std::string::npos;
  if (mLevel->themeKey != "rubber_hose_toon" && !ink)
    return false;
  auto& t = mToon;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (ink)
  {
    if (e.kind == "refill" && e.hasPos)
    {
      mInk.refills.push_back({e.x, e.y, false});
      return true;
    }
    return false;
  }
  t.on = true;
  if (e.kind == "tunnel" && e.hasPos)
  {
    ToonTunnel tn;
    tn.x = e.x;
    tn.y = e.y;
    tn.real = e.num("real", 0) != 0;
    if (tn.real)
      for (int by = tn.y; by <= tn.y + 2; ++by)
        for (int bx = tn.x; bx <= tn.x + 1; ++bx)
          if (mMap.block(bx, by) == Tile::Solid)
            mMap.setBlock(bx, by, Tile::Empty);
    t.tunnels.push_back(tn);
    return true;
  }
  if (e.kind == "spawner" && e.str("kind") == "piano_bird")
  {
    BirdSpawner s;
    s.trigger = e.num("trigger", e.hasPos ? e.x : 0) * kCellsPerTile;
    t.spawners.push_back(s);
    return true;
  }
  if (e.kind == "sign" && e.hasPos)
  {
    t.signX = e.x;
    t.signY = e.y;
    return true;
  }
  if (e.kind == "secret" && hasRect)
  {
    (e.id == "dressing" ? t.dressing : t.studio) = blockRect(x0, y0, x1, y1);
    return true;
  }
  if (e.kind == "prize" && e.hasPos)
  {
    t.prizeX = e.x;
    t.prizeY = e.y;
    t.prizeKind = e.str("kind", "buddy_sketch");
    t.prizeScore = e.num("score", 25000);
    return true;
  }
  if (e.kind == "fullheal" && e.hasPos)
  {
    t.fullhealX = e.x;
    t.fullhealY = e.y;
    return true;
  }
  if (e.kind == "fluid" && e.str("kind") == "pond" && hasRect)
  {
    t.ponds.push_back(blockRect(x0, y0, x1, y1));
    return true;
  }
  if (e.kind == "buddy" && e.hasPos)
  {
    auto& b = t.buddy;
    b.on = true;
    b.x = b.prevX = float(e.x * kCellsPerTile);
    b.startX = e.x * kCellsPerTile;
    b.y = b.prevY = float(e.y * kCellsPerTile + 1);
    if (e.rect("arena", x0, y0, x1, y1))
    {
      b.x0 = x0;
      b.y0 = y0;
      b.x1 = x1;
      b.y1 = y1;
    }
    if (e.rect("door", x0, y0, x1, y1))
    {
      b.doorX = x0;
      b.doorY0 = std::min(y0, y1);
      b.doorY1 = std::max(y0, y1);
    }
    const auto floor = e.list("floor"); // x0, segment width, segments, row (blocks)
    if (floor.size() == 4)
    {
      b.segX0 = floor[0];
      b.segW = std::max(1, floor[1]);
      b.segs = std::max(1, floor[2]);
      b.floorY = floor[3];
    }
    b.erased.assign(std::size_t(b.segs), 0);
    const auto plats = e.list("platforms"); // west x0,x1, east x0,x1, row
    if (plats.size() == 5)
    {
      b.platX[0][0] = plats[0];
      b.platX[0][1] = plats[1];
      b.platX[1][0] = plats[2];
      b.platX[1][1] = plats[3];
      b.platY = plats[4];
    }
    const auto drops = e.path("drops");
    for (std::size_t i = 0; i < drops.size() && i < 2; ++i)
    {
      b.dropX[i] = drops[i].first;
      b.dropY = drops[i].second;
    }
    const auto turbo = e.list("turbo");
    if (turbo.size() == 2)
    {
      b.turboX = turbo[0];
      b.turboY = turbo[1];
    }
    return true;
  }
  return false;
}

void World::setupToonEnemy(Enemy& en, const EntityDef& e)
{
  (void)e;
  if (en.kind != EnemyKind::GrinningBomb && en.kind != EnemyKind::PianoBird && en.kind != EnemyKind::RubberGoon)
    return;
  mToon.on = true;
  if (en.kind == EnemyKind::RubberGoon)
  {
    en.railX0 = en.x - 16;
    en.railX1 = en.x + 16;
  }
}

void World::linkToon()
{
  mInk.on = mLevel->rules.find("ink") != std::string::npos;
  if (mInk.on)
  {
    mInk.startX = mPlayer.x;
    mInk.startY = mPlayer.y;
    mInk.ink = kInkMax;
    return;
  }
  mToon.on = mToon.on || mLevel->themeKey == "rubber_hose_toon";
}

// --- The air-run ---------------------------------------------------------------------------

bool World::toonAirRun(int mvX)
{
  auto& t = mToon;
  auto& p = mPlayer;
  if (!t.on || mInk.on || p.vehicle >= 0)
    return false;
  const bool turbo = p.turbo > 0;
  if (t.airDir == 0)
  {
    // Off a ledge holding the way you were going: run on.
    if (mvX == 0 || mvX != p.facing)
      return false;
    t.airDir = mvX;
    t.airT = 0;
  }
  if (turbo)
  {
    // Turbo: the air-run lasts until Turbo ends, stopped or turned.
    if (mvX != 0)
      t.airDir = mvX;
    ++t.airT;
    return true;
  }
  if (mvX != t.airDir)
  {
    // Let go, or turned: look at the camera, hold up the sign, then fall.
    t.airDir = 0;
    t.airT = 0;
    t.uhOh = mCharacterIndex == 2 ? kUhOhNova : kUhOh;
    setVisual(PlayerVisual::Standing);
    playSound(Sfx::Whistle);
    return true;
  }
  ++t.airT;
  return true;
}

bool World::toonNoJump() const
{
  // No jumping on air (the first two frames off the ledge still count as ground).
  return mToon.on && (mToon.uhOh > 0 || (mToon.airDir != 0 && mToon.airT > 2));
}

bool World::airRunning() const
{
  return mToon.on && (mToon.airDir != 0 || mToon.uhOh > 0);
}

// Before the runner moves: the "uh-oh", a bonk, the end of an air-run.
bool World::toonHolds(const PlayerInput& in)
{
  auto& t = mToon;
  auto& p = mPlayer;
  if (!t.on)
    return false;
  t.jumpHeld = in.jump.pressed;
  if (p.state != PlayerState::OnGround)
  {
    t.airDir = 0;
    t.uhOh = 0;
  }
  else if (t.airDir != 0 && mMap.onSolidGround(p.box()))
    t.airDir = 0; // back on the ground
  if (t.uhOh > 0)
  {
    setVisual(PlayerVisual::Standing);
    if (--t.uhOh == 0)
      startFalling();
    return true;
  }
  if (t.bonk > 0)
  {
    --t.bonk;
    setVisual(PlayerVisual::Standing);
    return true;
  }
  return false;
}

// A painted tunnel in front of the runner, -1 if none.
int World::tunnelAhead(int dir, bool real) const
{
  const auto& p = mPlayer;
  const CellBox b = p.box();
  const int ax = dir > 0 ? b.right() + 1 : b.left() - 1;
  for (std::size_t i = 0; i < mToon.tunnels.size(); ++i)
  {
    const auto& tn = mToon.tunnels[i];
    if (tn.real != real)
      continue;
    const CellBox r = blockRect(tn.x, tn.y, tn.x + 1, tn.y + 2);
    if (r.contains(ax, b.bottom()) || r.contains(ax, b.top() + 1))
      return int(i);
  }
  return -1;
}

void World::toonAfterMove(int mvX)
{
  auto& t = mToon;
  auto& p = mPlayer;
  if (!t.on)
    return;
  // Running into painted rock: a bonk.
  if (p.state == PlayerState::OnGround && mvX != 0 && mvX == p.facing && p.x == p.prevX)
  {
    const int k = tunnelAhead(mvX, false);
    if (k >= 0 && !(t.hole.open && t.hole.tunnel == k))
    {
      if (!t.bonkLatch)
      {
        t.bonkLatch = true;
        t.bonk = kBonk;
        playSound(Sfx::Boing);
        showMessage("BONK!");
        const CellBox b = p.box();
        burst({(float(b.x) + 1.5f) * kCellSize, float(b.top()) * kCellSize}, rgb(255, 240, 120), rgb(255, 255, 255), 8, 1.0f);
      }
      return;
    }
  }
  if (mvX != p.facing || tunnelAhead(p.facing, false) < 0)
    t.bonkLatch = false;
}

// --- The Portable Hole ---------------------------------------------------------------------

bool World::holeBusy() const
{
  return mToon.hole.open || mToon.toss.on;
}

void World::throwHole(int ox, int oy)
{
  const auto& p = mPlayer;
  auto& s = mToon.toss;
  s.on = true;
  s.fx = float(ox) + 0.5f;
  s.fy = float(oy) + 0.5f;
  const float f = float(p.facing);
  switch (p.stance)
  {
    case Stance::Up:
      s.vx = 0.3f * f;
      s.vy = -2.0f;
      break;
    case Stance::Crouched:
      s.vx = 1.4f * f;
      s.vy = -0.3f;
      break;
    case Stance::Down:
    case Stance::Jetpack:
      s.vx = 0.0f;
      s.vy = 1.0f;
      break;
    default:
      s.vx = 2.0f * f;
      s.vy = -0.9f;
      break;
  }
  if (mMap.solid(int(std::floor(s.fx)), int(std::floor(s.fy))))
  {
    s.fx = float(p.x) + 1.5f;
    s.fy = float(p.y) - 2.5f;
  }
  playSound(Sfx::Throw);
}

// Tap fire: a blaster shot (no ammo). Hold it kHoleHold frames: the hole
// is thrown (one out at a time).
void World::updateHoleShooting(const Button& fire)
{
  auto& p = mPlayer;
  if (fire.triggered && (p.shotCooldown == 0 || p.turbo > 0))
  {
    p.weapon = Weapon::Normal;
    fireShot();
    p.weapon = Weapon::Proto;
    p.shotCooldown = 1;
  }
  if (!fire.pressed)
  {
    p.charge = 0;
    return;
  }
  if (++p.charge != kHoleHold)
    return;
  if (holeBusy())
  {
    playSound(Sfx::Click);
    return;
  }
  fireShot();
}

void World::refundHole()
{
  auto& p = mPlayer;
  if (p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::PortableHole)
    p.ammo = std::min(protoDef(p.proto).maxAmmo, p.ammo + 1);
  else if (mLevelProto == int(ProtoId::PortableHole))
  {
    p.weapon = Weapon::Proto;
    p.proto = mLevelProto;
    p.ammo = 1;
  }
}

void World::openHole(bool wall, int x0, int y0, int x1, int y1)
{
  auto& h = mToon.hole;
  h = ToonHole{};
  h.open = true;
  h.wall = wall;
  h.t = kHoleOpen;
  h.x0 = x0;
  h.y0 = y0;
  h.x1 = x1;
  h.y1 = y1;
  for (int by = y0; by <= y1; ++by)
    for (int bx = x0; bx <= x1; ++bx)
    {
      const Tile was = mMap.block(bx, by);
      h.saved.push_back(int(was));
      if (was == Tile::Solid || was == Tile::Platform)
        mMap.setBlock(bx, by, Tile::Empty);
    }
  const CellBox r = blockRect(x0, y0, x1, y1);
  for (std::size_t i = 0; i < mToon.tunnels.size(); ++i)
  {
    const auto& tn = mToon.tunnels[i];
    if (!tn.real && blockRect(tn.x, tn.y, tn.x + 1, tn.y + 2).intersects(r))
    {
      // Behind the painted rock: the game's first programmer art.
      h.tunnel = int(i);
      mToon.retro = kHoleOpen;
      showMessage("BEHIND THE PAINT: THE ORIGINAL PROGRAMMER ART");
    }
  }
  // Whatever stood on a floor that is now a hole falls through: enemies for good.
  if (!wall)
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.hidden)
        continue;
      const CellBox b = e.box();
      if (b.bottom() + 1 >= r.top() && b.bottom() + 1 <= r.bottom() && b.right() >= r.left() && b.left() <= r.right())
      {
        burst(cellCenter(b), rgb(40, 30, 50), rgb(255, 255, 255), 10, 1.4f);
        killEnemy(e);
      }
    }
  playSound(Sfx::Gulp);
  burst(cellCenter(r), rgb(30, 20, 40), rgb(120, 100, 140), 14, 1.4f);
}

void World::closeHole(bool pickedUp)
{
  auto& h = mToon.hole;
  if (!h.open)
    return;
  std::size_t i = 0;
  for (int by = h.y0; by <= h.y1; ++by)
    for (int bx = h.x0; bx <= h.x1; ++bx, ++i)
    {
      const Tile was = Tile(h.saved[i]);
      if ((was == Tile::Solid || was == Tile::Platform) && mMap.block(bx, by) == Tile::Empty)
        mMap.setBlock(bx, by, was);
    }
  if (h.tunnel >= 0)
    mToon.retro = 0;
  h = ToonHole{};
  if (pickedUp)
  {
    refundHole();
    playSound(Sfx::Item);
  }
  else
    playSound(Sfx::Bloop);
}

// The hole lands: on a wall (3 blocks thick at most) a passage, on a floor
// a hole; true if it opened.
bool World::landHole(int cx, int cy, bool wall, int dir)
{
  const int bx = cx / kCellsPerTile, by = cy / kCellsPerTile;
  if (wall)
  {
    // On painted rock the hole opens just the painting, however deep the rock.
    for (const auto& tn : mToon.tunnels)
      if (!tn.real && bx >= tn.x && bx <= tn.x + 1 && by >= tn.y && by <= tn.y + 2)
      {
        openHole(true, tn.x, tn.y, tn.x + 1, tn.y + 2);
        return true;
      }
    int thick = 0;
    while (thick < 4 && mMap.block(bx + dir * thick, by) == Tile::Solid)
      ++thick;
    if (thick == 0 || thick > 3 || bx + dir * thick < 0 || bx + dir * thick >= mMap.widthBlocks())
      return false;
    // Three blocks tall, sitting on the floor in front of the wall if it is close.
    int bottom = by;
    const int front = bx - dir;
    while (bottom < by + 2 && !mMap.solidTop(front * kCellsPerTile, (bottom + 1) * kCellsPerTile))
      ++bottom;
    const int a = std::min(bx, bx + dir * (thick - 1)), b = std::max(bx, bx + dir * (thick - 1));
    openHole(true, a, bottom - 2, b, bottom);
    return true;
  }
  const Tile t = mMap.block(bx, by);
  if (t != Tile::Solid && t != Tile::Platform)
    return false;
  openHole(false, bx - 1, by, bx + 1, by);
  return true;
}

void World::updateHole()
{
  auto& t = mToon;
  auto& s = t.toss;
  auto& p = mPlayer;
  if (s.on)
  {
    const float px = s.fx, py = s.fy;
    s.vy = std::min(2.0f, s.vy + kTossGravity);
    const int steps = std::max(1, int(std::ceil(std::max(std::fabs(s.vx), std::fabs(s.vy)) / 0.5f)));
    for (int k = 0; k < steps && s.on; ++k)
    {
      const float nx = s.fx + s.vx / float(steps), ny = s.fy + s.vy / float(steps);
      const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
      // A floor of one-way platforms counts too, from above.
      const bool platform = s.vy > 0.0f && mMap.solidTop(cx, cy) && !mMap.solid(cx, cy) &&
        int(std::floor(s.fy)) < cy && cy % kCellsPerTile == 0;
      if (mMap.solid(cx, cy) || platform)
      {
        const bool sideways = mMap.solid(cx, int(std::floor(s.fy))) && !mMap.solid(int(std::floor(s.fx)), cy);
        const bool wall = sideways && std::fabs(s.vx) > 0.01f;
        s.on = false;
        if (s.vy < 0.0f && !wall)
        {
          // Hit a ceiling: it drops.
          s.on = true;
          s.vy = 0.0f;
          break;
        }
        if (!landHole(cx, cy, wall, s.vx < 0.0f ? -1 : 1))
        {
          refundHole(); // too thick, or nothing to open: it comes back
          playSound(Sfx::Bloop);
        }
        break;
      }
      s.fx = nx;
      s.fy = ny;
    }
    (void)px;
    (void)py;
    if (s.on && s.fy > float(mMap.height() + 4))
    {
      s.on = false;
      refundHole();
    }
  }
  auto& h = t.hole;
  if (!h.open)
    return;
  const CellBox r = blockRect(h.x0, h.y0, h.x1, h.y1);
  const CellBox reach{r.x, r.y - (h.wall ? 0 : 1), r.w, r.h + (h.wall ? 0 : 1)};
  const bool in = p.state != PlayerState::Dying && p.box().intersects(reach);
  if (in)
    h.touched = true;
  else if (h.touched)
  {
    closeHole(true); // walked through (or fell through) it: picked up
    return;
  }
  if (h.t > 0)
    --h.t;
  if (h.t > 0)
    return;
  // It closes by itself, once nothing is in it.
  bool busy = p.box().intersects(r);
  for (const auto& e : mEnemies)
    busy = busy || (e.alive && !e.hidden && e.box().intersects(r));
  if (!busy)
    closeHole(false);
}

// --- Enemies -----------------------------------------------------------------------------------

void World::updateGrinningBomb(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.hitBox();
  const int pcx = pb.x + pb.w / 2, ecx = b.x + b.w / 2;
  if (e.attach == 2)
  {
    // The lunge: two cells a frame; it goes off on contact.
    for (int k = 0; k < 2; ++k)
    {
      const CellBox nb = boxAt(e.x + e.dir, e.y, e.w, e.h);
      if (mMap.overlapsSolid(nb) || !mMap.solidTop(e.dir > 0 ? nb.right() : nb.left(), nb.bottom() + 1))
        break;
      e.x += e.dir;
    }
    const CellBox blast{e.x - kBombBlast, e.y - e.h + 1 - kBombBlast, e.w + 2 * kBombBlast, e.h + 2 * kBombBlast};
    if (alive && blast.intersects(pb))
    {
      hurtPlayer(1);
      e.alive = false;
      const Vec2 c = cellCenter(e.box());
      burst(c, rgb(255, 220, 90), rgb(255, 90, 30), 24, 2.6f);
      flashAt(c, 110.0f, rgb(255, 170, 70), 18);
      playSound(Sfx::Explosion);
      return;
    }
    if (--e.tell <= 0)
    {
      e.attach = 0; // missed: back to the chase
      e.cool = 20;
    }
    return;
  }
  if (e.attach == 1)
  {
    // It stops smiling: a frown, then the lunge.
    if (--e.tell <= 0)
    {
      e.attach = 2;
      e.tell = kLungeLen;
      playSound(Sfx::Fuse);
    }
    return;
  }
  if (e.cool > 0)
    --e.cool;
  if (!alive)
    return;
  // Chases; jumped over, it runs on past and takes a while to turn.
  const int want = pcx < ecx ? -1 : 1;
  if (want != e.dir)
  {
    if (e.aimX == 0)
      e.aimX = kBombTurn;
    if (--e.aimX <= 0)
    {
      e.dir = want;
      e.aimX = 0;
    }
  }
  else
    e.aimX = 0;
  const int gap = e.dir > 0 ? pb.left() - b.right() : b.left() - pb.right();
  if (e.cool == 0 && want == e.dir && gap <= kBombReach && gap >= -1 && std::abs(pb.bottom() - b.bottom()) <= 3)
  {
    e.attach = 1;
    e.tell = def.tell;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (!wall && mMap.solidTop(aheadX, b.bottom() + 1))
    e.x += e.dir;
}

void World::updatePianoBird(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  // Flies east along its row; over the runner, it lets a piano go.
  e.dir = 1;
  e.fx += kBirdSpeed;
  e.x = int(std::lround(e.fx));
  if (e.x > mMap.width() + 8)
  {
    e.alive = false;
    e.hidden = true;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box();
  const int pcx = p.x + 1;
  if (e.cool == 0 && p.state != PlayerState::Dying && pcx >= b.x + 8 && pcx <= b.x + 14)
  {
    // The piano drops where the runner will be about 22 frames on.
    Piano pn;
    pn.x = std::clamp(b.x + 12 - 3, 0, mMap.width() - kPianoW * kCellsPerTile);
    pn.y = float(b.bottom() + 1);
    int floor = b.bottom() + 1;
    while (floor < mMap.height() - 1)
    {
      bool top = false;
      for (int x = pn.x; x < pn.x + kPianoW * kCellsPerTile; ++x)
        top = top || mMap.solidTop(x, floor);
      if (top)
        break;
      ++floor;
    }
    pn.floor = floor;
    pn.state = 0;
    pn.t = kPianoTell;
    mToon.pianos.push_back(pn);
    e.cool = def.cooldown + kPianoTell;
    playSound(Sfx::Whistle);
  }
}

void World::updateRubberGoon(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  CellBox b = e.box();
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // Landed on: flat, he is a trampoline; Rocco squashes him.
  if (alive && p.y > p.prevY && p.prevY < b.top() && p.y >= b.top() - 1 && p.x + Player::kWidth > b.x &&
      p.x < b.x + b.w)
  {
    if (e.dive > 0)
    {
      p.y = b.top() - 1;
      startLaunch(kGoonBounce + (mToon.jumpHeld ? jumpHeight() : 0));
      playSound(Sfx::Boing);
      e.aimY = 6; // the bounce
      return;
    }
    if (mCharacterIndex == 1)
    {
      p.y = b.top() - 1;
      startLaunch(4);
      playSound(Sfx::Stomp);
      squashGoon(e);
      damageEnemy(e, 2);
      return;
    }
  }
  if (e.aimY > 0)
    --e.aimY;
  if (e.dive > 0)
  {
    if (--e.dive == 0)
    {
      // Back up, if there is room.
      const CellBox up = boxAt(e.x, e.y, e.w, enemyDef(e.def).h);
      if (mMap.overlapsSolid(up) || up.intersects(p.box()))
        e.dive = 1;
      else
        e.h = enemyDef(e.def).h;
    }
    return;
  }
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
    --e.cool;
  if (e.tell > 0)
  {
    // The arm coils; then it stretches three blocks.
    if (--e.tell == 0)
    {
      e.attach = 6; // frames the arm shows
      playSound(Sfx::Swoosh);
      const CellBox arm = e.dir > 0 ? CellBox{b.right() + 1, b.top() + 1, kGoonReach, 2}
                                    : CellBox{b.left() - kGoonReach, b.top() + 1, kGoonReach, 2};
      if (alive && arm.intersects(p.hitBox()))
        hurtPlayer(1);
      e.cool = def.cooldown;
    }
    return;
  }
  if (e.attach > 0)
  {
    --e.attach;
    return;
  }
  const CellBox pb = p.hitBox();
  const int gap = e.dir > 0 ? pb.left() - b.right() : b.left() - pb.right();
  const bool level = pb.bottom() >= b.top() && pb.top() <= b.bottom();
  if (alive && e.cool == 0 && level && std::abs(gap) <= kGoonReach)
  {
    e.dir = pb.x + 1 < b.x + b.w / 2 ? -1 : 1;
    e.tell = def.tell;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool end = e.dir > 0 ? b.right() + 1 > e.railX1 + e.w : b.left() - 1 < e.railX0;
  if (wall || end || !mMap.solidTop(aheadX, b.bottom() + 1))
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

void World::squashGoon(Enemy& e)
{
  // Squashed flat for 45 frames: no punch, no bite, a trampoline.
  e.dive = kGoonFlat;
  e.h = 2;
  e.tell = 0;
  e.attach = 0;
  playSound(Sfx::Squish);
}

bool World::toonShotHit(Enemy& e)
{
  if (e.kind == EnemyKind::RubberGoon && e.alive)
  {
    squashGoon(e);
    return false;
  }
  return false;
}

// --- Pianos, birds, the pond -----------------------------------------------------------------

void World::updatePianos()
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  for (auto& pn : mToon.pianos)
  {
    switch (pn.state)
    {
      case 0:
        if (--pn.t <= 0)
        {
          pn.state = 1;
          pn.vy = 1.0f;
        }
        break;
      case 1:
      {
        pn.vy = std::min(3.0f, pn.vy + 0.25f);
        pn.y += pn.vy;
        const CellBox box{pn.x, int(pn.y), kPianoW * kCellsPerTile, kPianoH * kCellsPerTile};
        if (alive && box.intersects(p.hitBox()))
        {
          hurtPlayer(2);
          playSound(Sfx::Crash);
        }
        for (auto& e : mEnemies)
          if (e.alive && !e.hidden && e.kind != EnemyKind::PianoBird && box.intersects(e.box()))
            damageEnemy(e, 6);
        if (pn.y + float(kPianoH * kCellsPerTile) >= float(pn.floor))
        {
          // Lands: a step for 150 frames.
          pn.state = 2;
          pn.t = kPianoStay;
          pn.bx = (pn.x + 1) / kCellsPerTile;
          pn.by = pn.floor / kCellsPerTile - kPianoH;
          pn.y = float(pn.by * kCellsPerTile);
          pn.x = pn.bx * kCellsPerTile;
          pn.saved.clear();
          for (int by = pn.by; by < pn.by + kPianoH; ++by)
            for (int bx = pn.bx; bx < pn.bx + kPianoW; ++bx)
            {
              pn.saved.push_back(int(mMap.block(bx, by)));
              if (mMap.block(bx, by) == Tile::Empty)
                mMap.setBlock(bx, by, Tile::Solid);
            }
          playSound(Sfx::Crash);
          mCamera.shake(6, 1.5f);
          burst({(float(pn.x) + 3.0f) * kCellSize, float(pn.floor) * kCellSize}, rgb(255, 255, 255), rgb(200, 200, 200), 14, 1.6f);
          // A runner caught under it is pushed out on top.
          if (p.box().intersects(blockRect(pn.bx, pn.by, pn.bx + kPianoW - 1, pn.by + kPianoH - 1)))
          {
            p.y = pn.by * kCellsPerTile - 1;
            p.state = PlayerState::OnGround;
          }
        }
        break;
      }
      case 2:
        if (--pn.t <= 0)
        {
          std::size_t i = 0;
          for (int by = pn.by; by < pn.by + kPianoH; ++by)
            for (int bx = pn.bx; bx < pn.bx + kPianoW; ++bx, ++i)
              if (Tile(pn.saved[i]) == Tile::Empty)
                mMap.setBlock(bx, by, Tile::Empty);
          pn.state = 3;
          if (mPlayer.state == PlayerState::OnGround && !mMap.onSolidGround(mPlayer.box()))
            startFalling();
        }
        break;
      default:
        break;
    }
  }
  mToon.pianos.erase(std::remove_if(mToon.pianos.begin(), mToon.pianos.end(), [](const Piano& pn) { return pn.state == 3; }),
    mToon.pianos.end());
}

void World::spawnPianoBird(int x)
{
  const int def = enemyIndex("piano_bird");
  if (def < 0)
    return;
  spawnEnemy(def, x, kBirdRow * kCellsPerTile + 1);
  Enemy& e = mEnemies.back();
  e.active = true;
  e.dir = 1;
  e.fx = float(x);
  e.cool = 10;
}

void World::toonPond()
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  for (const auto& w : mToon.ponds)
    if (w.intersects(p.box()) && p.box().bottom() > w.top())
    {
      burst(cellCenter(p.box()), rgb(30, 20, 40), rgb(80, 60, 90), 16, 1.6f);
      playSound(Sfx::Splash);
      hurtPlayer(1);
      if (p.state == PlayerState::Dying)
        return;
      // Back on the nearest floor still there.
      const int row = w.top() - 1;
      int best = -1;
      for (int d = 0; d < 80 && best < 0; ++d)
        for (int s : {-1, 1})
        {
          const int x = p.x + s * d;
          if (x < 0 || x + 2 >= mMap.width())
            continue;
          for (int y = row; y >= std::max(0, row - 30); --y)
          {
            const CellBox b = boxAt(x, y, Player::kWidth, Player::kHeight);
            if (!mMap.overlapsSolid(b) && mMap.onSolidGround(b))
            {
              best = x;
              p.x = p.prevX = x;
              p.y = p.prevY = y;
              break;
            }
          }
          if (best >= 0)
            break;
        }
      p.state = PlayerState::OnGround;
      mToon.airDir = 0;
      mToon.uhOh = 0;
      setVisual(PlayerVisual::Standing);
      return;
    }
}

// --- Buddy Boom --------------------------------------------------------------------------------

CellBox World::buddyBox() const
{
  const auto& b = mToon.buddy;
  return boxAt(int(std::lround(b.x)), int(std::lround(b.y)), Buddy::kW, Buddy::kH);
}

CellBox World::buddyNose() const
{
  const auto& b = mToon.buddy;
  const CellBox body = buddyBox();
  // After a slam he is bent over the mallet: the nose is down at the
  // height of a runner's shot (a standing or a crouching one).
  const int y = b.nose > 0 ? body.bottom() - 4 : body.bottom() - 12;
  return {b.dir > 0 ? body.right() - 1 : body.left() - 2, y, 4, 4};
}

bool World::buddyFight() const
{
  const auto& b = mToon.buddy;
  return b.on && b.doorShut && b.state != BuddyState::Waiting && b.state != BuddyState::Done;
}

int World::buddyHp() const
{
  const auto& b = mToon.buddy;
  if (!b.on || b.state >= BuddyState::Dizzy)
    return 0;
  return (3 - b.phase) * kBuddyHp + std::max(0, b.hp);
}

bool World::buddyHittable() const
{
  const auto& b = mToon.buddy;
  if (!b.on)
    return false;
  switch (b.phase)
  {
    case 1:
      return b.state == BuddyState::Walk || b.state == BuddyState::Slam;
    case 2:
      return b.state == BuddyState::Hop || b.state == BuddyState::Pie;
    default:
      return b.state == BuddyState::Stand || b.state == BuddyState::Erase;
  }
}

void World::shutBuddyDoor(bool shut)
{
  auto& b = mToon.buddy;
  b.doorShut = shut;
  for (int by = b.doorY0; by <= b.doorY1; ++by)
    mMap.setBlock(b.doorX, by, shut ? Tile::Solid : Tile::Empty);
}

void World::setSegment(int s, bool there)
{
  auto& b = mToon.buddy;
  for (int bx = b.segX0 + s * b.segW; bx < b.segX0 + (s + 1) * b.segW; ++bx)
    mMap.setBlock(bx, b.floorY, there ? Tile::Solid : Tile::Empty);
}

void World::setBuddyLine(bool solid)
{
  auto& b = mToon.buddy;
  if (b.lineY < 0 || solid == b.lineSolid)
    return;
  if (solid)
  {
    b.lineSaved.clear();
    for (int bx = b.lineX0; bx <= b.lineX1; ++bx)
    {
      b.lineSaved.push_back(int(mMap.block(bx, b.lineY)));
      if (mMap.block(bx, b.lineY) == Tile::Empty)
        mMap.setBlock(bx, b.lineY, Tile::Platform);
    }
  }
  else
  {
    for (int bx = b.lineX0; bx <= b.lineX1; ++bx)
    {
      const std::size_t i = std::size_t(bx - b.lineX0);
      if (i < b.lineSaved.size() && Tile(b.lineSaved[i]) == Tile::Empty && mMap.block(bx, b.lineY) == Tile::Platform)
        mMap.setBlock(bx, b.lineY, Tile::Empty);
    }
    if (mPlayer.state == PlayerState::OnGround && !mMap.onSolidGround(mPlayer.box()) && !airRunning())
      startFalling();
  }
  b.lineSolid = solid;
}

void World::damageBuddy(int amount)
{
  auto& b = mToon.buddy;
  if (b.state == BuddyState::Dizzy)
  {
    // Ten more hits crack the line under him.
    b.flash = 4;
    b.t = 0;
    if (++b.dizzyHits >= kCrackHits)
      buddyFall(false);
    playSound(Sfx::Tink);
    return;
  }
  if (!buddyHittable())
    return;
  if (b.phase == 1 && b.nose == 0)
    return;
  b.hp -= amount;
  b.flash = 4;
  playSound(Sfx::Boing);
  if (b.hp > 0)
    return;
  const Vec2 at = cellCenter(buddyBox());
  if (b.phase < 3)
  {
    // He spins on the spot and drops a health box.
    ++b.phase;
    b.hp = kBuddyHp;
    b.state = BuddyState::Spin;
    b.t = 0;
    b.nose = 0;
    b.waves.clear();
    Item it;
    it.kind = ItemKind::Health;
    const int k = b.phase == 2 ? 0 : 1;
    it.x = it.prevX = b.dropX[k] * kCellsPerTile;
    it.y = it.prevY = b.dropY * kCellsPerTile + 1 - 10;
    it.pickupDelay = 4;
    mItems.push_back(it);
    showMessage(b.phase == 2 ? "BUDDY BOOM: PIE TIME!" : "BUDDY BOOM REDRAWS THE SET!");
    burst(at, rgb(255, 230, 120), rgb(255, 255, 255), 20, 1.8f);
    playSound(Sfx::Applause);
    return;
  }
  b.hp = 0;
  b.state = BuddyState::Dizzy;
  b.t = 0;
  b.dizzyHits = 0;
  b.pies.clear();
  b.eraseSeg = -1;
  addScore(kBuddyScore, at);
  showMessage("BUDDY BOOM IS SEEING STARS!");
  playSound(Sfx::Cheer);
}

void World::buddyFall(bool holed)
{
  auto& b = mToon.buddy;
  b.state = BuddyState::Fall;
  b.t = 0;
  b.vy = 0.0f;
  b.holed = holed;
  if (holed)
  {
    addScore(5000, cellCenter(buddyBox()));
    showMessage("THE END?");
  }
  else
    showMessage("CRACK! DOWN HE GOES");
  setBuddyLine(false);
  b.lineY = -1;
  playSound(Sfx::Whistle);
}

int World::segmentUnder(int cx) const
{
  const auto& b = mToon.buddy;
  const int bx = cx / kCellsPerTile;
  if (bx < b.segX0 || bx >= b.segX0 + b.segs * b.segW)
    return -1;
  return (bx - b.segX0) / b.segW;
}

void World::updateBuddy()
{
  auto& b = mToon.buddy;
  auto& p = mPlayer;
  if (!b.on)
    return;
  b.prevX = b.x;
  b.prevY = b.y;
  if (b.flash > 0)
    --b.flash;
  if (b.nose > 0)
    --b.nose;
  if (b.whiteL > 0)
    --b.whiteL;
  if (b.whiteR > 0)
    --b.whiteR;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const float pcx = float(p.x) + 1.5f;
  const float bcx = b.x + Buddy::kW * 0.5f;
  const float floorFeet = float(b.floorY * kCellsPerTile - 1);
  // The floor's segments come back.
  for (int s = 0; s < b.segs; ++s)
    if (b.erased[std::size_t(s)] > 0 && --b.erased[std::size_t(s)] == 0)
    {
      setSegment(s, true);
      if (p.box().intersects(blockRect(b.segX0 + s * b.segW, b.floorY, b.segX0 + (s + 1) * b.segW - 1, b.floorY)))
        p.y = b.floorY * kCellsPerTile - 1;
    }
  // Shockwaves run along the floor; a segment that is gone stops one.
  for (auto& w : b.waves)
  {
    w.x += float(kWaveSpeed * w.dir);
    const int cx = int(std::floor(w.x));
    if (!mMap.solidTop(cx, w.y + 1) || mMap.solid(cx, w.y))
      w.dir = 0;
    else if (alive && CellBox{cx, w.y, 2, 1}.intersects(p.hitBox()))
    {
      hurtPlayer(1);
      w.dir = 0;
    }
  }
  b.waves.erase(std::remove_if(b.waves.begin(), b.waves.end(), [](const MalletWave& w) { return w.dir == 0; }), b.waves.end());
  // Cream pies.
  for (auto& pie : b.pies)
  {
    pie.vy = std::min(1.5f, pie.vy + 0.08f);
    pie.x += pie.vx;
    pie.y += pie.vy;
    const CellBox pb{int(pie.x), int(pie.y), 2, 2};
    if (alive && pb.intersects(p.hitBox()))
    {
      hurtPlayer(1);
      (pie.side < 0 ? b.whiteL : b.whiteR) = kPieWhite;
      playSound(Sfx::Squelch);
      pie.side = 9;
    }
    else if (mMap.solid(pb.x, pb.y + 1) || pie.y > float(mMap.height()))
    {
      burst({pie.x * kCellSize, pie.y * kCellSize}, rgb(255, 255, 255), rgb(250, 240, 220), 8, 1.2f);
      pie.side = 9;
    }
  }
  b.pies.erase(std::remove_if(b.pies.begin(), b.pies.end(), [](const CreamPie& c) { return c.side == 9; }), b.pies.end());

  if (b.state == BuddyState::Waiting)
  {
    if (alive && p.x > (b.doorX + 1) * kCellsPerTile && p.x < (b.x1 + 1) * kCellsPerTile &&
        p.y >= b.y0 * kCellsPerTile && p.y <= (b.y1 + 1) * kCellsPerTile)
    {
      shutBuddyDoor(true);
      b.state = BuddyState::Walk;
      b.cool = 30;
      b.dir = pcx < bcx ? -1 : 1;
      showMessage("IT'S BUDDY BOOM!");
      playSound(Sfx::Applause);
      mCamera.shake(8, 1.5f);
    }
    return;
  }
  ++b.t;
  // Phase 2: a Piano Bird across the top every 150 frames.
  if (b.phase == 2 && b.state != BuddyState::Spin && ++b.birdT >= kBirdEvery)
  {
    b.birdT = 0;
    spawnPianoBird(b.x0 * kCellsPerTile - 4);
  }
  auto clampX = [&](float x) {
    return std::clamp(x, float((b.x0 + 1) * kCellsPerTile), float(b.x1 * kCellsPerTile - Buddy::kW));
  };
  switch (b.state)
  {
    case BuddyState::Walk:
    {
      b.dir = pcx < bcx ? -1 : 1;
      if (std::abs(pcx - bcx) > 12.0f)
        b.x = clampX(b.x + 0.5f * float(b.dir));
      // On the floor (a segment that is gone: he stands on the ink regardless).
      b.y = floorFeet;
      if (--b.cool > 0 || !alive)
        break;
      if (std::abs(pcx - bcx) <= 14.0f || b.t > 90)
      {
        b.state = BuddyState::Slam;
        b.t = 0;
        playSound(Sfx::Rattle);
      }
      break;
    }
    case BuddyState::Slam:
      if (b.t == kSlamTell)
      {
        const CellBox body = buddyBox();
        playSound(Sfx::Crash);
        mCamera.shake(8, 2.0f);
        b.waves.push_back({float(body.left() - 2), body.bottom(), -1});
        b.waves.push_back({float(body.right() + 1), body.bottom(), 1});
        b.nose = kNoseFlash;
      }
      if (b.t >= kSlamTell + 6)
      {
        b.state = BuddyState::Walk;
        b.t = 0;
        b.cool = 45;
      }
      break;
    case BuddyState::Spin:
      if (b.t >= kSpin)
      {
        b.t = 0;
        b.attacks = 0;
        if (b.phase == 2)
        {
          b.state = BuddyState::Hop;
          b.target = pcx < bcx ? 1 : 0;
          b.birdT = kBirdEvery - 40;
          const float tx = float((b.platX[b.target][0] + b.platX[b.target][1] + 1) * kCellsPerTile / 2) - Buddy::kW * 0.5f;
          b.vx = (tx - b.x) / float(kHopLen);
          b.vy = -2.2f;
        }
        else
        {
          b.state = BuddyState::Pencil;
          if (!b.turboDropped && b.turboX >= 0)
          {
            b.turboDropped = true;
            Item it;
            it.kind = ItemKind::Turbo;
            it.x = it.prevX = b.turboX * kCellsPerTile;
            it.y = it.prevY = b.turboY * kCellsPerTile + 1 - 6;
            it.pickupDelay = 4;
            mItems.push_back(it);
          }
        }
      }
      break;
    case BuddyState::Hop:
    {
      b.x = clampX(b.x + b.vx);
      b.vy += 0.2f;
      b.y += b.vy;
      const float platFeet = float(b.platY * kCellsPerTile - 1);
      if (b.vy > 0.0f && b.t >= kHopLen / 2 && b.y >= platFeet)
      {
        b.y = platFeet;
        b.state = BuddyState::Pie;
        b.t = 0;
        b.attacks = 0;
        b.cool = 10;
        playSound(Sfx::Thud);
      }
      if (b.y > floorFeet)
        b.y = floorFeet;
      break;
    }
    case BuddyState::Pie:
      b.dir = pcx < bcx ? -1 : 1;
      if (b.cool > 0)
      {
        --b.cool;
        b.t = 0;
        break;
      }
      if (b.t == kPieTell && alive)
      {
        // A cream pie, lobbed at the runner.
        CreamPie pie;
        const CellBox body = buddyBox();
        pie.x = float(b.dir > 0 ? body.right() : body.left() - 1);
        pie.y = float(body.top() + 3);
        const float dx = pcx - pie.x;
        const float frames = std::clamp(std::abs(dx) / 1.0f, 12.0f, 40.0f);
        pie.vx = dx / frames;
        const float dy = float(p.y - 3) - pie.y;
        pie.vy = (dy - 0.04f * frames * frames) / frames;
        pie.side = pcx < bcx ? 1 : -1; // the half of the screen it comes from
        b.pies.push_back(pie);
        playSound(Sfx::Throw);
      }
      if (b.t >= kPieTell + 4)
      {
        b.t = 0;
        b.cool = 30;
        if (++b.attacks >= 2)
        {
          b.state = BuddyState::Hop;
          b.target = 1 - b.target;
          const float tx = float((b.platX[b.target][0] + b.platX[b.target][1] + 1) * kCellsPerTile / 2) - Buddy::kW * 0.5f;
          b.vx = (tx - b.x) / float(kHopLen);
          b.vy = -2.2f;
          b.t = 0;
        }
      }
      break;
    case BuddyState::Pencil:
    {
      // Up on the giant pencil, and a new floor line in the air.
      if (b.t == 1)
      {
        setBuddyLine(false);
        b.lineY = (b.attacks % 2 == 0) ? 26 : 22;
        const int px = (p.x + 1) / kCellsPerTile;
        b.lineX0 = std::clamp(px < (b.x0 + b.x1) / 2 ? px + 3 : px - 12, b.x0 + 1, b.x1 - 10);
        b.lineX1 = b.lineX0 + 9;
      }
      const float tx = float((b.lineX0 + b.lineX1 + 1) * kCellsPerTile / 2) - Buddy::kW * 0.5f;
      const float ty = float(b.lineY * kCellsPerTile - 1);
      b.x += std::clamp(tx - b.x, -1.5f, 1.5f);
      b.y += std::clamp(ty - b.y, -1.5f, 1.5f);
      if (b.t >= kPencilLen)
      {
        b.x = tx;
        b.y = ty;
        b.state = BuddyState::Draw;
        b.t = 0;
        playSound(Sfx::Trim);
      }
      break;
    }
    case BuddyState::Draw:
      if (b.t >= kOutline)
      {
        setBuddyLine(true);
        b.state = BuddyState::Stand;
        b.t = 0;
        b.cool = 30;
        b.attacks = 0;
      }
      break;
    case BuddyState::Stand:
      b.dir = pcx < bcx ? -1 : 1;
      if (--b.cool > 0 || !alive)
        break;
      if (b.attacks >= 3)
      {
        b.state = BuddyState::Pencil;
        b.t = 0;
        b.attacks = b.lineY == 26 ? 1 : 0;
        break;
      }
      {
        const int s = segmentUnder(p.x + 1);
        if (s >= 0 && b.erased[std::size_t(s)] == 0)
        {
          b.eraseSeg = s;
          b.state = BuddyState::Erase;
          b.t = 0;
          playSound(Sfx::Rattle);
        }
        else
          b.cool = 15;
      }
      break;
    case BuddyState::Erase:
      if (b.t >= kEraseTell)
      {
        if (b.eraseSeg >= 0)
        {
          setSegment(b.eraseSeg, false);
          b.erased[std::size_t(b.eraseSeg)] = kErased;
          playSound(Sfx::Swoosh);
          if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()) && !airRunning())
            startFalling();
        }
        b.eraseSeg = -1;
        ++b.attacks;
        b.state = BuddyState::Stand;
        b.t = 0;
        b.cool = 45;
      }
      break;
    case BuddyState::Dizzy:
      // A Portable Hole on the line under his feet drops him through.
      if (mToon.hole.open && !mToon.hole.wall && mToon.hole.y0 == b.lineY)
      {
        const CellBox body = buddyBox();
        const CellBox r = blockRect(mToon.hole.x0, mToon.hole.y0, mToon.hole.x1, mToon.hole.y1);
        if (r.right() >= body.left() && r.left() <= body.right())
        {
          buddyFall(true);
          break;
        }
      }
      if (b.t >= kDizzy)
        buddyFall(false);
      break;
    case BuddyState::Fall:
    {
      b.vy = std::min(2.0f, b.vy + 0.2f);
      const float inkFeet = float((b.floorY + 2) * kCellsPerTile);
      b.y = std::min(inkFeet, b.y + b.vy);
      if (b.y >= inkFeet)
      {
        b.deflate = std::min(1.0f, b.deflate + 1.0f / float(kDeflate));
        if (b.deflate >= 1.0f)
        {
          b.state = BuddyState::Done;
          shutBuddyDoor(false);
          showMessage("THAT'S ALL, FOLKS! THE IRIS OPENS");
          playSound(Sfx::Applause);
        }
      }
      break;
    }
    default:
      break;
  }
  // Running into him hurts (not while he is up on the pencil, dizzy or down).
  const bool solid = b.state == BuddyState::Walk || b.state == BuddyState::Slam || b.state == BuddyState::Spin ||
    b.state == BuddyState::Hop || b.state == BuddyState::Pie || b.state == BuddyState::Stand ||
    b.state == BuddyState::Erase;
  if (alive && solid && buddyBox().intersects(p.hitBox()))
    hurtPlayer(1);
}

bool World::shotAtToon(Projectile& pr, const CellBox& hit)
{
  auto& b = mToon.buddy;
  if (b.on && (b.state != BuddyState::Waiting && b.state != BuddyState::Fall && b.state != BuddyState::Done))
  {
    if (b.state == BuddyState::Dizzy)
    {
      const CellBox line = b.lineY >= 0 ? blockRect(b.lineX0, b.lineY, b.lineX1, b.lineY) : CellBox{-9, -9, 0, 0};
      if (buddyBox().intersects(hit) || line.intersects(hit))
      {
        damageBuddy(pr.damage);
        burst(cellCenter(hit), rgb(255, 255, 255), rgb(255, 230, 120), 5, 1.2f);
        return true;
      }
    }
    else if (b.phase == 1 && b.nose > 0 && buddyNose().intersects(hit))
    {
      damageBuddy(pr.damage);
      burst(cellCenter(hit), rgb(255, 80, 80), rgb(255, 255, 255), 6, 1.2f);
      return true;
    }
    else if (buddyBox().intersects(hit))
    {
      if (buddyHittable() && b.phase > 1)
      {
        damageBuddy(pr.damage);
        burst(cellCenter(hit), rgb(255, 255, 255), rgb(255, 230, 120), 5, 1.2f);
      }
      else
        playSound(Sfx::Tink); // rubber: it bounces off
      return true;
    }
  }
  return false;
}

// --- Each frame ----------------------------------------------------------------------------

void World::updateToon(const PlayerInput& input)
{
  (void)input;
  auto& t = mToon;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (t.tip > 0)
    --t.tip;
  if (t.retro > 0 && !t.hole.open)
    t.retro = 0;
  updateHole();
  updatePianos();
  for (auto& s : t.spawners)
  {
    if (s.enemy >= 0)
    {
      const Enemy& e = mEnemies[std::size_t(s.enemy)];
      if (!e.alive)
        s.enemy = -1; // one at a time: gone, the next may come
      continue;
    }
    if (!s.fired && alive && p.x >= s.trigger)
    {
      s.fired = true;
      spawnPianoBird(std::max(0, p.x - 40));
      s.enemy = int(mEnemies.size()) - 1;
    }
  }
  // Air-running past the canyon sign: a hat tipped to it.
  if (t.signX >= 0 && !t.tipped && t.airDir != 0 && std::abs(p.x + 1 - t.signX * kCellsPerTile) <= 2)
  {
    t.tipped = true;
    t.tip = 30;
    showMessage("GRAVITY HASN'T BEEN INVENTED YET");
  }
  // Secrets.
  if (alive && !t.studioFound && t.studio.intersects(p.box()))
  {
    t.studioFound = true;
    addScore(5000, cellCenter(p.box()));
    showMessage("SECRET: THE CARTOON STUDIO");
    playSound(Sfx::Glint);
  }
  if (alive && !t.dressingFound && t.dressing.intersects(p.box()))
  {
    t.dressingFound = true;
    addScore(5000, cellCenter(p.box()));
    showMessage("SECRET: BUDDY BOOM'S DRESSING ROOM");
    playSound(Sfx::Glint);
  }
  if (alive && t.prizeX >= 0 && !t.prizeTaken && blockRect(t.prizeX, t.prizeY - 1, t.prizeX, t.prizeY).intersects(p.box()))
  {
    t.prizeTaken = true;
    addScore(t.prizeScore, cellCenter(blockRect(t.prizeX, t.prizeY - 1, t.prizeX, t.prizeY)));
    showMessage("BUDDY BOOM'S ORIGINAL SKETCH!");
    playSound(Sfx::LettersComplete);
  }
  if (alive && t.fullhealX >= 0 && !t.fullhealUsed && p.hp < p.maxHp &&
      blockRect(t.fullhealX, t.fullhealY - 1, t.fullhealX, t.fullhealY).intersects(p.box()))
  {
    t.fullhealUsed = true;
    p.hp = p.maxHp;
    playSound(Sfx::Health);
    showMessage("FULL HEAL");
  }
  toonPond();
  updateBuddy();
}

void World::resetToon()
{
  auto& t = mToon;
  auto& b = t.buddy;
  t.airDir = 0;
  t.airT = 0;
  t.uhOh = 0;
  t.bonk = 0;
  t.bonkLatch = false;
  t.toss.on = false;
  if (t.hole.open)
    closeHole(false);
  for (auto& pn : t.pianos)
    if (pn.state == 2)
    {
      std::size_t i = 0;
      for (int by = pn.by; by < pn.by + kPianoH; ++by)
        for (int bx = pn.bx; bx < pn.bx + kPianoW; ++bx, ++i)
          if (Tile(pn.saved[i]) == Tile::Empty)
            mMap.setBlock(bx, by, Tile::Empty);
    }
  t.pianos.clear();
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::PianoBird && e.alive && std::size_t(&e - mEnemies.data()) >= mLevelEnemyCount)
      e.alive = false;
  for (auto& s : t.spawners)
  {
    s.enemy = -1;
    s.fired = false;
  }
  if (b.on && b.state != BuddyState::Done)
  {
    setBuddyLine(false);
    b.lineY = -1;
    for (int s = 0; s < b.segs; ++s)
      if (b.erased[std::size_t(s)] > 0)
      {
        b.erased[std::size_t(s)] = 0;
        setSegment(s, true);
      }
    b.state = BuddyState::Waiting;
    b.phase = 1;
    b.hp = kBuddyHp;
    b.x = b.prevX = float(b.startX);
    b.y = b.prevY = float(b.floorY * kCellsPerTile - 1);
    b.t = 0;
    b.cool = 30;
    b.attacks = 0;
    b.flash = 0;
    b.nose = 0;
    b.waves.clear();
    b.pies.clear();
    b.whiteL = b.whiteR = 0;
    b.eraseSeg = -1;
    b.deflate = 0.0f;
    if (b.doorShut)
      shutBuddyDoor(false);
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::toonCanSave() const
{
  const auto& t = mToon;
  if (mInk.on)
    return false;
  return !t.hole.open && !t.toss.on && t.pianos.empty() && t.airDir == 0 && t.uhOh == 0 && t.bonk == 0 &&
    (!t.buddy.on || t.buddy.state == BuddyState::Waiting || t.buddy.state == BuddyState::Done);
}

std::vector<int> World::toonSave() const
{
  // [studio, dressing room, prize, full heal, hat tipped, Buddy done, then per spawner fired]
  const auto& t = mToon;
  std::vector<int> s{t.studioFound ? 1 : 0, t.dressingFound ? 1 : 0, t.prizeTaken ? 1 : 0, t.fullhealUsed ? 1 : 0,
    t.tipped ? 1 : 0, t.buddy.state == BuddyState::Done ? 1 : 0};
  for (const auto& sp : t.spawners)
    s.push_back(sp.fired ? 1 : 0);
  return s;
}

bool World::validToonSave(const std::vector<int>& s) const
{
  return s.size() == 6 + mToon.spawners.size();
}

void World::loadToon(const std::vector<int>& s)
{
  auto& t = mToon;
  std::size_t i = 0;
  t.studioFound = s[i++] != 0;
  t.dressingFound = s[i++] != 0;
  t.prizeTaken = s[i++] != 0;
  t.fullhealUsed = s[i++] != 0;
  t.tipped = s[i++] != 0;
  if (s[i++] != 0 && t.buddy.on)
  {
    t.buddy.state = BuddyState::Done;
    t.buddy.phase = 3;
    t.buddy.hp = 0;
    t.buddy.deflate = 1.0f;
    t.buddy.y = float((t.buddy.floorY + 2) * kCellsPerTile);
  }
  for (auto& sp : t.spawners)
    sp.fired = s[i++] != 0;
}

// --- The Inkwell bonus ---------------------------------------------------------------------

void World::updateInk(const PlayerInput& input)
{
  auto& k = mInk;
  auto& p = mPlayer;
  // Lines fade.
  for (auto& b : k.blocks)
    if (++b.age >= kInkFade && mMap.block(b.x, b.y) == Tile::Platform)
      mMap.setBlock(b.x, b.y, Tile::Empty);
  k.blocks.erase(std::remove_if(k.blocks.begin(), k.blocks.end(), [](const InkBlock& b) { return b.age >= kInkFade; }),
    k.blocks.end());
  if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
    startFalling();
  // Refills.
  for (auto& r : k.refills)
    if (!r.taken && blockRect(r.x, r.y - 1, r.x, r.y).intersects(p.box()))
    {
      r.taken = true;
      k.ink = std::min(kInkMax, k.ink + kInkRefill);
      playSound(Sfx::Bloop);
      showMessage("INK +80");
    }
  // Off the bottom of the page: back to the start, with what you have.
  if (p.y >= mMap.height() - 2 && p.state != PlayerState::Dying)
  {
    p.x = p.prevX = k.startX;
    p.y = p.prevY = k.startY;
    p.state = PlayerState::OnGround;
    setVisual(PlayerVisual::Standing);
    k.drawing = false;
    playSound(Sfx::Whistle);
    return;
  }
  // Hold fire to draw: the pen tip starts under the runner's leading foot
  // and follows their aim a cell a frame.
  if (!input.fire.pressed || k.ink <= 0 || p.state == PlayerState::Dying)
  {
    k.drawing = false;
    return;
  }
  const int dx = p.facing;
  const int dy = input.up ? -1 : (input.down ? 1 : 0);
  if (!k.drawing || input.fire.triggered)
  {
    k.drawing = true;
    k.penX = float(dx > 0 ? p.x + Player::kWidth : p.x - 1);
    // The block row under the one the feet are in: a line drawn at the top
    // of a jump is landed on, not stood inside.
    k.penY = float((p.y / kCellsPerTile + 1) * kCellsPerTile);
  }
  else
  {
    k.penX += float(dx);
    k.penY += float(dy);
  }
  k.penDx = dx;
  k.penDy = dy;
  const int cx = int(k.penX), cy = int(k.penY);
  if (cx < 0 || cy < 0 || cx >= mMap.width() || cy >= mMap.height())
  {
    k.drawing = false;
    return;
  }
  --k.ink;
  const int bx = cx / kCellsPerTile, by = cy / kCellsPerTile;
  if (mMap.block(bx, by) == Tile::Empty && !p.box().intersects(blockRect(bx, by, bx, by)))
  {
    mMap.setBlock(bx, by, Tile::Platform);
    k.blocks.push_back({bx, by, 0});
  }
  else
    for (auto& b : k.blocks)
      if (b.x == bx && b.y == by)
        b.age = 0; // drawn over: fresh again
}

} // namespace gr
