// Level 25, Tiny Kingdom: the garden's day/dusk cycle (sundial, time-lapse
// sweep, the pond), buds and seeds the Chrono Mister fast-forwards, Soldier
// Ant columns, Jumping Spiders, Bombardier Beetles, the narrator, the
// dandelion's seed head, the rare butterfly and the ladybug. The day and
// dusk platforms themselves are timer layers (world_level.cpp). Drawing is
// in world_garden_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

CellBox blockBox(int x0, int y0, int w, int h)
{
  return {x0 * kCellsPerTile, y0 * kCellsPerTile, w * kCellsPerTile, h * kCellsPerTile};
}

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupGardenEntity(const EntityDef& e)
{
  auto& g = mGarden;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.kind == "bud" && e.hasPos)
  {
    g.buds.push_back({e.x, e.y, std::max(1, e.num("w", 3)), 0});
    g.on = true;
    return true;
  }
  if (e.kind == "seed")
  {
    Seed s;
    s.x = e.hasPos ? e.x : e.num("x");
    s.y = e.hasPos ? e.y : e.num("y");
    s.h = std::max(1, e.num("h", 6));
    g.seeds.push_back(s);
    g.on = true;
    return true;
  }
  if (e.kind == "antcolumn")
  {
    const auto path = e.path("path");
    if (path.size() < 2)
      return false;
    const int def = enemyIndex("soldier_ant");
    AntColumn c;
    c.id = e.id;
    c.x0 = std::min(path[0].first, path[1].first) * kCellsPerTile;
    c.x1 = std::max(path[0].first, path[1].first) * kCellsPerTile;
    c.y = path[0].second * kCellsPerTile + 1;
    c.period = std::max(20, e.num("period", 300));
    c.carrier = e.num("carrier", 0) != 0;
    const int count = std::max(1, e.num("count", 5));
    for (int i = 0; i < count && def >= 0; ++i)
    {
      spawnEnemy(def, c.x0, c.y);
      Enemy& en = mEnemies.back();
      en.carrier = c.carrier;
      en.attach = int(g.columns.size()); // its column
      en.variant = i;                    // its place in the column
      en.hidden = true;
      c.ants.push_back(int(mEnemies.size()) - 1);
    }
    g.columns.push_back(c);
    g.on = true;
    return true;
  }
  if (e.kind == "narrate")
  {
    Narration n;
    n.id = e.id;
    n.runner = e.str("runner", "any");
    n.line = e.str("line");
    n.on = e.str("on", "enter");
    if (e.rect("rect", x0, y0, x1, y1))
    {
      n.x0 = x0;
      n.y0 = y0;
      n.x1 = x1;
      n.y1 = y1;
    }
    g.lines.push_back(n);
    g.on = true;
    return true;
  }
  if (e.kind == "fluid" && e.str("kind") == "pond" && e.rect("rect", x0, y0, x1, y1))
  {
    g.ponds.push_back({x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 + 1) * kCellsPerTile - 1, (y1 + 1) * kCellsPerTile - 1});
    g.on = true;
    return true;
  }
  if (e.kind == "dandelion")
  {
    g.dandelionX = e.hasPos ? e.x : e.num("x");
    g.dandelionY = e.hasPos ? e.y : e.num("y");
    g.on = true;
    return true;
  }
  if (e.kind == "prize" && e.str("kind") == "rare_butterfly")
  {
    for (const auto& [px, py] : e.path("path"))
      g.bflyPath.emplace_back(px * kCellsPerTile, py * kCellsPerTile);
    if (g.bflyPath.empty() && e.hasPos)
      g.bflyPath.emplace_back(e.x * kCellsPerTile, e.y * kCellsPerTile);
    if (g.bflyPath.empty())
      return false;
    g.butterfly = true;
    g.bx = float(g.bflyPath[0].first);
    g.by = float(g.bflyPath[0].second);
    g.bflyScore = e.num("score", 10000);
    g.on = true;
    return true;
  }
  if (e.kind == "ladybug")
  {
    g.ladybugX = e.hasPos ? e.x : e.num("x");
    g.ladybugY = e.hasPos ? e.y : e.num("y");
    g.on = true;
    return true;
  }
  return false;
}

void World::setupGardenEnemy(Enemy& en, const EntityDef& e)
{
  if (en.kind == EnemyKind::SoldierAnt || en.kind == EnemyKind::JumpingSpider || en.kind == EnemyKind::BombardierBeetle)
  {
    en.attach = en.kind == EnemyKind::SoldierAnt ? -1 : 0; // a lone ant has no column
    mGarden.on = true;
  }
  // `patrol=x0,x1` (blocks): a beetle turns at these ends too.
  if (en.kind == EnemyKind::BombardierBeetle)
  {
    const auto patrol = e.list("patrol");
    en.railX0 = patrol.size() == 2 ? patrol[0] * kCellsPerTile : -1;
    en.railX1 = patrol.size() == 2 ? (patrol[1] + 1) * kCellsPerTile : -1;
  }
}

void World::linkGarden()
{
  auto& g = mGarden;
  for (const auto& l : mLayers)
    if (l.driver == LayerDriver::Timer && (l.id.rfind("day_", 0) == 0 || l.id.rfind("dusk_", 0) == 0))
      g.cycle = g.on = true;
  if (!g.on)
    return;
  for (std::size_t i = 0; i < mPlatforms.size(); ++i)
    if (mPlatforms[i].id == "seedhead")
    {
      g.seedhead = int(i);
      mPlatforms[i].hidden = true; // until the dandelion goes to seed
    }
  // The ladybug sits in front of the bonus door, which sleeps until she is misted.
  if (g.ladybugX >= 0)
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = true;
  g.safeX = mPlayer.x;
  g.safeY = mPlayer.y;
}

int World::gardenClock() const
{
  return ((mStats.frames % kGardenCycle) + kGardenCycle) % kGardenCycle;
}

bool World::gardenDusk() const
{
  return gardenClock() >= kGardenHalf;
}

void World::gardenSay(const std::string& line)
{
  auto& g = mGarden;
  g.subtitleText = line;
  g.subtitle = 30 + int(line.size()) * 2;
  playSound(Sfx::Click);
}

// --- The Chrono Mister ---------------------------------------------------------------------

CellBox World::mistBox() const
{
  const auto& p = mPlayer;
  const int dir = mGarden.mistFace;
  const int top = p.stance == Stance::Crouched ? p.y - 3 : p.y - 4;
  return {dir > 0 ? p.x + Player::kWidth : p.x - 6, top, 6, 4};
}

void World::updateMister(const Button& fire)
{
  auto& p = mPlayer;
  auto& g = mGarden;
  // Up and down it fires a plain puff (for switches overhead).
  if (p.stance == Stance::Up || p.stance == Stance::Down || p.stance == Stance::Jetpack)
  {
    g.mist = 0;
    if (fire.triggered && (p.shotCooldown == 0 || p.turbo > 0))
    {
      fireShot();
      p.shotCooldown = 4;
    }
    return;
  }
  if (!fire.pressed)
  {
    g.mist = 0;
    g.ladybugMisting = false;
    return;
  }
  ++g.mist;
  g.mistFace = p.facing;
  if (g.mist == 1)
    playSound(Sfx::Puff);
  p.muzzleTicks = 4;
  p.muzzleStance = p.stance;
  mistCone();
  // A unit of ammo every 4 frames held (Turbo: free).
  if (g.mist % kMistAmmoEvery == 0 && p.turbo == 0 && --p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    g.mist = 0;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  }
}

void World::mistCone()
{
  auto& g = mGarden;
  const CellBox cone = mistBox();
  // Enemies: fast-forwarded to harm.
  if ((g.mist - 1) % kMistDamageEvery == 0)
    for (auto& e : mEnemies)
    {
      if (!e.alive || e.hidden || !e.box().intersects(cone))
        continue;
      if (e.kind == EnemyKind::JumpingSpider && e.attach == 1)
        e.tell = 1; // a crouching spider leaps at once
      if (e.kind == EnemyKind::SoldierAnt)
        provokeAnts(e.attach);
      const bool wasAlive = e.alive;
      shotHitsEnemy(e, g.mistFace, protoDef(ProtoId::ChronoMister).damage);
      if (wasAlive && !e.alive)
        ++mStats.protoKills;
    }
  // Buds open, seeds sprout.
  for (auto& b : g.buds)
    if (b.open == 0 && blockBox(b.x, b.y, b.w, 2).intersects(cone))
    {
      const CellBox row = blockBox(b.x, b.y, b.w, 1);
      if (row.intersects(mPlayer.box()))
        continue; // never opens inside the runner
      b.open = kBudOpen;
      for (int x = b.x; x < b.x + b.w; ++x)
        if (mMap.block(x, b.y) == Tile::Empty)
          mMap.setBlock(x, b.y, Tile::Platform);
      burst(cellCenter(row), rgb(255, 170, 220), rgb(255, 240, 120), 12, 1.2f);
      playSound(Sfx::Item);
    }
  for (auto& s : g.seeds)
    if (!s.grown && s.sprout == 0 && blockBox(s.x, s.y, 1, 1).intersects(cone))
    {
      s.sprout = kSeedSprout;
      playSound(Sfx::Item);
    }
  // The dandelion's base: its head goes to seed.
  if (g.dandelionX >= 0 && !g.seeded && g.seeding == 0 &&
      blockBox(g.dandelionX - 1, g.dandelionY - 3, 3, 4).intersects(cone))
    g.seeding = kDandelionSeed;
  // The rare butterfly: caught.
  if (g.butterfly && g.bflyState == 0)
  {
    const CellBox bb{int(g.bx) - 1, int(g.by) - 1, 3, 3};
    if (bb.intersects(cone))
    {
      g.bflyState = 1;
      addScore(g.bflyScore, {g.bx, g.by});
      playSound(Sfx::LettersComplete);
      showMessage("A RARE SPECIMEN! +" + std::to_string(g.bflyScore));
    }
  }
  // The ladybug: her spots spell G-U-N and she lets you climb on.
  if (g.ladybugX >= 0)
  {
    const bool on = blockBox(g.ladybugX, g.ladybugY, 2, 3).intersects(cone);
    if (on && !g.ladybugMisting && g.ladybug < 2)
    {
      ++g.ladybug;
      playSound(Sfx::LettersComplete);
      if (g.ladybug == 1)
      {
        for (auto& pr : mProps)
          if (pr.kind == PropKind::BonusDoor)
            pr.dormant = false;
        showMessage("G-U-N... SHE LETS YOU CLIMB ON");
      }
      else
        showMessage("4-2");
    }
    g.ladybugMisting = on;
  }
}

// --- Enemies -------------------------------------------------------------------------------

void World::provokeAnts(int column)
{
  auto& g = mGarden;
  if (column < 0 || column >= int(g.columns.size()))
    return;
  auto& c = g.columns[std::size_t(column)];
  if (c.hostile > 0 || c.tell > 0)
    return;
  c.tell = kAntTell;
  c.homing = false;
  playSound(Sfx::Chitter);
}

void World::updateSoldierAnt(Enemy& e, const EnemyDef& def)
{
  (void)def;
  auto& g = mGarden;
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const bool lone = e.attach < 0 || e.attach >= int(g.columns.size());
  AntColumn* c = lone ? nullptr : &g.columns[std::size_t(e.attach)];
  const bool angry = c ? c->hostile > 0 : true;
  if (c && c->tell > 0)
    return; // antennae up, the column halts
  if (!angry && c && !c->homing)
  {
    // Marching: out for the first half of the period, pingpong along the
    // path one block behind the ant ahead; in the anthill for the second.
    const int half = c->period / 2;
    const int lead = c->phase / 2 - e.variant * kCellsPerTile;
    const int len = std::max(1, c->x1 - c->x0);
    if (c->phase >= half || lead < 0)
    {
      e.hidden = true;
      e.x = e.prevX = c->x0;
      e.y = e.prevY = c->y;
      return;
    }
    const int m = lead % (2 * len);
    const int nx = c->x0 + (m <= len ? m : 2 * len - m);
    e.dir = (m < len) ? 1 : -1;
    if (e.hidden)
    {
      e.hidden = false;
      e.drawSnap = true;
      e.prevX = nx;
    }
    e.x = nx;
    e.y = c->y;
    // Touched: the column turns.
    if (p.state != PlayerState::Dying && e.box().intersects(p.hitBox()))
      provokeAnts(e.attach);
    return;
  }
  if (e.hidden)
    return;
  // Falling.
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (c && c->homing)
  {
    // Back to the anthill.
    const int dx = c->x1 - e.x;
    if (std::abs(dx) <= 1)
    {
      e.hidden = true;
      return;
    }
    e.dir = dx > 0 ? 1 : -1;
    if (!(e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b)))
      e.x += e.dir;
    else
      e.hidden = true; // walled off: it goes underground
    return;
  }
  // Angry: chase and bite.
  const int dx = (p.x + 1) - (e.x + 1);
  e.dir = dx < 0 ? -1 : 1;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (std::abs(dx) > 1 && !wall && mMap.solidTop(aheadX, b.bottom() + 1))
    e.x += e.dir;
  if (e.cool == 0 && p.state != PlayerState::Dying && e.box().intersects(p.hitBox()))
  {
    e.cool = def.cooldown;
    if (e.carrier)
    {
      infect(); // it clings
      e.alive = false;
    }
    else
      hurtPlayer(1);
  }
}

void World::updateJumpingSpider(Enemy& e, const EnemyDef& def)
{
  auto& g = mGarden;
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  SpiderLeap* leap = nullptr;
  const int idx = int(&e - mEnemies.data());
  for (auto& l : g.leaps)
    if (l.enemy == idx)
      leap = &l;
  const CellBox b = e.box();
  switch (e.attach)
  {
    case 0: // stalking, about six blocks off
    {
      if (!mMap.onSolidGround(b))
      {
        mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
        return;
      }
      const int dx = (p.x + 1) - (e.x + e.w / 2);
      e.dir = dx < 0 ? -1 : 1;
      const bool seen = std::abs(dx) <= 18 && std::abs(p.y - e.y) <= 12 && p.state != PlayerState::Dying;
      if (seen && e.cool == 0 && p.state == PlayerState::OnGround)
      {
        e.attach = 1;
        e.tell = def.tell;
        e.ox = p.x;
        e.oy = p.y;
        return;
      }
      if (e.timer % std::max(1, def.stepEvery) != 0)
        return;
      const int want = std::abs(dx) < 10 ? -e.dir : (std::abs(dx) > 14 ? e.dir : 0);
      if (want == 0)
        return;
      const int aheadX = want > 0 ? b.right() + 1 : b.left() - 1;
      const bool wall = want > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
      if (!wall && mMap.solidTop(aheadX, b.bottom() + 1))
        e.x += want;
      return;
    }
    case 1: // crouched: it leaps where the runner stood when it crouched
      if (--e.tell > 0)
        return;
      {
        if (!leap)
        {
          g.leaps.push_back({idx});
          leap = &g.leaps.back();
        }
        const float gy = 0.25f, T = 14.0f;
        leap->fx = float(e.x);
        leap->fy = float(e.y);
        const float tx = float(e.ox + 1 - e.w / 2), ty = float(e.oy);
        leap->vx = (tx - leap->fx) / T;
        leap->vy = (ty - leap->fy - 0.5f * gy * T * T) / T;
        e.attach = 2;
        e.timer = 0;
        playSound(Sfx::Jump);
      }
      return;
    case 2: // in the air: coming down on the runner bites
    {
      if (!leap)
      {
        e.attach = 0;
        return;
      }
      leap->vy = std::min(leap->vy + 0.25f, 1.5f);
      const int nx = int(std::lround(leap->fx + leap->vx));
      if (!mMap.overlapsSolid(boxAt(nx, e.y, e.w, e.h)))
      {
        leap->fx += leap->vx;
        e.x = nx;
      }
      else
        leap->vx = 0.0f;
      const int ny = int(std::lround(leap->fy + leap->vy));
      if (ny > e.y)
      {
        for (int y = e.y + 1; y <= ny; ++y)
        {
          if (mMap.overlapsSolid(boxAt(e.x, y, e.w, e.h)))
          {
            leap->vy = 0.0f;
            e.attach = 0;
            e.cool = def.cooldown;
            playSound(Sfx::Land);
            break;
          }
          e.y = y;
        }
        leap->fy = float(e.y);
      }
      else if (ny < e.y && !mMap.overlapsSolid(boxAt(e.x, ny, e.w, e.h)))
      {
        e.y = ny;
        leap->fy += leap->vy;
      }
      else if (ny < e.y)
        leap->vy = 0.0f;
      // Coming down on the runner (or landing on them) bites, once a leap.
      if (e.tell == 0 && (e.timer >= 10 || e.attach == 0) && p.state != PlayerState::Dying &&
          e.box().intersects(p.hitBox()))
      {
        e.tell = 1;
        hurtPlayer(1);
      }
      if (e.timer > 40 || e.y > mMap.height() + 4)
      {
        e.attach = 0;
        e.cool = def.cooldown;
        if (e.y > mMap.height() + 4)
          e.alive = false;
      }
      return;
    }
    default:
      e.attach = 0;
  }
}

void World::updateBombardierBeetle(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  const CellBox b = e.box();
  if (!mMap.onSolidGround(b))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  // The jet: a 3-block cone out of its back.
  const CellBox jet{e.dir > 0 ? b.left() - 6 : b.right() + 1, b.top() - 1, 6, b.h + 1};
  if (e.attach == 1)
  {
    if (--e.tell > 0)
      return;
    e.attach = 0;
    e.cool = def.cooldown;
    burst(cellCenter(jet), rgb(255, 250, 220), rgb(255, 170, 60), 18, 1.8f);
    playSound(Sfx::Puff);
    if (p.state != PlayerState::Dying && jet.intersects(p.hitBox()))
      hurtPlayer(1);
    return;
  }
  // The runner behind it, within 4 blocks: the abdomen swells.
  const int back = e.dir > 0 ? b.left() : b.right();
  const int px = p.x + 1;
  const bool behind = e.dir > 0 ? (px < back && back - px <= 8) : (px > back && px - back <= 8);
  if (behind && e.cool == 0 && std::abs(p.y - e.y) <= 4 && p.state != PlayerState::Dying)
  {
    e.attach = 1;
    e.tell = def.tell;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool end = e.railX0 >= 0 && (e.dir > 0 ? b.right() + 1 >= e.railX1 : b.left() <= e.railX0);
  if (wall || end || !mMap.solidTop(aheadX, b.bottom() + 1))
    e.dir = -e.dir;
  else
    e.x += e.dir;
}

// Shots: an ant hit turns its column; anything but the mister scares the
// rare butterfly off for good.
int World::shotAtGardenEnemy(Projectile& pr, Enemy& e)
{
  (void)pr;
  if (e.kind == EnemyKind::SoldierAnt)
    provokeAnts(e.attach);
  return 0;
}

bool World::shotAtGarden(Projectile& pr, const CellBox& b)
{
  auto& g = mGarden;
  (void)pr;
  if (g.butterfly && g.bflyState == 0)
  {
    const CellBox bb{int(g.bx) - 1, int(g.by) - 1, 3, 3};
    if (bb.intersects(b))
    {
      g.bflyState = 2;
      showMessage("THE RARE BUTTERFLY FLIES OFF");
    }
  }
  return false;
}

// --- Update --------------------------------------------------------------------------------

void World::updateGarden(const PlayerInput& input)
{
  auto& g = mGarden;
  auto& p = mPlayer;
  const int clock = gardenClock();
  if (g.subtitle > 0)
    --g.subtitle;

  // The last solid ground stood on (not a flower or a mushroom).
  if (p.state == PlayerState::OnGround)
  {
    const CellBox pb = p.box();
    bool solid = true;
    for (int x = pb.left(); x <= pb.right(); x += 2)
    {
      const int bx = x / kCellsPerTile, by = (pb.bottom() + 1) / kCellsPerTile;
      if (mMap.block(bx, by) == Tile::Empty)
        continue;
      solid = solid && mMap.block(bx, by) == Tile::Solid &&
        mLayerMask[std::size_t(std::clamp(by, 0, mLevel->height - 1) * mLevel->width + std::clamp(bx, 0, mLevel->width - 1))] == 0;
    }
    if (solid && !riding())
    {
      g.safeX = p.x;
      g.safeY = p.y;
    }
  }
  // The pond: a heart, and back on the bank.
  for (const auto& pond : g.ponds)
  {
    const CellBox pr{pond.x0, pond.y0, pond.x1 - pond.x0 + 1, pond.y1 - pond.y0 + 1};
    if (p.state != PlayerState::Dying && pr.intersects(p.box()) && p.box().bottom() > pond.y0)
    {
      burst(cellCenter(p.box()), rgb(150, 210, 255), rgb(255, 255, 255), 16, 1.6f);
      playSound(Sfx::Splash);
      hurtPlayer(1);
      if (p.state != PlayerState::Dying && g.safeX >= 0)
      {
        p.x = p.prevX = g.safeX;
        p.y = p.prevY = g.safeY;
        p.state = PlayerState::OnGround;
        setVisual(PlayerVisual::Standing);
      }
      break;
    }
  }

  // Buds close, seeds sprout.
  for (auto& b : g.buds)
    if (b.open > 0 && --b.open == 0)
      for (int x = b.x; x < b.x + b.w; ++x)
        if (mMap.block(x, b.y) == Tile::Platform)
          mMap.setBlock(x, b.y, Tile::Empty);
  for (auto& s : g.seeds)
    if (!s.grown && s.sprout > 0 && --s.sprout == 0)
    {
      s.grown = true;
      for (int y = s.y; y > s.y - s.h; --y)
        if (mMap.block(s.x, y) == Tile::Empty)
          mMap.setBlock(s.x, y, Tile::Ladder);
      burst(cellCenter(blockBox(s.x, s.y - s.h + 1, 1, s.h)), rgb(120, 220, 90), rgb(220, 255, 160), 20, 1.4f);
      playSound(Sfx::Grow);
    }

  // Ant columns.
  for (std::size_t i = 0; i < g.columns.size(); ++i)
  {
    auto& c = g.columns[i];
    if (c.tell > 0 && --c.tell == 0)
      c.hostile = kAntHostile;
    else if (c.hostile > 0 && --c.hostile == 0)
      c.homing = true;
    if (c.homing)
    {
      bool all = true;
      for (int a : c.ants)
        all = all && (!mEnemies[std::size_t(a)].alive || mEnemies[std::size_t(a)].hidden);
      if (all)
      {
        c.homing = false;
        c.phase = c.period / 2; // in the anthill for the rest of the period
      }
    }
    if (c.tell == 0 && c.hostile == 0 && !c.homing)
      c.phase = (c.phase + 1) % c.period;
  }

  // The dandelion: misted, or at the next time-lapse sweep, its head goes
  // to seed and the seed head waits at its foot for a rider.
  if (g.dandelionX >= 0 && !g.seeded)
  {
    if (g.seeding == 0 && g.cycle && (clock == kGardenHalf - kGardenSweep || clock == kGardenCycle - kGardenSweep))
      g.seeding = kDandelionSeed;
    if (g.seeding > 0 && --g.seeding == 0)
    {
      g.seeded = true;
      if (g.seedhead >= 0)
        mPlatforms[std::size_t(g.seedhead)].hidden = false;
      burst({float(g.dandelionX * kCellsPerTile + 1), float((g.dandelionY - 32) * kCellsPerTile)}, rgb(255, 255, 255),
        rgb(240, 240, 220), 30, 2.0f);
      playSound(Sfx::Grow);
    }
  }
  if (g.seedhead >= 0 && g.seeded)
  {
    const auto& pl = mPlatforms[std::size_t(g.seedhead)];
    for (auto& n : g.lines)
      if (!n.said && n.on == "seedhead" && !pl.parked)
      {
        n.said = true;
        gardenSay(n.line);
      }
  }

  // The rare butterfly flutters along its path.
  if (g.butterfly && g.bflyState == 0 && g.bflyPath.size() > 1)
  {
    const auto [tx, ty] = g.bflyPath[std::size_t(g.bflyTarget)];
    const float dx = float(tx) - g.bx, dy = float(ty) - g.by;
    const float d = std::hypot(dx, dy);
    if (d < 0.6f)
      g.bflyTarget = (g.bflyTarget + 1) % int(g.bflyPath.size());
    else
    {
      g.bx += dx / d * 0.5f;
      g.by += dy / d * 0.5f + std::sin(float(mStats.frames) * 0.4f) * 0.2f;
    }
  }
  else if (g.bflyState == 2)
    g.by -= 0.6f; // off over the hedge

  // The narrator.
  for (auto& n : g.lines)
  {
    if (n.said || (n.runner != "any" && n.runner != mCharacter.id))
      continue;
    bool trigger = false;
    if (n.on == "enter" && n.x1 >= n.x0)
      trigger = blockBox(n.x0, n.y0, n.x1 - n.x0 + 1, n.y1 - n.y0 + 1).intersects(p.box());
    else if (n.on == "hurt")
      trigger = mStats.tookDamage;
    if (trigger)
    {
      n.said = true;
      gardenSay(n.line);
    }
  }
  // Standing still for 30 seconds.
  const bool still = p.x == p.prevX && p.y == p.prevY && !input.left && !input.right && !input.jump.pressed &&
    !input.fire.pressed && p.state == PlayerState::OnGround;
  g.still = still ? g.still + 1 : 0;
  if (g.still >= kStillLine && !g.stillSaid)
  {
    g.stillSaid = true;
    gardenSay("THEY... JUST STAND THERE. MAGNIFICENT.");
  }
}

void World::resetGarden()
{
  auto& g = mGarden;
  for (auto& c : g.columns)
  {
    c.tell = 0;
    if (c.hostile > 0)
    {
      c.hostile = 0;
      c.homing = true;
    }
  }
  for (auto& e : mEnemies)
    if (e.kind == EnemyKind::JumpingSpider && e.attach != 0)
    {
      e.attach = 0;
      e.cool = 30;
    }
  g.leaps.clear();
  g.mist = 0;
  g.still = 0;
}

// --- Saves ---------------------------------------------------------------------------------

bool World::gardenCanSave() const
{
  const auto& g = mGarden;
  bool busy = g.seeding > 0 || g.leaps.size() > 0;
  for (const auto& c : g.columns)
    busy = busy || c.tell > 0 || c.hostile > 0 || c.homing;
  for (const auto& s : g.seeds)
    busy = busy || s.sprout > 0;
  for (const auto& e : mEnemies)
    busy = busy || (e.alive && e.kind == EnemyKind::JumpingSpider && e.attach != 0);
  return !busy;
}

std::vector<int> World::gardenSave() const
{
  // [seeded, ladybug, butterfly, still said, then per narration, seed,
  //  bud (frames open), column (phase)]
  const auto& g = mGarden;
  std::vector<int> s{g.seeded ? 1 : 0, g.ladybug, g.bflyState, g.stillSaid ? 1 : 0};
  for (const auto& n : g.lines)
    s.push_back(n.said ? 1 : 0);
  for (const auto& sd : g.seeds)
    s.push_back(sd.grown ? 1 : 0);
  for (const auto& b : g.buds)
    s.push_back(b.open);
  for (const auto& c : g.columns)
    s.push_back(c.phase);
  return s;
}

bool World::validGardenSave(const std::vector<int>& s) const
{
  const auto& g = mGarden;
  return s.size() == 4 + g.lines.size() + g.seeds.size() + g.buds.size() + g.columns.size();
}

void World::loadGarden(const std::vector<int>& s)
{
  auto& g = mGarden;
  std::size_t i = 0;
  g.seeded = s[i++] != 0;
  g.seeding = 0;
  if (g.seedhead >= 0)
    mPlatforms[std::size_t(g.seedhead)].hidden = !g.seeded;
  g.ladybug = s[i++];
  if (g.ladybug > 0)
    for (auto& pr : mProps)
      if (pr.kind == PropKind::BonusDoor)
        pr.dormant = false;
  g.bflyState = s[i++];
  g.stillSaid = s[i++] != 0;
  for (auto& n : g.lines)
    n.said = s[i++] != 0;
  for (auto& sd : g.seeds)
  {
    sd.grown = s[i++] != 0;
    sd.sprout = 0;
    if (sd.grown)
      for (int y = sd.y; y > sd.y - sd.h; --y)
        if (mMap.block(sd.x, y) == Tile::Empty)
          mMap.setBlock(sd.x, y, Tile::Ladder);
  }
  for (auto& b : g.buds)
  {
    b.open = s[i++];
    for (int x = b.x; x < b.x + b.w; ++x)
      if (b.open > 0 && mMap.block(x, b.y) == Tile::Empty)
        mMap.setBlock(x, b.y, Tile::Platform);
  }
  for (auto& c : g.columns)
  {
    c.phase = s[i++];
    c.tell = c.hostile = 0;
    c.homing = false;
  }
  g.leaps.clear();
  g.mist = 0;
  g.still = 0;
  g.subtitle = 0;
}

} // namespace gr
