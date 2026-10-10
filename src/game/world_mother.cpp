// Episode 7, DEEP SPACE (docs/DEEP_SPACE.md). Level 49, The Hive Mother:
// the episode's boss in the egg chamber at the hive's heart, and her brood.
// The fight shuts the arena door behind the runner and runs in three
// phases. The crown: it glows when it can be hurt, high on her head (the
// goo walls on the resin column reach it) except while she bows to lay an
// egg, when a jump puts a shot into it; her eggs hatch Egg Guards. The
// sacs: she breathes in and pulls the runner toward her mouth, and the
// sacs on her flanks swell open (the gullet tube takes you round behind
// her to the one on her back). The charge: she drops to all fours and
// runs at the far wall; jump onto a ledge (or swap past her with a
// crystal) and shoot her while she is dazed against it. Spore Nurses heal
// her while they live. A respawn opens the door; she waits where she was.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr int kWake = 60;     // frames she takes to rise before the fight
constexpr int kCrownCycle = 160;
constexpr int kLayAt = 35;    // frame of the crown cycle an egg comes out
constexpr int kHatch = 60;    // frames before a laid egg hatches
constexpr int kBrood = 3;     // eggs and guards of hers at once
constexpr int kInhaleCycle = 200;
constexpr int kGustAt = 110;  // the breath out after the breath in
constexpr int kRear = 30;     // frames she rears up before a charge
constexpr int kDaze = 60;     // frames dazed against a wall
constexpr int kDying = 90;
const Color kResin = rgb(255, 180, 90);
const Color kGreen = rgb(190, 255, 90);

} // namespace

// --- Setup and state -------------------------------------------------------------------

bool World::setupMotherEntity(const EntityDef& e)
{
  if (e.kind != "hive_mother" || !e.hasPos)
    return false;
  // `@ hive_mother x y arena=x0,y0,x1,y1 door=x0,y0,x1,y1`: her left on
  // the throne, the arena inside its walls, the door that shuts behind you.
  HiveMother m;
  m.on = true;
  m.x = m.prevX = float(e.x * kCellsPerTile);
  m.throneX = e.x * kCellsPerTile;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (e.rect("arena", x0, y0, x1, y1))
  {
    m.arena = {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
    m.floor = (y1 + 1) * kCellsPerTile - 1;
  }
  else
    m.floor = e.y * kCellsPerTile + 1;
  if (e.rect("door", x0, y0, x1, y1))
  {
    m.doorX0 = x0;
    m.doorY0 = y0;
    m.doorX1 = x1;
    m.doorY1 = y1;
  }
  m.hp = HiveMother::kHpCrown;
  mSpace.mother = m;
  return true;
}

bool World::motherFight() const
{
  const auto& m = mSpace.mother;
  return m.on && !m.away && (m.phase == MotherPhase::Wake || m.phase == MotherPhase::Crown ||
                               m.phase == MotherPhase::Inhale || m.phase == MotherPhase::Charge);
}

int World::motherHpMax() const { return HiveMother::kHpCrown + HiveMother::kHpInhale + HiveMother::kHpCharge; }

int World::motherHp() const
{
  const auto& m = mSpace.mother;
  if (!m.on)
    return 0;
  switch (m.phase)
  {
    case MotherPhase::Asleep:
    case MotherPhase::Wake: return motherHpMax();
    case MotherPhase::Crown: return m.hp + HiveMother::kHpInhale + HiveMother::kHpCharge;
    case MotherPhase::Inhale: return m.hp + HiveMother::kHpCharge;
    case MotherPhase::Charge: return m.hp;
    default: return 0;
  }
}

void World::shutMotherDoor(bool shut)
{
  const auto& m = mSpace.mother;
  for (int ty = m.doorY0; ty <= m.doorY1; ++ty)
    for (int tx = m.doorX0; tx <= m.doorX1; ++tx)
      mMap.setBlock(tx, ty, shut ? Tile::Solid : Tile::Empty);
  if (shut)
  {
    playSound(Sfx::Crash);
    mCamera.shake(8, 2.0f);
  }
}

void World::motherPhase(MotherPhase phase)
{
  auto& m = mSpace.mother;
  m.phase = phase;
  m.t = m.cycle = m.run = 0;
  auto nurses = [&]() {
    // Two Spore Nurses drift down from the chamber's roof, low enough to
    // shoot from the ledges.
    int alive = 0;
    for (const auto& e : mEnemies)
      alive += e.alive && e.kind == EnemyKind::SporeNurse;
    for (int k = alive; k < 2; ++k)
    {
      const int nx = m.arena.x + m.arena.w / 4 + k * m.arena.w / 3;
      spawnEnemy(enemyIndex("brood_nurse"), nx, m.floor - 7 - k * 2);
      mEnemies.back().active = true;
    }
  };
  switch (phase)
  {
    case MotherPhase::Wake:
      playSound(Sfx::Rumble);
      showMessage("THE HIVE MOTHER WAKES");
      break;
    case MotherPhase::Crown:
      m.hp = HiveMother::kHpCrown;
      nurses();
      showMessage("HER CROWN - SHOOT IT WHILE IT GLOWS");
      break;
    case MotherPhase::Inhale:
      m.hp = HiveMother::kHpInhale;
      nurses();
      mCamera.shake(14, 2.5f);
      playSound(Sfx::Screech);
      showMessage("SHE BREATHES IN - SHOOT THE SACS ON HER FLANKS");
      break;
    case MotherPhase::Charge:
    {
      m.hp = HiveMother::kHpCharge;
      mCamera.shake(14, 2.5f);
      playSound(Sfx::Screech);
      showMessage("SHE CHARGES - GET UP OUT OF HER WAY");
      // A Turbo box drops into the middle of the arena.
      Item it;
      it.kind = ItemKind::Turbo;
      it.x = it.prevX = m.arena.x + m.arena.w / 2 - 1;
      it.y = it.prevY = m.floor - 10;
      it.pickupDelay = 4;
      mItems.push_back(it);
      break;
    }
    case MotherPhase::Dying:
      mCamera.shake(kDying, 2.5f);
      playSound(Sfx::Explosion);
      for (auto& e : mEnemies)
        if (e.alive && (e.kind == EnemyKind::EggGuard || e.kind == EnemyKind::SporeNurse))
          damageEnemy(e, e.hp);
      break;
    case MotherPhase::Done:
    {
      const Vec2 c{(m.x + float(HiveMother::kW) * 0.5f) * kCellSize, float(m.floor - 6) * kCellSize};
      addScore(50000, c);
      flashAt(c, 300.0f, kResin, 40);
      playSound(Sfx::LettersComplete);
      showMessage("THE HIVE MOTHER FALLS - VURR IS FREE");
      break;
    }
    default:
      break;
  }
}

void World::hurtMother(int damage, const CellBox& at)
{
  auto& m = mSpace.mother;
  m.hp -= damage;
  m.flash = 6;
  burst(cellCenter(at), kGreen, rgb(255, 255, 255), 8, 1.4f);
  playSound(Sfx::Hit);
  if (m.hp > 0)
    return;
  m.hp = 0;
  switch (m.phase)
  {
    case MotherPhase::Crown: motherPhase(MotherPhase::Inhale); break;
    case MotherPhase::Inhale: motherPhase(MotherPhase::Charge); break;
    default: motherPhase(MotherPhase::Dying); break;
  }
}

bool World::shotAtMother(Projectile& pr, const CellBox& b)
{
  auto& m = mSpace.mother;
  if (!m.on || pr.kind == ShotKind::Enemy || m.phase == MotherPhase::Asleep || m.phase == MotherPhase::Dying ||
      m.phase == MotherPhase::Done)
    return false;
  constexpr int kId = -300;
  if (std::find(pr.hit.begin(), pr.hit.end(), kId) != pr.hit.end())
    return false; // a star already through her
  auto hurt = [&](const CellBox& at) {
    hurtMother(pr.damage, at);
    if (!pr.pierce)
      return true;
    pr.hit.push_back(kId);
    return false;
  };
  if (m.crownLit() && m.crown().intersects(b))
    return hurt(m.crown());
  if (m.inhaling())
    for (int k = 0; k < 2; ++k)
      if (m.sac(k).intersects(b))
        return hurt(m.sac(k));
  if (!m.body().intersects(b) && !m.crown().intersects(b))
    return false;
  if (m.phase == MotherPhase::Charge && m.run == 2)
    return hurt(m.body()); // dazed: her back is open
  // Chitin: the shot glances off.
  burst(cellCenter(b), rgb(255, 230, 180), kResin, 4, 1.0f);
  return true;
}

void World::resetMother()
{
  auto& m = mSpace.mother;
  if (!m.on || m.phase == MotherPhase::Asleep || m.phase == MotherPhase::Done)
    return;
  if (m.phase == MotherPhase::Dying)
  {
    motherPhase(MotherPhase::Done);
    return;
  }
  // Back at the checkpoint: the door opens, and she waits on her throne for
  // the runner to come in again.
  m.away = true;
  shutMotherDoor(false);
  m.cycle = m.run = 0;
  m.x = m.prevX = float(m.throneX);
  m.dir = -1;
}

// --- Per frame ---------------------------------------------------------------------------

void World::updateMother()
{
  auto& m = mSpace.mother;
  if (!m.on)
    return;
  auto& p = mPlayer;
  m.prevX = m.x;
  if (m.flash > 0)
    --m.flash;
  if (m.heal > 0)
    --m.heal;
  ++m.t;
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const CellBox hit = p.hitBox();
  const int left = m.arena.x, right = m.arena.x + m.arena.w - HiveMother::kW;
  const bool inside = vulnerable && p.x >= m.arena.x + 2 && p.x + Player::kWidth <= m.arena.x + m.arena.w &&
    p.y <= m.floor && p.y > m.arena.y;
  if (m.phase == MotherPhase::Asleep)
  {
    if (!inside)
      return;
    shutMotherDoor(true);
    motherPhase(MotherPhase::Wake);
    return;
  }
  if (m.phase == MotherPhase::Done)
    return;
  if (m.away)
  {
    if (!inside)
      return;
    m.away = false;
    shutMotherDoor(true);
  }
  // Touching her hurts, and throws you back from her.
  auto contact = [&](const CellBox& b, int damage) {
    if (!vulnerable || !b.intersects(hit) || p.mercy > 0)
      return;
    const bool fresh = p.turbo == 0;
    hurtPlayer(damage);
    if (!fresh)
      return;
    // Thrown out past her side: the side you are on, unless she is up
    // against the wall there (or tramples you in a charge).
    const int mx = int(m.x);
    const int roomL = mx - m.arena.x, roomR = m.arena.x + m.arena.w - (mx + HiveMother::kW);
    int away = p.x + 1 < mx + HiveMother::kW / 2 ? -1 : 1;
    if (m.phase == MotherPhase::Charge && m.run == 1)
      away = -m.dir;
    if ((away < 0 ? roomL : roomR) < Player::kWidth + 2)
      away = -away;
    const int to = away < 0 ? mx - Player::kWidth - 3 : mx + HiveMother::kW + 3;
    mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), to - p.x);
    if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
      startFalling();
  };
  const float pcx = float(p.x) + 1.5f, mcx = m.x + float(HiveMother::kW) * 0.5f;

  switch (m.phase)
  {
    case MotherPhase::Wake:
      if (m.t % 8 == 0)
        mCamera.shake(6, 2.0f);
      if (m.t >= kWake)
        motherPhase(MotherPhase::Crown);
      return;

    case MotherPhase::Crown:
    {
      m.dir = pcx < mcx ? -1 : 1;
      if (++m.cycle >= kCrownCycle)
        m.cycle = 0;
      if (m.cycle == kLayAt)
      {
        // An egg, rolled out in front of her.
        int brood = 0;
        for (const auto& e : mEnemies)
          brood += e.alive && e.kind == EnemyKind::EggGuard && e.def == enemyIndex("brood_egg");
        if (brood < kBrood)
        {
          const int ex = m.dir < 0 ? int(m.x) - 4 : int(m.x) + HiveMother::kW + 1;
          spawnEnemy(enemyIndex("brood_egg"), std::clamp(ex, m.arena.x + 1, m.arena.x + m.arena.w - 4), m.floor);
          Enemy& egg = mEnemies.back();
          egg.active = true;
          egg.cool = kHatch;
          egg.dir = m.dir;
          playSound(Sfx::Gulp);
        }
      }
      contact(m.body(), 1);
      break;
    }

    case MotherPhase::Inhale:
    {
      m.dir = pcx < mcx ? -1 : 1;
      if (++m.cycle >= kInhaleCycle)
        m.cycle = 0;
      if (m.inhaling() && inside && p.vehicle < 0 && p.tube < 0 && m.t % 3 == 0)
      {
        // Pulled toward her mouth.
        const int toward = pcx < mcx ? 1 : -1;
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), toward);
      }
      if (m.cycle == 20)
        playSound(Sfx::Inhale);
      if (m.cycle == kGustAt && inside && std::abs(pcx - mcx) < 40.0f)
      {
        // The breath out: a gust that throws you back.
        const int away = pcx < mcx ? -1 : 1;
        mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), away * 6);
        playSound(Sfx::Whoosh);
      }
      if (m.inhaling())
        contact(m.mouth(), 1);
      contact(m.body(), 1);
      break;
    }

    case MotherPhase::Charge:
      switch (m.run)
      {
        case 0: // rearing up, facing the runner
          m.dir = pcx < mcx ? -1 : 1;
          if (++m.cycle >= kRear)
          {
            m.run = 1;
            m.cycle = 0;
            playSound(Sfx::Screech);
          }
          contact(m.body(), 1);
          break;
        case 1: // running at the far wall
          m.x += 2.0f * float(m.dir);
          if (m.x <= float(left) || m.x >= float(right))
          {
            m.x = std::clamp(m.x, float(left), float(right));
            m.run = 2;
            m.cycle = 0;
            mCamera.shake(16, 3.0f);
            playSound(Sfx::Crash);
            burst({(m.dir < 0 ? m.x : m.x + float(HiveMother::kW)) * kCellSize, float(m.floor - 4) * kCellSize},
              kResin, rgb(120, 60, 30), 20, 2.4f, false);
          }
          contact(m.body(), 2);
          break;
        default: // dazed against the wall
          if (++m.cycle >= kDaze)
          {
            m.run = 0;
            m.cycle = 0;
          }
          contact(m.body(), 1);
          break;
      }
      break;

    case MotherPhase::Dying:
      if (m.t % 4 == 0)
        burst({(m.x + float((m.t * 7) % HiveMother::kW)) * kCellSize, float(m.floor - (m.t * 3) % 14) * kCellSize},
          kResin, kGreen, 6, 1.8f);
      if (m.t >= kDying)
        motherPhase(MotherPhase::Done);
      return;

    default:
      return;
  }
}

// --- Her brood -----------------------------------------------------------------------------

void World::updateEggGuard(Enemy& e, const EnemyDef& def)
{
  // attach 0 an egg, 1 a guard on the prowl, 2 lowering its horns, 3
  // charging.
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const auto& p = mPlayer;
  const int dxp = (p.x + 1) - (e.x + e.w / 2);
  const bool level = std::abs(p.y - e.y) <= 2 && p.state != PlayerState::Dying && !p.hidden;
  const CellBox b = e.box();
  auto blocked = [&](int dir) {
    const bool wall = dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
    const int aheadX = dir > 0 ? b.right() + 1 : b.left() - 1;
    return wall || !mMap.solidTop(aheadX, b.bottom() + 1);
  };
  switch (e.attach)
  {
    case 0:
      if ((e.cool > 0 && --e.cool == 0) || (e.cool == 0 && level && std::abs(dxp) <= def.range))
      {
        e.attach = 1;
        e.cool = def.cooldown;
        playSound(Sfx::Hiss);
        burst(cellCenter(b), rgb(250, 240, 200), kGreen, 10, 1.4f, false);
      }
      break;
    case 1:
      if (e.cool > 0)
        --e.cool;
      if (dxp != 0)
        e.dir = dxp < 0 ? -1 : 1;
      if (e.cool == 0 && level && std::abs(dxp) <= 12)
      {
        e.attach = 2;
        e.tell = 8;
        break;
      }
      if (e.timer % std::max(1, def.stepEvery) == 0 && !blocked(e.dir))
        e.x += e.dir;
      break;
    case 2:
      if (--e.tell <= 0)
      {
        e.attach = 3;
        e.ox = e.x;
      }
      break;
    default:
      for (int k = 0; k < 2; ++k)
      {
        if (blocked(e.dir) || std::abs(e.x - e.ox) >= 14)
        {
          e.attach = 1;
          e.cool = def.cooldown;
          break;
        }
        e.x += e.dir;
      }
      break;
  }
}

void World::updateSporeNurse(Enemy& e, const EnemyDef& def)
{
  // Drifts over where it started; every so often a beam of spores heals
  // the Hive Mother.
  if (e.attach == 0)
  {
    e.attach = 1;
    e.ox = e.x;
    e.oy = e.y;
  }
  const float t = float(e.timer);
  e.x = e.ox + int(std::lround(float(def.range) * std::sin(t * 0.03f)));
  e.y = e.oy + int(std::lround(3.0f * std::sin(t * 0.07f)));
  e.dir = std::cos(t * 0.03f) >= 0.0f ? 1 : -1;
  auto& m = mSpace.mother;
  if (!m.on || m.away || e.timer % std::max(1, def.cooldown) != 0)
    return;
  const int full = m.phase == MotherPhase::Crown ? HiveMother::kHpCrown
    : m.phase == MotherPhase::Inhale             ? HiveMother::kHpInhale
    : m.phase == MotherPhase::Charge             ? HiveMother::kHpCharge
                                                 : 0;
  if (full == 0 || m.hp >= full)
    return;
  ++m.hp;
  m.heal = 14;
  m.healX = e.x + e.w / 2;
  m.healY = e.y - e.h / 2;
  playSound(Sfx::Chime);
}

// --- Drawing -------------------------------------------------------------------------------

void World::drawMotherBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& m = mSpace.mother;
  if (!m.on)
    return;
  // The throne: a mound of resin and eggs at the arena's end.
  const float tx = float(m.throneX) * kCellPx - camX, ty = float(m.floor + 1) * kCellPx - camY;
  const float tw = float(HiveMother::kW) * kCellPx;
  if (tx + tw > -200.0f && tx < float(kScreenW) + 200.0f)
  {
    const Texture* throne = &styledEnemySprite(mArt, r, mTheme, "mother_throne", 0, 0, HiveMother::kW + 4, 8).get(1);
    r.draw(*throne, tx + tw * 0.5f, ty);
  }
  // Her, in the pose of the moment.
  int pose = 0;
  switch (m.phase)
  {
    case MotherPhase::Asleep: pose = 5; break;
    case MotherPhase::Crown: pose = m.bowed() ? 1 : 0; break;
    case MotherPhase::Inhale: pose = m.inhaling() ? 2 : 0; break;
    case MotherPhase::Charge: pose = m.run == 1 ? 3 : (m.run == 2 ? 4 : 0); break;
    case MotherPhase::Dying:
    case MotherPhase::Done: pose = 6; break;
    default: break;
  }
  const float x = (m.prevX + (m.x - m.prevX) * alpha) * kCellPx - camX;
  const float bottom = float(m.floor + 1) * kCellPx - camY;
  if (x + tw < -200.0f || x > float(kScreenW) + 200.0f)
    return;
  const Texture* spr =
    &styledEnemySprite(mArt, r, mTheme, "hive_mother", pose, (frame / 8) % 2, HiveMother::kW + 8, HiveMother::kH + 6)
       .get(m.dir);
  DrawOpts o;
  if (m.phase == MotherPhase::Done)
    o.alpha = 0.7f;
  const float sink = m.phase == MotherPhase::Dying ? float(m.t) * 0.8f : (m.phase == MotherPhase::Done ? 72.0f : 0.0f);
  r.draw(*spr, x + tw * 0.5f, bottom + sink, o);
  if (m.flash > 0)
  {
    DrawOpts fo;
    fo.blend = Blend::Add;
    fo.alpha = 0.6f;
    r.draw(*spr, x + tw * 0.5f, bottom + sink, fo);
  }
}

void World::drawMotherFront(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& m = mSpace.mother;
  if (!m.on || m.phase == MotherPhase::Asleep || m.phase == MotherPhase::Done)
    return;
  auto glowAt = [&](const CellBox& b, Color c, float a, float radius) {
    drawGlow(r, mArt, (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, (float(b.y) + float(b.h) * 0.5f) * kCellPx - camY,
      radius, c, a);
  };
  // The crown pulses while it can be hurt.
  if (m.crownLit())
    glowAt(m.crown(), kGreen, 0.55f + 0.3f * std::sin(float(frame) * 0.3f), m.bowed() ? 110.0f : 90.0f);
  // The sacs, swollen while she breathes in, and the air rushing to her.
  if (m.inhaling())
  {
    for (int k = 0; k < 2; ++k)
      glowAt(m.sac(k), rgb(255, 120, 180), 0.6f + 0.25f * std::sin(float(frame) * 0.4f + float(k)), 70.0f);
    const CellBox mo = m.mouth();
    const float mx = (float(mo.x) + float(mo.w) * 0.5f) * kCellPx - camX, my = (float(mo.y) + 3.0f) * kCellPx - camY;
    for (int k = 0; k < 10; ++k)
    {
      const float u = std::fmod(float(frame) * 0.04f + float(k) * 0.1f, 1.0f);
      const float d = (1.0f - u) * 700.0f * float(m.dir);
      const float y = my + std::sin(float(k) * 2.3f) * 120.0f * (1.0f - u);
      r.drawLine(mx + d, y, mx + d - 40.0f * float(m.dir), y, 3.0f, rgba(255, 240, 220, int(120 * u)), Blend::Add);
    }
  }
  // Dazed: her back glows, stars round her head.
  if (m.phase == MotherPhase::Charge && m.run == 2)
  {
    glowAt(m.body(), rgb(255, 120, 180), 0.45f, 220.0f);
    const CellBox b = m.body();
    for (int k = 0; k < 4; ++k)
    {
      const float a = float(frame) * 0.2f + float(k) * 1.57f;
      const float sx = (float(b.x) + float(b.w) * 0.5f + std::cos(a) * 4.0f) * kCellPx - camX;
      const float sy = (float(b.y) - 1.0f) * kCellPx - camY + std::sin(a) * 12.0f;
      drawGlow(r, mArt, sx, sy, 22, rgb(255, 240, 120), 0.9f);
    }
  }
  // A Spore Nurse's healing beam.
  if (m.heal > 0)
  {
    const CellBox b = m.body();
    const float x0 = float(m.healX) * kCellPx - camX, y0 = float(m.healY) * kCellPx - camY;
    const float x1 = (float(b.x) + float(b.w) * 0.5f) * kCellPx - camX, y1 = (float(b.y) + 3.0f) * kCellPx - camY;
    r.drawLine(x0, y0, x1, y1, 10.0f, rgba(160, 255, 120, 90), Blend::Add);
    r.drawLine(x0, y0, x1, y1, 3.0f, rgb(230, 255, 200));
    drawGlow(r, mArt, x1, y1, 80, kGreen, 0.7f);
  }
}

void World::drawMotherHud(Renderer& r, int frame) const
{
  if (!motherFight())
    return;
  const auto& m = mSpace.mother;
  // Low on the screen, under the arena floor the camera keeps in view.
  const float bw = 600.0f, bx = (float(kScreenW) - bw) * 0.5f, by = float(kScreenH) - 20.0f;
  const float total = float(motherHpMax());
  r.fillRect(bx - 10, by - 26, bw + 20, 42, rgba(8, 6, 22, 200));
  r.drawText("THE HIVE MOTHER", bx, by - 23, {15.0f, kResin, rgb(30, 10, 4)});
  r.fillRect(bx, by, bw, 10, rgba(255, 255, 255, 40));
  r.fillRect(bx, by, bw * std::min(1.0f, float(motherHp()) / total), 10,
    (m.flash > 0 && (frame / 2) % 2) ? rgb(255, 255, 255) : (m.heal > 0 ? kGreen : kResin));
  for (const float cut : {float(HiveMother::kHpCharge) / total,
         float(HiveMother::kHpCharge + HiveMother::kHpInhale) / total})
    r.fillRect(bx + bw * cut - 1.0f, by - 2, 3, 14, rgb(20, 16, 30));
}

} // namespace gr
