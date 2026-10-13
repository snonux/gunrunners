// Level 37, Wardrobe: changing booths (each runner with their own hearts),
// heavy plates, shutters and cutting tables, spring racks under ceiling
// spikes, the archive's push rack and vault door, the Decoy Launcher,
// Mannequins, Powder Puffers and Rack Runners, the easter eggs (the
// Gorgazilla suit, the bulb mirror's mustache, the OUT OF ORDER booth), and
// the Totem bonus (rules=totem). Drawing is in world_wardrobe_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

bool alive(const Player& p) { return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting; }

// `runner=` on a booth: Dash, Rocco, Nova; -1 any (the exit), -2 the toon booth.
int boothRunner(const std::string& name)
{
  if (name == "dash")
    return 0;
  if (name == "rocco")
    return 1;
  if (name == "nova")
    return 2;
  if (name == "toon")
    return -2;
  return -1;
}

std::size_t poolSlot(int index) { return std::size_t(std::clamp(index, 0, 7)); }

const char* runnerWord(int index) { return index == 1 ? "ROCCO" : index == 2 ? "NOVA" : "DASH"; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupWardrobeEntity(const EntityDef& e)
{
  const bool totem = mLevel->rules.find("totem") != std::string::npos;
  if (mLevel->themeKey != "wardrobe" && !totem)
    return false;
  auto& s = mWardrobe;
  s.on = true;
  s.totem = totem;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (e.kind == "booth" && e.hasPos)
  {
    Booth b;
    b.x = e.x * kCellsPerTile;
    b.y = e.y * kCellsPerTile + 1;
    b.runner = boothRunner(e.str("runner", "any"));
    b.exit = e.num("exit", 0) != 0;
    s.booths.push_back(b);
    return true;
  }
  if (e.kind == "switch" && e.str("kind") == "plate")
  {
    HeavyPlate pl;
    pl.id = e.id;
    pl.x = e.x * kCellsPerTile;
    pl.y = e.y * kCellsPerTile + 1;
    pl.w = e.num("w", 2) * kCellsPerTile;
    pl.weight = e.num("weight", 2);
    pl.latch = e.num("latch", 1) != 0;
    pl.target = e.str("target");
    pl.onTable = e.str("table");
    s.plates.push_back(pl);
    return true;
  }
  if (e.kind == "shutter" && hasRect)
  {
    Shutter sh;
    sh.id = e.id;
    sh.x0 = x0;
    sh.y0 = y0;
    sh.x1 = x1;
    sh.y1 = y1;
    s.shutters.push_back(sh);
    return true;
  }
  if (e.kind == "table" && hasRect)
  {
    CutTable t;
    t.id = e.id;
    t.x0 = x0;
    t.x1 = x1;
    t.row = y0;
    t.toRow = e.num("to", y0);
    s.tables.push_back(t);
    return true;
  }
  if (e.kind == "rack" && e.hasPos)
  {
    SpringRack r;
    r.x = e.x * kCellsPerTile;
    r.y = e.y * kCellsPerTile + 1;
    r.w = e.num("w", 2) * kCellsPerTile;
    s.racks.push_back(r);
    return true;
  }
  if (e.kind == "pushrack" && e.hasPos)
  {
    PushRack r;
    r.id = e.id;
    r.x = r.homeX = e.x;
    r.y = e.y;
    r.w = e.num("w", 2);
    r.h = e.num("h", 3);
    s.pushRacks.push_back(r);
    return true;
  }
  if (e.kind == "vaultdoor" && e.hasPos)
  {
    VaultDoor d;
    d.x = e.x * kCellsPerTile;
    d.y = e.y * kCellsPerTile + 1;
    const auto to = e.list("to");
    if (to.size() == 2)
    {
      d.toX = to[0] * kCellsPerTile;
      d.toY = to[1] * kCellsPerTile + 1;
    }
    s.doors.push_back(d);
    return true;
  }
  if (e.kind == "suit" && e.hasPos)
  {
    s.suitX = e.x * kCellsPerTile;
    s.suitY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "mirror" && e.hasPos)
  {
    s.mirrorX = e.x * kCellsPerTile;
    s.mirrorY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "curtain" && hasRect)
  {
    s.curtains.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile});
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    WardrobeStep st;
    st.x = e.x * kCellsPerTile;
    st.y = e.y * kCellsPerTile + 1;
    st.act = e.str("act");
    st.arg = e.str("arg");
    s.route.push_back(st);
    return true;
  }
  if (e.kind == "tower" && e.hasPos)
  {
    s.towers.push_back({e.x * kCellsPerTile, e.num("plate", e.x + 5) * kCellsPerTile});
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's boa and top hat are only drawn
  return false;
}

void World::setupWardrobeEnemy(Enemy& en, const EntityDef& e)
{
  auto& s = mWardrobe;
  const int i = int(&en - mEnemies.data());
  if (en.kind == EnemyKind::Mannequin)
    s.mannequins.push_back({i, 0, true, int(s.mannequins.size()) % 3});
  else if (en.kind == EnemyKind::PowderPuffer)
  {
    Puffer f;
    f.enemy = i;
    f.homeY = en.y;
    s.puffers.push_back(f);
  }
  else if (en.kind == EnemyKind::RackRunner)
  {
    RackRunner r;
    r.enemy = i;
    const auto lane = e.list("lane");
    r.x0 = lane.size() == 2 ? lane[0] * kCellsPerTile : en.x - 20;
    r.x1 = lane.size() == 2 ? (lane[1] + 1) * kCellsPerTile : en.x + en.w + 20;
    s.rackRunners.push_back(r);
  }
}

void World::linkWardrobe()
{
  auto& s = mWardrobe;
  if (!s.on)
    return;
  for (const auto& sh : s.shutters)
    for (int ty = sh.y0; ty <= sh.y1; ++ty)
      for (int tx = sh.x0; tx <= sh.x1; ++tx)
        mMap.setBlock(tx, ty, Tile::Solid);
  for (const auto& t : s.tables)
    for (int tx = t.x0; tx <= t.x1; ++tx)
      mMap.setBlock(tx, t.row, Tile::Platform);
  for (const auto& r : s.pushRacks)
    for (int ty = r.y; ty < r.y + r.h; ++ty)
      for (int tx = r.x; tx < r.x + r.w; ++tx)
        mMap.setBlock(tx, ty, Tile::Solid);
  s.checkpointRunner = mCharacterIndex;
  s.pools.fill(-1);
  s.pools[poolSlot(mCharacterIndex)] = mPlayer.hp;
  if (s.totem)
    resetTotem();
}

// --- Runners -------------------------------------------------------------------------------

// The player becomes runner `index` where they stand; each runner keeps
// their own hearts (full the first time). The Totem shares one pool.
void World::becomeRunner(int index)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  if (index < 0 || index >= characterCount())
    return;
  if (!s.totem)
    s.pools[poolSlot(mCharacterIndex)] = p.hp;
  const auto& next = characterByIndex(index);
  mCharacter = next;
  mCharacterIndex = index;
  if (s.totem)
    return;
  p.maxHp = next.maxHp;
  int& pool = s.pools[poolSlot(index)];
  p.hp = pool < 0 ? p.maxHp : std::clamp(pool, 1, p.maxHp);
  pool = p.hp;
}

bool World::wardrobeHeavy() const
{
  return mCharacterIndex == 1 || mCharacterIndex == 5 || mPlayer.turbo > 0;
}

int World::wardrobeWeight() const
{
  const auto& s = mWardrobe;
  if (!s.totem)
    return wardrobeHeavy() ? 2 : 1;
  int w = 0;
  for (int i = totemPlayer(); i >= 0; i = totemAbove(i))
    w += s.bodies[std::size_t(i)].runner == 1 ? 2 : 1;
  return w;
}

// Cells a spring rack throws the runner: Rocco 4, Dash 14, Nova 22 (4 more
// for every cell of jump past Dash's), any Turbo runner 14.
int World::rackLaunch() const
{
  if (mPlayer.turbo > 0)
    return kRackTurbo;
  const int h = jumpHeight();
  return h <= 6 ? kRackRocco : kRackDash + (h - 7) * 4;
}

// --- Input -----------------------------------------------------------------------------------

PlayerInput World::wardrobeInput(const PlayerInput& raw)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  PlayerInput in = raw;
  const bool upEdge = in.up && !s.upHeld;
  s.upHeld = in.up;
  s.pushDir = in.right && !in.left ? 1 : in.left && !in.right ? -1 : 0;
  if (s.totem)
    return totemInput(in);
  if (s.boothT > 0 || s.sit > 0)
  {
    s.pushDir = 0;
    return PlayerInput{};
  }
  if (s.tumble > 0)
    in.left = in.right = false;
  if (!alive(p) || p.state != PlayerState::OnGround)
    return in;
  if (upEdge)
  {
    const int mid = p.x + 1;
    for (std::size_t i = 0; i < s.booths.size(); ++i)
    {
      auto& b = s.booths[i];
      if (b.exit || mid < b.x || mid >= b.x + 4 || std::abs(p.y - b.y) > 1)
        continue;
      s.booth = int(i);
      s.boothT = kBoothCurtain;
      s.boothTo = b.runner;
      b.curtain = kBoothCurtain + 6;
      p.mercy = std::max(p.mercy, kBoothCurtain + 2);
      playSound(Sfx::Swoosh);
      return PlayerInput{};
    }
    for (const auto& d : s.doors)
    {
      if (mid < d.x - 1 || mid > d.x + 2 || std::abs(p.y - d.y) > 1)
        continue;
      if (vaultDoorBlocked(d))
      {
        showMessage("SOMETHING HEAVY IS IN FRONT OF THE DOOR");
        playSound(Sfx::Thud);
        return PlayerInput{};
      }
      p.x = p.prevX = d.toX;
      p.y = p.prevY = d.toY;
      mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
      playSound(Sfx::Creak);
      return PlayerInput{};
    }
  }
  if (in.down && s.mirrorX >= 0 && !s.mustache && std::abs(p.x + 1 - (s.mirrorX + 2)) <= 2 && std::abs(p.y - s.mirrorY) <= 1)
  {
    s.sit = kSitFrames;
    playSound(Sfx::Creak);
    return PlayerInput{};
  }
  return in;
}

// The OUT OF ORDER booth's cartoon: off a ledge holding the way, the runner
// runs on air for a few frames.
bool World::wardrobeAirRun(int mvX)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  if (!s.toon || mvX == 0 || mvX != p.facing || s.toonAir >= kToonAirRun)
    return false;
  if (s.toonAir == 0)
    playSound(Sfx::SlideWhistle);
  ++s.toonAir;
  int x = p.x;
  mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), mvX);
  p.x = x;
  setVisual(PlayerVisual::Walking);
  ++p.walkFrame;
  return true;
}

bool World::vaultDoorBlocked(const VaultDoor& d) const
{
  const int col = d.x / kCellsPerTile, row = d.y / kCellsPerTile;
  for (const auto& r : mWardrobe.pushRacks)
    if (col >= r.x && col < r.x + r.w && row >= r.y && row < r.y + r.h)
      return true;
  return false;
}

// --- Plates, shutters, tables, racks ---------------------------------------------------------

CutTable* World::wardrobeTable(const std::string& id)
{
  for (auto& t : mWardrobe.tables)
    if (t.id == id)
      return &t;
  return nullptr;
}

void World::pressPlate(HeavyPlate& pl)
{
  auto& s = mWardrobe;
  pl.down = true;
  playSound(Sfx::Click);
  for (auto& sh : s.shutters)
    if (sh.id == pl.target && !sh.open)
    {
      sh.open = true;
      sh.t = 0;
      for (int ty = sh.y0; ty <= sh.y1; ++ty)
        for (int tx = sh.x0; tx <= sh.x1; ++tx)
          mMap.setBlock(tx, ty, Tile::Empty);
      playSound(Sfx::Shutter);
      showMessage(s.totem ? "THE GATE GOES UP" : "THE SHUTTER ROLLS UP");
    }
  if (CutTable* t = wardrobeTable(pl.target); t && !t->lower)
  {
    t->lower = true;
    t->t = 0;
    playSound(Sfx::Rattle);
    showMessage("THE CUTTING TABLE COMES DOWN");
  }
}

void World::updateHeavyPlates()
{
  auto& s = mWardrobe;
  const auto& p = mPlayer;
  const bool standing = alive(p) && p.state == PlayerState::OnGround;
  const int weight = wardrobeWeight();
  for (auto& pl : s.plates)
  {
    if (pl.down && pl.latch)
      continue;
    const CutTable* t = pl.onTable.empty() ? nullptr : wardrobeTable(pl.onTable);
    const bool ready = !t || t->down();
    const bool on = standing && ready && p.y == pl.y && p.x < pl.x + pl.w && pl.x < p.x + Player::kWidth;
    if (on && weight < pl.weight)
    {
      if (pl.hint == 0)
        showMessage(s.totem ? "THE PLATE NEEDS ALL THREE OF YOU" : "TOO LIGHT - THE PLATE NEEDS ROCCO");
      pl.hint = 45;
    }
    if (pl.hint > 0)
      --pl.hint;
    if (on && weight >= pl.weight && !pl.down)
      pressPlate(pl);
    else if (!pl.latch && !(on && weight >= pl.weight))
      pl.down = false;
  }
  for (auto& sh : s.shutters)
    if (sh.open && sh.t < 12)
      ++sh.t;
}

void World::updateTables()
{
  for (auto& t : mWardrobe.tables)
  {
    if (!t.lower || t.down())
      continue;
    if (++t.t % kTableStep != 0)
      continue;
    for (int tx = t.x0; tx <= t.x1; ++tx)
      mMap.setBlock(tx, t.row, Tile::Empty);
    ++t.row;
    for (int tx = t.x0; tx <= t.x1; ++tx)
      mMap.setBlock(tx, t.row, Tile::Platform);
    if (t.down())
    {
      playSound(Sfx::Clunk);
      mCamera.shake(4, 0.8f);
    }
  }
}

void World::updateRacks()
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  for (auto& r : s.racks)
    if (r.bounce > 0)
      --r.bounce;
  if (!alive(p))
    return;
  if (p.state == PlayerState::OnGround)
  {
    int under = -1;
    for (std::size_t i = 0; i < s.racks.size(); ++i)
    {
      const auto& r = s.racks[i];
      if (p.y == r.y && p.x < r.x + r.w && r.x < p.x + Player::kWidth)
        under = int(i);
    }
    if (under >= 0 && under != s.onRack && !s.tumbled)
    {
      startLaunch(rackLaunch());
      s.racks[std::size_t(under)].bounce = 8;
      playSound(Sfx::Boing);
    }
    s.onRack = under;
    s.tumbled = false;
  }
  else
  {
    s.onRack = -1;
    // Ceiling spikes: a heart, and a tumble back down without air control.
    if (s.tumble == 0 && mMap.overlapsHazard({p.x, p.y - p.height(), Player::kWidth, 2}))
    {
      hurtPlayer(1);
      mLaunch = 0;
      if (p.state != PlayerState::Dying)
        startFalling();
      s.tumble = kTumble;
      s.tumbled = true;
      playSound(Sfx::Crash);
    }
  }
}

void World::updatePushRacks()
{
  auto& s = mWardrobe;
  const auto& p = mPlayer;
  for (auto& r : s.pushRacks)
  {
    if (r.slide > 0)
      --r.slide;
    const int rx = r.x * kCellsPerTile, rw = r.w * kCellsPerTile;
    const int bottom = (r.y + r.h) * kCellsPerTile - 1;
    const bool against = s.pushDir > 0 ? p.x + Player::kWidth == rx : s.pushDir < 0 && p.x == rx + rw;
    if (!alive(p) || p.state != PlayerState::OnGround || p.y != bottom || !against)
    {
      r.push = 0;
      continue;
    }
    if (wardrobeWeight() < 2)
    {
      if (r.push++ == 0)
        showMessage("IT WON'T BUDGE - TOO LIGHT");
      continue;
    }
    if (++r.push < kPushRackEvery * 2)
      continue;
    r.push = 0;
    const int d = s.pushDir;
    if (std::abs(r.x + d - r.homeX) > 3)
      continue;
    const int into = d > 0 ? r.x + r.w : r.x - 1;
    bool free = mMap.block(into, r.y + r.h) == Tile::Solid;
    for (int ty = r.y; ty < r.y + r.h && free; ++ty)
      free = mMap.block(into, ty) == Tile::Empty;
    if (!free)
      continue;
    const int leave = d > 0 ? r.x : r.x + r.w - 1;
    for (int ty = r.y; ty < r.y + r.h; ++ty)
    {
      mMap.setBlock(leave, ty, Tile::Empty);
      mMap.setBlock(into, ty, Tile::Solid);
    }
    r.x += d;
    r.slide = 4;
    playSound(Sfx::Creak);
  }
}

// --- The Decoy Launcher ----------------------------------------------------------------------

void World::fireDecoy(const Button& fire)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  if (!fire.triggered || p.ammo <= 0 || (p.shotCooldown > 0 && p.turbo == 0) || !canFire())
    return;
  if (s.decoys.size() >= 3)
    s.decoys.erase(s.decoys.begin());
  Decoy d;
  d.x = float(p.facing > 0 ? p.x + Player::kWidth : p.x - 3);
  d.y = float(p.y - 1);
  if (mMap.overlapsSolid(d.box()))
    d.x = float(p.x);
  d.vx = 2.0f * float(p.facing);
  d.vy = -2.5f;
  d.facing = -p.facing; // it lands looking back at you: enemies past it come to it
  d.look = mCharacterIndex;
  s.decoys.push_back(d);
  p.shotCooldown = protoDef(p.proto).cooldown;
  playSound(Sfx::Throw);
  if (p.turbo == 0 && --p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    showMessage("OUT OF DECOYS - BACK TO THE BLASTER");
  }
}

void World::updateDecoys()
{
  auto& s = mWardrobe;
  for (auto& d : s.decoys)
  {
    if (d.landed)
    {
      --d.life;
      if (!mMap.onSolidGround(d.box()))
        d.y = std::min(d.y + 2.0f, float(mMap.height() + 8));
      continue;
    }
    // Lobbed: across, up and over, cell by cell.
    d.vy = std::min(d.vy + 0.5f, 3.0f);
    const int steps = int(std::ceil(std::max(std::fabs(d.vx), std::fabs(d.vy))));
    for (int k = 0; k < steps && !d.landed; ++k)
    {
      const float nx = d.x + d.vx / float(steps);
      if (!mMap.overlapsSolid(boxAt(int(std::floor(nx)), int(d.y), 3, 5)))
        d.x = nx;
      else
        d.vx = 0.0f;
      const float ny = d.y + d.vy / float(steps);
      if (d.vy > 0.0f && mMap.onSolidGround(boxAt(int(d.x), int(std::floor(ny)), 3, 5)))
      {
        d.y = std::floor(ny);
        d.landed = true;
      }
      else if (mMap.overlapsSolid(boxAt(int(d.x), int(std::floor(ny)), 3, 5)))
        d.vy = std::max(d.vy, 0.0f);
      else
        d.y = ny;
    }
    if (d.y > float(mMap.height() + 8))
      d.life = 0;
    if (d.landed)
    {
      // The landing bonk.
      const Vec2 c = cellCenter(d.box());
      for (auto& e : mEnemies)
      {
        if (!e.alive)
          continue;
        const Vec2 ec = cellCenter(e.box());
        if (std::fabs(ec.x - c.x) <= float(kDecoyBonk * kCellSize) && std::fabs(ec.y - c.y) <= float(kDecoyBonk * kCellSize))
          damageEnemy(e, protoDef(int(ProtoId::DecoyLauncher)).damage);
      }
      burst(c, rgb(240, 200, 170), rgb(255, 255, 255), 10, 1.4f, false);
      playSound(Sfx::Thud);
    }
  }
  s.decoys.erase(std::remove_if(s.decoys.begin(), s.decoys.end(), [&](const Decoy& d) {
    if (d.life > 0 && d.hp > 0)
      return false;
    burst(cellCenter(d.box()), rgb(240, 200, 170), rgb(120, 90, 80), 12, 1.6f, false);
    playSound(Sfx::Puff);
    return true;
  }), s.decoys.end());
}

// A landed decoy within reach of (x, y) that enemies go for, -1 if none.
int World::decoyNear(int x, int y) const
{
  const auto& s = mWardrobe;
  int best = -1, bestD = kDecoyLure + 1;
  for (std::size_t i = 0; i < s.decoys.size(); ++i)
  {
    const auto& d = s.decoys[i];
    if (!d.landed)
      continue;
    const int dist = std::abs(int(d.x) + 1 - x);
    if (dist < bestD && std::abs(int(d.y) - y) <= 12)
    {
      bestD = dist;
      best = int(i);
    }
  }
  return best;
}

// --- Enemies -------------------------------------------------------------------------------

void World::updateMannequin(Enemy& e, const EnemyDef& def)
{
  auto& s = mWardrobe;
  Mannequin* m = nullptr;
  for (auto& k : s.mannequins)
    if (k.enemy == int(&e - mEnemies.data()))
      m = &k;
  if (!m)
    return;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (!mMap.onSolidGround(e.box()))
  {
    int y = e.y;
    mMap.moveVertically(e.x, y, e.w, e.h, 2);
    e.y = y;
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const int mid = e.x + e.w / 2;
  const int lure = decoyNear(mid, e.y);
  // Watched: the runner faces it, or a decoy stands facing it.
  bool watched = alive(p) && std::abs(p.x + 1 - mid) <= 48 && std::abs(p.y - e.y) <= 16 &&
                 sgn(mid - (p.x + 1)) == p.facing;
  for (const auto& d : s.decoys)
    if (d.landed && std::abs(int(d.x) + 1 - mid) <= kDecoyLure && std::abs(int(d.y) - e.y) <= 8 &&
        sgn(mid - (int(d.x) + 1)) == d.facing)
      watched = true;
  if (watched)
  {
    m->frozen = true;
    m->twitch = 0;
  }
  else if (m->frozen)
  {
    if (m->twitch == 0)
    {
      m->twitch = kMannequinTwitch;
      playSound(Sfx::Creak);
    }
    else if (--m->twitch == 0)
      m->frozen = false;
  }
  // Contact: the runner (not in the suit), a decoy.
  if (!s.suit && alive(p) && e.cool == 0 && e.box().intersects(p.hitBox()))
  {
    hurtPlayer(1);
    e.cool = def.cooldown;
  }
  if (lure >= 0 && e.cool == 0 && e.box().intersects(s.decoys[std::size_t(lure)].box()))
  {
    --s.decoys[std::size_t(lure)].hp;
    e.cool = def.cooldown;
    playSound(Sfx::Thud);
  }
  if (m->frozen || e.timer % std::max(1, def.stepEvery) != 0)
    return;
  // Walks to the decoy, else the runner; away from them in the suit.
  int tx = -1;
  if (lure >= 0)
    tx = int(s.decoys[std::size_t(lure)].x) + 1;
  else if (alive(p) && std::abs(p.x + 1 - mid) <= 48 && std::abs(p.y - e.y) <= 12)
    tx = p.x + 1;
  if (tx < 0 || std::abs(tx - mid) <= 1)
    return;
  int dir = sgn(tx - mid);
  if (s.suit && lure < 0)
    dir = -dir;
  e.dir = dir;
  const CellBox b = e.box();
  const int ahead = dir > 0 ? b.right() + 1 : b.x - 1;
  if ((dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)) || !mMap.solidTop(ahead, b.bottom() + 1))
    return;
  e.x += dir;
  m->pose = (m->pose + 1) % 4;
}

void World::updatePowderPuffer(Enemy& e, const EnemyDef& def)
{
  auto& s = mWardrobe;
  Puffer* f = nullptr;
  for (auto& k : s.puffers)
    if (k.enemy == int(&e - mEnemies.data()))
      f = &k;
  if (!f)
    return;
  auto& p = mPlayer;
  e.y = f->homeY - ((e.timer / 10) % 2); // bobs a cell
  if (f->cloud > 0)
  {
    const CellBox cloud{f->cloudX, f->cloudY, kPuffW, kPuffH};
    if (alive(p) && cloud.intersects(p.box()))
    {
      ++s.puffed;
      if (e.carrier && !f->hit)
      {
        f->hit = true;
        infect();
      }
    }
    if (--f->cloud == 0)
    {
      e.cool = def.cooldown;
      f->hit = false;
    }
    return;
  }
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      f->cloud = kPuffCloud;
      f->cloudDir = e.dir;
      f->cloudX = e.dir > 0 ? e.x + e.w : e.x - kPuffW;
      f->cloudY = f->homeY - kPuffH - 1;
      f->hit = false;
      playSound(Sfx::Puff);
    }
    return;
  }
  if (s.suit)
    return; // they keep their distance from Gorgazilla
  // Puffs at a decoy, else at the runner, in its sight on its band.
  const int mid = e.x + e.w / 2;
  int tx = -1;
  const int lure = decoyNear(mid, f->homeY);
  if (lure >= 0)
    tx = int(s.decoys[std::size_t(lure)].x) + 1;
  else if (alive(p) && std::abs(p.x + 1 - mid) <= kPuffSight && p.y >= f->homeY - kPuffH - 1 && p.y - 4 <= f->homeY)
    tx = p.x + 1;
  if (tx < 0)
    return;
  e.dir = tx < mid ? -1 : 1;
  e.tell = def.tell;
  playSound(Sfx::Creak);
}

void World::updateRackRunner(Enemy& e, const EnemyDef& def)
{
  auto& s = mWardrobe;
  RackRunner* r = nullptr;
  for (auto& k : s.rackRunners)
    if (k.enemy == int(&e - mEnemies.data()))
      r = &k;
  if (!r)
    return;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  // Contact: knocked back 3 blocks and a heart (not in the suit).
  if (!s.suit && alive(p) && p.mercy == 0 && e.box().intersects(p.hitBox()))
  {
    const int dir = (p.x + 1) < e.x + e.w / 2 ? -1 : 1;
    hurtPlayer(1);
    int x = p.x;
    mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), dir * kRackKnock);
    p.x = x;
    playSound(Sfx::Rattle);
  }
  if (r->stopped > 0)
  {
    --r->stopped;
    return;
  }
  if (r->roll != 0)
  {
    const int dir = sgn(r->roll);
    for (int k = 0; k < 2 && r->roll != 0; ++k)
    {
      // A decoy in its way stops it while it stands.
      const CellBox nb{e.x + dir, e.y - e.h + 1, e.w, e.h};
      for (auto& d : s.decoys)
        if (d.landed && nb.intersects(d.box()))
        {
          d.hp -= 2;
          r->stopped = d.life;
          r->roll = 0;
          playSound(Sfx::Thud);
          return;
        }
      const bool wall = dir > 0 ? mMap.touchingRightWall(e.box()) : mMap.touchingLeftWall(e.box());
      if (wall || e.x + dir < r->x0 || e.x + e.w + dir > r->x1)
      {
        r->roll = 0;
        break;
      }
      e.x += dir;
      r->roll -= dir;
    }
    if (r->roll == 0)
      e.cool = def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
      r->roll = e.dir * (r->x1 - r->x0);
    return;
  }
  if (e.cool > 0)
    return;
  // Rolls to the far end (away from Gorgazilla).
  int dir = (e.x - r->x0) < (r->x1 - (e.x + e.w)) ? 1 : -1;
  if (s.suit && alive(p))
    dir = (p.x + 1) < e.x + e.w / 2 ? 1 : -1;
  if ((dir > 0 && e.x + e.w >= r->x1) || (dir < 0 && e.x <= r->x0))
    return;
  e.dir = dir;
  e.tell = def.tell;
  playSound(Sfx::Rattle);
}

// --- Every frame -----------------------------------------------------------------------------

void World::updateWardrobe(const PlayerInput& input)
{
  (void)input;
  auto& s = mWardrobe;
  auto& p = mPlayer;
  if (s.totem)
  {
    updateTotem();
    updateHeavyPlates();
    return;
  }
  for (auto& b : s.booths)
    if (b.curtain > 0)
      --b.curtain;
  if (s.boothT > 0)
  {
    p.mercy = std::max(p.mercy, 2);
    if (--s.boothT == 0)
    {
      const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize};
      if (s.boothTo == -2)
      {
        s.toon = true;
        showMessage("OUT OF ORDER - YOU'RE A CARTOON NOW");
        playSound(Sfx::SlideWhistle);
      }
      else if (s.boothTo >= 0)
      {
        if (s.boothTo != mCharacterIndex)
          becomeRunner(s.boothTo);
        showMessage(std::string(runnerWord(s.boothTo)) + " STEPS OUT");
      }
      if (s.suitTouched && !s.suit)
      {
        s.suit = true;
        showMessage("GORGAZILLA STEPS OUT - THEY'RE RUNNING!");
      }
      burst(c, mArt.runnerColor(mCharacter), rgb(255, 255, 255), 16, 1.8f);
      s.booth = -1;
    }
  }
  if (s.sit > 0 && --s.sit == 0)
  {
    s.mustache = true;
    showMessage("LOOKING SHARP");
    playSound(Sfx::Bell);
  }
  if (s.tumble > 0)
    --s.tumble;
  if (p.state == PlayerState::OnGround && mMap.onSolidGround(p.box()))
    s.toonAir = 0;
  // A new checkpoint: the runner who touched it is who you come back as;
  // the suit, the mustache and the cartoon come off.
  int active = 0;
  for (const auto& cp : mCheckpoints)
    active += cp.active ? 1 : 0;
  if (active > s.checkpointsSeen)
  {
    s.checkpointsSeen = active;
    s.checkpointRunner = mCharacterIndex;
    s.suit = s.suitTouched = s.mustache = s.toon = false;
  }
  if (s.suitX >= 0 && !s.suitTouched && !s.suit && alive(p) && p.box().intersects(boxAt(s.suitX, s.suitY, 4, 6)))
  {
    s.suitTouched = true;
    showMessage("A GORGAZILLA SUIT - TRY IT ON IN A BOOTH");
    playSound(Sfx::Click);
  }
  if (s.puffed > 0)
    --s.puffed;
  updateHeavyPlates();
  updateTables();
  updateRacks();
  updatePushRacks();
  updateDecoys();
  // The bot's way through: past every step that is done (a booth while the
  // runner is the one it wants, a plate down) or reached and done.
  while (s.routeAt < int(s.route.size()) && wardrobeStepDone(s.routeAt))
    ++s.routeAt;
}

bool World::wardrobeStepDone(int i) const
{
  const auto& s = mWardrobe;
  const auto& st = s.route[std::size_t(i)];
  if (st.act == "booth")
    return mCharacterIndex == boothRunner(st.arg);
  if (st.act == "plate")
  {
    for (const auto& pl : s.plates)
      if (pl.id == st.arg)
        return pl.down;
    return true;
  }
  return true;
}

void World::resetWardrobe()
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  if (!s.on)
    return;
  s.booth = -1;
  s.boothT = 0;
  s.sit = 0;
  s.tumble = 0;
  s.tumbled = false;
  s.onRack = -1;
  s.decoys.clear();
  s.suit = s.suitTouched = s.mustache = s.toon = false;
  s.toonAir = 0;
  for (auto& r : s.rackRunners)
    r.stopped = 0;
  if (s.totem)
  {
    resetTotem();
    return;
  }
  // Back as the runner who touched the checkpoint, every pool full.
  if (s.checkpointRunner >= 0 && s.checkpointRunner != mCharacterIndex)
    becomeRunner(s.checkpointRunner);
  s.pools.fill(-1);
  p.maxHp = mCharacter.maxHp;
  p.hp = p.maxHp;
  s.pools[poolSlot(mCharacterIndex)] = p.hp;
  // The route goes back to the furthest step at or before where the runner is.
  while (s.routeAt > 0 && s.route[std::size_t(s.routeAt - 1)].x > p.x + 8)
    --s.routeAt;
}

// --- Totem -----------------------------------------------------------------------------------

int World::totemPlayer() const
{
  const auto& b = mWardrobe.bodies;
  for (std::size_t i = 0; i < b.size(); ++i)
    if (b[i].active)
      return int(i);
  return -1;
}

int World::totemAbove(int i) const
{
  const auto& b = mWardrobe.bodies;
  for (std::size_t j = 0; j < b.size(); ++j)
    if (b[j].on == i)
      return int(j);
  return -1;
}

int World::totemBase(int i) const
{
  const auto& b = mWardrobe.bodies;
  while (i >= 0 && b[std::size_t(i)].on >= 0)
    i = b[std::size_t(i)].on;
  return i;
}

// The three of them in one stack at the respawn point, Rocco at the bottom.
void World::resetTotem()
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  s.bodies.clear();
  const int order[3] = {1, 0, 2};
  for (int k = 0; k < 3; ++k)
  {
    TotemBody b;
    b.runner = order[k];
    b.x = p.x;
    b.y = p.y - k * Player::kHeight;
    b.on = k - 1;
    b.active = k == 0;
    s.bodies.push_back(b);
  }
  s.downHeld = 0;
  s.throwT = 0;
  if (mCharacterIndex != 1)
    becomeRunner(1);
}

// Stacked bodies follow the one under them; true if they all fit.
bool World::totemStackFits(int base, int x, int y) const
{
  int k = 1;
  for (int i = totemAbove(base); i >= 0; i = totemAbove(i), ++k)
    if (mMap.overlapsSolid(boxAt(x, y - k * Player::kHeight, Player::kWidth, Player::kHeight)))
      return false;
  return true;
}

void World::totemFollow(int base)
{
  auto& b = mWardrobe.bodies;
  int k = 1;
  for (int i = totemAbove(base); i >= 0; i = totemAbove(i), ++k)
  {
    b[std::size_t(i)].x = b[std::size_t(base)].x;
    b[std::size_t(i)].y = b[std::size_t(base)].y - k * Player::kHeight;
  }
}

// Control passes to body i (where it stands).
void World::totemControl(int i)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  const int cur = totemPlayer();
  if (cur >= 0)
  {
    s.bodies[std::size_t(cur)].active = false;
    s.bodies[std::size_t(cur)].x = p.x;
    s.bodies[std::size_t(cur)].y = p.y;
  }
  auto& b = s.bodies[std::size_t(i)];
  b.active = true;
  p.x = p.prevX = b.x;
  p.y = p.prevY = b.y;
  becomeRunner(b.runner);
}

PlayerInput World::totemInput(PlayerInput in)
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  const int me = totemPlayer();
  if (me < 0 || !alive(p))
    return in;
  const bool stacked = totemAbove(me) >= 0;
  // Down held: control to the group with Rocco in it, else the next one.
  if (in.down && p.state == PlayerState::OnGround)
  {
    if (++s.downHeld >= kTotemSwitch)
    {
      s.downHeld = 0;
      int to = -1;
      for (std::size_t i = 0; i < s.bodies.size(); ++i)
        if (s.bodies[i].runner == 1)
          to = totemBase(int(i));
      if (to == me || to < 0)
        for (int k = 1; k <= int(s.bodies.size()); ++k)
        {
          const int j = (me + k) % int(s.bodies.size());
          if (s.bodies[std::size_t(j)].on < 0 && j != me)
          {
            to = j;
            break;
          }
        }
      if (to >= 0 && to != me)
      {
        totemControl(to);
        playSound(Sfx::Click);
      }
    }
    in.down = false;
    in.left = in.right = false;
    return in;
  }
  s.downHeld = 0;
  if (!stacked)
    return in;
  // The stack walks; jump throws the top runner, who is played from then on.
  if (in.jump.triggered && p.state == PlayerState::OnGround)
  {
    int top = me;
    while (totemAbove(top) >= 0)
      top = totemAbove(top);
    auto& t = s.bodies[std::size_t(top)];
    const int under = t.on;
    t.on = -1;
    s.bodies[std::size_t(me)].active = false;
    s.bodies[std::size_t(me)].x = p.x;
    s.bodies[std::size_t(me)].y = p.y;
    (void)under;
    t.active = true;
    p.x = p.prevX = t.x;
    p.y = p.prevY = t.y;
    becomeRunner(t.runner);
    startLaunch(kTotemThrowUp);
    s.throwT = kTotemThrowFwd;
    s.throwDir = p.facing;
    playSound(Sfx::Throw);
    in.jump = Button{};
    in.left = in.right = false;
    return in;
  }
  in.jump = Button{};
  return in;
}

void World::updateTotem()
{
  auto& s = mWardrobe;
  auto& p = mPlayer;
  const int me = totemPlayer();
  if (me < 0)
    return;
  // A throw carries the runner forward.
  if (s.throwT > 0)
  {
    --s.throwT;
    int x = p.x;
    mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), s.throwDir);
    p.x = x;
  }
  auto& b = s.bodies;
  // The stack must fit where the runner walked.
  if (totemAbove(me) >= 0 && !totemStackFits(me, p.x, p.y))
  {
    p.x = p.prevX;
    if (!totemStackFits(me, p.x, p.y))
      p.y = p.prevY;
  }
  b[std::size_t(me)].x = p.x;
  b[std::size_t(me)].y = p.y;
  totemFollow(me);
  // Bodies on their own fall to the floor.
  for (std::size_t i = 0; i < b.size(); ++i)
  {
    if (b[i].active || b[i].on >= 0)
      continue;
    for (int k = 0; k < 2; ++k)
      if (!mMap.onSolidGround(boxAt(b[i].x, b[i].y, Player::kWidth, Player::kHeight)))
        ++b[i].y;
    totemFollow(int(i));
  }
  if (!alive(p) || p.state != PlayerState::OnGround)
    return;
  // Restack: a group the stack walks into on its floor, or one standing
  // just above its top, joins it; Rocco, Dash, Nova from the bottom up.
  for (std::size_t i = 0; i < b.size(); ++i)
  {
    if (b[i].active || b[i].on >= 0 || totemBase(me) == int(i))
      continue;
    int topY = p.y;
    for (int j = totemAbove(me); j >= 0; j = totemAbove(j))
      topY = b[std::size_t(j)].y;
    const bool beside = std::abs(b[i].x - p.x) < Player::kWidth;
    const bool sameFloor = std::abs(b[i].y - p.y) <= 1;
    const bool above = b[i].y <= topY - Player::kHeight && b[i].y >= topY - Player::kHeight - kCellsPerTile;
    if (!beside || !(sameFloor || above))
      continue;
    // Everyone in both groups, bottom up by runner.
    std::vector<int> all;
    for (int j = me; j >= 0; j = totemAbove(j))
      all.push_back(j);
    for (int j = int(i); j >= 0; j = totemAbove(j))
      all.push_back(j);
    auto rank = [&](int j) { return b[std::size_t(j)].runner == 1 ? 0 : b[std::size_t(j)].runner == 0 ? 1 : 2; };
    std::sort(all.begin(), all.end(), [&](int a, int c) { return rank(a) < rank(c); });
    // The new base stands where the stack's base stands.
    const int bx = p.x, by = p.y;
    bool fits = true;
    for (std::size_t k = 1; k < all.size() && fits; ++k)
      fits = !mMap.overlapsSolid(boxAt(bx, by - int(k) * Player::kHeight, Player::kWidth, Player::kHeight));
    if (!fits)
      continue;
    for (std::size_t k = 0; k < all.size(); ++k)
    {
      auto& o = b[std::size_t(all[k])];
      o.on = k == 0 ? -1 : all[k - 1];
      o.active = false;
      o.x = bx;
      o.y = by - int(k) * Player::kHeight;
    }
    b[std::size_t(all[0])].active = true;
    becomeRunner(b[std::size_t(all[0])].runner);
    playSound(Sfx::Boing);
    showMessage(all.size() == 3 ? "TOTEM!" : "STACKED");
    break;
  }
}

// --- Saves ---------------------------------------------------------------------------------

bool World::wardrobeCanSave() const
{
  const auto& s = mWardrobe;
  if (s.totem || s.boothT > 0 || s.sit > 0 || s.tumble > 0 || !s.decoys.empty())
    return false;
  for (const auto& t : s.tables)
    if (t.lower && !t.down())
      return false;
  return true;
}

// [checkpoint runner, checkpoints seen, 8 pools] [plates down] [shutters
// open] [table rows] [push rack x] [route at, suit, mustache, toon]
std::vector<int> World::wardrobeSave() const
{
  const auto& s = mWardrobe;
  std::vector<int> v{s.checkpointRunner, s.checkpointsSeen};
  v.insert(v.end(), s.pools.begin(), s.pools.end());
  v.push_back(mPlayer.hp);
  for (const auto& pl : s.plates)
    v.push_back(pl.down ? 1 : 0);
  for (const auto& sh : s.shutters)
    v.push_back(sh.open ? 1 : 0);
  for (const auto& t : s.tables)
    v.push_back(t.row * 2 + (t.lower ? 1 : 0));
  for (const auto& r : s.pushRacks)
    v.push_back(r.x);
  v.push_back(s.routeAt);
  v.push_back(s.suitTouched ? 1 : 0);
  v.push_back(s.suit ? 1 : 0);
  v.push_back(s.mustache ? 1 : 0);
  v.push_back(s.toon ? 1 : 0);
  return v;
}

bool World::validWardrobeSave(const std::vector<int>& v) const
{
  const auto& s = mWardrobe;
  return v.size() == 2 + 8 + 1 + s.plates.size() + s.shutters.size() + s.tables.size() + s.pushRacks.size() + 5;
}

void World::loadWardrobe(const std::vector<int>& v)
{
  auto& s = mWardrobe;
  if (!validWardrobeSave(v))
    return;
  std::size_t at = 0;
  s.checkpointRunner = v[at++];
  s.checkpointsSeen = v[at++];
  for (auto& pool : s.pools)
    pool = v[at++];
  mPlayer.hp = std::clamp(v[at++], 1, mPlayer.maxHp);
  for (auto& pl : s.plates)
    pl.down = v[at++] != 0;
  for (auto& sh : s.shutters)
  {
    sh.open = v[at++] != 0;
    sh.t = sh.open ? 12 : 0;
    for (int ty = sh.y0; ty <= sh.y1; ++ty)
      for (int tx = sh.x0; tx <= sh.x1; ++tx)
        mMap.setBlock(tx, ty, sh.open ? Tile::Empty : Tile::Solid);
  }
  for (auto& t : s.tables)
  {
    for (int tx = t.x0; tx <= t.x1; ++tx)
      mMap.setBlock(tx, t.row, Tile::Empty);
    t.row = v[at] / 2;
    t.lower = v[at++] % 2 != 0;
    for (int tx = t.x0; tx <= t.x1; ++tx)
      mMap.setBlock(tx, t.row, Tile::Platform);
  }
  for (auto& r : s.pushRacks)
  {
    for (int ty = r.y; ty < r.y + r.h; ++ty)
      for (int tx = r.x; tx < r.x + r.w; ++tx)
        mMap.setBlock(tx, ty, Tile::Empty);
    r.x = v[at++];
    for (int ty = r.y; ty < r.y + r.h; ++ty)
      for (int tx = r.x; tx < r.x + r.w; ++tx)
        mMap.setBlock(tx, ty, Tile::Solid);
  }
  s.routeAt = v[at++];
  s.suitTouched = v[at++] != 0;
  s.suit = v[at++] != 0;
  s.mustache = v[at++] != 0;
  s.toon = v[at++] != 0;
  s.boothT = 0;
  s.booth = -1;
  s.decoys.clear();
}

} // namespace gr
