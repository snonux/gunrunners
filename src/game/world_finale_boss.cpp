// Level 42's boss: Lance Marquee on his hover podium (the prize wheel, rain
// of gold, the trapdoors into the Blooper Reel, the laugh-track shockwave,
// the studio light drop, his cue cards), the Encore Engine (six attacks,
// each with its counter prototype; the core opens after every attack) and
// Off the Air (the race to the master switch). See finale.hpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

int sgn(int v) { return (v > 0) - (v < 0); }

bool alive(const Player& p) { return p.state != PlayerState::Dying && p.state != PlayerState::Teleporting; }

constexpr int kGoldW = 6;        // cells: a column of gold bars
constexpr int kGoldApart = 12;   // cells between the columns' middles
constexpr int kLampW = 6;        // cells: the studio lamp
constexpr int kStrikeW = 4;      // cells: an Arc Storm strike
constexpr int kStrikeApart = 12;
constexpr int kEngineGap = 15;   // frames between the Engine's moves
constexpr int kCoinW = 2;

int moveTell(FnEngineMove m)
{
  switch (m)
  {
    case FnEngineMove::Barrage:
      return 8;
    case FnEngineMove::Fist:
      return 15;
    case FnEngineMove::Pulse:
      return 22;
    case FnEngineMove::Arc:
      return 22;
    case FnEngineMove::Magnet:
      return 15;
    case FnEngineMove::Breath:
      return 15;
  }
  return 15;
}

} // namespace

// The prototype that counters each of the Engine's attacks.
int finaleCounter(FnEngineMove m)
{
  switch (m)
  {
    case FnEngineMove::Barrage:
      return int(ProtoId::PulsePistol);
    case FnEngineMove::Fist:
      return int(ProtoId::JadeBow);
    case FnEngineMove::Pulse:
      return int(ProtoId::DeflectorBracer);
    case FnEngineMove::Arc:
      return int(ProtoId::TeslaRod);
    case FnEngineMove::Magnet:
      return int(ProtoId::MagnetGun);
    case FnEngineMove::Breath:
      return int(ProtoId::FreezeRay);
  }
  return -1;
}

CellBox finalePodium(const FnBoss& b) { return {b.x, b.y, kFnPodiumW, kFnPodiumH}; }
CellBox finaleEngine(const FnBoss& b) { return {b.ex, b.ey - kFnEngineH + 1, kFnEngineW, kFnEngineH}; }
CellBox finaleCore(const FnBoss& b) { return {b.ex, b.ey - 5, 4, 4}; } // on its front face, toward the stage
CellBox finaleCoverBox(const World& w)
{
  const int c = w.finale().cover;
  if (c < 0)
    return {0, 0, 0, 0};
  const Layer& l = w.layers()[std::size_t(c)];
  return {l.x0 * kCellsPerTile, l.y0 * kCellsPerTile, (l.x1 - l.x0 + 1) * kCellsPerTile, (l.y1 - l.y0 + 1) * kCellsPerTile};
}

bool World::finaleFight() const
{
  const auto& b = mFinale.boss;
  return b.on && b.phase != FnPhase::Waiting && b.phase != FnPhase::Switched && mPlayer.x >= b.arenaX0;
}

void World::finaleHurts(int damage) { hurtPlayer(damage); }

void World::finaleDrop(ItemKind kind, int x, int y)
{
  Item it;
  it.kind = kind;
  it.x = it.prevX = x;
  it.y = it.prevY = y;
  it.pickupDelay = 4;
  mItems.push_back(it);
  burst({(float(x) + 1.0f) * kCellSize, (float(y) - 1.0f) * kCellSize}, rgb(255, 230, 120), rgb(255, 255, 255), 12, 1.6f);
}

void World::restartOffAir()
{
  auto& b = mFinale.boss;
  auto& p = mPlayer;
  // CUT TO COMMERCIAL: the race starts again (no damage).
  p.x = p.prevX = b.restartX;
  p.y = p.prevY = b.restartY;
  p.state = PlayerState::OnGround;
  p.vine = -1;
  setVisual(PlayerVisual::Standing);
  b.runX = b.runStartX;
  b.runSub = 0;
  b.trapT = 0;
  b.lightT = 0;
  b.lampX = -1;
  b.openTrap = -1;
  for (int k = 0; k < 3; ++k)
    setFinaleLayer(mFinale.traps[std::size_t(k)], true);
  updateLayers(true);
  showMessage("WE'RE BACK! THE RACE IS ON AGAIN");
}

void World::updateFinaleBoss(const PlayerInput& /*input*/)
{
  auto& f = mFinale;
  auto& b = f.boss;
  auto& p = mPlayer;
  b.prevX = b.x;
  b.prevY = b.y;
  if (b.hitFlash > 0)
    --b.hitFlash;
  if (b.phase == FnPhase::Switched)
  {
    ++b.signDead;
    return;
  }
  const bool inside = alive(p) && p.x >= b.arenaX0;
  // A fall through an open trapdoor.
  if (alive(p))
    for (int k = 0; k < 3; ++k)
    {
      const int li = f.traps[std::size_t(k)];
      if (li < 0)
        continue;
      const Layer& l = mLayers[std::size_t(li)];
      const int x0 = l.x0 * kCellsPerTile, x1 = (l.x1 + 1) * kCellsPerTile - 1, top = l.y0 * kCellsPerTile;
      if (p.y < top + 2 || p.x + 1 < x0 || p.x + 1 > x1)
        continue;
      if (b.phase == FnPhase::OffAir)
      {
        b.commercial = kFnCommercial; // the card, then the race again
        playSound(Sfx::Klaxon);
        showMessage("CUT TO COMMERCIAL!");
        return;
      }
      if (b.phase == FnPhase::Lance && !f.bonusDone)
      {
        // Through the trapdoor: the Blooper Reel.
        f.bonusDone = true;
        mBonusRequested = true;
        playSound(Sfx::Teleport);
        showMessage("BLOOPER REEL!");
      }
      b.trapSkip = kFnTrapSkip;
      // Back at the trapdoor's edge.
      p.x = p.prevX = x0 - Player::kWidth - 1;
      p.y = p.prevY = b.floorY;
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Standing);
      break;
    }
  switch (b.phase)
  {
    case FnPhase::Waiting:
      if (!inside)
        return;
      b.phase = FnPhase::Intro;
      b.t = 0;
      setFinaleLayer(f.door, true);
      playSound(Sfx::HypeSting);
      showMessage("LANCE MARQUEE: WELCOME TO THE LIVE FINALE!");
      return;
    case FnPhase::Switched:
      return;
    default:
      break;
  }
  if (!inside)
    return; // back at the checkpoint: the show waits
  setFinaleLayer(f.door, true);
  ++b.t;
  switch (b.phase)
  {
    case FnPhase::Intro:
      if (b.t >= kFnIntro)
      {
        b.phase = FnPhase::Lance;
        b.t = 0;
        b.gap = kFnAttackGap;
      }
      break;
    case FnPhase::Lance:
      updateLance();
      break;
    case FnPhase::Engine:
      updateEncoreEngine();
      break;
    case FnPhase::Collapse:
      if (b.t >= kFnCollapse)
      {
        b.phase = FnPhase::OffAir;
        b.t = 0;
        b.runX = b.runStartX;
        b.runSub = 0;
        b.trapT = 0;
        b.lightT = 0;
        b.openTrap = -1;
        applyFinaleLayers();
        playSound(Sfx::Klaxon);
        showMessage("OFF THE AIR! BREAK THE GLASS, HIT THE SWITCH");
      }
      break;
    case FnPhase::OffAir:
      updateOffAir();
      break;
    default:
      break;
  }
}

// --- Lance ---------------------------------------------------------------------------------

void World::updateLance()
{
  auto& f = mFinale;
  auto& b = f.boss;
  // The podium: along its path at half a cell a frame; it sinks for the cue cards.
  if (b.cue > 0)
  {
    if (b.y < b.pathY + kFnSinkRows * kCellsPerTile)
      ++b.y;
    if (--b.cue == 0)
      playSound(Sfx::Whoosh);
  }
  else if (b.y > b.pathY)
    --b.y;
  else if (b.t % 2 == 0)
  {
    b.x += b.dir;
    if (b.x <= b.pathX0 || b.x >= b.pathX1)
    {
      b.x = std::clamp(b.x, b.pathX0, b.pathX1);
      b.dir = -b.dir;
    }
  }
  if (b.trapSkip > 0)
    --b.trapSkip;
  if (b.lanceHp <= 0)
  {
    // Lance flies off; the Encore Engine powers up.
    b.phase = FnPhase::Engine;
    b.t = 0;
    b.attack = FnAttack::None;
    b.wheel = -1;
    b.cue = 0;
    b.waveL = b.waveR = -1;
    b.lampX = -1;
    if (b.openTrap >= 0)
      setFinaleLayer(f.traps[std::size_t(b.openTrap)], true);
    b.openTrap = -1;
    b.trap = -1;
    b.moveT = -1;
    b.gap = kFnAttackGap;
    b.move = FnEngineMove::Barrage;
    if (!b.drops.empty())
      finaleDrop(ItemKind::Health, b.drops[0].first, b.drops[0].second);
    playSound(Sfx::TitanStomp);
    showMessage("THE ENCORE ENGINE!");
    return;
  }
  if (b.attack != FnAttack::None)
  {
    updateLanceAttack();
    return;
  }
  if (b.cue > 0)
    return;
  if (b.wheel >= 0)
  {
    if (b.wheel % 3 == 0 && b.wheel < kFnWheelFrames - kFnWheelHold)
      playSound(Sfx::Click);
    if (++b.wheel >= kFnWheelFrames)
    {
      b.wheel = -1;
      startLanceAttack();
    }
    return;
  }
  if (b.gap > 0)
  {
    --b.gap;
    return;
  }
  // The prize wheel: an attack the round hasn't had yet (Trapdoor sits out
  // 300 frames after a fall).
  std::vector<FnAttack> left;
  for (FnAttack a : {FnAttack::Gold, FnAttack::Trapdoor, FnAttack::Laugh, FnAttack::Light})
    if ((b.bag >> int(a) & 1) == 0 && !(a == FnAttack::Trapdoor && (b.trapSkip > 0 || f.traps[0] < 0)))
      left.push_back(a);
  if (left.empty())
  {
    b.bag = 0;
    for (FnAttack a : {FnAttack::Gold, FnAttack::Laugh, FnAttack::Light})
      left.push_back(a);
    if (b.trapSkip == 0 && f.traps[0] >= 0)
      left.push_back(FnAttack::Trapdoor);
  }
  b.pick = left[finaleRand() % left.size()];
  b.bag |= 1 << int(b.pick);
  b.wheel = 0;
}

void World::startLanceAttack()
{
  auto& f = mFinale;
  auto& b = f.boss;
  const auto& p = mPlayer;
  const int mid = p.x + 1;
  b.attack = b.pick;
  b.attackT = 0;
  switch (b.attack)
  {
    case FnAttack::Gold:
      for (int k = 0; k < 3; ++k)
        b.gold[std::size_t(k)] =
          std::clamp(mid - kGoldW / 2 + (k - 1) * kGoldApart, b.arenaX0, b.arenaX1 - kGoldW + 1);
      showMessage("RAIN OF GOLD!");
      break;
    case FnAttack::Trapdoor:
    {
      int best = 0, bestD = 1 << 20;
      for (int k = 0; k < 3; ++k)
      {
        const int li = f.traps[std::size_t(k)];
        if (li < 0)
          continue;
        const Layer& l = mLayers[std::size_t(li)];
        const int d = std::abs((l.x0 + l.x1 + 1) * kCellsPerTile / 2 - mid);
        if (d < bestD)
        {
          bestD = d;
          best = k;
        }
      }
      b.trap = best;
      showMessage("TRAPDOOR!");
      break;
    }
    case FnAttack::Laugh:
      b.waveX = b.x + kFnPodiumW / 2;
      showMessage("LAUGH TRACK!");
      break;
    case FnAttack::Light:
      b.lampX = mid;
      break;
    case FnAttack::None:
      break;
  }
  playSound(Sfx::Drum);
}

void World::updateLanceAttack()
{
  auto& f = mFinale;
  auto& b = f.boss;
  const auto& p = mPlayer;
  const CellBox hb = p.hitBox();
  const int tall = b.floorY - b.arenaY0 + 1;
  bool done = false;
  ++b.attackT;
  switch (b.attack)
  {
    case FnAttack::Gold:
      if (b.attackT == kFnGoldTell)
      {
        for (int g : b.gold)
          if (hb.intersects({g, b.arenaY0, kGoldW, tall}))
            finaleHurts(1);
        playSound(Sfx::Thud);
      }
      done = b.attackT >= kFnGoldTell + 10;
      break;
    case FnAttack::Trapdoor:
      if (b.attackT == kFnTrapTell)
      {
        b.openTrap = b.trap;
        setFinaleLayer(f.traps[std::size_t(b.trap)], false);
        playSound(Sfx::Clunk);
      }
      if (b.attackT >= kFnTrapTell + kFnTrapOpen)
      {
        setFinaleLayer(f.traps[std::size_t(b.trap)], true);
        b.openTrap = -1;
        b.trap = -1;
        done = true;
      }
      break;
    case FnAttack::Laugh:
      if (b.attackT == kFnLaughTell)
      {
        b.waveL = b.waveR = b.waveX;
        playSound(Sfx::Applause);
      }
      if (b.waveR >= 0) // waveL may run below 0 in an arena at the map's left edge
      {
        b.waveL -= kFnWaveSpeed;
        b.waveR += kFnWaveSpeed;
        for (int wx : {b.waveL, b.waveR})
          if (hb.intersects({wx, b.floorY - 1, 2, 2}))
            finaleHurts(1);
        if (b.waveL < b.arenaX0 && b.waveR > b.arenaX1)
        {
          b.waveL = b.waveR = -1;
          done = true;
        }
      }
      break;
    case FnAttack::Light:
      if (b.attackT == kFnLampTell)
      {
        if (hb.intersects({b.lampX - kLampW / 2, b.arenaY0, kLampW, tall}))
          finaleHurts(1);
        playSound(Sfx::Thud);
      }
      if (b.attackT >= kFnLampTell + 6)
      {
        b.lampX = -1;
        done = true;
      }
      break;
    case FnAttack::None:
      done = true;
      break;
  }
  if (!done)
    return;
  b.attack = FnAttack::None;
  b.gap = kFnAttackGap;
  if (++b.attacks % 2 == 0)
  {
    // His cue cards: the podium sinks and its front panel can be hit.
    b.cue = kFnCueCards;
    showMessage("LANCE READS HIS CUE CARDS");
  }
}

// --- The Encore Engine ---------------------------------------------------------------------

void World::updateEncoreEngine()
{
  auto& b = mFinale.boss;
  auto& p = mPlayer;
  const CellBox hb = p.hitBox();
  if (b.engineHp <= 0)
  {
    b.phase = FnPhase::Collapse;
    b.t = 0;
    b.moveT = -1;
    b.coinL = b.coinR = -1;
    b.ring = -1;
    b.breath = 0;
    b.stagger = b.core = 0;
    if (b.drops.size() > 1)
      finaleDrop(ItemKind::Health, b.drops[1].first, b.drops[1].second);
    if (b.turboX >= 0)
      finaleDrop(ItemKind::Turbo, b.turboX, b.turboY);
    playSound(Sfx::Explosion);
    mCamera.shake(14, 1.5f);
    showMessage("THE ENCORE ENGINE COLLAPSES!");
    return;
  }
  // Its bulk hurts to touch.
  if (alive(p) && hb.intersects(finaleEngine(b)))
    finaleHurts(1);
  if (b.stagger > 0)
  {
    if (--b.stagger == 0)
      b.gap = kEngineGap;
    return;
  }
  if (b.core > 0)
  {
    if (--b.core == 0)
      b.gap = kEngineGap;
    return;
  }
  if (b.moveT < 0)
  {
    if (b.gap > 0)
    {
      --b.gap;
      return;
    }
    b.moveT = 0;
    b.countered = false;
    b.shots = 0;
    if (b.move == FnEngineMove::Arc)
      for (int k = 0; k < 3; ++k)
        b.arcs[std::size_t(k)] =
          std::clamp(p.x + 1 - kStrikeW / 2 + (k - 1) * kStrikeApart, b.arenaX0, b.arenaX1 - kStrikeW + 1);
    playSound(Sfx::Warn);
  }
  const int tell = moveTell(b.move);
  if (++b.moveT <= tell)
    return;
  const int at = b.moveT - tell - 1;
  bool done = false;
  switch (b.move)
  {
    case FnEngineMove::Barrage:
      // Four shots on the beat, a cell a frame, at chest height (crouch or jump).
      if (at % 8 == 0 && b.shots < 4)
      {
        spawnProjectile(ShotKind::Enemy, b.ex - 2, b.ey - 4, -1, 0); // a crouch ducks under it
        ++b.shots;
        playSound(Sfx::EnemyShot);
      }
      done = at >= 32;
      break;
    case FnEngineMove::Fist:
      if (at == 0)
      {
        b.coinL = b.ex - kCoinW;
        b.coinR = b.ex + kFnEngineW;
        playSound(Sfx::TitanStomp);
        mCamera.shake(8, 1.0f);
      }
      b.coinL -= 2;
      b.coinR += 2;
      for (int cx : {b.coinL, b.coinR})
        if (hb.intersects({cx, b.floorY - 1, kCoinW, 2}))
          finaleHurts(1);
      if (b.coinL < b.arenaX0 && b.coinR > b.arenaX1)
      {
        b.coinL = b.coinR = -1;
        done = true;
      }
      break;
    case FnEngineMove::Pulse:
      // A ring at chest height out to the walls: crouch under it.
      b.ring = (at + 1) * kFnRingSpeed;
      if (hb.intersects({b.ex - b.ring, b.ey - 8, b.ring, 5}))
        finaleHurts(2);
      if (b.ex - b.ring < b.arenaX0)
      {
        b.ring = -1;
        done = true;
      }
      break;
    case FnEngineMove::Arc:
      if (at == 0)
      {
        for (int a : b.arcs)
          if (hb.intersects({a, b.arenaY0, kStrikeW, b.floorY - b.arenaY0 + 1}))
            finaleHurts(1);
        playSound(Sfx::Zap);
      }
      done = at >= 8;
      break;
    case FnEngineMove::Magnet:
      // Pulls the runner a cell a frame toward its fist.
      if (alive(p) && p.vehicle < 0)
      {
        const int dir = sgn(b.ex - (p.x + Player::kWidth));
        if (dir != 0)
        {
          int x = p.x;
          mMap.moveHorizontally(x, p.y, Player::kWidth, p.height(), dir);
          p.x = x;
        }
      }
      done = at >= kFnPullFrames;
      break;
    case FnEngineMove::Breath:
      b.breath = at + 1;
      if (hb.intersects({b.ex - kFnBreathReach, b.ey - kFnEngineH + 1, kFnBreathReach, kFnEngineH}))
        finaleHurts(1);
      if (at >= 30)
      {
        b.breath = 0;
        done = true;
      }
      break;
  }
  if (!done)
    return;
  // Recovery: the core opens anyway.
  b.moveT = -1;
  b.core = kFnCoreOpen;
  b.move = FnEngineMove((int(b.move) + 1) % 6);
  playSound(Sfx::CoreOpen);
}

// --- Off the Air ---------------------------------------------------------------------------

void World::updateOffAir()
{
  auto& f = mFinale;
  auto& b = f.boss;
  const auto& p = mPlayer;
  // Lance runs along the back wall for the broadcast desk.
  if (++b.runSub >= kFnRunEvery)
  {
    b.runSub = 0;
    ++b.runX;
  }
  if (b.runX >= b.deskX - kFnPodiumW / 2)
  {
    b.commercial = kFnCommercial;
    playSound(Sfx::Klaxon);
    showMessage("LANCE CUTS TO COMMERCIAL!");
    return;
  }
  // The trapdoors open in turn: an outline, then open.
  ++b.trapT;
  const int k = (b.trapT / kFnTrapTurn) % 3, at = b.trapT % kFnTrapTurn;
  b.trap = k;
  const int want = at >= kFnTrapTell ? k : -1;
  if (want != b.openTrap)
  {
    if (b.openTrap >= 0)
      setFinaleLayer(f.traps[std::size_t(b.openTrap)], true);
    b.openTrap = want;
    if (want >= 0)
    {
      setFinaleLayer(f.traps[std::size_t(want)], false);
      playSound(Sfx::Clunk);
    }
  }
  // A light drop every 60 frames.
  if (++b.lightT >= kFnLightEvery && b.lampX < 0)
  {
    b.lightT = 0;
    b.lampX = p.x + 1;
    b.attackT = 0;
  }
  if (b.lampX >= 0)
  {
    if (++b.attackT == kFnLampTell)
    {
      if (p.hitBox().intersects({b.lampX - kLampW / 2, b.arenaY0, kLampW, b.floorY - b.arenaY0 + 1}))
        finaleHurts(1);
      playSound(Sfx::Thud);
    }
    if (b.attackT >= kFnLampTell + 6)
      b.lampX = -1;
  }
}

// --- Shots ---------------------------------------------------------------------------------

bool World::shotAtBoss(Projectile& pr, const CellBox& sb)
{
  auto& f = mFinale;
  auto& b = f.boss;
  switch (b.phase)
  {
    case FnPhase::Lance:
    {
      if (!sb.intersects(finalePodium(b)))
        return false;
      if (b.cue > 0 && b.y >= b.pathY + kFnSinkRows * kCellsPerTile - 1)
      {
        b.lanceHp -= pr.damage;
        b.hitFlash = 6;
        playSound(Sfx::Hit);
      }
      else
        playSound(Sfx::Tink); // the podium's armour
      return true;
    }
    case FnPhase::Engine:
    {
      if (!sb.intersects(finaleEngine(b)))
        return false;
      const int counter = finaleCounter(b.move);
      const bool logged = std::find(f.wheel.begin(), f.wheel.end(), counter) != f.wheel.end();
      const bool core = sb.intersects(finaleCore(b));
      const bool attacking = b.moveT >= 0 && b.stagger == 0 && b.core == 0;
      const bool counterShot = pr.kind == ShotKind::Proto && pr.encore == counter &&
        (counter != int(ProtoId::PulsePistol) || pr.strong);
      const bool blaster = pr.kind != ShotKind::Proto || pr.encore < 0;
      if ((attacking && logged && counterShot) || (!logged && blaster && core && b.core > 0))
      {
        // Countered: staggered, the core open and every hit counting double.
        b.stagger = kFnStagger;
        b.core = 0;
        b.moveT = -1;
        b.coinL = b.coinR = -1;
        b.ring = -1;
        b.breath = 0;
        b.move = FnEngineMove((int(b.move) + 1) % 6);
        playSound(Sfx::CoreOpen);
        showMessage("COUNTERED! THE ENGINE STAGGERS");
      }
      if (core && (b.core > 0 || b.stagger > 0))
      {
        b.engineHp -= pr.damage * (b.stagger > 0 ? 2 : 1);
        b.hitFlash = 6;
        playSound(Sfx::Hit);
      }
      else
        playSound(Sfx::Tink);
      return true;
    }
    case FnPhase::OffAir:
    {
      if (b.coverHp <= 0 || !sb.intersects(finaleCoverBox(*this)))
        return false;
      b.coverHp -= pr.damage;
      b.hitFlash = 4;
      if (b.coverHp <= 0)
      {
        b.coverHp = 0;
        setFinaleLayer(f.cover, false);
        playSound(Sfx::BoxBreak);
        showMessage("THE GLASS BREAKS - HIT THE SWITCH!");
      }
      else
        playSound(Sfx::Tink);
      return true;
    }
    default:
      return false;
  }
}

bool World::shotAtFinale(Projectile& pr, const CellBox& b)
{
  if (pr.kind == ShotKind::Enemy || !mFinale.boss.on)
    return false;
  return shotAtBoss(pr, b);
}

} // namespace gr
