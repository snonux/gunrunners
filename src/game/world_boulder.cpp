// Level 13, Boulder Run (SPEC 13): boulders that drop out of the ceiling
// and roll after you along the corridor (core 3.6 `@ boulder`), the alcoves
// to shelter in, the chutes that swallow them, the crack and the lead
// hatches (the secrets), Spear Runners, Pit Snakes and Totem Stacks.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kDustFrames = 15;  // dust from the hole before a boulder drops
constexpr int kShutFrames = 15;  // a chute's flaps shut this long after
constexpr int kLipFrames = 45;   // sheltering this long sets BD1 teetering
constexpr int kTeeterFrames = 90;
constexpr int kStepUp = 4;       // cells a boulder climbs onto a step
constexpr int kRearFrames = 20;  // a Pit Snake stays up this long
constexpr int kTotemPhase = 11;  // frames between a stack's heads spitting
const Color kStone = rgb(150, 120, 86);
const Color kStoneDark = rgb(92, 70, 48);
const Color kDust = rgb(214, 190, 150);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

// Speed `3/4` as cells per frame.
void parseSpeed(const std::string& s, int& num, int& den)
{
  const auto slash = s.find('/');
  num = std::max(1, std::atoi(s.substr(0, slash).c_str()));
  den = slash == std::string::npos ? 1 : std::max(1, std::atoi(s.substr(slash + 1).c_str()));
}

// Whether a circle (centre cx, cy, radius r, cells) touches a box.
bool circleHits(float cx, float cy, float r, const CellBox& b)
{
  const float nx = std::clamp(cx, float(b.x), float(b.x + b.w));
  const float ny = std::clamp(cy, float(b.y), float(b.y + b.h));
  return (nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) < r * r;
}

} // namespace

// --- Setup ----------------------------------------------------------------------------

bool World::setupBoulderEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "boulder" && e.hasPos)
  {
    Boulder b;
    b.size = e.num("size", 7) * kCellsPerTile;
    b.x = b.prevX = b.homeX = e.x * kCellsPerTile;
    b.y = b.prevY = b.homeY = e.y * kCellsPerTile;
    parseSpeed(e.str("speed", "3/4"), b.num, b.den);
    b.trigger = e.has("trigger") ? e.num("trigger", 0) * kCellsPerTile : -1;
    b.delay = e.num("delay", 0);
    b.haltFrames = e.num("haltframes", 90);
    b.haltX = e.has("halt") ? e.num("halt", 0) * kCellsPerTile : -1;
    // Alcoves and chutes are named; linkBoulders turns the names into indices.
    b.wake = e.has("wake") ? -2 - int(mAlcoveNames.size()) : -1;
    if (e.has("wake"))
      mAlcoveNames.push_back(e.str("wake"));
    b.teeter = e.has("teeter") ? -2 - int(mAlcoveNames.size()) : -1;
    if (e.has("teeter"))
      mAlcoveNames.push_back(e.str("teeter"));
    b.chute = e.has("chute") ? -2 - int(mChuteNames.size()) : -1;
    if (e.has("chute"))
      mChuteNames.push_back(e.str("chute"));
    b.camera = e.num("camera", 0) != 0 ? -2 : -1;
    if (e.has("exit"))
      mSurf.exitX = e.num("exit", 0) * kCellsPerTile; // Boulder Surfing: the exit ledge
    if (e.has("restart"))
    {
      // Boulder Surfing: `restart=1,20,40` (blocks), where a fall restarts.
      const std::string list = e.str("restart");
      std::size_t at = 0;
      while (at < list.size())
      {
        const std::size_t comma = std::min(list.find(',', at), list.size());
        mSurf.restarts.push_back(std::atoi(list.substr(at, comma - at).c_str()) * kCellsPerTile);
        at = comma + 1;
      }
    }
    mBoulders.push_back(b);
    return true;
  }
  if (e.kind == "chute")
  {
    // `@ chute ID x0 x1 row`: the flaps over an 8-block shaft.
    Chute c;
    c.bx0 = e.num("x0", e.x);
    c.bx1 = e.num("x1", e.x);
    c.row = e.num("row", e.y);
    c.x0 = c.bx0 * kCellsPerTile;
    c.x1 = (c.bx1 + 1) * kCellsPerTile - 1;
    mChutes.push_back(c);
    mChuteIds.push_back(e.id);
    return true;
  }
  if (e.kind == "alcove" && e.hasPos)
  {
    // The shelter: 4 blocks from x, its floor at row y.
    Alcove a;
    a.id = e.id;
    a.x0 = e.x * kCellsPerTile;
    a.x1 = (e.x + 4) * kCellsPerTile - 1;
    a.feet = e.y * kCellsPerTile - 1;
    mAlcoves.push_back(a);
    return true;
  }
  if (e.kind == "crack" && hasRect)
  {
    Crack c;
    c.bx0 = x0;
    c.by0 = y0;
    c.bx1 = x1;
    c.by1 = y1;
    c.still = e.num("still", 30);
    c.alcove = -2 - int(mAlcoveNames.size());
    mAlcoveNames.push_back(e.str("alcove"));
    mCracks.push_back(c);
    return true;
  }
  if (e.kind == "leadhatch" && hasRect)
  {
    LeadHatch h;
    h.bx0 = x0;
    h.by0 = y0;
    h.bx1 = x1;
    h.by1 = y1;
    h.lead = e.num("lead", 0) * kCellsPerTile;
    h.near = e.num("near", 8) * kCellsPerTile;
    h.ladderX = e.num("ladder", -1);
    mLeadHatches.push_back(h);
    return true;
  }
  return false;
}

void World::setupBoulderEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::SpearRunner)
  {
    en.active = true; // wakes 20 blocks ahead, off screen
    en.attach = 0;
    en.dir = 1;
  }
  if (en.kind == EnemyKind::PitSnake)
  {
    // In its hole: (x, y) is the hole's block; coiled, only its head shows.
    en.x = en.prevX = e.x * kCellsPerTile;
    en.y = en.prevY = e.y * kCellsPerTile + 1;
    en.h = 2;
    en.aimY = e.y * kCellsPerTile; // the hole's top row
    en.attach = 0;
  }
  if (en.kind == EnemyKind::Totem)
  {
    // (x, y) is the top head's block; the stack stands on the block under
    // its last head.
    const int heads = std::clamp(e.num("heads", 4), 1, 6);
    en.x = en.prevX = e.x * kCellsPerTile;
    en.y = en.prevY = (e.y + heads) * kCellsPerTile - 1;
    en.w = 2;
    en.h = heads * kCellsPerTile;
    en.hp = 2 * heads;
    en.attach = 0; // heads whose blocks are solid (syncTotems)
    en.railX0 = heads;
  }
}

void World::linkBoulders()
{
  auto alcoveIndex = [&](int code) {
    if (code > -2)
      return code;
    const std::string& name = mAlcoveNames[std::size_t(-2 - code)];
    for (std::size_t i = 0; i < mAlcoves.size(); ++i)
      if (mAlcoves[i].id == name)
        return int(i);
    return -1;
  };
  for (auto& b : mBoulders)
  {
    b.wake = alcoveIndex(b.wake);
    b.teeter = alcoveIndex(b.teeter);
    if (b.chute <= -2)
    {
      const std::string& name = mChuteNames[std::size_t(-2 - b.chute)];
      b.chute = -1;
      for (std::size_t i = 0; i < mChuteIds.size(); ++i)
        if (mChuteIds[i] == name)
          b.chute = int(i);
    }
    // The candid camera put on the boulder rides along.
    if (b.camera == -2)
    {
      b.camera = -1;
      for (std::size_t i = 0; i < mEnemies.size(); ++i)
        if (mEnemies[i].kind == EnemyKind::Camera && mEnemies[i].box().intersects(b.box()))
          b.camera = int(i);
    }
    // The teetering boulder carries the bonus patch: it is not there before.
    if (b.teeter >= 0)
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor)
          pr.dormant = true;
  }
  for (auto& c : mCracks)
    c.alcove = alcoveIndex(c.alcove);
  for (auto& c : mChutes)
    setChute(c, false);
  for (auto& h : mLeadHatches)
  {
    h.open = true; // so the first update shuts it
  }
  syncTotems();
}

// --- Helpers ----------------------------------------------------------------------------

bool World::inShelter(int alcove) const
{
  if (alcove < 0 || std::size_t(alcove) >= mAlcoves.size())
    return false;
  const Alcove& a = mAlcoves[std::size_t(alcove)];
  const auto& p = mPlayer;
  return p.state == PlayerState::OnGround && p.y == a.feet && p.x >= a.x0 && p.x + Player::kWidth - 1 <= a.x1;
}

// The highest floor under a boulder's footprint, looked for from `top`
// down: the row of the first solid cell. -1 when there is none (the cliff).
int World::rollFloor(int x, int top, int size) const
{
  int best = INT_MAX;
  for (int c = x; c < x + size; ++c)
  {
    if (c < 0 || c >= mMap.width())
      continue;
    for (int y = std::max(0, top); y < mMap.height(); ++y)
      if (mMap.solid(c, y))
      {
        best = std::min(best, y);
        break;
      }
  }
  return best == INT_MAX ? -1 : best;
}

void World::setChute(Chute& c, bool open)
{
  c.open = open;
  for (int tx = c.bx0; tx <= c.bx1; ++tx)
    mMap.setBlock(tx, c.row, open ? Tile::Empty : Tile::Solid);
}

// A Totem Stack's heads are solid blocks; as heads go, the blocks go.
void World::syncTotems()
{
  for (auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::Totem)
      continue;
    const int heads = e.alive ? (e.hp + 1) / 2 : 0;
    const int tx = e.x / kCellsPerTile, bottom = e.y / kCellsPerTile;
    // Solid blocks for the heads there are, empty above them.
    for (int i = 0; i < e.railX0; ++i)
      mMap.setBlock(tx, bottom - i, i < heads ? Tile::Solid : Tile::Empty);
    e.attach = heads;
    e.h = std::max(1, heads) * kCellsPerTile;
  }
}

void World::resetBoulders()
{
  // Back to the last alcove: the boulder that was after you goes back up
  // its hole and drops again when you cross its line.
  for (auto& b : mBoulders)
  {
    if (b.state == Boulder::Wait || b.state == Boulder::Gone || b.state == Boulder::Chute)
      continue;
    if (b.wake >= 0)
      continue; // the demo boulder rolls on
    b.state = Boulder::Wait;
    b.x = b.prevX = b.homeX;
    b.y = b.prevY = b.homeY;
    b.acc = b.vy = b.timer = 0;
    b.halted = false;
    b.shelter = 0;
  }
}

// --- Per frame --------------------------------------------------------------------------

void World::updateBoulders(const PlayerInput& /*input*/)
{
  if (mBoulders.empty() && mCracks.empty())
    return;
  const auto& p = mPlayer;
  mStill = p.state == PlayerState::OnGround && p.x == p.prevX && p.y == p.prevY ? mStill + 1 : 0;

  // The demo boulder's line, and the WRONG WAY sign.
  if (!mWrongWay && !mSurfing && p.x < 4 * kCellsPerTile && p.state != PlayerState::Dying)
  {
    mWrongWay = true;
    showMessage("WRONG WAY, HERO");
  }

  for (auto& b : mBoulders)
  {
    if (b.state == Boulder::Surf)
      continue; // ridden: updateSurf moves it
    b.prevX = b.x;
    b.prevY = b.y;
    b.prevAngle = b.angle;
    updateBoulder(b);
    if (b.camera >= 0)
    {
      Enemy& cam = mEnemies[std::size_t(b.camera)];
      cam.x = cam.prevX = b.x + 2;
      cam.y = cam.prevY = b.y + 4 + cam.h - 1;
      cam.drawSnap = true;
      cam.hidden = b.state == Boulder::Wait || b.state == Boulder::Gone || b.state == Boulder::Chute;
    }
  }

  for (auto& c : mChutes)
    if (c.shut > 0 && --c.shut == 0)
    {
      setChute(c, false);
      if (isOnScreen({c.x0, c.row * kCellsPerTile, c.x1 - c.x0 + 1, 2}, 4))
        playSound(Sfx::Crash);
    }

  // The crack in the alcove wall shows once you have stood still in it.
  for (auto& c : mCracks)
  {
    if (c.glint > 0)
      --c.glint;
    if (c.open || !inShelter(c.alcove) || mStill < c.still)
      continue;
    c.open = true;
    c.glint = 30;
    for (int ty = c.by0; ty <= c.by1; ++ty)
      for (int tx = c.bx0; tx <= c.bx1; ++tx)
        mMap.setBlock(tx, ty, Tile::Empty);
    playSound(Sfx::Chime);
    flashAt({(float(c.bx0) + 0.5f) * kCellsPerTile * kCellSize, (float(c.by0) + 1.0f) * kCellsPerTile * kCellSize},
      60.0f, rgb(255, 230, 160), 20);
  }

  // Lead hatches: open only with a lead on the boulder, never with one close.
  for (auto& h : mLeadHatches)
  {
    bool close = false;
    int lead = INT_MAX;
    bool any = false;
    for (const auto& b : mBoulders)
    {
      if (!b.rolling())
        continue;
      any = true;
      const CellBox hb{h.bx0 * kCellsPerTile, h.by0 * kCellsPerTile, (h.bx1 - h.bx0 + 1) * kCellsPerTile, 2};
      const int gap = std::max(hb.x - (b.x + b.size), b.x - (hb.x + hb.w));
      close = close || gap < h.near;
      if (b.x < p.x)
        lead = std::min(lead, p.x - (b.x + b.size));
    }
    const bool open = !close && (h.lead == 0 || (any && lead != INT_MAX && lead >= h.lead));
    if (open == h.open)
      continue;
    h.open = open;
    for (int ty = h.by0; ty <= h.by1; ++ty)
      for (int tx = h.bx0; tx <= h.bx1; ++tx)
        mMap.setBlock(tx, ty, open ? (tx == h.ladderX ? Tile::Ladder : Tile::Empty) : Tile::Solid);
    if (isOnScreen({h.bx0 * kCellsPerTile, h.by0 * kCellsPerTile, 4, 2}, 2))
      playSound(Sfx::Clunk);
  }

  // Fan Darts break a thrown spear.
  for (auto& s : mProjectiles)
  {
    if (!s.alive || !s.spear)
      continue;
    for (auto& d : mProjectiles)
      if (d.alive && d.kind == ShotKind::Proto && d.proto == int(ProtoId::FanDarts) && d.box().intersects(s.box()))
      {
        s.alive = d.alive = false;
        burst(cellCenter(s.box()), rgb(255, 255, 255), rgb(180, 140, 90), 6, 1.2f);
        playSound(Sfx::Snap);
        break;
      }
  }

  syncTotems();
}

void World::updateBoulder(Boulder& b)
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const float r = float(b.size) * 0.5f - 0.5f;
  switch (b.state)
  {
    case Boulder::Wait:
      if (b.wake >= 0)
      {
        // The demo boulder: on the floor already, it sets off a while after
        // the runner first stands in the shelter.
        if (b.timer > 0 || inShelter(b.wake))
          ++b.timer;
        if (b.timer > b.delay)
        {
          b.state = Boulder::Roll;
          b.timer = 0;
          playSound(Sfx::Rumble);
        }
        return;
      }
      // Infected runners are too slow: the boulder waits out the Virus.
      if (b.trigger >= 0 && alive && p.virus == 0 && p.x + 1 >= b.trigger)
      {
        b.state = Boulder::Dust;
        b.timer = kDustFrames;
        playSound(Sfx::Rumble);
        mCamera.shake(kDustFrames, 1.0f);
      }
      return;
    case Boulder::Dust:
      if (mStats.frames % 2 == 0)
        burst({(float(b.x) + float(b.size) * 0.5f + float(int(hash2(mStats.frames, b.x) % 9u)) - 4.0f) * kCellSize,
                float(b.y + b.size) * kCellSize},
          kDust, kStoneDark, 2, 0.6f, false);
      if (--b.timer <= 0)
      {
        b.state = Boulder::Drop;
        b.vy = 0;
      }
      return;
    case Boulder::Halt:
    case Boulder::Teeter:
      if (--b.timer > 0)
      {
        if (b.state == Boulder::Teeter)
          b.angle = b.prevAngle = 0.08f * std::sin(float(b.timer) * 0.4f);
        return;
      }
      if (b.state == Boulder::Teeter)
        for (auto& pr : mProps)
          if (pr.kind == PropKind::BonusDoor && !pr.used)
            pr.dormant = true;
      b.state = Boulder::Roll;
      return;
    case Boulder::Lip:
      // At the chute's lip: sheltering long enough sets it teetering.
      if (inShelter(b.teeter))
      {
        if (++b.shelter >= kLipFrames)
        {
          b.state = Boulder::Teeter;
          b.timer = kTeeterFrames;
          for (auto& pr : mProps)
            if (pr.kind == PropKind::BonusDoor && !pr.used)
              pr.dormant = false;
          playSound(Sfx::Creak);
        }
        return;
      }
      b.state = Boulder::Roll;
      b.teeter = -1;
      return;
    case Boulder::Chute:
    {
      b.vy = std::min(b.vy + 1, 4);
      b.y += b.vy;
      const Chute& c = mChutes[std::size_t(b.chute)];
      if (b.y > (c.row + 1) * kCellsPerTile + 2)
      {
        b.state = Boulder::Gone;
        mChutes[std::size_t(b.chute)].shut = kShutFrames;
        if (isOnScreen(b.box(), 6))
          playSound(Sfx::Crash);
      }
      return;
    }
    case Boulder::Fly:
      // Off the end of the floor: an arc out over the sky.
      b.acc += b.num;
      while (b.acc >= b.den)
      {
        b.acc -= b.den;
        ++b.x;
      }
      if (mStats.frames % 2 == 0)
        b.vy = std::min(b.vy + 1, 3);
      b.y += b.vy;
      b.angle += float(b.num) / float(b.den) / (float(b.size) * 0.5f);
      if (b.y > mMap.height() + 4)
        b.state = Boulder::Gone;
      break;
    case Boulder::Drop:
    case Boulder::Roll:
    {
      if (b.state == Boulder::Roll)
      {
        // Its chute: once it is right over it the flaps open.
        if (b.chute >= 0)
        {
          Chute& c = mChutes[std::size_t(b.chute)];
          if (b.teeter >= 0 && b.x + b.size / 2 >= c.x0 - 1 && b.x < c.x0)
          {
            b.state = Boulder::Lip;
            return;
          }
          if (b.x >= c.x0 + 1)
          {
            setChute(c, true);
            b.state = Boulder::Chute;
            b.vy = 0;
            if (isOnScreen(b.box(), 6))
              playSound(Sfx::Clunk);
            return;
          }
        }
        if (b.haltX >= 0 && !b.halted && b.x >= b.haltX)
        {
          b.halted = true;
          b.state = Boulder::Halt;
          b.timer = b.haltFrames;
          return;
        }
        b.acc += b.num;
        while (b.acc >= b.den)
        {
          b.acc -= b.den;
          ++b.x;
          b.angle += 1.0f / (float(b.size) * 0.5f);
        }
        // The teetering boulder counts the runner sheltering behind it.
        if (b.teeter >= 0)
        {
          const Alcove& a = mAlcoves[std::size_t(b.teeter)];
          if (b.x > a.x1)
            b.shelter = inShelter(b.teeter) ? b.shelter + 1 : 0;
        }
      }
      const int floor = rollFloor(b.x, b.y + b.size - kStepUp - 1, b.size);
      if (floor < 0)
      {
        if (b.state == Boulder::Roll)
        {
          b.state = Boulder::Fly;
          b.vy = 0;
        }
        else
          b.y += 2;
        break;
      }
      const int want = floor - b.size;
      if (b.y > want)
        b.y = std::max(want, b.y - 2); // up a step
      else if (b.y < want)
      {
        b.vy = std::min(b.vy + 1, 4);
        b.y = std::min(want, b.y + b.vy);
        if (b.y == want)
        {
          if (b.state == Boulder::Drop || b.vy >= 3)
          {
            mCamera.shake(10, 3.0f);
            playSound(Sfx::Crash);
            burst({(float(b.x) + float(b.size) * 0.5f) * kCellSize, float(b.y + b.size) * kCellSize}, kDust, kStoneDark,
              24, 2.2f, false);
          }
          b.vy = 0;
          b.state = Boulder::Roll;
        }
      }
      else
      {
        b.vy = 0;
        b.state = Boulder::Roll;
      }
      // The rumble.
      if (b.state == Boulder::Roll && mStats.frames % 20 == 0 && isOnScreen(b.box(), 30))
      {
        playSound(Sfx::Rumble);
        mCamera.shake(4, 1.0f);
      }
      break;
    }
    case Boulder::Gone:
      return;
  }

  // It crushes what it touches.
  const float cx = float(b.x) + float(b.size) * 0.5f, cy = float(b.y) + float(b.size) * 0.5f;
  if (alive && p.turbo == 0 && !mGod && circleHits(cx, cy, r, p.box()))
  {
    if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
      std::fprintf(stderr, "crushed at %d,%d frame %d\n", p.x, p.y, mStats.frames);
    killPlayer();
  }
  for (auto& e : mEnemies)
    if (e.alive && !e.hidden && e.kind != EnemyKind::Camera && circleHits(cx, cy, r, e.box()))
      damageEnemy(e, 1000);
}

// --- Shots ------------------------------------------------------------------------------

bool World::shotAtTotem(Projectile& pr, const CellBox& b)
{
  if (pr.kind == ShotKind::Enemy)
    return false;
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Totem || !e.box().intersects(b))
      continue;
    const int heads = (e.hp + 1) / 2;
    damageEnemy(e, pr.damage);
    burst(cellCenter(b), rgb(255, 255, 255), rgb(200, 150, 90), 5, 1.2f);
    // The heads above the one that went drop a block; each head scores.
    const int now = e.alive ? (e.hp + 1) / 2 : 0;
    if (now < heads && e.alive)
    {
      addScore(enemyDef(e.def).score, cellCenter(e.box()));
      playSound(Sfx::Clunk);
    }
    syncTotems();
    return true;
  }
  return false;
}

// --- Enemies ----------------------------------------------------------------------------

void World::updateSpearRunner(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  // Falls off steps.
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  switch (e.attach)
  {
    case 0:
    {
      // Asleep until the runner comes within 20 blocks behind it (one awake at a time).
      if (p.x >= e.x || e.x - p.x > 40 || std::abs(p.y - e.y) > 12 || p.state == PlayerState::Dying)
        return;
      for (const auto& o : mEnemies)
        if (&o != &e && o.alive && o.kind == EnemyKind::SpearRunner && o.attach > 0 && o.attach < 4)
          return;
      e.attach = 1;
      e.dir = 1;
      playSound(Sfx::Yell);
      return;
    }
    case 2:
      // Turned round with the spear up: then it throws.
      e.dir = -1;
      if (--e.tell > 0)
        return;
      {
        Projectile pr;
        pr.kind = ShotKind::Enemy;
        pr.dx = -1;
        pr.speed = 2;
        pr.w = 3;
        pr.h = 1;
        pr.x = pr.prevX = e.x - 3;
        pr.y = pr.prevY = e.y - 3;
        pr.spear = true;
        mProjectiles.push_back(pr);
        playSound(Sfx::Whoosh);
      }
      e.attach = 3;
      e.dir = 1;
      return;
    default:
      break;
  }
  // Fleeing right at 3/4 cell a frame; once, it turns to throw.
  if (e.attach == 1 && p.x < e.x && e.x - (p.x + Player::kWidth) <= def.range && p.state != PlayerState::Dying)
  {
    e.attach = 2;
    e.tell = def.tell;
    e.dir = -1;
    return;
  }
  if (e.timer % 4 == 0)
    return;
  const CellBox ahead = boxAt(e.x + 1, e.y, e.w, e.h);
  if (!mMap.overlapsSolid(ahead))
    ++e.x;
  else if (!mMap.overlapsSolid(boxAt(e.x + 1, e.y - 2, e.w, e.h)))
  {
    e.x += 1;
    e.y -= 2;
  }
  // Out of the corridor (an alcove's drop or the end): it is gone.
  if (e.x > mMap.width() - 2)
    e.alive = false;
}

void World::updatePitSnake(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  const int ecx = e.x + 1, pcx = pb.x + 1;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  switch (e.attach)
  {
    case 0: // coiled in its hole
      if (vulnerable && std::abs(pcx - ecx) <= def.range && std::abs(pb.bottom() - e.aimY) <= 6)
      {
        e.attach = 1;
        e.tell = def.tell;
        if (isOnScreen(e.box(), 2))
          playSound(Sfx::Hiss);
      }
      break;
    case 1: // hissing, dust puffing out of the hole
      if (e.tell % 4 == 0)
        burst({float(ecx) * kCellSize, float(e.aimY) * kCellSize}, kDust, kStoneDark, 2, 0.5f, false);
      if (--e.tell > 0)
        break;
      e.attach = 2;
      e.dive = 0;
      e.h = 4;
      e.dir = pcx < ecx ? -1 : 1;
      break;
    case 2: // reared: strikes a block up and a block forward
    {
      ++e.dive;
      if (e.dive >= 2 && e.dive < 8 && vulnerable)
      {
        const CellBox strike{e.dir > 0 ? e.x + e.w : e.x - 2, e.y - e.h - 1, 2, 3};
        if (strike.intersects(p.hitBox()))
          touchPlayer(e);
      }
      if (e.dive >= kRearFrames)
      {
        e.attach = 3;
        e.h = 2;
        e.cool = def.cooldown;
      }
      break;
    }
    default: // back in the hole, catching its breath
      if (--e.cool <= 0)
        e.attach = 0;
      break;
  }
}

void World::updateTotem(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (p.state == PlayerState::Dying)
    return;
  const int heads = (e.hp + 1) / 2;
  const int cd = std::max(1, def.cooldown);
  e.dir = p.x + 1 < e.x ? -1 : 1;
  // Each head spits on its own phase; its mouth glows first.
  e.tell = 0;
  for (int i = 0; i < heads; ++i)
  {
    const int t = ((mStats.frames - i * kTotemPhase) % cd + cd) % cd;
    const int top = e.y - e.h + 1 + i * kCellsPerTile;
    if (t >= cd - def.tell)
      e.tell = i + 1;
    if (t == 0 && isOnScreen(e.box(), 2))
    {
      Projectile pr;
      pr.kind = ShotKind::Enemy;
      pr.dx = e.dir;
      pr.speed = 2;
      pr.w = 2;
      pr.h = 1;
      pr.x = pr.prevX = e.dir > 0 ? e.x + e.w : e.x - 2;
      pr.y = pr.prevY = top + 1;
      mProjectiles.push_back(pr);
      playSound(Sfx::Dart);
    }
  }
}

// --- Drawing ----------------------------------------------------------------------------

void World::drawBoulderBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  // The crack's glint, the chutes' flaps.
  for (const auto& c : mCracks)
  {
    const float x = float(c.bx0) * 64.0f - camX, y = float(c.by0) * 64.0f - camY;
    if (!visible(x, y, 64, 128))
      continue;
    if (c.open)
    {
      r.fillRect(x + 8, y, 48, float(c.by1 - c.by0 + 1) * 64.0f, rgba(20, 12, 6, 160));
      if (c.glint > 0)
        drawGlow(r, mArt, x + 32, y + 64, 90, rgb(255, 230, 160), float(c.glint) / 30.0f);
    }
  }
  for (const auto& c : mChutes)
  {
    const float x = float(c.bx0) * 64.0f - camX, y = float(c.row) * 64.0f - camY;
    const float w = float(c.bx1 - c.bx0 + 1) * 64.0f;
    if (!visible(x, y, w, 64))
      continue;
    if (!c.open)
    {
      // Two stone flaps with a seam in the middle.
      r.fillRect(x, y, w, 10, rgb(120, 92, 62));
      r.fillRect(x + w * 0.5f - 2, y, 4, 64, rgba(30, 20, 10, 180));
      for (float gx = x + 24.0f; gx < x + w; gx += 64.0f)
        r.fillRect(gx, y + 20, 16, 4, rgba(40, 28, 16, 140));
    }
    else
    {
      // Hanging open.
      r.fillRect(x, y, 10, 64, rgb(120, 92, 62));
      r.fillRect(x + w - 10, y, 10, 64, rgb(120, 92, 62));
    }
  }
  (void)frame;
}

void World::drawBoulderFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  for (const auto& b : mBoulders)
  {
    if (b.state == Boulder::Gone)
      continue;
    const float bx = (float(b.prevX) + float(b.x - b.prevX) * alpha) * kCellPx - camX;
    const float by = (float(b.prevY) + float(b.y - b.prevY) * alpha) * kCellPx - camY;
    const float size = float(b.size) * kCellPx;
    if (!visible(bx, by, size, size))
      continue;
    const float cx = bx + size * 0.5f, cy = by + size * 0.5f, rad = size * 0.5f;
    // The stone: a shaded ball, and carvings that turn as it rolls.
    const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "boulder", 0, 0, b.size, b.size).get(1);
    r.draw(*tex, cx, by + size);
    const float a = b.prevAngle + (b.angle - b.prevAngle) * alpha;
    for (int k = 0; k < 6; ++k)
    {
      const float ang = a + float(k) * 1.0472f;
      const float c0 = std::cos(ang), s0 = std::sin(ang);
      // Only the carvings on the front half show.
      const float px = cx + c0 * rad * 0.62f, py = cy + s0 * rad * 0.62f;
      const float dx = -s0 * rad * 0.18f, dy = c0 * rad * 0.18f;
      r.drawLine(px - dx, py - dy, px + dx, py + dy, 7.0f, rgba(60, 40, 24, 170));
    }
    // A glyph ring around the middle.
    for (int k = 0; k < 12; ++k)
    {
      const float ang = a * 1.0f + float(k) * 0.5236f;
      r.fillRect(cx + std::cos(ang) * rad * 0.3f - 5, cy + std::sin(ang) * rad * 0.3f - 5, 10, 10,
        rgba(70, 48, 28, 150));
    }
    // The bonus patch on its flank while it teeters.
    if (b.state == Boulder::Teeter)
      drawGlow(r, mArt, bx + size * 0.12f, by + size * 0.35f, 70, rgb(160, 220, 255), 0.5f + 0.2f * std::sin(float(frame) * 0.4f));
    // The stagehand's door while it halts.
    if (b.state == Boulder::Halt)
    {
      const float open = std::min(1.0f, float(std::min(b.timer, b.haltFrames - b.timer)) / 15.0f);
      r.fillRect(cx - 40, cy - 30, 80, 110, rgb(40, 28, 18));
      r.fillRect(cx - 40, cy - 30, 80.0f * (1.0f - open), 110, rgb(170, 140, 100));
      if (open > 0.6f)
        r.drawText("*nod*", cx, cy - 60, {28.0f, rgb(255, 240, 200), rgb(40, 20, 10)}, Align::Center);
    }
  }

  // Spears in flight.
  for (const auto& pr : mProjectiles)
  {
    if (!pr.alive || !pr.spear)
      continue;
    const float x = float(pr.x) * kCellPx - camX, y = float(pr.y) * kCellPx - camY + 16.0f;
    r.drawLine(x, y, x + 96.0f, y, 6.0f, rgb(120, 80, 40));
    r.drawLine(x - 8.0f, y, x + 14.0f, y, 10.0f, rgb(220, 220, 230));
  }

  // A boulder off screen to the left: an arrow at the edge and how far.
  const auto& p = mPlayer;
  int nearest = INT_MAX;
  for (const auto& b : mBoulders)
    if (b.rolling() && b.state != Boulder::Dust && b.x + b.size < p.x)
    {
      const float sx = float(b.x + b.size) * kCellPx - camX;
      if (sx < 0.0f)
        nearest = std::min(nearest, p.x - (b.x + b.size));
    }
  if (nearest != INT_MAX)
  {
    const float y = float(kScreenH) * 0.5f;
    const float pulse = 0.6f + 0.4f * std::sin(float(frame) * 0.3f);
    r.fillRect(0, y - 70, 14, 140, rgba(255, 120, 40, int(120 * pulse)));
    r.drawLine(40, y - 30, 16, y, 8.0f, rgb(255, 210, 120));
    r.drawLine(16, y, 40, y + 30, 8.0f, rgb(255, 210, 120));
    r.drawText(std::to_string(nearest / kCellsPerTile), 56, y + 12, {30.0f, rgb(255, 230, 180), rgb(40, 20, 10)});
  }
}

} // namespace gr
