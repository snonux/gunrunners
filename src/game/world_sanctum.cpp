// Level 14, The Idol Awakens (SPEC 14): Gold Fever (the greed meter, the
// offering altars and the false altar over the treasury), Coin Beetles out
// of the coin heaps, War Drummers, Glyph Sentinels, the jade basin and
// Kaan-Tolok, the Idol Golem, in three phases: stomps, three bouncing
// heads, and arm sweeps while the arena's walls close in.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kOfferEvery = 4;   // frames between gems given at an altar
constexpr int kFalseStand = 30;  // frames on the false altar to drop its floor
constexpr int kHeapNear = 24;    // cells: a heap stirs with the runner this close
constexpr int kMaxBeetles = 4;
constexpr int kDrumReach = 20;   // cells: a drummer's beat carries this far
constexpr int kGlyphTell = 24;   // the glyphs light one by one, 6 frames each
constexpr int kBeam = 10;
constexpr int kHeadGap = 15;     // heads=3: the row's parts fire this far apart
// Kaan-Tolok.
constexpr int kRise = 60;
constexpr int kWink = 12;
constexpr int kStompEvery = 60;
constexpr int kStompTell = 12;
constexpr int kOpen = 30;        // frames the chest gem shows after an attack
constexpr int kRockFall = 15;
constexpr int kBreak = 30;
constexpr int kChargeEvery = 90;
constexpr int kRumble = 12;
constexpr int kRebuild = 30;
constexpr int kSweepEvery = 45;
constexpr int kSweepTell = 14;
constexpr int kSweepFrames = 10;
constexpr int kCrumble = 60;
constexpr int kSlideDust = 12;
constexpr int kSlideFrames = 8;
constexpr int kPlateHp = 4;
constexpr int kHp1 = 32, kHeadHp = 8, kHp3 = 20;
constexpr int kHeadSize = 6;     // cells
constexpr float kHeadGravity = 0.25f;
constexpr int kKeepOff = 6;      // cells it stops short of the runner
const Color kGold = rgb(240, 196, 80);
const Color kGoldDark = rgb(150, 104, 30);
const Color kJade = rgb(80, 220, 150);

bool visible(float x, float y, float w, float h)
{
  return x + w > -96.0f && x < float(kScreenW) + 96.0f && y + h > -96.0f && y < float(kScreenH) + 96.0f;
}

CellBox glyphBand(const GlyphRow& g, int part = -1)
{
  const int x0 = g.x0 * kCellsPerTile, w = (g.x1 - g.x0 + 1) * kCellsPerTile;
  if (part < 0 || g.heads <= 1)
    return {x0, g.row * kCellsPerTile, w, kCellsPerTile};
  // The part nearest the master (at the row's right end) fires first.
  const int pw = (w + g.heads - 1) / g.heads;
  const int right = x0 + w - part * pw;
  const int left = std::max(x0, right - pw);
  return {left, g.row * kCellsPerTile, right - left, kCellsPerTile};
}

CellBox headBox(const Golem::Head& h)
{
  return {int(std::floor(h.x)), int(std::lround(h.y)) - kHeadSize + 1, kHeadSize, kHeadSize};
}

float launchSpeed(int cells)
{
  return std::sqrt(2.0f * kHeadGravity * float(cells));
}

} // namespace

// --- Setup ----------------------------------------------------------------------------

bool World::setupSanctumEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "altar" && e.hasPos)
  {
    // 2 x 1 blocks, solid: a step up, with its bowl of incense on top.
    Altar a;
    a.id = e.id;
    a.bx = e.x;
    a.by = e.y;
    a.offer = e.str("kind", "offer") != "false";
    if (e.rect("trapdoor", x0, y0, x1, y1))
    {
      a.tx0 = x0;
      a.ty0 = y0;
      a.tx1 = x1;
      a.ty1 = y1;
    }
    mMap.setBlock(a.bx, a.by, Tile::Solid);
    mMap.setBlock(a.bx + 1, a.by, Tile::Solid);
    mAltars.push_back(a);
    return true;
  }
  if (e.kind == "coinheap" && e.hasPos)
  {
    CoinHeap h;
    h.x = e.x * kCellsPerTile;
    h.y = e.y * kCellsPerTile + 1;
    h.waves = e.num("waves", 2);
    h.count = e.num("count", 2);
    mCoinHeaps.push_back(h);
    return true;
  }
  if (e.kind == "refill" && e.hasPos)
  {
    mRefillX = e.x * kCellsPerTile;
    mRefillY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "golem" && e.hasPos)
  {
    Golem g;
    g.on = true;
    g.x = g.prevX = float(e.x * kCellsPerTile);
    if (e.rect("arena", x0, y0, x1, y1))
    {
      g.arena = {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
      g.floor = (y1 + 1) * kCellsPerTile - 1;
      g.wallL = g.wallL0 = x0 - 1;
      g.wallR = g.wallR0 = x1 + 1;
    }
    if (e.rect("door", x0, y0, x1, y1))
    {
      g.doorX0 = x0;
      g.doorY0 = y0;
      g.doorX1 = x1;
      g.doorY1 = y1;
    }
    const auto ex = e.list("exit");
    if (ex.size() == 2)
    {
      g.exitX = ex[0];
      g.exitY = ex[1];
    }
    for (auto& h : g.heads)
      h.alive = false;
    mGolem = g;
    return true;
  }
  return false;
}

void World::setupSanctumEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::Sentinel)
  {
    // row=x0,x1,row (blocks): its glyphs, carved in the wall behind.
    const auto v = e.list("row");
    if (v.size() == 3)
    {
      GlyphRow g;
      g.enemy = int(mEnemies.size()) - 1;
      g.x0 = v[0];
      g.x1 = v[1];
      g.row = v[2];
      g.heads = std::clamp(e.num("heads", 1), 1, 3);
      en.attach = int(mGlyphRows.size());
      mGlyphRows.push_back(g);
    }
    else
      en.attach = -1;
  }
}

void World::linkSanctum()
{
  mGreedOn = !mAltars.empty();
  // The false altar keeps the bonus entrance hidden until an empty-handed
  // offering.
  for (const auto& a : mAltars)
    if (!a.offer)
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor)
          pr.dormant = true;
  // The candid camera under the golem's eyelid.
  auto& g = mGolem;
  if (g.on)
    for (std::size_t i = 0; i < mEnemies.size(); ++i)
      if (mEnemies[i].kind == EnemyKind::Camera && mEnemies[i].box().intersects(g.body()))
      {
        g.camera = int(i);
        mEnemies[i].hidden = true;
      }
}

// --- Gold Fever ------------------------------------------------------------------------

bool World::besideAltar(int index) const
{
  if (index < 0 || index >= int(mAltars.size()))
    return false;
  const Altar& a = mAltars[std::size_t(index)];
  const auto& p = mPlayer;
  const int ax = a.bx * kCellsPerTile, ay = a.by * kCellsPerTile;
  const CellBox pb = p.box();
  return !a.dropped && p.state == PlayerState::OnGround && (p.y == ay + 1 || p.y == ay - 1) && pb.right() >= ax - 2 &&
    pb.left() <= ax + 5;
}

void World::gainGreed()
{
  if (mGreedOn)
    mGreed = std::min(99, mGreed + 1);
}

void World::offerGem(Altar& a)
{
  --mStats.gems;
  mStats.gemsTotal = std::max(0, mStats.gemsTotal - 1);
  mGreed = std::max(0, mGreed - 1);
  ++mOffered;
  ++a.offered;
  a.flare = 10;
  const Vec2 c{(float(a.bx) + 1.0f) * float(kCellsPerTile * kCellSize), float(a.by) * float(kCellsPerTile * kCellSize) - 8.0f};
  burst(c, kGold, rgb(255, 250, 220), 6, 1.0f);
  playSound(Sfx::Gem);
  if (mStats.gems == 0)
    showMessage(mOffered == 42 ? "42 GEMS GIVEN. THE IDOL SMILES" : "EMPTY HANDS - THE GODS ARE PLEASED");
}

void World::dropGems(int x, int y, int n)
{
  for (int i = 0; i < n; ++i)
  {
    Item it;
    it.kind = ItemKind::Gem;
    it.x = it.prevX = x;
    it.y = it.prevY = y;
    it.variant = (x + i) % 4;
    it.vx = n == 1 ? 0 : (i % 2 ? 1 : -1);
    it.pickupDelay = 4;
    mItems.push_back(it);
    ++mStats.gemsTotal;
  }
}

bool World::drummed(const Enemy& e) const
{
  for (const auto& d : mEnemies)
    if (d.alive && d.kind == EnemyKind::Drummer && std::abs(d.x - e.x) <= kDrumReach && std::abs(d.y - e.y) <= 10)
      return true;
  return false;
}

void World::resetSanctum()
{
  mRefillUsed = false;
  for (auto& g : mGlyphRows)
  {
    g.t = 0;
    g.cool = 0;
  }
  auto& g = mGolem;
  if (!g.on || g.phase == GolemPhase::Seated || g.phase == GolemPhase::Done)
    return;
  // Back at the arena door: it opens, the walls go back, and Kaan-Tolok
  // waits where it was for the runner to come in again.
  g.away = true;
  for (int ty = g.doorY0; ty <= g.doorY1; ++ty)
    for (int tx = g.doorX0; tx <= g.doorX1; ++tx)
      mMap.setBlock(tx, ty, Tile::Empty);
  while (g.wallL > g.wallL0)
    setWall(g.wallL--, false);
  while (g.wallR < g.wallR0)
    setWall(g.wallR++, false);
  g.slid = g.slide = 0;
  g.waves.clear();
  g.rocks.clear();
  g.tell = g.sweep = g.cycle = 0;
  for (auto& h : g.heads)
  {
    h.stop = 0;
    h.charge = false;
  }
}

void World::setWall(int bx, bool solid)
{
  const auto& g = mGolem;
  const int ty0 = g.arena.y / kCellsPerTile, ty1 = (g.arena.y + g.arena.h) / kCellsPerTile - 1;
  for (int ty = ty0; ty <= ty1; ++ty)
    mMap.setBlock(bx, ty, solid ? Tile::Solid : Tile::Empty);
}

// --- Per frame --------------------------------------------------------------------------

void World::updateSanctum(const PlayerInput& input)
{
  if (!mGreedOn && mCoinHeaps.empty() && mRefillX < 0 && !mGolem.on)
    return;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox pb = p.box();

  // The altars.
  for (auto& a : mAltars)
  {
    if (a.flare > 0)
      --a.flare;
    if (a.dropped)
      continue;
    const int ax = a.bx * kCellsPerTile, ay = a.by * kCellsPerTile;
    const bool ground = alive && p.state == PlayerState::OnGround;
    const bool up = alive && besideAltar(int(&a - mAltars.data())) && input.up && !input.left && !input.right;
    if (a.offer)
    {
      // Hold up beside it: a gem every 4 frames, while there are gems.
      a.stand = up ? a.stand + 1 : 0;
      if (up && a.stand % kOfferEvery == 1 && mStats.gems > 0)
        offerGem(a);
      continue;
    }
    if (a.open)
      continue;
    const bool onTop = ground && p.y == ay - 1 && pb.right() >= ax && pb.left() <= ax + 3;
    a.stand = onTop ? a.stand + 1 : 0;
    if ((up || a.stand >= kFalseStand) && mStats.gems > 0)
    {
      // The floor drops into the priests' treasury, altar and all.
      a.open = a.dropped = true;
      for (int ty = a.ty0; ty <= a.ty1 && a.tx1 >= a.tx0; ++ty)
        for (int tx = a.tx0; tx <= a.tx1; ++tx)
          mMap.setBlock(tx, ty, Tile::Empty);
      mMap.setBlock(a.bx, a.by, Tile::Empty);
      mMap.setBlock(a.bx + 1, a.by, Tile::Empty);
      mCamera.shake(10, 2.0f);
      playSound(Sfx::Crash);
      burst({(float(a.bx) + 1.0f) * float(kCellsPerTile * kCellSize), float(a.by + 1) * float(kCellsPerTile * kCellSize)}, kGold, kGoldDark, 18, 2.0f,
        false);
      showMessage("A FALSE ALTAR! THE FLOOR GIVES WAY");
    }
    else if (up && mStats.gems == 0)
    {
      // Nothing to give: the gods show the way out instead.
      a.open = true;
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor && !pr.used)
          pr.dormant = false;
      a.flare = 30;
      playSound(Sfx::Chime);
      showMessage("NOTHING TO GIVE? A DOOR OPENS ANYWAY");
    }
  }

  // Coin heaps stir as the runner comes by.
  for (auto& h : mCoinHeaps)
  {
    if (h.cool > 0)
    {
      --h.cool;
      continue;
    }
    if (h.waves <= 0 || !alive || std::abs(pb.x + 1 - h.x) > kHeapNear || std::abs(p.y - h.y) > 8)
      continue;
    int out = 0;
    for (const auto& e : mEnemies)
      out += e.alive && e.kind == EnemyKind::CoinBeetle;
    const int n = std::min(h.count, kMaxBeetles - out);
    if (n <= 0)
      continue;
    static const int kBeetle = enemyIndex("coinbeetle");
    for (int i = 0; i < n; ++i)
    {
      spawnEnemy(kBeetle, h.x + i * 2 - 1, h.y);
      Enemy& en = mEnemies.back();
      en.active = true;
      en.dir = pb.x < h.x ? -1 : 1;
      en.attach = 2; // hops out of the heap
      en.dive = i * 2;
    }
    --h.waves;
    h.cool = 45;
    playSound(Sfx::Rattle);
    burst({(float(h.x) + 2.0f) * kCellSize, float(h.y) * kCellSize}, kGold, rgb(255, 240, 180), 10, 1.4f, false);
  }

  // The jade basin: full health and a full quiver, once a life.
  if (mRefillX >= 0 && !mRefillUsed && alive && CellBox{mRefillX, mRefillY - 3, 4, 4}.intersects(pb))
  {
    mRefillUsed = true;
    p.hp = p.maxHp;
    if (p.weapon == Weapon::Proto && p.proto >= 0)
      p.ammo = protoDef(p.proto).maxAmmo;
    const Vec2 c{(float(mRefillX) + 2.0f) * kCellSize, float(mRefillY - 2) * kCellSize};
    burst(c, kJade, rgb(220, 255, 240), 20, 1.8f);
    flashAt(c, 120.0f, kJade, 20);
    playSound(Sfx::Health);
    showMessage("THE JADE BASIN - FULL HEALTH, FULL QUIVER");
  }

  updateGolem();
}

// --- Enemies ----------------------------------------------------------------------------

void World::updateDrummer(Enemy& e, const EnemyDef& /*def*/)
{
  // Two beats a 15-frame bar (8, 7, 8, 7): the drum glows on each.
  const int inBar = mStats.frames % 15;
  if (inBar == 0 || inBar == 8)
  {
    e.tell = 4;
    if (isOnScreen(e.box(), 0))
      playSound(Sfx::Drum);
  }
  else if (e.tell > 0)
    --e.tell;
  e.dir = mPlayer.x + 1 < e.x + 2 ? -1 : 1;
}

void World::updateCoinBeetle(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  // A drummer's beat: half as fast again, and quicker to hop.
  const int steps = drummed(e) && mStats.frames % 2 == 0 ? 2 : 1;
  for (int s = 0; s < steps && e.alive; ++s)
  {
    if (e.cool > 0)
      --e.cool;
    if (e.attach == 1)
    {
      // Rattling before a hop.
      if (--e.tell <= 0)
      {
        e.attach = 2;
        e.dive = 0;
      }
      continue;
    }
    if (e.attach == 2)
    {
      // A hop: two blocks up and forward.
      static const int kHop[8] = {-2, -1, -1, 0, 0, 1, 1, 2};
      if (e.dive < 8)
      {
        const int dy = kHop[e.dive++];
        if (!mMap.overlapsSolid(boxAt(e.x + e.dir, e.y, e.w, e.h)))
          e.x += e.dir;
        for (int k = 0; k < std::abs(dy); ++k)
        {
          const int ny = e.y + (dy < 0 ? -1 : 1);
          if (mMap.overlapsSolid(boxAt(e.x, ny, e.w, e.h)))
            break;
          e.y = ny;
        }
        if (dy > 0 && mMap.onSolidGround(e.box()))
          e.dive = 8;
        continue;
      }
      e.attach = 0;
      e.cool = steps > 1 ? def.cooldown * 2 / 3 : def.cooldown;
      continue;
    }
    if (!mMap.onSolidGround(e.box()))
    {
      mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
      if (e.y > mMap.height() + 4)
        e.alive = false;
      continue;
    }
    const int pcx = p.x + 1, ecx = e.x + 1;
    e.dir = pcx < ecx ? -1 : 1;
    const bool close = std::abs(pcx - ecx) <= 6 && std::abs(p.y - e.y) <= 8 && vulnerable;
    const bool wall = mMap.overlapsSolid(boxAt(e.x + e.dir, e.y, e.w, e.h));
    if ((close || wall) && e.cool == 0)
    {
      e.attach = 1;
      e.tell = def.tell;
      if (isOnScreen(e.box(), 0))
        playSound(Sfx::Rattle);
      continue;
    }
    if (!wall && pcx != ecx)
      e.x += e.dir;
  }
}

void World::updateSentinel(Enemy& e, const EnemyDef& def)
{
  if (e.attach < 0 || e.attach >= int(mGlyphRows.size()))
    return;
  GlyphRow& g = mGlyphRows[std::size_t(e.attach)];
  const auto& p = mPlayer;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  e.tell = 0;
  if (g.t == 0)
  {
    if (g.cool > 0)
    {
      --g.cool;
      return;
    }
    // Armed only while the runner stands on its row's floor, in its line.
    if (vulnerable && p.state == PlayerState::OnGround && glyphBand(g).intersects(p.hitBox()))
    {
      g.t = 1;
      if (isOnScreen(e.box(), 4))
        playSound(Sfx::Warn);
    }
    return;
  }
  ++g.t;
  const int parts = std::max(1, g.heads);
  for (int part = 0; part < parts; ++part)
  {
    const int start = kGlyphTell + part * kHeadGap;
    if (g.t == start + 1)
      playSound(Sfx::Zap);
    if (g.t > start && g.t <= start + kBeam && vulnerable && glyphBand(g, parts > 1 ? part : -1).intersects(p.hitBox()))
      hurtPlayer(1);
  }
  e.tell = g.t <= kGlyphTell ? g.t : 0;
  if (g.t >= kGlyphTell + kBeam + (parts - 1) * kHeadGap)
  {
    g.t = 0;
    g.cool = drummed(e) ? def.cooldown * 2 / 3 : def.cooldown;
  }
}

bool World::shotAtGlyph(Projectile& pr, const CellBox& b)
{
  if (pr.kind == ShotKind::Enemy)
    return false;
  for (const auto& g : mGlyphRows)
  {
    if (g.enemy < 0 || !mEnemies[std::size_t(g.enemy)].alive)
      continue;
    const Enemy& m = mEnemies[std::size_t(g.enemy)];
    if (!glyphBand(g).intersects(b) || m.box().intersects(b))
      continue;
    // A full draw of the Jade Bow flies along the glyphs to their master.
    if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::JadeBow) && pr.strong)
      return false;
    burst(cellCenter(b), rgb(255, 240, 180), kGold, 5, 1.0f);
    return true;
  }
  return false;
}

// --- Kaan-Tolok -------------------------------------------------------------------------

bool World::golemFight() const
{
  const auto& g = mGolem;
  return g.on && g.phase != GolemPhase::Seated && g.phase != GolemPhase::Done && !g.away;
}

int World::golemHp() const
{
  const auto& g = mGolem;
  if (!g.on)
    return 0;
  const int armor = g.plates > 0 ? (g.plates - 1) * kPlateHp + g.plateHp : 0;
  switch (g.phase)
  {
    case GolemPhase::Seated:
    case GolemPhase::Rise:
      return kHp1 + std::min(6, mGreed / 10) * kPlateHp + 3 * kHeadHp + kHp3;
    case GolemPhase::Stomp:
      return g.hp + armor + 3 * kHeadHp + kHp3;
    case GolemPhase::Break:
      return 3 * kHeadHp + kHp3;
    case GolemPhase::Heads:
    {
      int hp = kHp3;
      for (const auto& h : g.heads)
        hp += h.alive ? h.hp : 0;
      return hp;
    }
    case GolemPhase::Rebuild:
      return kHp3;
    case GolemPhase::Sweep:
      return g.hp;
    case GolemPhase::Crumble:
    case GolemPhase::Done:
      return 0;
  }
  return 0;
}

void World::golemPhase(GolemPhase phase)
{
  auto& g = mGolem;
  g.phase = phase;
  g.t = g.cycle = g.tell = g.sweep = g.open = 0;
  g.waves.clear();
  g.rocks.clear();
  auto drop = [&](ItemKind kind, int bx) {
    Item it;
    it.kind = kind;
    it.x = it.prevX = bx * kCellsPerTile;
    it.y = it.prevY = g.floor - 8;
    it.pickupDelay = 4;
    mItems.push_back(it);
  };
  switch (phase)
  {
    case GolemPhase::Stomp:
      g.hp = kHp1;
      g.plates = std::min(6, mGreed / 10);
      g.plateHp = kPlateHp;
      if (g.plates > 0)
        showMessage(std::to_string(g.plates) + (g.plates == 1 ? " GOLD PLATE" : " GOLD PLATES") + " - YOUR GREED, ITS ARMOR");
      break;
    case GolemPhase::Break:
      mCamera.shake(20, 3.0f);
      playSound(Sfx::Explosion);
      drop(ItemKind::Health, g.exitX);
      if (g.camera >= 0)
        mEnemies[std::size_t(g.camera)].hidden = true;
      break;
    case GolemPhase::Heads:
    {
      static const int kBounce[3] = {10, 8, 6};
      for (int k = 0; k < 3; ++k)
      {
        auto& h = g.heads[std::size_t(k)];
        h = Golem::Head{};
        h.x = h.prevX = g.x + float(k * 5);
        h.y = h.prevY = float(g.floor);
        h.vx = k == 1 ? 1.0f : -1.0f;
        h.bounce = kBounce[k];
        h.vy = -launchSpeed(h.bounce);
        h.hp = kHeadHp;
      }
      g.charger = 0;
      break;
    }
    case GolemPhase::Rebuild:
      drop(ItemKind::Health, g.exitX - 10);
      drop(ItemKind::Turbo, g.exitX + 10);
      mCamera.shake(kRebuild, 1.5f);
      playSound(Sfx::Rumble);
      break;
    case GolemPhase::Sweep:
      g.hp = kHp3;
      g.x = g.prevX = float(g.exitX * kCellsPerTile - 8);
      g.sweeps = 0;
      break;
    case GolemPhase::Crumble:
      mCamera.shake(kCrumble, 2.5f);
      playSound(Sfx::Explosion);
      break;
    case GolemPhase::Done:
    {
      const Vec2 c{(g.x + 8.0f) * kCellSize, float(g.floor - 4) * kCellSize};
      addScore(50000, c);
      flashAt(c, 260.0f, kGold, 40);
      playSound(Sfx::LettersComplete);
      showMessage(mGreed >= 10 ? "KAAN-TOLOK FALLS - GOLD FEVER PAYS DOUBLE" : "KAAN-TOLOK FALLS");
      break;
    }
    default:
      break;
  }
}

void World::hurtGolem(int damage, bool full)
{
  auto& g = mGolem;
  const Vec2 c = cellCenter(g.chest());
  if (g.plates > 0)
  {
    // The plates first: a full draw breaks one outright, anything else chips.
    g.plateHp -= full ? kPlateHp : damage;
    if (g.plateHp <= 0)
    {
      --g.plates;
      g.plateHp = kPlateHp;
      burst(c, kGold, rgb(255, 255, 220), 16, 2.0f);
      playSound(Sfx::Clunk);
    }
    else
    {
      burst(c, kGold, kGoldDark, 5, 1.2f);
      playSound(Sfx::Land);
    }
    return;
  }
  g.hp -= damage;
  g.flash = 6;
  burst(c, kJade, rgb(255, 255, 255), 8, 1.4f);
  playSound(Sfx::Hit);
  if (g.hp > 0)
    return;
  g.hp = 0;
  golemPhase(g.phase == GolemPhase::Stomp ? GolemPhase::Break : GolemPhase::Crumble);
}

bool World::shotAtGolem(Projectile& pr, const CellBox& b)
{
  auto& g = mGolem;
  if (!g.on || pr.kind == ShotKind::Enemy || g.phase == GolemPhase::Seated || g.phase == GolemPhase::Done)
    return false;
  // The candid camera in its eye is a target of its own.
  if (g.camera >= 0)
  {
    const Enemy& cam = mEnemies[std::size_t(g.camera)];
    if (cam.alive && !cam.hidden && cam.box().intersects(b))
      return false;
  }
  const bool full = pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::JadeBow) && pr.strong;
  if (g.phase == GolemPhase::Heads)
  {
    for (int k = 0; k < 3; ++k)
    {
      auto& h = g.heads[std::size_t(k)];
      const int id = -200 - k;
      if (!h.alive || !headBox(h).intersects(b) || std::find(pr.hit.begin(), pr.hit.end(), id) != pr.hit.end())
        continue;
      h.hp -= pr.damage;
      h.flash = 6;
      const Vec2 c = cellCenter(headBox(h));
      burst(c, kJade, rgb(255, 255, 255), 6, 1.2f);
      playSound(Sfx::Hit);
      if (h.hp <= 0)
      {
        h.alive = false;
        burst(c, rgb(170, 140, 90), kGold, 24, 2.4f);
        flashAt(c, 120.0f, kGold, 16);
        playSound(Sfx::Explosion);
        addScore(2000, c);
        bool any = false;
        for (const auto& o : g.heads)
          any = any || o.alive;
        if (!any)
          golemPhase(GolemPhase::Rebuild);
      }
      if (!pr.pierce)
        return true;
      pr.hit.push_back(id);
    }
    return false;
  }
  if (g.phase == GolemPhase::Crumble || !g.body().intersects(b))
    return false;
  // The gem faces the runner wherever they stand: a shot at its height
  // hits it while it shows.
  const CellBox gem = g.chest();
  if ((g.phase == GolemPhase::Stomp || g.phase == GolemPhase::Sweep) && g.open > 0 && b.y + b.h > gem.y &&
      b.y < gem.y + gem.h)
  {
    hurtGolem(pr.damage, full);
    return true;
  }
  // Stone: the shot sparks off.
  burst(cellCenter(b), rgb(255, 250, 220), kGold, 4, 1.0f);
  return true;
}

void World::updateGolem()
{
  auto& g = mGolem;
  if (!g.on)
    return;
  auto& p = mPlayer;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox hit = p.hitBox();
  g.prevX = g.x;
  for (auto& h : g.heads)
  {
    h.prevX = h.x;
    h.prevY = h.y;
    if (h.flash > 0)
      --h.flash;
  }
  if (g.flash > 0)
    --g.flash;
  if (g.open > 0)
    --g.open;
  ++g.t;

  const int left = (g.wallL + 1) * kCellsPerTile, right = g.wallR * kCellsPerTile; // inner, cells
  const bool inside = vulnerable && p.x >= g.arena.x + 2 && p.x + Player::kWidth <= g.arena.x + g.arena.w &&
    p.y <= g.floor && p.y > g.arena.y;
  auto shut = [&]() {
    for (int ty = g.doorY0; ty <= g.doorY1; ++ty)
      for (int tx = g.doorX0; tx <= g.doorX1; ++tx)
        mMap.setBlock(tx, ty, Tile::Solid);
    playSound(Sfx::Crash);
    mCamera.shake(8, 2.0f);
  };

  if (g.phase == GolemPhase::Seated)
  {
    if (!inside)
      return;
    shut();
    golemPhase(GolemPhase::Rise);
    g.wink = mOffered == 42;
    showMessage("KAAN-TOLOK AWAKENS");
    return;
  }
  if (g.away)
  {
    if (!inside)
      return;
    g.away = false;
    shut();
  }

  // Touching it hurts, except as the pile it ends as.
  auto contact = [&](const CellBox& b) {
    if (vulnerable && b.intersects(hit))
      hurtPlayer(1);
  };

  if (g.camera >= 0 && (g.phase == GolemPhase::Rise || g.phase == GolemPhase::Stomp))
  {
    // The candid camera rides in its left eye.
    Enemy& cam = mEnemies[std::size_t(g.camera)];
    cam.x = int(g.x) + 3;
    cam.y = g.floor - 14;
  }

  switch (g.phase)
  {
    case GolemPhase::Rise:
    {
      const int wink = g.wink ? kWink : 0;
      if (g.t > wink && (g.t - wink) % 6 == 0)
        mCamera.shake(6, 2.0f);
      if (g.t == wink + 1)
        playSound(Sfx::Rumble);
      if (g.t == wink + kRise / 2)
      {
        g.lid = true; // the gold lid over its left eye swings up
        if (g.camera >= 0)
          mEnemies[std::size_t(g.camera)].hidden = false;
      }
      if (g.t >= wink + kRise)
        golemPhase(GolemPhase::Stomp);
      return;
    }

    case GolemPhase::Stomp:
    {
      ++g.cycle;
      // Toward the runner at a quarter of a cell a frame, stopping short.
      const float cx = g.x + float(Golem::kW) * 0.5f, pcx = float(p.x) + 1.5f;
      const float gap = std::abs(pcx - cx) - float(Golem::kW) * 0.5f - 1.5f;
      if (g.tell == 0 && gap > float(kKeepOff))
        g.x += pcx < cx ? -0.25f : 0.25f;
      g.x = std::clamp(g.x, float(left), float(right - Golem::kW));
      if (g.cycle == kStompEvery - kStompTell)
      {
        g.tell = kStompTell; // a foot comes up, the rune on it glows
        playSound(Sfx::Creak);
      }
      else if (g.tell > 0 && --g.tell == 0)
      {
        // The stomp: a shockwave each way, three rocks from the ceiling.
        mCamera.shake(12, 3.0f);
        playSound(Sfx::Crash);
        g.waves.push_back({g.x - 2.0f, -1});
        g.waves.push_back({g.x + float(Golem::kW), 1});
        const int px = p.x + 1;
        const int shift = (g.stomps % 3 - 1) * 2;
        for (int k = -1; k <= 1; ++k)
        {
          const int rx = std::clamp(px + k * 8 + shift - 1, left, right - 3);
          g.rocks.push_back({rx, kRockFall, float(g.arena.y)});
        }
        ++g.stomps;
        g.open = kOpen;
        g.cycle = 0;
      }
      contact(g.body());
      break;
    }

    case GolemPhase::Break:
      if (g.t % 5 == 0)
        burst({(g.x + float(g.t % 16)) * kCellSize, float(g.floor - 10) * kCellSize}, rgb(170, 140, 90), kGold, 6, 1.6f,
          false);
      if (g.t >= kBreak)
        golemPhase(GolemPhase::Heads);
      break;

    case GolemPhase::Heads:
    {
      ++g.cycle;
      for (int k = 0; k < 3; ++k)
      {
        auto& h = g.heads[std::size_t(k)];
        if (!h.alive)
          continue;
        const float lo = float(left), hi = float(right - kHeadSize);
        if (h.charge)
        {
          // Along the floor to the wall at 2 cells a frame.
          h.x += h.vx > 0.0f ? 2.0f : -2.0f;
          if (h.x <= lo || h.x >= hi)
          {
            h.x = std::clamp(h.x, lo, hi);
            h.charge = false;
            h.vx = h.x <= lo ? 1.0f : -1.0f;
            h.vy = -launchSpeed(h.bounce);
            mCamera.shake(8, 2.0f);
            playSound(Sfx::Crash);
          }
        }
        else if (h.stop > 0)
        {
          if (h.stop % 4 == 0)
            mCamera.shake(4, 1.0f);
          if (--h.stop == 0)
          {
            h.charge = true;
            h.vx = float(p.x) + 1.5f < h.x + 3.0f ? -1.0f : 1.0f;
            playSound(Sfx::Rumble);
          }
        }
        else
        {
          h.vy += kHeadGravity;
          h.y += h.vy;
          h.x += h.vx;
          if (h.x <= lo || h.x >= hi)
          {
            h.x = std::clamp(h.x, lo, hi);
            h.vx = -h.vx;
          }
          h.down = h.y >= float(g.floor);
          if (h.down)
          {
            h.y = float(g.floor);
            if (g.cycle >= kChargeEvery && k == g.charger)
            {
              // This one stops, rumbles, and charges.
              h.vy = 0.0f;
              h.stop = kRumble;
              g.cycle = 0;
              for (int n = 1; n <= 3; ++n)
                if (g.heads[std::size_t((k + n) % 3)].alive)
                {
                  g.charger = (k + n) % 3;
                  break;
                }
            }
            else
              h.vy = -launchSpeed(h.bounce);
            if (isOnScreen(headBox(h), 0))
              playSound(Sfx::Land);
          }
        }
        if (!g.heads[std::size_t(g.charger)].alive)
          g.charger = k;
        contact(headBox(h));
      }
      break;
    }

    case GolemPhase::Rebuild:
      if (g.t >= kRebuild)
        golemPhase(GolemPhase::Sweep);
      break;

    case GolemPhase::Sweep:
    {
      ++g.cycle;
      if (g.cycle == kSweepEvery - kSweepTell)
      {
        // The arm goes up: an open hand sweeps low (jump), a fist high (crouch).
        g.tell = kSweepTell;
        g.sweepHigh = g.sweeps % 2 == 1;
        g.sweepDir = float(p.x) + 1.5f < g.x + float(Golem::kW) * 0.5f ? -1 : 1;
        playSound(Sfx::Creak);
      }
      else if (g.tell > 0)
      {
        if (--g.tell == 0)
        {
          g.sweep = kSweepFrames;
          playSound(Sfx::Whoosh);
        }
      }
      else if (g.sweep > 0)
      {
        const int f = kSweepFrames - g.sweep + 1;
        const int edge = g.sweepDir > 0 ? int(g.x) + Golem::kW : int(g.x);
        const int wall = g.sweepDir > 0 ? right : left;
        const int hx = edge + (wall - edge) * f / kSweepFrames;
        const CellBox hand = g.sweepHigh ? CellBox{std::min(edge, hx), g.floor - 7, std::abs(hx - edge), 4}
                                         : CellBox{std::min(edge, hx), g.floor - 2, std::abs(hx - edge), 3};
        contact(hand);
        if (--g.sweep == 0)
        {
          g.open = kOpen;
          ++g.sweeps;
          g.cycle = 0;
        }
      }
      // The walls close in: two blocks for every 4 HP it has lost.
      const int want = std::min(4, (kHp3 - g.hp) / 4);
      if (g.slide > 0)
      {
        --g.slide;
        if (g.slide == kSlideFrames / 2 || g.slide == 0)
        {
          // A column each side, unless the runner is in the way.
          const int pl = p.x / kCellsPerTile, pr = (p.x + Player::kWidth - 1) / kCellsPerTile;
          if (g.wallL + 1 < pl)
            setWall(++g.wallL, true);
          if (g.wallR - 1 > pr)
            setWall(--g.wallR, true);
          mCamera.shake(4, 1.5f);
          playSound(Sfx::Rumble);
        }
        if (g.slide == 0)
          ++g.slid;
      }
      else if (g.slid < want)
        g.slide = kSlideDust + kSlideFrames;
      contact(g.body());
      break;
    }

    case GolemPhase::Crumble:
      if (g.t % 4 == 0)
        burst({(g.x + float((g.t * 7) % 16)) * kCellSize, float(g.floor - (g.t * 3) % 18) * kCellSize}, kGold,
          rgb(255, 250, 220), 6, 1.8f);
      if (g.t >= kCrumble)
        golemPhase(GolemPhase::Done);
      return;

    default:
      return;
  }

  // Shockwaves along the floor (jump them) and the rocks they shake loose.
  for (auto& w : g.waves)
  {
    w.x += 2.0f * float(w.dir);
    contact({int(w.x), g.floor, 2, 1});
  }
  g.waves.erase(std::remove_if(g.waves.begin(), g.waves.end(),
                  [&](const Golem::Wave& w) { return w.x < float(left) - 2.0f || w.x > float(right); }),
    g.waves.end());
  for (auto& r : g.rocks)
  {
    if (--r.t == 0)
    {
      contact({r.x, g.floor - 2, 3, 3});
      burst({(float(r.x) + 1.5f) * kCellSize, float(g.floor) * kCellSize}, rgb(170, 140, 90), rgb(90, 70, 40), 10, 1.6f,
        false);
      playSound(Sfx::Clunk);
    }
  }
  g.rocks.erase(std::remove_if(g.rocks.begin(), g.rocks.end(), [](const Golem::Rock& r) { return r.t <= 0; }),
    g.rocks.end());
}

// --- Drawing ----------------------------------------------------------------------------

void World::drawSanctumBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  // Glyph rows carved in the wall; they light up toward a beam.
  for (const auto& g : mGlyphRows)
  {
    const bool on = g.enemy >= 0 && mEnemies[std::size_t(g.enemy)].alive;
    const int n = g.x1 - g.x0 + 1;
    const int lit = g.t > 0 && g.t <= kGlyphTell ? (g.t + 5) / 6 : 0; // quarters of the row
    for (int i = 0; i < n; ++i)
    {
      const int bx = g.x0 + i;
      const float x = float(bx) * 64.0f - camX, y = float(g.row) * 64.0f - camY;
      if (!visible(x, y, 64, 64))
        continue;
      r.fillRect(x + 10, y + 10, 44, 44, rgba(60, 40, 12, 150));
      const bool glow = on && lit > 0 && (n - 1 - i) * 4 < lit * n;
      const Color c = glow ? rgb(255, 240, 160) : (on ? rgb(170, 130, 50) : rgb(90, 70, 40));
      // A little carved face: a frame, two eyes, a mouth line.
      r.fillRect(x + 16, y + 14, 32, 4, c);
      r.fillRect(x + 16, y + 46, 32, 4, c);
      r.fillRect(x + 22, y + 24, 6, 6, c);
      r.fillRect(x + 36, y + 24, 6, 6, c);
      r.fillRect(x + 24 + float((bx * 7) % 3) * 2.0f, y + 36, 16, 3, c);
      if (glow)
        drawGlow(r, mArt, x + 32, y + 32, 40, rgb(255, 220, 120), 0.5f);
    }
  }

  // Coin heaps, lower with every wave that crawls out.
  for (const auto& h : mCoinHeaps)
  {
    const float x = float(h.x) * kCellPx - camX, base = float(h.y + 1) * kCellPx - camY;
    if (!visible(x - 32, base - 96, 192, 96))
      continue;
    const int rows = 1 + std::max(0, h.waves);
    for (int row = 0; row < rows; ++row)
      for (int k = 0; k < 6 - row * 2; ++k)
      {
        const float cx = x - 8.0f + float(row) * 18.0f + float(k) * 24.0f, cy = base - 10.0f - float(row) * 14.0f;
        r.fillRect(cx, cy, 22, 9, (k + row) % 2 ? kGold : rgb(255, 222, 120));
        r.fillRect(cx, cy + 7, 22, 2, kGoldDark);
      }
  }

  // The altars: gold blocks with jade inlay and a bowl of incense.
  for (const auto& a : mAltars)
  {
    if (a.dropped)
      continue;
    const float x = float(a.bx) * 64.0f - camX, y = float(a.by) * 64.0f - camY;
    if (!visible(x, y - 96, 128, 160))
      continue;
    r.fillRect(x, y, 128, 64, a.offer ? kGold : rgb(214, 170, 70));
    r.fillRect(x, y, 128, 8, rgb(255, 236, 150));
    r.fillRect(x + 8, y + 56, 112, 8, kGoldDark);
    r.fillRect(x + 40, y + 20, 48, 24, a.offer ? kJade : rgb(120, 160, 110));
    r.fillRect(x + 48, y + 26, 32, 12, a.offer ? rgb(30, 120, 80) : rgb(70, 90, 60));
    // The bowl and its smoke.
    r.fillRect(x + 44, y - 14, 40, 14, rgb(120, 84, 40));
    const float flare = float(a.flare) / 10.0f;
    for (int k = 0; k < 4; ++k)
    {
      const float t = float((frame + k * 11) % 44) / 44.0f;
      const float sx = x + 64.0f + std::sin(t * 6.0f + float(k)) * 10.0f, sy = y - 20.0f - t * 70.0f;
      r.fillRect(sx - 6, sy - 6, 12, 12, rgba(230, 220, 200, int(110 * (1.0f - t))));
    }
    if (a.flare > 0)
      drawGlow(r, mArt, x + 64, y - 20, 50 + 40 * flare, rgb(255, 200, 90), std::min(1.0f, flare));
    if (a.offered > 0)
      r.drawText(std::to_string(a.offered), x + 64, y - 70, {20.0f, rgb(255, 240, 190), rgb(40, 24, 8)}, Align::Center);
  }

  // The jade basin.
  if (mRefillX >= 0)
  {
    const float x = float(mRefillX) * kCellPx - camX, base = float(mRefillY + 1) * kCellPx - camY;
    if (visible(x, base - 128, 128, 128))
    {
      r.fillRect(x + 40, base - 64, 48, 64, rgb(60, 140, 100));
      r.fillRect(x + 8, base - 96, 112, 32, rgb(80, 190, 140));
      r.fillRect(x + 16, base - 96, 96, 10, mRefillUsed ? rgb(40, 80, 60) : rgb(170, 255, 220));
      if (!mRefillUsed)
        drawGlow(r, mArt, x + 64, base - 96, 60 + 6 * std::sin(float(frame) * 0.2f), kJade, 0.6f);
    }
  }

  // The arena door when it is shut.
  const auto& g = mGolem;
  if (g.on && g.doorX1 >= g.doorX0 && mMap.solid(g.doorX0 * kCellsPerTile, g.doorY0 * kCellsPerTile))
  {
    const float x = float(g.doorX0) * 64.0f - camX, y = float(g.doorY0) * 64.0f - camY;
    const float h = float(g.doorY1 - g.doorY0 + 1) * 64.0f;
    r.fillRect(x, y, 64, h, kGold);
    r.fillRect(x + 8, y + 8, 48, h - 16, rgb(200, 150, 50));
    r.fillRect(x + 26, y + 8, 12, h - 16, kJade);
  }
}

void World::drawSanctumFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  // Beams along the glyph rows.
  for (const auto& g : mGlyphRows)
  {
    if (g.t <= kGlyphTell)
      continue;
    const int parts = std::max(1, g.heads);
    for (int part = 0; part < parts; ++part)
    {
      const int start = kGlyphTell + part * kHeadGap;
      if (g.t <= start || g.t > start + kBeam)
        continue;
      const CellBox b = glyphBand(g, parts > 1 ? part : -1);
      const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
      const float w = float(b.w) * kCellPx;
      const float flick = 0.8f + 0.2f * float((frame / 2) % 2);
      r.fillRect(x, y + 14, w, 36, rgba(255, 210, 90, int(120 * flick)), Blend::Add);
      r.fillRect(x, y + 26, w, 12, rgba(255, 255, 230, int(230 * flick)), Blend::Add);
    }
  }

  const auto& g = mGolem;
  if (!g.on)
    return;
  const float gx = (g.prevX + (g.x - g.prevX) * alpha) * kCellPx - camX;
  const float base = float(g.floor + 1) * kCellPx - camY;
  const float bw = float(Golem::kW) * kCellPx, bh = float(Golem::kH) * kCellPx;

  // Rock shadows, falling rocks, shockwaves.
  for (const auto& rk : g.rocks)
  {
    const float x = float(rk.x) * kCellPx - camX;
    const float grow = 1.0f - float(rk.t) / float(kRockFall);
    r.fillRect(x + 48.0f * (1.0f - grow), base - 10, 96.0f * grow, 10, rgba(20, 10, 0, 150));
    if (rk.t <= 8)
    {
      const float y = base - 96.0f - float(rk.t) * 60.0f;
      r.fillRect(x + 6, y, 84, 84, rgb(150, 120, 80));
      r.fillRect(x + 6, y, 84, 14, rgb(200, 170, 120));
    }
  }
  for (const auto& w : g.waves)
  {
    const float x = w.x * kCellPx - camX;
    r.fillRect(x, base - 40, 64, 40, rgba(255, 220, 140, 170), Blend::Add);
    r.fillRect(x + (w.dir > 0 ? 40.0f : 0.0f), base - 56, 24, 56, rgba(255, 250, 220, 200), Blend::Add);
  }

  // The heads.
  if (g.phase == GolemPhase::Heads)
    for (int k = 0; k < 3; ++k)
    {
      const auto& h = g.heads[std::size_t(k)];
      if (!h.alive)
        continue;
      const float hx = (h.prevX + (h.x - h.prevX) * alpha) * kCellPx - camX;
      const float hy = (h.prevY + (h.y - h.prevY) * alpha + 1.0f) * kCellPx - camY;
      // Its landing shadow.
      const float lift = std::max(0.0f, base - hy);
      const float sw = std::max(60.0f, 192.0f - lift * 0.3f);
      r.fillRect(hx + 96.0f - sw * 0.5f, base - 8, sw, 8, rgba(20, 10, 0, 140));
      float shake = 0.0f;
      if (h.stop > 0)
        shake = (frame % 2 ? 4.0f : -4.0f);
      const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "golem_head", k, 0, kHeadSize, kHeadSize).get(h.vx < 0 ? -1 : 1);
      DrawOpts o;
      if (h.flash > 0)
        o.tint = rgb(255, 255, 255);
      r.draw(*tex, hx + 96.0f + shake, hy, o);
      if (h.charge || h.stop > 0)
        drawGlow(r, mArt, hx + 96.0f, hy - 96.0f, 80, rgb(255, 120, 60), 0.4f);
    }

  // The body.
  if (g.phase == GolemPhase::Heads || g.phase == GolemPhase::Rebuild)
  {
    if (g.phase == GolemPhase::Rebuild)
    {
      // Building itself again: the body rises out of a heap.
      const float k = float(g.t) / float(kRebuild);
      const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "golem", 1, 0, Golem::kW, Golem::kH).get(1);
      DrawOpts o;
      o.alpha = k;
      r.draw(*tex, float(g.exitX * kCellsPerTile) * kCellPx - camX, base + bh * (1.0f - k) * 0.4f, o);
    }
  }
  else if (g.phase == GolemPhase::Crumble || g.phase == GolemPhase::Done)
  {
    // A pile of gold, and the words on its base.
    const float k = g.phase == GolemPhase::Done ? 1.0f : float(g.t) / float(kCrumble);
    if (g.phase == GolemPhase::Crumble)
    {
      const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "golem", 1, 0, Golem::kW, Golem::kH).get(1);
      DrawOpts o;
      o.alpha = 1.0f - k;
      r.draw(*tex, gx + bw * 0.5f + (frame % 2 ? 4.0f : -4.0f), base + bh * k * 0.5f, o);
    }
    for (int row = 0; row < 4; ++row)
      for (int c = 0; c < 10 - row * 2; ++c)
      {
        const float px = gx + 32.0f + float(row) * 32.0f + float(c) * 44.0f, py = base - 20.0f - float(row) * 22.0f * k;
        r.fillRect(px, py, 40, 18, (c + row) % 2 ? kGold : rgb(255, 222, 120));
      }
    r.drawText("MADE IN NEON CITY", gx + bw * 0.5f, base - 4.0f, {14.0f, rgb(80, 50, 10)}, Align::Center);
  }
  else if (g.phase != GolemPhase::Break || g.t < kBreak / 2)
  {
    int variant = 0;
    float lift = 0.0f;
    switch (g.phase)
    {
      case GolemPhase::Seated:
        variant = 0;
        break;
      case GolemPhase::Rise:
      {
        const int wink = g.wink ? kWink : 0;
        variant = g.t > wink + kRise / 2 ? 1 : 0;
        lift = -std::max(0.0f, std::min(1.0f, float(g.t - wink) / float(kRise))) * 0.0f;
        break;
      }
      case GolemPhase::Stomp:
        variant = g.tell > 0 ? 2 : 1;
        break;
      case GolemPhase::Sweep:
        variant = g.tell > 0 || g.sweep > 0 ? (g.sweepHigh ? 4 : 3) : 1;
        break;
      default:
        variant = 1;
        break;
    }
    float shake = 0.0f;
    if (g.phase == GolemPhase::Break || (g.phase == GolemPhase::Rise && g.t > (g.wink ? kWink : 0)))
      shake = frame % 2 ? 5.0f : -5.0f;
    const int dir = g.phase == GolemPhase::Sweep ? g.sweepDir : (float(mPlayer.x) < g.x ? -1 : 1);
    const Texture* tex = &styledEnemySprite(mArt, r, mTheme, "golem", variant, 0, Golem::kW, Golem::kH).get(dir);
    DrawOpts o;
    if (g.flash > 0 && (frame / 2) % 2)
      o.tint = rgb(255, 200, 200);
    r.draw(*tex, gx + bw * 0.5f + shake, base + lift, o);
    if (g.phase == GolemPhase::Seated)
      r.drawText("MADE IN NEON CITY", gx + bw * 0.5f, base - 8.0f, {14.0f, rgb(80, 50, 10)}, Align::Center);

    // The eyes: the gold lid over the left one (the camera), the wink.
    const float ey = base - bh + 4.5f * kCellPx;
    if (!g.lid)
      r.fillRect(gx + 3.0f * kCellPx, ey - 8, 2.0f * kCellPx + 8, 2.0f * kCellPx + 8, kGold);
    const bool winking = g.wink && g.phase == GolemPhase::Rise && g.t <= kWink;
    if (winking)
      r.fillRect(gx + 11.0f * kCellPx, ey + 24, 2.0f * kCellPx, 10, rgb(60, 40, 10));
    else if (g.phase != GolemPhase::Seated)
      drawGlow(r, mArt, gx + 12.0f * kCellPx, ey + 32, 40, kJade, 0.7f);

    // The chest gem, its plates, the open window.
    if (g.phase == GolemPhase::Stomp || g.phase == GolemPhase::Sweep)
    {
      const CellBox c = g.chest();
      const float cx = gx + float(c.x - int(g.x)) * kCellPx, cy = float(c.y) * kCellPx - camY;
      const bool open = g.open > 0;
      r.fillRect(cx, cy, 128, 128, open ? rgb(60, 255, 170) : rgb(30, 110, 80));
      if (open)
        drawGlow(r, mArt, cx + 64, cy + 64, 110, kJade, 0.6f + 0.2f * std::sin(float(frame) * 0.5f));
      for (int k = 0; k < g.plates; ++k)
      {
        const float py = cy - 10.0f + float(k) * 24.0f;
        r.fillRect(cx - 12, py, 152, 18, k == g.plates - 1 && g.plateHp < kPlateHp ? rgb(200, 150, 60) : kGold);
        r.fillRect(cx - 12, py + 14, 152, 4, kGoldDark);
      }
    }
    // The stomping foot's rune.
    if (g.phase == GolemPhase::Stomp && g.tell > 0)
      drawGlow(r, mArt, gx + bw * 0.5f, base - 24, 70, rgb(255, 140, 60), 0.8f);
    // The sweeping arm.
    if (g.phase == GolemPhase::Sweep && g.sweep > 0)
    {
      const int f = kSweepFrames - g.sweep + 1;
      const int left = (g.wallL + 1) * kCellsPerTile, right = g.wallR * kCellsPerTile;
      const int edge = g.sweepDir > 0 ? int(g.x) + Golem::kW : int(g.x);
      const int wall = g.sweepDir > 0 ? right : left;
      const float hx = float(edge + (wall - edge) * f / kSweepFrames) * kCellPx - camX;
      const float sx = float(edge) * kCellPx - camX;
      const float hy = g.sweepHigh ? base - 6.0f * kCellPx : base - 1.5f * kCellPx;
      r.drawLine(sx, base - bh * 0.6f, hx, hy, 40.0f, rgb(190, 150, 70));
      r.fillRect(hx - 48, hy - 48, 96, 96, g.sweepHigh ? rgb(170, 130, 60) : rgb(214, 170, 80));
      r.fillRect(hx - 48, hy - 48, 96, 12, rgb(255, 230, 150));
    }
  }

  // The walls' dust before a slide.
  if (g.phase == GolemPhase::Sweep && g.slide > kSlideFrames)
    for (int side : {0, 1})
    {
      const float x = float(side ? g.wallR : g.wallL + 1) * 64.0f - camX;
      for (int k = 0; k < 4; ++k)
        r.fillRect(x - 12 + float((frame + k * 5) % 24), float(g.arena.y) * kCellPx - camY + float(k) * 200.0f +
          float(frame % 40) * 4.0f, 24, 24, rgba(220, 190, 130, 150));
    }
}

void World::drawSanctumHud(Renderer& r, int frame) const
{
  if (mGreedOn)
  {
    // The greed meter: GREED while it counts against you, BONUS once it pays.
    const bool pays = mGolem.phase == GolemPhase::Done && mGreed >= 10;
    const float x = float(kScreenW) - 312.0f, y = 96.0f;
    r.fillRect(x, y, 300, 52, rgba(8, 6, 22, 190));
    r.drawText(pays ? "BONUS" : "GREED", x + 10, y + 4, {17.0f, pays ? kJade : kGold, rgb(20, 12, 4)});
    r.fillRect(x + 90, y + 11, 160, 8, rgba(255, 255, 255, 40));
    r.fillRect(x + 90, y + 11, 160.0f * float(mGreed) / 99.0f, 8, pays ? kJade : kGold);
    for (int k = 1; k < 10; ++k)
      r.fillRect(x + 90 + 160.0f * float(k * 10) / 99.0f, y + 9, 2, 12, rgb(20, 12, 4));
    r.drawText(std::to_string(mGreed), x + 290, y + 2, {20.0f, rgb(255, 240, 200), rgb(20, 12, 4)}, Align::Right);
    r.drawText("OFFERED " + std::to_string(mOffered), x + 10, y + 28, {15.0f, rgb(200, 190, 160), rgb(20, 12, 4)});
  }
  if (golemFight())
  {
    const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 54.0f;
    const int plates = std::min(6, mGreed / 10);
    const float total = float(kHp1 + 3 * kHeadHp + kHp3 + plates * kPlateHp);
    r.fillRect(bx - 10, by - 30, bw + 20, 52, rgba(8, 6, 22, 200));
    r.drawText("KAAN-TOLOK", bx, by - 26, {17.0f, kGold, rgb(20, 12, 4)});
    r.fillRect(bx, by, bw, 12, rgba(255, 255, 255, 40));
    r.fillRect(bx, by, bw * std::min(1.0f, float(golemHp()) / total), 12,
      (mGolem.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : kGold);
    for (const float cut : {float(kHp3) / total, float(kHp3 + 3 * kHeadHp) / total})
      r.fillRect(bx + bw * cut - 1.0f, by - 3, 3, 18, rgb(20, 16, 30));
  }
}

} // namespace gr
