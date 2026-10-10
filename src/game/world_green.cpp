// Level 17, Hydroponics (SPEC 17): Grow Lamps. A shot switch lights its
// lamps; while every lamp a plant hangs under is lit, the plant grows from
// its root outward (bridges and stairs are solid, leaves one-way, creepers
// climbable), and once one goes out it withers tip-first. The Hedge
// Trimmer is a spinning cone held in front of the runner. The domes'
// staff: Spore Puffers, Snapjaws and Globs that split. Growth Spurt, the
// bonus, makes the runner bigger with each gem (rules=grow).

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kGrowFrames = 45;   // a plant grows in full
constexpr int kWitherFrames = 30; // and withers
constexpr int kUnit = 90;         // a tile's growth, in Plant::prog units
constexpr int kSlow = 15;         // frames a spore cloud slows the runner
constexpr int kCloud = 6;         // a cloud's size, cells
constexpr int kCloudLife = 45;
constexpr float kCloudDrift = 0.25f;
constexpr int kSwitchRest = 10;   // frames a switch ignores hits after one
constexpr int kMaxGlobs = 8;
constexpr int kMaxSize = 4;       // Growth Spurt: x2.0

// A Glob's hop: 2 blocks over 8 frames, up and down again.
constexpr int kHop[] = {-2, -1, -1, 0, 0, 1, 1, 2};

int sgn(int v) { return (v > 0) - (v < 0); }

std::vector<std::string> splitIds(const std::string& s)
{
  std::vector<std::string> out;
  std::size_t at = 0;
  while (at <= s.size())
  {
    const std::size_t comma = s.find(',', at);
    const std::string id = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!id.empty())
      out.push_back(id);
    if (comma == std::string::npos)
      break;
    at = comma + 1;
  }
  return out;
}

CellBox switchBox(const GreenSwitch& s) { return {s.bx * kCellsPerTile, s.by * kCellsPerTile, 2, 2}; }

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupGreenEntity(const EntityDef& e)
{
  auto& g = mGreen;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "switch" && e.hasPos)
  {
    // Level 7's hook latch is a switch too: only levels with grow lamps
    // wire their switches here.
    bool lamps = false;
    for (const auto& o : mLevel->entities)
      lamps = lamps || (o.kind == "lamp" && o.has("switch"));
    if (!lamps)
      return false;
    GreenSwitch s;
    s.id = e.id;
    s.bx = e.x;
    s.by = e.y;
    g.switches.push_back(s);
    return true;
  }
  // The pinball table's lanterns are lamps too, without a switch.
  if (e.kind == "lamp" && e.hasPos && e.has("switch"))
  {
    GrowLamp l;
    l.id = e.id;
    l.bx = e.x;
    l.by = e.y;
    l.timer = std::max(0, e.num("timer", 0));
    l.basil = e.num("basil", 0) != 0;
    // Wired up in linkGreen (the switch may come later in the file).
    l.sw = -1;
    g.lamps.push_back(l);
    mGreenWires.push_back(e.str("switch"));
    return true;
  }
  if (e.kind == "plant")
  {
    Plant pl;
    pl.id = e.id;
    const std::string kind = e.str("kind", "bridge");
    pl.kind = kind == "leaf" ? PlantKind::Leaf
      : kind == "ladder"     ? PlantKind::Ladder
      : kind == "stairs"     ? PlantKind::Stairs
      : kind == "flower"     ? PlantKind::Flower
                             : PlantKind::Bridge;
    // rect=, rect2=, ... rect6=: the blocks it grows into.
    for (const char* key : {"rect", "rect2", "rect3", "rect4", "rect5", "rect6"})
      if (e.rect(key, x0, y0, x1, y1))
        for (int ty = y0; ty <= y1; ++ty)
          for (int tx = x0; tx <= x1; ++tx)
            pl.tiles.push_back({tx, ty});
    const auto root = e.list("root");
    const int rx = root.size() == 2 ? root[0] : (pl.tiles.empty() ? 0 : pl.tiles.front().first);
    const int ry = root.size() == 2 ? root[1] : (pl.tiles.empty() ? 0 : pl.tiles.front().second);
    // Root outward: nearest first, then bottom to top, left to right.
    std::stable_sort(pl.tiles.begin(), pl.tiles.end(), [&](const auto& a, const auto& b) {
      const int da = std::abs(a.first - rx) + std::abs(a.second - ry);
      const int db = std::abs(b.first - rx) + std::abs(b.second - ry);
      if (da != db)
        return da < db;
      if (a.second != b.second)
        return a.second > b.second;
      return a.first < b.first;
    });
    g.plants.push_back(pl);
    mPlantWires.push_back(e.str("lamp"));
    return true;
  }
  return false;
}

void World::setupGreenEnemy(Enemy& en, const EntityDef& e)
{
  // face=floor|ceiling|left|right (the wall it grows on) overrides the
  // guess from the map in linkGreen.
  const std::string face = e.str("face");
  if (!face.empty())
  {
    en.variant = face == "ceiling" ? 1 : (face == "left" ? 2 : (face == "right" ? 3 : 0));
    en.aimY = 1;
  }
  if (en.kind == EnemyKind::Puffer || en.kind == EnemyKind::Snapjaw || en.kind == EnemyKind::Glob)
    mGreen.on = true;
  if (en.kind == EnemyKind::Glob)
    en.cool = 10 + (en.x % 3) * 6; // not all in step
}

void World::linkGreen()
{
  auto& g = mGreen;
  g.grow = mLevel->rules.find("grow") != std::string::npos;
  g.on = g.on || g.grow || !g.lamps.empty() || !g.plants.empty() || !g.pools.empty() ||
    mLevel->weapon == "hedge_trimmer";
  for (std::size_t i = 0; i < g.lamps.size() && i < mGreenWires.size(); ++i)
    for (std::size_t s = 0; s < g.switches.size(); ++s)
      if (g.switches[s].id == mGreenWires[i])
        g.lamps[i].sw = int(s);
  for (std::size_t i = 0; i < g.plants.size() && i < mPlantWires.size(); ++i)
  {
    for (const auto& id : splitIds(mPlantWires[i]))
      for (std::size_t l = 0; l < g.lamps.size(); ++l)
        if (g.lamps[l].id == id)
          g.plants[i].lamps.push_back(int(l));
    // The plant draws its own tiles (world_green_draw.cpp).
    for (const auto& [tx, ty] : g.plants[i].tiles)
      if (tx >= 0 && ty >= 0 && tx < mLevel->width && ty < mLevel->height)
        mLayerMask[std::size_t(ty * mLevel->width + tx)] = 1;
  }
  mGreenWires.clear();
  mPlantWires.clear();
  if (!g.on)
    return;
  // Puffers and Snapjaws face away from what they grow on: variant 0
  // floor, 1 ceiling, 2 a wall on their left, 3 a wall on their right.
  for (auto& e : mEnemies)
  {
    if (e.kind != EnemyKind::Puffer && e.kind != EnemyKind::Snapjaw)
      continue;
    if (e.kind == EnemyKind::Puffer)
      e.cool = 12 + (e.x % 4) * 5;
    e.dir = 1; // the art faces the way its variant says: never mirrored
    if (e.aimY == 1)
      continue; // face= given
    const CellBox b = e.box();
    if (mMap.solid(b.x + 1, b.bottom() + 1))
      e.variant = 0;
    else if (mMap.solid(b.x + 1, b.top() - 1))
      e.variant = 1;
    else if (mMap.solid(b.left() - 1, b.y + 1))
      e.variant = 2;
    else if (mMap.solid(b.right() + 1, b.y + 1))
      e.variant = 3;
  }
  g.lastGems = mStats.gems;
  g.arc = mCharacter.jumpArc;
}

void World::resetGreen()
{
  auto& g = mGreen;
  g.slow = 0;
  g.trim = 0;
  g.clouds.clear();
}

// --- Switches, lamps and plants ------------------------------------------------------------

void World::hitSwitch(int s)
{
  auto& sw = mGreen.switches[std::size_t(s)];
  if (sw.flash > 0)
    return;
  sw.flash = kSwitchRest;
  playSound(Sfx::Click);
  for (std::size_t i = 0; i < mGreen.lamps.size(); ++i)
  {
    auto& l = mGreen.lamps[i];
    if (l.sw != s || l.locked)
      continue;
    // A lamp that stays on goes off when shot again; a timed one starts
    // its time over.
    lightLamp(int(i), l.timer > 0 || !l.lit);
  }
}

void World::lightLamp(int i, bool on)
{
  auto& l = mGreen.lamps[std::size_t(i)];
  const bool was = l.lit;
  l.lit = on;
  l.left = on ? l.timer : 0;
  if (on && !was)
  {
    playSound(Sfx::LampOn);
    if (l.basil)
    {
      // The basil pot: the station's oldest crew member hums a note.
      l.hum = 30;
      playSound(Sfx::Cheer);
    }
  }
}

bool World::shotAtGreen(Projectile& pr, const CellBox& b)
{
  for (std::size_t s = 0; s < mGreen.switches.size(); ++s)
    if (switchBox(mGreen.switches[s]).intersects(b))
    {
      hitSwitch(int(s));
      burst(cellCenter(switchBox(mGreen.switches[s])), rgb(255, 255, 255), rgb(255, 120, 220), 8, 1.2f);
      pr.alive = false;
      return true;
    }
  return false;
}

void World::updatePlants()
{
  auto& g = mGreen;
  auto& p = mPlayer;
  for (auto& s : g.switches)
    if (s.flash > 0)
      --s.flash;
  for (std::size_t i = 0; i < g.lamps.size(); ++i)
  {
    auto& l = g.lamps[i];
    if (l.hum > 0)
      --l.hum;
    if (l.lit && !l.locked && l.timer > 0 && --l.left <= 0)
      lightLamp(int(i), false);
  }
  const CellBox body = p.box();
  bool ladderGone = false;
  for (auto& pl : g.plants)
  {
    const int n = int(pl.tiles.size());
    if (n == 0)
      continue;
    bool lit = !pl.lamps.empty();
    for (int l : pl.lamps)
      lit = lit && g.lamps[std::size_t(l)].lit;
    if (lit && pl.lamps.size() > 1)
      for (int l : pl.lamps)
        g.lamps[std::size_t(l)].locked = true; // the stairs: lit for good
    if (lit != pl.growing && (lit ? pl.prog < n * kUnit : pl.prog > 0))
      playSound(Sfx::Rustle);
    pl.growing = lit;
    if (lit)
      pl.prog = std::min(n * kUnit, pl.prog + n * kUnit / kGrowFrames);
    else
      pl.prog = std::max(0, pl.prog - n * kUnit / kWitherFrames);
    const Tile tile = pl.kind == PlantKind::Ladder ? Tile::Ladder
      : (pl.kind == PlantKind::Leaf || pl.kind == PlantKind::Flower) ? Tile::Platform
                                                                     : Tile::Solid;
    if (lit)
    {
      // Grow into the next tiles, never into the runner.
      while (pl.shown < n && pl.prog >= (pl.shown + 1) * kUnit)
      {
        const auto [tx, ty] = pl.tiles[std::size_t(pl.shown)];
        const CellBox tb{tx * kCellsPerTile, ty * kCellsPerTile, kCellsPerTile, kCellsPerTile};
        if (tile != Tile::Ladder && tb.intersects(body))
          break;
        if (mMap.block(tx, ty) == Tile::Empty)
          mMap.setBlock(tx, ty, tile);
        ++pl.shown;
      }
    }
    else
      while (pl.shown > 0 && pl.prog <= (pl.shown - 1) * kUnit)
      {
        --pl.shown;
        const auto [tx, ty] = pl.tiles[std::size_t(pl.shown)];
        if (mMap.block(tx, ty) == tile)
          mMap.setBlock(tx, ty, Tile::Empty);
        ladderGone = ladderGone || tile == Tile::Ladder;
        const Vec2 c{(float(tx) + 0.5f) * kCellSize * kCellsPerTile, (float(ty) + 0.5f) * kCellSize * kCellsPerTile};
        burst(c, rgb(150, 110, 60), rgb(110, 160, 70), 4, 0.8f, false);
      }
  }
  // Off a creeper that withered away: down you go.
  if (ladderGone && p.state == PlayerState::Ladder)
  {
    bool still = false;
    for (int yy = body.top(); yy <= body.bottom() && !still; ++yy)
      still = mMap.ladder(p.x + 1, yy);
    if (!still)
      startFalling();
  }
}

// --- The Hedge Trimmer ---------------------------------------------------------------------

CellBox World::trimBox() const
{
  const auto& p = mPlayer;
  const int dir = mGreen.trimFace;
  const int top = p.stance == Stance::Crouched ? p.y - 2 : p.y - 3;
  return {dir > 0 ? p.x + Player::kWidth : p.x - 4, top, 4, 3};
}

void World::trimCone(int dir)
{
  auto& g = mGreen;
  auto& p = mPlayer;
  g.trimFace = dir;
  const CellBox cone = trimBox();
  int damage = protoDef(ProtoId::HedgeTrimmer).damage;
  if (p.turbo > 0)
    damage *= 2;
  for (auto& e : mEnemies)
  {
    if (!e.alive || !e.box().intersects(cone))
      continue;
    const bool wasAlive = e.alive;
    shotHitsEnemy(e, dir, damage);
    if (wasAlive && !e.alive)
      ++mStats.protoKills;
  }
  // Thorn walls (and anything else that breaks) take four times as much.
  hitBreakable(cone, damage * 4, 6);
  for (auto& pr : mProjectiles)
    if (pr.alive && pr.kind == ShotKind::Enemy && pr.box().intersects(cone))
    {
      pr.alive = false;
      burst(cellCenter(pr.box()), rgb(200, 255, 160), rgb(90, 200, 70), 4, 1.0f);
    }
  for (auto& c : g.clouds)
  {
    const CellBox cb{int(c.x), int(c.y), kCloud, kCloud};
    if (c.life > 0 && cb.intersects(cone))
    {
      c.life = 0;
      burst(cellCenter(cb), rgb(220, 255, 190), c.carrier ? rgb(120, 255, 90) : rgb(200, 220, 160), 10, 1.4f, false);
    }
  }
  // A switch in reach: one flick per press.
  if (g.trim == 1)
    for (std::size_t s = 0; s < g.switches.size(); ++s)
      if (switchBox(g.switches[s]).intersects(cone))
        hitSwitch(int(s));
}

void World::updateTrimmer(const Button& fire)
{
  auto& p = mPlayer;
  auto& g = mGreen;
  const ProtoDef& def = protoDef(ProtoId::HedgeTrimmer);
  // Up and down it fires a plain shot (for switches overhead).
  if (p.stance == Stance::Up || p.stance == Stance::Down || p.stance == Stance::Jetpack)
  {
    g.trim = 0;
    if (fire.triggered && (p.shotCooldown == 0 || p.turbo > 0))
    {
      fireShot();
      p.shotCooldown = 2;
    }
    return;
  }
  if (!fire.pressed)
  {
    g.trim = 0;
    return;
  }
  ++g.trim;
  // A cut every 3 frames (Turbo: every frame), a unit of ammo each.
  const int every = p.turbo > 0 ? 1 : def.cooldown;
  if ((g.trim - 1) % every != 0)
    return;
  trimCone(p.facing);
  playSound(Sfx::Trim);
  p.muzzleTicks = 6;
  p.muzzleStance = p.stance;
  if (--p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    g.trim = 0;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  }
}

// --- The domes' staff ----------------------------------------------------------------------

void World::updatePuffer(Enemy& e, const EnemyDef& def)
{
  if (e.cool > 0)
    --e.cool;
  switch (e.attach)
  {
    case 0:
      if (e.cool == 0 && isOnScreen(e.box(), 2))
      {
        e.attach = 1;
        e.tell = def.tell;
      }
      break;
    case 1: // the bulb swells
      if (--e.tell <= 0)
      {
        SporeCloud c;
        const CellBox b = e.box();
        c.x = float(b.x + b.w / 2 - kCloud / 2);
        const float dir = e.variant == 1 ? 1.0f : (e.variant == 0 ? -1.0f : 0.0f);
        c.y = e.variant == 1 ? float(b.bottom() + 1) : (e.variant == 0 ? float(b.top() - kCloud) : float(b.y - 1));
        if (e.variant >= 2)
          c.x = e.variant == 2 ? float(b.right() + 1) : float(b.left() - kCloud);
        c.dy = dir * kCloudDrift;
        c.dx = e.variant == 2 ? kCloudDrift : (e.variant == 3 ? -kCloudDrift : 0.0f);
        c.life = kCloudLife;
        c.carrier = e.carrier;
        mGreen.clouds.push_back(c);
        e.attach = 2;
        e.dive = 6;
        e.cool = def.cooldown;
        if (isOnScreen(b, 0))
          playSound(Sfx::Puff);
      }
      break;
    default: // deflated
      if (--e.dive <= 0)
        e.attach = 0;
      break;
  }
}

void World::updateSnapjaw(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box();
  const CellBox wake{b.x - def.range, b.y - def.range, b.w + 2 * def.range, b.h + 2 * def.range};
  const CellBox reach{b.x - 2, b.y - 2, b.w + 4, b.h + 4};
  const bool near = wake.intersects(p.hitBox()) && p.state != PlayerState::Dying;
  switch (e.attach)
  {
    case 0: // asleep, jaws closed
      if (near && e.cool == 0)
      {
        e.attach = 1;
        e.tell = def.tell;
      }
      break;
    case 1: // the jaws twitch
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.dive = 4;
        if (isOnScreen(b, 0))
          playSound(Sfx::Snap);
        if (reach.intersects(p.hitBox()) && p.state != PlayerState::Dying && p.tube < 0)
          hurtPlayer(1);
      }
      break;
    default: // snapped shut again
      if (--e.dive <= 0)
      {
        e.attach = 0;
        e.cool = def.cooldown;
      }
      break;
  }
}

void World::updateGlob(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (e.attach == 2)
  {
    // The hop: half a cell a frame sideways, up and down.
    const int f = e.dive++;
    if (f % 2 == 0)
      mMap.moveHorizontally(e.x, e.y, e.w, e.h, e.dir);
    const int dy = f < int(std::size(kHop)) ? kHop[f] : 2;
    for (int i = 0; i < std::abs(dy); ++i)
      if (mMap.moveVertically(e.x, e.y, e.w, e.h, sgn(dy)) != MoveResult::Completed)
        break;
    if (f >= int(std::size(kHop)) - 1 && mMap.onSolidGround(e.box()))
    {
      e.attach = 0;
      e.cool = def.cooldown;
      if (isOnScreen(e.box(), 0))
        playSound(Sfx::Squish);
    }
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.cool > 0)
  {
    --e.cool;
    return;
  }
  if (e.attach == 0)
  {
    e.attach = 1;
    e.tell = def.tell;
    return;
  }
  // Squashed flat, then off it goes, toward the runner.
  if (--e.tell <= 0)
  {
    e.attach = 2;
    e.dive = 0;
    const int dx = (p.x + 1) - (e.x + e.w / 2);
    e.dir = dx < 0 ? -1 : (dx > 0 ? 1 : e.dir);
  }
}

void World::splitGlob(Enemy& e)
{
  const std::string key = enemyDef(e.def).key;
  const char* next = key == "glob" ? "glob_medium" : (key == "glob_medium" ? "glob_small" : nullptr);
  burst(cellCenter(e.box()), rgb(150, 255, 120), rgb(220, 255, 200), 12, 1.6f);
  playSound(Sfx::Squish);
  if (!next)
    return;
  // With 8 Glob bodies about, a hit kills instead of splitting.
  int bodies = int(mGreen.splits.size());
  for (const auto& o : mEnemies)
    bodies += o.alive && o.kind == EnemyKind::Glob;
  if (bodies + 2 > kMaxGlobs)
    return;
  const int def = enemyIndex(next);
  const int w = enemyDef(def).w;
  mGreen.splits.push_back({e.x, e.y, def, -1});
  mGreen.splits.push_back({e.x + e.w - w, e.y, def, 1});
}

// --- Every frame ---------------------------------------------------------------------------

void World::updateGreen(const PlayerInput& input)
{
  auto& g = mGreen;
  if (!g.on)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (!(p.weapon == Weapon::Proto && ProtoId(p.proto) == ProtoId::HedgeTrimmer))
    g.trim = 0;

  updatePlants();

  // Spore clouds drift off and thin out; walking through one is slow
  // going, and two at once (one of them a carrier's) infect.
  for (auto& c : g.clouds)
  {
    c.x += c.dx;
    c.y += c.dy;
    --c.life;
  }
  g.clouds.erase(std::remove_if(g.clouds.begin(), g.clouds.end(), [](const SporeCloud& c) { return c.life <= 0; }),
    g.clouds.end());
  if (g.slow > 0)
    --g.slow;
  if (alive)
  {
    int in = 0;
    bool carrier = false;
    for (const auto& c : g.clouds)
      if (CellBox{int(c.x), int(c.y), kCloud, kCloud}.intersects(p.hitBox()))
      {
        ++in;
        carrier = carrier || c.carrier;
      }
    if (in > 0)
      g.slow = kSlow;
    if (in >= 2 && carrier && p.virus == 0 && p.turbo == 0)
      infect();
  }

  // Globs split off the one that just burst.
  for (const auto& s : g.splits)
  {
    spawnEnemy(s[2], s[0], s[1]);
    Enemy& e = mEnemies.back();
    e.active = true;
    e.dir = s[3];
    e.attach = 2; // flung apart
    e.dive = 0;
  }
  g.splits.clear();

  // Growth Spurt: a gem is a size up, a shot a size down.
  if (g.grow)
  {
    if (g.sizeFlash > 0)
      --g.sizeFlash;
    const int gems = mStats.gems - g.lastGems;
    g.lastGems = mStats.gems;
    if (gems > 0 && g.size < kMaxSize)
    {
      g.size = std::min(kMaxSize, g.size + gems);
      g.sizeFlash = 12;
      playSound(Sfx::Grow);
    }
    const float s = g.scale();
    for (std::size_t i = 0; i < g.arc.size(); ++i)
      g.arc[i] = int(std::lround(float(mCharacter.jumpArc[i]) * s));
    // Big enough, it walks through the cracked blocks.
    const int mvX = input.right ? 1 : (input.left ? -1 : 0);
    if (g.size >= 2 && mvX != 0 && alive && p.state == PlayerState::OnGround)
    {
      const CellBox ahead{mvX > 0 ? p.x + Player::kWidth : p.x - 1, p.y - Player::kHeight + 1, 1, Player::kHeight};
      if (mMap.overlapsSolid(ahead))
        hitBreakable(ahead, 4, 2);
    }
  }
}

void World::growShot()
{
  auto& g = mGreen;
  if (!g.grow || g.size == 0)
    return;
  --g.size;
  g.sizeFlash = 12;
  playSound(Sfx::Grow);
}

bool World::growBlocked(int x, int y) const
{
  // A bigger runner is taller: no squeezing into a tunnel built for size 1.
  if (!mGreen.grow || mGreen.size == 0)
    return false;
  const int h = int(std::ceil(float(Player::kHeight) * mGreen.scale()));
  return mMap.overlapsSolid(boxAt(x, y, Player::kWidth, h));
}

bool World::greenCanSave() const
{
  return mGreen.splits.empty();
}

} // namespace gr
