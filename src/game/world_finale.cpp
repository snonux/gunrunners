// Level 42, The Live Finale: the Season Recap's set wagons (the turntable's
// roll-in, each wagon's trick, the RERUN), the gravity plates, the studio
// monitors, cartoon air-running, Censor Bars, Ad Breaks and their sample
// bots, Stunt Doubles, the Encore and its shared pool, the backstage
// refill; the Blooper Reel bonus (rules=blooper). The boss is in
// world_finale_boss.cpp. See finale.hpp.

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

bool isFinaleEnemy(EnemyKind k)
{
  return k == EnemyKind::CensorBar || k == EnemyKind::AdBreak || k == EnemyKind::SampleBot || k == EnemyKind::StuntDouble;
}

const char* wagonName(FnWagonKind k)
{
  switch (k)
  {
    case FnWagonKind::Neon:
      return "NEON CITY";
    case FnWagonKind::Temple:
      return "THE TEMPLE";
    case FnWagonKind::Station:
      return "THE STATION";
    case FnWagonKind::Western:
      return "THE WESTERN";
    case FnWagonKind::Toon:
      return "THE TOON";
    case FnWagonKind::Studio:
      return "THE STUDIO";
  }
  return "";
}

const char* reelName(int rule)
{
  static const char* const kNames[kFnReelRules] = {"AIR JUMP", "ECHO ROOM", "LIGHT TRAIL", "STOP MOTION", "HEATSTROKE",
    "TRIGGER HAPPY"};
  return rule >= 0 && rule < kFnReelRules ? kNames[rule] : "";
}

// A Stunt Double's jump (cells): Dash, Rocco, Nova.
constexpr int kDoubleJump[] = {7, 6, 9};
constexpr int kDoubleHp[] = {9, 12, 7};
constexpr int kDoubleShotRow = 4;   // cells over its feet: a crouching runner ducks under
constexpr int kAdReach = 36;        // cells: an Ad Break jingles for a runner this close
constexpr int kSampleReach = 40;    // cells: sample bots this close count as an Ad Break's
constexpr int kBarReach = 14;       // cells: a Censor Bar lays its bar over a runner this close

} // namespace

const char* finaleWagonName(FnWagonKind k) { return wagonName(k); }
const char* finaleReelName(int rule) { return reelName(rule); }

// --- Setup ---------------------------------------------------------------------------------

bool World::setupFinaleEntity(const EntityDef& e)
{
  const bool reel = mLevel->rules.find("blooper") != std::string::npos;
  if (mLevel->themeKey != "main_stage" && !reel)
    return false;
  auto& f = mFinale;
  f.on = true;
  f.reel = reel;
  if (e.kind == "duck")
    return true; // the duck's sequined host jacket is only drawn
  if (e.kind == "deco" && e.hasPos && e.str("kind") == "pedestal")
  {
    f.pedX = e.x * kCellsPerTile;
    f.pedY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "wagon")
  {
    static const char* const kKinds[] = {"neon", "temple", "station", "western", "toon", "studio"};
    FnWagon w;
    w.id = e.id;
    const std::string kind = e.str("kind", "neon");
    for (int i = 0; i < 6; ++i)
      if (kind == kKinds[i])
        w.kind = FnWagonKind(i);
    const auto slot = e.list("slot");
    if (slot.size() == 2)
    {
      w.x0 = slot[0];
      w.x1 = slot[1];
    }
    w.trigger = e.num("trigger", w.x0 - 4);
    f.wagons.push_back(w);
    return true;
  }
  if (e.kind == "plate" && e.hasPos)
  {
    FnPlate pl;
    pl.id = e.id;
    pl.zoneId = e.str("zone");
    pl.bx = e.x;
    pl.by = e.y;
    pl.dir = e.str("dir", "up") == "down" ? Grav::Down : Grav::Up;
    f.plates.push_back(pl);
    return true;
  }
  if (e.kind == "portal" && e.hasPos)
  {
    FnPortal po;
    po.id = e.id;
    po.toId = e.str("to");
    po.label = e.str("label", e.id);
    po.bx = e.x;
    po.by = e.y;
    f.portals.push_back(po);
    return true;
  }
  if (e.kind == "step" && e.hasPos)
  {
    f.route.push_back({e.x * kCellsPerTile, e.y * kCellsPerTile + 1, e.str("act", "go"), e.str("arg")});
    return true;
  }
  if (e.kind == "refill" && e.hasPos)
  {
    f.refillX = e.x * kCellsPerTile;
    f.refillY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "lance" && e.hasPos)
  {
    auto& b = f.boss;
    b.on = true;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (e.rect("arena", x0, y0, x1, y1))
    {
      b.arenaX0 = x0 * kCellsPerTile;
      b.arenaY0 = y0 * kCellsPerTile;
      b.arenaX1 = (x1 + 1) * kCellsPerTile - 1;
      b.arenaY1 = (y1 + 1) * kCellsPerTile - 1;
      b.floorY = (y1 + 1) * kCellsPerTile - 1;
    }
    const auto path = e.path("path");
    b.pathX0 = (path.empty() ? e.x : path.front().first) * kCellsPerTile;
    b.pathX1 = (path.empty() ? e.x : path.back().first) * kCellsPerTile;
    b.pathY = (path.empty() ? e.y : path.front().second) * kCellsPerTile;
    b.x = b.prevX = e.x * kCellsPerTile;
    b.y = b.prevY = b.pathY;
    const auto eng = e.list("engine");
    if (eng.size() == 2)
    {
      b.ex = eng[0] * kCellsPerTile;
      b.ey = eng[1] * kCellsPerTile + kFnEngineH - 1;
    }
    b.deskX = e.num("desk", x1) * kCellsPerTile;
    const auto rs = e.list("restart");
    if (rs.size() == 2)
    {
      b.restartX = rs[0] * kCellsPerTile;
      b.restartY = rs[1] * kCellsPerTile + 1;
    }
    for (const auto& [dx, dy] : e.path("drops"))
      b.drops.push_back({dx * kCellsPerTile, dy * kCellsPerTile + 1});
    const auto tb = e.list("turbo");
    if (tb.size() == 2)
    {
      b.turboX = tb[0] * kCellsPerTile;
      b.turboY = tb[1] * kCellsPerTile + 1;
    }
    b.runStartX = b.pathX0;
    b.lampY = b.arenaY0;
    return true;
  }
  return false;
}

void World::setupFinaleEnemy(Enemy& en, const EntityDef& e)
{
  if (!mFinale.on || !isFinaleEnemy(en.kind) || en.kind == EnemyKind::SampleBot)
    return;
  FnActor a;
  a.enemy = int(&en - mEnemies.data());
  switch (en.kind)
  {
    case EnemyKind::CensorBar:
      a.kind = FnActorKind::Censor;
      break;
    case EnemyKind::AdBreak:
      a.kind = FnActorKind::AdBreak;
      a.wagonId = e.str("wagon");
      a.homeX = en.x;
      en.hidden = true; // in the wings until its wagon's trick runs
      break;
    default:
    {
      a.kind = FnActorKind::Double;
      const std::string who = e.str("runner", "dash");
      a.runner = who == "rocco" ? 1 : (who == "nova" ? 2 : 0);
      en.hp = kDoubleHp[a.runner];
      break;
    }
  }
  for (const auto& [px, py] : e.path("path"))
    a.path.push_back({px * kCellsPerTile, py * kCellsPerTile + 1});
  if (a.path.empty())
    a.path.push_back({en.x, en.y});
  a.target = a.path.size() > 1 ? 1 : 0;
  mFinale.actors.push_back(a);
}

void World::linkFinale()
{
  auto& f = mFinale;
  const bool reel = mLevel->rules.find("blooper") != std::string::npos;
  if (mLevel->themeKey == "main_stage" || reel)
  {
    f.on = true;
    f.reel = reel;
  }
  if (!f.on)
    return;
  std::stable_sort(f.wagons.begin(), f.wagons.end(), [](const FnWagon& a, const FnWagon& b) { return a.x0 < b.x0; });
  for (auto& a : f.actors)
    for (std::size_t i = 0; i < f.wagons.size(); ++i)
      if (f.wagons[i].id == a.wagonId)
        a.wagon = int(i);
  for (auto& pl : f.plates)
    for (std::size_t i = 0; i < mGrav.zones.size(); ++i)
      if (mGrav.zones[i].id == pl.zoneId)
        pl.zone = int(i);
  for (auto& po : f.portals)
    for (std::size_t i = 0; i < f.portals.size(); ++i)
      if (f.portals[i].id == po.toId)
        po.to = int(i);
  for (std::size_t i = 0; i < mLayers.size(); ++i)
  {
    const std::string& id = mLayers[i].id;
    for (int k = 0; k < 3; ++k)
    {
      if (id == "S" + std::to_string(k + 1))
        f.signs[std::size_t(k)] = int(i);
      if (id == "TD" + std::to_string(k + 1))
        f.traps[std::size_t(k)] = int(i);
    }
    if (id == "onair")
      f.door = int(i);
    else if (id == "cover")
      f.cover = int(i);
    else if (id == "dstep")
      f.deckStep = int(i);
    else if (id == "dmid")
      f.deckMid = int(i);
    else if (id == "dhigh")
      f.deckHigh = int(i);
  }
  for (const auto& v : mVines)
    f.vineAmp = std::max(f.vineAmp, v.rest);
  for (auto& w : f.wagons)
    setWagonTrick(w, false); // every wagon starts in its rest state
  applyFinaleLayers();
  if (f.reel)
  {
    f.rng ^= 0x2545F491u;
    updateReel(); // the first rule from the first frame
  }
}

void World::setFinaleArsenal(const std::vector<std::string>& keys)
{
  auto& f = mFinale;
  f.arsenal = keys;
  f.wheel = {-1};
  for (int i = 0; i < kProtoCount; ++i)
  {
    const ProtoDef& d = protoDef(i);
    if (d.level >= 1 && d.level <= 41 && std::find(keys.begin(), keys.end(), d.key) != keys.end())
      f.wheel.push_back(i);
  }
  f.sel = std::clamp(f.sel, 0, int(f.wheel.size()) - 1);
}

bool World::secretsComplete() const
{
  // Every $ cache of the level opened and every letter of G-U-N found.
  for (const auto& pr : mProps)
    if (pr.kind == PropKind::GemCache && !pr.used)
      return false;
  const std::string& l = mStats.letters;
  bool letters = false;
  for (const auto& it : mItems)
    letters = letters || it.kind == ItemKind::LetterG || it.kind == ItemKind::LetterU || it.kind == ItemKind::LetterN;
  return !letters || (l.find('G') != std::string::npos && l.find('U') != std::string::npos && l.find('N') != std::string::npos);
}

// --- Layers --------------------------------------------------------------------------------

void World::setFinaleLayer(int index, bool solid)
{
  if (index < 0 || index >= int(mLayers.size()))
    return;
  Layer& l = mLayers[std::size_t(index)];
  l.driver = LayerDriver::Script;
  l.scriptSolid = solid;
  if (!solid && l.solid)
    applyLayer(l, false); // gone at once; solid again once nobody stands in it (updateLayers)
}

void World::applyFinaleLayers()
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  bool neon = false;
  for (const auto& w : f.wagons)
    neon = neon || (w.kind == FnWagonKind::Neon && w.state == FnWagonState::Running);
  for (int s : f.signs)
    if (s >= 0)
    {
      Layer& l = mLayers[std::size_t(s)];
      l.driver = neon ? LayerDriver::Beat : LayerDriver::Script;
      l.scriptSolid = true;
    }
  const bool offAir = b.phase == FnPhase::OffAir || b.phase == FnPhase::Switched;
  setFinaleLayer(f.deckStep, offAir);
  setFinaleLayer(f.deckMid, offAir);
  setFinaleLayer(f.deckHigh, offAir);
  setFinaleLayer(f.cover, b.coverHp > 0);
  const bool shut = b.phase != FnPhase::Waiting && b.phase != FnPhase::Switched && mPlayer.x >= b.arenaX0;
  setFinaleLayer(f.door, shut);
  for (int k = 0; k < 3; ++k)
    setFinaleLayer(f.traps[std::size_t(k)], b.openTrap != k);
  updateLayers(true);
}

// --- Frames --------------------------------------------------------------------------------

bool World::finaleFrozen() const
{
  const auto& f = mFinale;
  if (f.wipe > 0 || f.boss.commercial > 0)
    return true;
  for (const auto& w : f.wagons)
    if (w.state == FnWagonState::Rolling)
      return true;
  return false;
}

void World::updateFinaleFrames(const PlayerInput& input)
{
  auto& f = mFinale;
  auto& p = mPlayer;
  if (f.reel)
  {
    updateReel();
    updateFrame(input);
    // TRIGGER HAPPY: a press on the ground jumped; its shot comes now.
    if (mFly.groundPress)
    {
      mFly.groundPress = false;
      if (alive(p) && p.state != PlayerState::Ladder)
        fireShot();
    }
    return;
  }
  if (mState != WorldState::Playing || !finaleFrozen())
  {
    updateFrame(input);
    if (mState == WorldState::Exiting && f.boss.on && f.boss.phase == FnPhase::OffAir)
    {
      // The master switch flips: the ON AIR sign dies.
      f.boss.phase = FnPhase::Switched;
      f.boss.signDead = 0;
      if (f.boss.openTrap >= 0)
        setFinaleLayer(f.traps[std::size_t(f.boss.openTrap)], true);
      f.boss.openTrap = -1;
      playSound(Sfx::SwitchThrow);
      showMessage("THE ON AIR SIGN DIES - THAT'S A WRAP!");
    }
    return;
  }
  // The world holds still: the turntable turns a wagon in, a monitor's
  // static wipe, a CUT TO COMMERCIAL card.
  p.prevX = p.x;
  p.prevY = p.y;
  for (auto& e : mEnemies)
  {
    e.prevX = e.x;
    e.prevY = e.y;
  }
  for (auto& it : mItems)
  {
    it.prevX = it.x;
    it.prevY = it.y;
  }
  for (auto& pr : mProjectiles)
  {
    pr.prevX = pr.x;
    pr.prevY = pr.y;
  }
  f.upHeld = input.up;
  if (f.boss.commercial > 0)
  {
    if (--f.boss.commercial == 0)
      restartOffAir();
    return;
  }
  if (f.wipe > 0)
  {
    if (--f.wipe == 0 && f.wipeTo >= 0)
    {
      // Out of the other monitor, standing on its bottom row.
      const FnPortal& to = f.portals[std::size_t(f.wipeTo)];
      p.x = p.prevX = to.bx * kCellsPerTile;
      p.y = p.prevY = (to.by + 2) * kCellsPerTile + 1;
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Standing);
      f.wipeTo = -1;
      playSound(Sfx::MonitorHop);
    }
    return;
  }
  for (auto& w : f.wagons)
    if (w.state == FnWagonState::Rolling && ++w.t >= kFnRoll)
    {
      w.state = FnWagonState::Running;
      w.t = 0;
      setWagonTrick(w, true);
      playSound(Sfx::WagonLock);
      showMessage(std::string("PREVIOUSLY ON GUNRUNNERS: ") + wagonName(w.kind));
    }
}

std::uint32_t World::finaleRand()
{
  auto& r = mFinale.rng;
  r ^= r << 13;
  r ^= r >> 17;
  r ^= r << 5;
  return r;
}

// --- The set wagons ------------------------------------------------------------------------

bool World::inWagon(FnWagonKind kind, bool running) const
{
  const auto& p = mPlayer;
  const int cx = (p.x + 1) / kCellsPerTile;
  for (const auto& w : mFinale.wagons)
    if (w.kind == kind && cx >= w.x0 && cx <= w.x1 && (!running || w.state == FnWagonState::Running))
      return true;
  return false;
}

void World::setWagonTrick(FnWagon& w, bool on)
{
  auto& f = mFinale;
  const int c0 = w.x0 * kCellsPerTile, c1 = (w.x1 + 1) * kCellsPerTile - 1;
  switch (w.kind)
  {
    case FnWagonKind::Neon:
      // The beat signs: on the beat while it runs, solid at rest.
      for (int s : f.signs)
        if (s >= 0)
        {
          Layer& l = mLayers[std::size_t(s)];
          l.driver = on ? LayerDriver::Beat : LayerDriver::Script;
          l.scriptSolid = true;
        }
      break;
    case FnWagonKind::Temple:
      // The vines swing while it runs and hang still at rest.
      for (auto& v : mVines)
        if (v.ax >= c0 && v.ax <= c1)
        {
          v.rest = on ? f.vineAmp : 0;
          const bool held = mPlayer.state == PlayerState::Swing && mPlayer.vine == int(&v - mVines.data());
          if (on || !held)
            v.amp = v.rest;
        }
      break;
    case FnWagonKind::Station:
      // Gravity down at rest; the plates' flips are called off.
      if (!on)
      {
        for (std::size_t i = 0; i < mGrav.zones.size(); ++i)
        {
          const auto& z = mGrav.zones[i];
          if (z.x0 <= w.x1 && z.x1 >= w.x0 && z.dir != Grav::Down)
            flipZone(int(i));
        }
        for (auto& pl : f.plates)
          if (pl.bx >= w.x0 && pl.bx <= w.x1)
            pl.flip = 0;
      }
      break;
    case FnWagonKind::Western:
      if (!on)
        for (auto& fu : mWest.fuses)
          for (auto& c : fu.cells)
            if (c.x >= w.x0 && c.x <= w.x1 && c.state == 1)
              c.state = 0; // no fire on the fuse at rest
      break;
    case FnWagonKind::Toon:
      if (!on)
      {
        f.air = 0;
        f.airT = 0;
      }
      break;
    case FnWagonKind::Studio:
      break;
  }
}

void World::updateWagons()
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  const int cx = (p.x + 1) / kCellsPerTile;
  const bool live = alive(p);
  const bool ground = live && p.state == PlayerState::OnGround && p.grav == Grav::Down && mMap.onSolidGround(p.box());
  bool rolling = false;
  for (const auto& w : f.wagons)
    rolling = rolling || w.state == FnWagonState::Rolling;
  for (auto& w : f.wagons)
  {
    switch (w.state)
    {
      case FnWagonState::Idle:
        if (ground && !rolling && cx >= w.trigger && cx <= w.x1)
        {
          // The turntable turns the wagon in (the world holds still).
          w.state = FnWagonState::Rolling;
          w.t = 0;
          rolling = true;
          playSound(Sfx::WagonRoll);
          showMessage(std::string("THE TURNTABLE TURNS: ") + wagonName(w.kind));
        }
        break;
      case FnWagonState::Rolling:
        break; // updateFinaleFrames turns it
      case FnWagonState::Running:
        ++w.t;
        if (live && cx > w.x1)
        {
          w.state = FnWagonState::Done; // it rolls off behind the runner
          setWagonTrick(w, false);
        }
        else if (w.t >= kFnTrick)
        {
          setWagonTrick(w, false);
          w.t = 0;
          if (live && cx >= w.x0)
          {
            w.state = FnWagonState::Rerun;
            ++w.reruns;
            playSound(Sfx::VhsRewind);
            showMessage("RERUN!");
          }
          else
            w.state = FnWagonState::Idle;
        }
        break;
      case FnWagonState::Rerun:
        if (live && cx > w.x1)
          w.state = FnWagonState::Done;
        else if (++w.t >= kFnRerun)
        {
          w.state = FnWagonState::Running;
          w.t = 0;
          setWagonTrick(w, true);
        }
        break;
      case FnWagonState::Done:
        if (live && cx < w.x0)
          w.state = FnWagonState::Idle; // back for another go
        break;
    }
    // At rest the fuse never burns (a shot lit it just now).
    if (w.kind == FnWagonKind::Western && w.state != FnWagonState::Running)
      setWagonTrick(w, false);
  }
}

// --- Plates, monitors, air-running ---------------------------------------------------------

void World::updateFinalePlates()
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  for (auto& pl : f.plates)
  {
    if (pl.zone < 0)
      continue;
    bool running = false;
    for (const auto& w : f.wagons)
      running = running || (w.kind == FnWagonKind::Station && pl.bx >= w.x0 && pl.bx <= w.x1 && w.state == FnWagonState::Running);
    const CellBox plate{pl.bx * kCellsPerTile, pl.by * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    const bool on = running && alive(p) && p.state == PlayerState::OnGround && p.box().intersects(plate);
    auto& z = mGrav.zones[std::size_t(pl.zone)];
    if (on && !pl.pressed && pl.flip == 0 && z.dir != pl.dir)
    {
      pl.flip = kFnFlip; // the arrows flash, then the chamber turns over
      playSound(Sfx::PlateClunk);
    }
    pl.pressed = on;
    if (pl.flip > 0 && --pl.flip == 0 && running && z.dir != pl.dir)
      flipZone(pl.zone);
  }
}

void World::updateFinalePortals(const PlayerInput& input)
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  const bool press = input.up && !f.upHeld;
  f.upHeld = input.up;
  if (!press || !alive(p) || p.state != PlayerState::OnGround || f.wipe > 0)
    return;
  bool running = false;
  for (const auto& w : f.wagons)
    running = running || (w.kind == FnWagonKind::Studio && w.state == FnWagonState::Running);
  if (!running)
    return;
  const int mid = p.x + 1;
  for (std::size_t i = 0; i < f.portals.size(); ++i)
  {
    const auto& po = f.portals[i];
    if (po.to < 0)
      continue;
    const int x0 = po.bx * kCellsPerTile, x1 = x0 + 2 * kCellsPerTile - 1;
    if (mid >= x0 && mid <= x1 && p.y == (po.by + 2) * kCellsPerTile + 1)
    {
      f.wipe = kFnWipe;
      f.wipeTo = po.to;
      playSound(Sfx::StaticWipe);
      return;
    }
  }
}

bool World::finaleAirRun(int mvX)
{
  auto& f = mFinale;
  auto& p = mPlayer;
  if (f.uhOh > 0)
  {
    // Looked down: the "uh-oh" sign, then the fall.
    setVisual(PlayerVisual::Standing);
    return --f.uhOh > 0;
  }
  if (!inWagon(FnWagonKind::Toon, true) || p.vehicle >= 0)
  {
    f.air = 0;
    return false;
  }
  if (f.air == 0)
  {
    // Off a ledge holding the way you were going: run on.
    if (mvX == 0 || mvX != p.facing)
      return false;
    f.air = mvX;
    f.airT = 0;
  }
  if (p.turbo > 0)
  {
    if (mvX != 0)
      f.air = mvX;
    ++f.airT;
    return true;
  }
  if (mvX != f.air)
  {
    f.air = 0;
    f.airT = 0;
    f.uhOh = mCharacterIndex == 2 ? 2 * kFnUhOh : kFnUhOh;
    setVisual(PlayerVisual::Standing);
    playSound(Sfx::SlideWhistle);
    return true;
  }
  ++f.airT;
  return true;
}

bool World::finaleNoJump() const
{
  // No jumping on air (the first two frames off the ledge still count as ground).
  const auto& f = mFinale;
  return f.uhOh > 0 || (f.air != 0 && f.airT > 2);
}

// --- Every frame ---------------------------------------------------------------------------

void World::updateFinale(const PlayerInput& input)
{
  auto& f = mFinale;
  auto& p = mPlayer;
  if (f.reel)
    return;
  // The air-run ends on the ground or in the air.
  if (p.state != PlayerState::OnGround)
  {
    f.air = 0;
    f.uhOh = 0;
  }
  else if (f.air != 0 && mMap.onSolidGround(p.box()))
    f.air = 0;
  updateWagons();
  updateFinalePlates();
  updateFinalePortals(input);
  updateFinaleActors();
  // The Encore's pool: a unit back every ticker beat.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::Encore))
  {
    if (++f.beatT >= kFnBeat)
    {
      f.beatT = 0;
      p.ammo = std::min(kFnPool, p.ammo + 1);
    }
    if (f.spin > 0)
      --f.spin;
  }
  // The backstage refill: full hearts and a full pool, once a life.
  if (f.refillX >= 0 && !f.refillUsed && alive(p) &&
      p.box().intersects({f.refillX, f.refillY - 3, 2 * kCellsPerTile, 4}))
  {
    f.refillUsed = true;
    p.hp = p.maxHp;
    if (mLevelProto == int(ProtoId::Encore))
    {
      equip(Weapon::Proto, int(ProtoId::Encore));
      p.ammo = kFnPool;
    }
    playSound(Sfx::RefillSparkle);
    showMessage("BACKSTAGE: FULL HEARTS, FULL POOL");
  }
  if (f.boss.on)
    updateFinaleBoss(input);
}

// --- Enemies -------------------------------------------------------------------------------

FnActor* World::finaleActor(const Enemy& e)
{
  for (auto& a : mFinale.actors)
    if (a.enemy == int(&e - mEnemies.data()))
      return &a;
  return nullptr;
}

void World::updateFinaleActors()
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  const int sample = enemyIndex("sample_bot");
  for (auto& a : f.actors)
  {
    Enemy& e = mEnemies[std::size_t(a.enemy)];
    if (a.kind == FnActorKind::Censor && a.bar >= 0 && a.bar < kFnBarUnroll)
      ++a.bar;
    if (a.kind != FnActorKind::AdBreak)
      continue;
    // An Ad Break waits in the flies until its wagon's trick runs (the
    // station's: once the runner is up on the ceiling), then drops in.
    if (a.roll < 0 && e.alive && a.wagon >= 0)
    {
      const FnWagon& w = f.wagons[std::size_t(a.wagon)];
      const bool cue = w.state == FnWagonState::Running && (w.kind != FnWagonKind::Station || p.grav == Grav::Up);
      if (cue)
      {
        a.roll = 0;
        playSound(Sfx::AdDrop);
      }
    }
    if (a.roll >= 0 && a.roll < kFnAdRoll)
      ++a.roll; // drawn dropping in from the flies
    e.hidden = a.roll < 0;
    // Its free samples, outside the enemy loop (spawning moves the list).
    // Copy what the spawn needs: spawning reallocates the enemy list.
    const int adX = e.x, adW = e.w, adY = e.y;
    const bool adAlive = e.alive;
    while (a.spawn > 0 && sample >= 0 && adAlive)
    {
      --a.spawn;
      const int dir = p.x + 1 < adX + adW / 2 ? -1 : 1;
      spawnEnemy(sample, dir < 0 ? adX - 2 + a.spawn : adX + adW - a.spawn, adY);
      Enemy& s = mEnemies.back();
      s.dir = dir;
      s.active = true;
    }
    a.spawn = 0;
  }
}

void World::updateCensorBar(Enemy& e, const EnemyDef& def)
{
  FnActor* a = finaleActor(e);
  if (!a)
    return;
  const auto& p = mPlayer;
  // Along its path, half a cell a frame.
  if (e.timer % std::max(1, def.stepEvery) == 0 && !a->path.empty())
  {
    const auto& [tx, ty] = a->path[std::size_t(a->target) % a->path.size()];
    if (e.x != tx)
      e.dir = sgn(tx - e.x);
    e.x += sgn(tx - e.x);
    e.y += sgn(ty - e.y);
    if (e.x == tx && e.y == ty)
      a->target = (a->target + 1) % int(a->path.size());
  }
  // A runner below: the black bar unrolls over the area under it, once.
  if (a->bar < 0 && alive(p) && std::abs(p.x + 1 - (e.x + e.w / 2)) <= kBarReach && p.y > e.y && isOnScreen(e.box(), 0))
  {
    a->bar = 0;
    a->barX = e.x + e.w / 2 - kFnBarW / 2;
    a->barY = e.y + 1;
    e.tell = def.tell;
    playSound(Sfx::CensorBleep);
  }
  if (e.tell > 0)
    --e.tell;
}

void World::updateAdBreak(Enemy& e, const EnemyDef& def)
{
  FnActor* a = finaleActor(e);
  if (!a || a->roll < kFnAdRoll)
    return;
  const auto& p = mPlayer;
  if (e.cool > 0)
    --e.cool;
  if (e.tell > 0)
  {
    // The Halcyon Arms jingle, then two free samples.
    if (--e.tell == 0)
    {
      a->spawn = 2;
      e.cool = def.cooldown;
      playSound(Sfx::SamplePop);
    }
    return;
  }
  if (e.cool > 0 || !alive(p) || !isOnScreen(e.box(), 0) || std::abs(p.x + 1 - (e.x + e.w / 2)) > kAdReach)
    return;
  int samples = 0;
  for (const auto& o : mEnemies)
    if (o.alive && o.kind == EnemyKind::SampleBot && std::abs(o.x - e.x) <= kSampleReach)
      ++samples;
  if (samples + 2 > kFnAdSamples)
    return;
  e.tell = def.tell;
  playSound(Sfx::AdJingle);
}

void World::updateSampleBot(Enemy& e, const EnemyDef& def)
{
  const auto& p = mPlayer;
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (alive(p) && mid != ex)
    e.dir = sgn(mid - ex);
  int x = e.x;
  if (mMap.moveHorizontally(x, e.y, e.w, e.h, e.dir) != MoveResult::Completed)
    e.dir = -e.dir;
  e.x = x;
}

void World::updateStuntDouble(Enemy& e, const EnemyDef& def)
{
  FnActor* a = finaleActor(e);
  if (!a)
    return;
  const auto& p = mPlayer;
  const int mid = p.x + 1, ex = e.x + e.w / 2;
  if (e.cool > 0)
    --e.cool;
  // In a jump: up with its runner's arc, then down.
  if (a->jumpT >= 0)
  {
    ++a->jumpT;
    const int up = kDoubleJump[a->runner];
    if (a->jumpT <= up)
    {
      if (mMap.moveVertically(e.x, e.y, e.w, e.h, -1) != MoveResult::Completed)
        a->jumpT = up + 1;
    }
    else if (mMap.moveVertically(e.x, e.y, e.w, e.h, 1) != MoveResult::Completed)
      a->jumpT = -1;
    if (mid != ex && alive(p))
    {
      int x = e.x;
      mMap.moveHorizontally(x, e.y, e.w, e.h, sgn(mid - ex));
      e.x = x;
    }
    return;
  }
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  if (e.tell > 0)
  {
    // The muzzle-flash pose, then the shot (always a heart).
    if (--e.tell == 0)
    {
      const int fx = e.dir > 0 ? e.x + e.w : e.x - 2;
      spawnProjectile(ShotKind::Enemy, fx, e.y - kDoubleShotRow, e.dir, 0);
      mProjectiles.back().speed = 2;
      e.cool = def.cooldown;
      playSound(Sfx::StuntShot);
    }
    return;
  }
  if (!alive(p) || !isOnScreen(e.box(), 0))
    return;
  if (mid != ex)
    e.dir = sgn(mid - ex);
  const int dy = p.y - e.y;
  if (e.cool == 0 && std::abs(mid - ex) <= def.range && std::abs(dy) <= 4)
  {
    e.tell = kFnDoubleTell;
    return;
  }
  // The runner up a ledge within reach: jump after them.
  if (dy < -2 && std::abs(mid - ex) <= 10)
  {
    a->jumpT = 0;
    return;
  }
  // Chase on its own floor, a cell a frame; it stops at a ledge.
  if (std::abs(mid - ex) <= 4)
    return;
  const CellBox b = e.box();
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  if (!mMap.solidTop(aheadX, b.bottom() + 1))
    return;
  int x = e.x;
  int y = e.y;
  mMap.moveHorizontallyWithStairStepping(x, y, e.w, e.h, e.dir);
  e.x = x;
  e.y = y;
}

// --- The Encore ----------------------------------------------------------------------------

bool World::updateEncore(const Button& fire)
{
  auto& f = mFinale;
  auto& p = mPlayer;
  // Up + fire flicks the wheel to the next logged prototype.
  const bool up = p.stance == Stance::Up || mUpHeld;
  if (up && fire.pressed && f.wheel.size() > 1)
  {
    if (fire.triggered && !f.upFire)
    {
      f.sel = (f.sel + 1) % int(f.wheel.size());
      f.spin = kFnWheelSpin;
      f.holdT = 0;
      p.charge = 0;
      playSound(Sfx::EncoreSpin);
      const int id = f.wheel[std::size_t(f.sel)];
      showMessage(id < 0 ? "THE ENCORE: THE BLASTER" : std::string("THE ENCORE: ") + protoDef(id).name);
    }
    f.upFire = true;
    return true;
  }
  f.upFire = false;
  const int id = f.wheel[std::size_t(std::clamp(f.sel, 0, int(f.wheel.size()) - 1))];
  const bool turbo = p.turbo > 0;
  auto shoot = [&](int cost) {
    if (id >= 0 && !turbo && p.ammo < cost)
    {
      // An empty pool fires the blaster.
      p.weapon = Weapon::Normal;
      fireShot();
      p.weapon = Weapon::Proto;
      return;
    }
    p.weapon = Weapon::Normal;
    fireShot();
    p.weapon = Weapon::Proto;
    if (id < 0 || mProjectiles.empty())
      return;
    const ProtoDef& def = protoDef(id);
    auto& pr = mProjectiles.back();
    pr.kind = ShotKind::Proto;
    pr.proto = int(ProtoId::Encore);
    pr.encore = id;
    pr.speed = std::max(1, def.speed);
    pr.damage = std::max(1, def.damage) * (turbo ? 2 : 1);
    pr.strong = id == int(ProtoId::PulsePistol) && onTheBeat();
    if (pr.strong)
      pr.damage *= 2;
    if (!turbo)
      p.ammo = std::max(0, p.ammo - cost);
  };
  if (id < 0)
  {
    if (fire.triggered && (p.shotCooldown == 0 || turbo))
      shoot(0);
    return true;
  }
  const ProtoDef& def = protoDef(id);
  switch (def.mode)
  {
    case FireMode::Tap:
      if (fire.triggered && (p.shotCooldown == 0 || turbo))
      {
        shoot(1);
        p.shotCooldown = def.cooldown;
      }
      break;
    case FireMode::Auto:
      if (fire.pressed && (p.shotCooldown == 0 || turbo))
      {
        shoot(1);
        p.shotCooldown = std::max(1, def.cooldown);
      }
      break;
    case FireMode::Hold:
      // Held: a shot every 4 frames, a unit each.
      if (fire.pressed)
      {
        if (f.holdT++ % 4 == 0)
          shoot(1);
      }
      else
        f.holdT = 0;
      break;
    case FireMode::Charge:
      if (fire.pressed)
      {
        ++p.charge;
        break;
      }
      if (p.charge > 0)
        shoot(3);
      p.charge = 0;
      break;
    case FireMode::Place:
      if (fire.triggered)
        shoot(3);
      break;
  }
  return true;
}

// --- The Blooper Reel ----------------------------------------------------------------------

void World::setReelRule(int rule, bool on)
{
  switch (rule)
  {
    case kFnAirJump:
      mAirJump = on;
      break;
    case kFnSonar:
      mSonar = on;
      break;
    case kFnLightTrail:
      mLightTrail = on;
      if (!on)
      {
        mTrailBlocks.clear();
        syncFloats();
      }
      break;
    case kFnStopMotion:
      mReactor.stopMotion = on;
      break;
    case kFnHeatstroke:
      mDish.swapped = on;
      break;
    case kFnTriggerHappy:
      mFly.trigger = on;
      mFly.groundPress = false;
      break;
    default:
      break;
  }
}

void World::updateReel()
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  if (f.card > 0)
    --f.card;
  if (f.rule >= 0 && ++f.reelT < kFnReelEvery)
    return;
  // The switch waits a moment while the runner is inside solid ground.
  if (f.rule >= 0 && mMap.overlapsSolid(p.box()) && f.waitT < kFnReelWait)
  {
    ++f.waitT;
    return;
  }
  const unsigned all = (1u << kFnReelRules) - 1u;
  if ((f.used & all) == all)
    f.used = f.rule >= 0 ? 1u << unsigned(f.rule) : 0u; // a new round, never the same rule twice running
  std::vector<int> left;
  for (int r = 0; r < kFnReelRules; ++r)
    if ((f.used >> unsigned(r) & 1u) == 0 && r != f.rule)
      left.push_back(r);
  if (left.empty())
    return;
  const int next = left[finaleRand() % left.size()];
  if (f.rule >= 0)
    setReelRule(f.rule, false);
  setReelRule(next, true);
  f.rule = next;
  f.used |= 1u << unsigned(next);
  f.reelT = 0;
  f.waitT = 0;
  f.card = kFnReelCard;
  playSound(Sfx::BlooperHonk);
  showMessage(std::string("BLOOPER! ") + reelName(next));
}

// --- Lives and savegames -------------------------------------------------------------------

bool World::finaleStepDone(int i) const
{
  const auto& f = mFinale;
  const auto& p = mPlayer;
  if (i < 0 || i >= int(f.route.size()))
    return true;
  const FnStep& st = f.route[std::size_t(i)];
  const int bx = (p.x + 1) / kCellsPerTile;
  if (st.act == "plate")
    return p.grav == Grav::Up || bx > st.x / kCellsPerTile + 10;
  if (st.act == "ceiling")
    return (p.grav == Grav::Down && bx >= st.x / kCellsPerTile - 4 && p.y > st.y + 8) || bx > st.x / kCellsPerTile + 4;
  if (st.act == "hop")
  {
    int ax = 0;
    std::sscanf(st.arg.c_str(), "%d", &ax);
    return bx >= ax - 1;
  }
  return bx >= st.x / kCellsPerTile;
}

void World::resetFinale()
{
  auto& f = mFinale;
  const auto& p = mPlayer;
  f.air = f.airT = f.uhOh = 0;
  f.wipe = 0;
  f.wipeTo = -1;
  f.refillUsed = false;
  f.holdT = 0;
  const int cx = (p.x + 1) / kCellsPerTile;
  // The wagons ahead of the checkpoint go back to rest, ready to roll in again.
  for (auto& w : f.wagons)
    if (w.x1 >= cx && w.state != FnWagonState::Idle)
    {
      setWagonTrick(w, false);
      w.state = FnWagonState::Idle;
      w.t = 0;
    }
  for (auto& pl : f.plates)
  {
    pl.flip = 0;
    pl.pressed = false;
  }
  for (auto& a : f.actors)
  {
    Enemy& e = mEnemies[std::size_t(a.enemy)];
    e.tell = 0;
    a.jumpT = -1;
  }
  if (f.boss.on)
    f.boss.commercial = 0;
  if (f.boss.on && f.boss.phase != FnPhase::Waiting && f.boss.phase != FnPhase::Switched)
  {
    // Back at the arena checkpoint: the ON AIR door opens again.
    auto& b = f.boss;
    b.attack = FnAttack::None;
    b.wheel = -1;
    b.gap = kFnAttackGap;
    b.waveL = b.waveR = -1;
    b.lampX = -1;
    b.moveT = -1;
    b.coinL = b.coinR = -1;
    b.ring = -1;
    b.breath = 0;
    b.trap = -1;
    if (b.phase == FnPhase::OffAir)
    {
      b.runX = b.runStartX;
      b.runSub = 0;
      b.trapT = 0;
      b.lightT = 0;
    }
    b.openTrap = -1;
    for (int k = 0; k < 3; ++k)
      setFinaleLayer(f.traps[std::size_t(k)], true);
    setFinaleLayer(f.door, false);
  }
}

bool World::finaleCanSave() const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  return !f.reel && !finaleFrozen() && (!b.on || b.phase == FnPhase::Waiting || b.phase == FnPhase::Switched);
}

std::vector<int> World::finaleSave() const
{
  const auto& f = mFinale;
  std::vector<int> v{f.air, f.airT, f.uhOh, f.refillUsed ? 1 : 0, f.upHeld ? 1 : 0, f.sel, f.spin, f.beatT, f.holdT,
    f.upFire ? 1 : 0, f.routeAt, int(f.rng), f.bonusDone ? 1 : 0, int(f.boss.phase), int(f.wagons.size()),
    int(f.plates.size()), int(f.actors.size())};
  for (const auto& w : f.wagons)
  {
    v.push_back(int(w.state));
    v.push_back(w.t);
    v.push_back(w.reruns);
  }
  for (const auto& pl : f.plates)
  {
    v.push_back(pl.flip);
    v.push_back(pl.pressed ? 1 : 0);
  }
  for (const auto& a : f.actors)
  {
    v.push_back(a.target);
    v.push_back(a.bar);
    v.push_back(a.barX);
    v.push_back(a.barY);
    v.push_back(a.roll);
    v.push_back(a.jumpT);
  }
  return v;
}

bool World::validFinaleSave(const std::vector<int>& v) const
{
  const auto& f = mFinale;
  constexpr std::size_t kHead = 17;
  return v.size() == kHead + f.wagons.size() * 3 + f.plates.size() * 2 + f.actors.size() * 6 &&
    v[14] == int(f.wagons.size()) && v[15] == int(f.plates.size()) && v[16] == int(f.actors.size());
}

void World::loadFinale(const std::vector<int>& v)
{
  auto& f = mFinale;
  std::size_t i = 0;
  f.air = v[i++];
  f.airT = v[i++];
  f.uhOh = v[i++];
  f.refillUsed = v[i++] != 0;
  f.upHeld = v[i++] != 0;
  f.sel = std::clamp(v[i++], 0, int(f.wheel.size()) - 1);
  f.spin = v[i++];
  f.beatT = v[i++];
  f.holdT = v[i++];
  f.upFire = v[i++] != 0;
  f.routeAt = v[i++];
  f.rng = std::uint32_t(v[i++]);
  f.bonusDone = v[i++] != 0;
  f.boss.phase = FnPhase(std::clamp(v[i++], 0, int(FnPhase::Switched)));
  i += 3;
  for (auto& w : f.wagons)
  {
    w.state = FnWagonState(std::clamp(v[i++], 0, int(FnWagonState::Done)));
    w.t = v[i++];
    w.reruns = v[i++];
  }
  for (auto& pl : f.plates)
  {
    pl.flip = v[i++];
    pl.pressed = v[i++] != 0;
  }
  for (auto& a : f.actors)
  {
    a.target = v[i++];
    a.bar = v[i++];
    a.barX = v[i++];
    a.barY = v[i++];
    a.roll = v[i++];
    a.jumpT = v[i++];
    if (a.kind == FnActorKind::AdBreak)
      mEnemies[std::size_t(a.enemy)].hidden = a.roll < 0;
  }
  f.wipe = 0;
  f.wipeTo = -1;
  if (f.boss.phase == FnPhase::Switched)
    f.boss.coverHp = 0;
  for (auto& w : f.wagons)
    setWagonTrick(w, w.state == FnWagonState::Running);
  applyFinaleLayers();
}

} // namespace gr
