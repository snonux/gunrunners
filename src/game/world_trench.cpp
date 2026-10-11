// Level 30, Fiber Trench: buoyancy (`@ fluid kind=water`: slow sinking,
// jumps are swim strokes, three before the runner must touch something),
// air pockets, currents, the abyss, the Harpoon Line and its lines,
// Anglerfish, Box Jellies and Cable Eels, and the Bubble Up bonus
// (rules=bubble). Drawing is in world_trench_draw.cpp.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace gr
{

namespace
{

CellBox blockRect(int x0, int y0, int x1, int y1)
{
  return {std::min(x0, x1) * kCellsPerTile, std::min(y0, y1) * kCellsPerTile, (std::abs(x1 - x0) + 1) * kCellsPerTile,
    (std::abs(y1 - y0) + 1) * kCellsPerTile};
}

int sgn(int v) { return (v > 0) - (v < 0); }

// "1/2" -> 2 frames a cell, "1" -> 1.
int pushEvery(const std::string& s)
{
  const auto slash = s.find('/');
  if (slash == std::string::npos)
    return 1;
  return std::max(1, std::atoi(s.c_str() + slash + 1) / std::max(1, std::atoi(s.c_str())));
}

const char* const kOldLines[] = {
  "...THE GRID IS DOWN. YOU'RE OUR ONLY RUNNERS...",
  "...FOLLOW THE CABLE. IT KNOWS THE WAY...",
  "...EVERY CHANNEL IS A LEVEL...",
  "...DON'T FIGHT THE NOISE. TUNE IT...",
};

} // namespace

// --- Setup ---------------------------------------------------------------------------------

bool World::setupTrenchEntity(const EntityDef& e)
{
  auto& t = mTrench;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  if (mLevel->themeKey != "deep_sea" || (!t.on && !(e.kind == "fluid" && e.str("kind") == "water") &&
                                          mLevel->rules.find("bubble") == std::string::npos))
    return false;
  if (e.kind == "fluid" && e.str("kind") == "water" && hasRect)
  {
    t.on = t.water = true;
    t.waterBox = blockRect(x0, y0, x1, y1);
    return true;
  }
  if (e.kind == "airpocket" && hasRect)
  {
    t.pockets.push_back(blockRect(x0, y0, x1, y1));
    return true;
  }
  if (e.kind == "current" && hasRect)
  {
    Current c;
    c.rect = blockRect(x0, y0, x1, y1);
    const std::string dir = e.str("dir", "d");
    c.dx = dir == "l" ? -1 : (dir == "r" ? 1 : 0);
    c.dy = dir == "u" ? -1 : (dir == "d" ? 1 : 0);
    c.every = pushEvery(e.str("push", "1"));
    t.currents.push_back(c);
    return true;
  }
  if (e.kind == "abyss" && hasRect)
  {
    t.abyss.push_back(blockRect(x0, y0, x1, y1));
    return true;
  }
  if (e.kind == "dark" && hasRect)
  {
    t.darks.push_back(blockRect(x0, y0, x1, y1));
    return true;
  }
  if (e.kind == "anchor" && e.hasPos)
  {
    Anchor a;
    a.x = e.x * kCellsPerTile;
    a.y = e.y * kCellsPerTile;
    a.spotX = e.num("spot", e.x) * kCellsPerTile + 1;
    t.anchors.push_back(a);
    return true;
  }
  if (e.kind == "junction" && e.hasPos)
  {
    t.quietX = e.x * kCellsPerTile;
    t.quietY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "porthole" && e.hasPos)
  {
    t.portX = e.x * kCellsPerTile;
    t.portY = e.y * kCellsPerTile + 1;
    return true;
  }
  if (e.kind == "ring" && e.hasPos)
  {
    t.rings.push_back(e.y * kCellsPerTile + 1);
    return true;
  }
  if (e.kind == "duck")
    return true; // the duck's costume is only drawn
  return false;
}

void World::setupTrenchEnemy(Enemy& en, const EntityDef& e)
{
  auto& t = mTrench;
  if (!t.on || (en.kind != EnemyKind::Anglerfish && en.kind != EnemyKind::BoxJelly && en.kind != EnemyKind::CableEel))
    return;
  const int index = int(mEnemies.size()) - 1;
  if (en.kind == EnemyKind::Anglerfish)
  {
    en.variant = e.num("docile", 0); // 1: never bites
    en.ox = en.x;
    en.oy = en.y;
    en.dir = e.num("dir", -1) < 0 ? -1 : 1;
  }
  if (en.kind == EnemyKind::BoxJelly)
  {
    en.carrier = e.num("carrier", 0) != 0;
    JellyColumn j;
    j.enemy = index;
    j.homeX = en.x;
    j.bottom = en.y;
    j.rise = e.num("rise", 6) * kCellsPerTile;
    j.up = std::max(1, e.num("up", j.rise * 4));
    j.hold = e.num("hold", 30);
    j.down = std::max(1, e.num("down", j.rise * 4));
    j.rest = e.num("rest", 30);
    j.phase = e.num("phase", 0);
    t.jellies.push_back(j);
  }
  if (en.kind == EnemyKind::CableEel)
  {
    // The eel lives in a solid junction box; it lunges out along its facing.
    EelBox b;
    b.enemy = index;
    b.bx = e.x;
    b.by = e.y;
    const std::string face = e.str("face", "l");
    b.fx = face == "l" ? -1 : (face == "r" ? 1 : 0);
    b.fy = face == "u" ? -1 : (face == "d" ? 1 : 0);
    mMap.setBlock(b.bx, b.by, Tile::Solid);
    en.w = b.fx != 0 ? kEelReach : 3;
    en.h = b.fx != 0 ? 3 : kEelReach;
    en.hidden = true;
    t.eels.push_back(b);
  }
}

void World::linkTrench()
{
  auto& t = mTrench;
  t.bubble = mLevel->rules.find("bubble") != std::string::npos;
  t.on = t.on || t.bubble;
  if (!t.on)
    return;
  auto& p = mPlayer;
  t.groundX = p.x;
  t.groundY = p.y;
  t.ringX = p.x;
  t.ringY = p.y;
  // Each anchor's line runs at the height of the ground at its spot.
  for (auto& a : t.anchors)
  {
    a.row = a.y + 1;
    for (int y = a.y; y < mMap.height(); ++y)
      if (mMap.solid(a.spotX, y))
      {
        a.row = y;
        break;
      }
  }
}

// --- The water -------------------------------------------------------------------------------

bool World::inPocket(int cx, int cy) const
{
  for (const auto& b : mTrench.pockets)
    if (b.contains(cx, cy))
      return true;
  return false;
}

bool World::underwater() const
{
  const auto& t = mTrench;
  if (!t.water)
    return false;
  const auto& p = mPlayer;
  const int cx = p.x + 1, cy = p.y - 2;
  return t.waterBox.contains(cx, cy) && !inPocket(cx, cy);
}

int World::strokesMax() const
{
  return mCharacterIndex == 2 ? kStrokes + 1 : kStrokes;
}

// A press of jump in the water, in mid-air: another stroke, if one is left.
bool World::trenchStroke(const Button& jump)
{
  auto& t = mTrench;
  const auto& p = mPlayer;
  if (!jump.triggered || !underwater() || (p.state != PlayerState::Falling && !(p.state == PlayerState::Jumping && p.frames > 0)))
    return false;
  if (p.turbo == 0 && t.strokes >= strokesMax())
    return false;
  if (mMap.touchingCeiling(p.box()))
    return false;
  ++t.strokes;
  return true;
}

// Cells sunk this frame in the water: a cell every third frame (Rocco every
// other).
int World::trenchSink()
{
  auto& t = mTrench;
  const int every = mCharacterIndex == 1 ? 2 : kSinkEvery;
  return ++t.sinkTick % every == 0 ? 1 : 0;
}

void World::updateTrench(const PlayerInput& input)
{
  (void)input;
  auto& t = mTrench;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (t.fade > 0)
    --t.fade;
  // Sound only: a jump in the water (a stroke) is heard as a swim stroke.
  if (underwater())
    std::replace(mSounds.begin(), mSounds.end(), Sfx::Jump, Sfx::SwimStroke);
  if (t.bubble)
  {
    updateBubbleWorld();
    return;
  }
  // Strokes come back on anything solid, a line, or in an air pocket.
  const bool ground = p.state == PlayerState::OnGround || p.state == PlayerState::Ladder;
  if (ground || !underwater())
    t.strokes = 0;
  else if (t.wasGround && p.state == PlayerState::Jumping)
    t.strokes = 1; // the jump off the ground was the first stroke
  t.wasGround = ground;
  // The last solid ground stood on (not a line): where the abyss sends you.
  if (alive && p.state == PlayerState::OnGround)
  {
    bool solid = false;
    for (int x = p.x; x < p.x + Player::kWidth; ++x)
      solid = solid || mMap.solid(x, p.y + 1);
    bool deep = false;
    for (const auto& a : t.abyss)
      deep = deep || a.intersects(p.box());
    if (solid && !deep)
    {
      t.groundX = p.x;
      t.groundY = p.y;
    }
  }
  // Currents push swimmers in the air (Rocco is too heavy for them).
  ++t.pushTick;
  if (alive && mCharacterIndex != 1 && underwater() && p.state != PlayerState::OnGround && p.vehicle < 0)
    for (const auto& c : t.currents)
      if (c.rect.contains(p.x + 1, p.y - 2) && t.pushTick % c.every == 0)
      {
        if (c.dx != 0)
          mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), c.dx);
        if (c.dy != 0)
        {
          if (c.dy > 0 && mMap.onSolidGround(p.box()))
            continue;
          mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), c.dy);
        }
      }
  // The abyss: a heart, and back to the last ground.
  if (alive)
    for (const auto& a : t.abyss)
      if (a.intersects(p.box()))
      {
        hurtPlayer(1);
        if (p.state == PlayerState::Dying)
          break;
        p.x = p.prevX = t.groundX;
        p.y = p.prevY = t.groundY;
        p.state = PlayerState::OnGround;
        p.frames = 0;
        setVisual(PlayerVisual::Standing);
        t.fade = kAbyssFade;
        t.strokes = 0;
        playSound(Sfx::AbyssSplash);
        break;
      }
  // Harpooned enemies come in.
  for (auto& pl : t.pulls)
  {
    if (pl.second <= 0 || std::size_t(pl.first) >= mEnemies.size())
      continue;
    --pl.second;
    auto& e = mEnemies[std::size_t(pl.first)];
    if (!e.alive)
    {
      pl.second = 0;
      continue;
    }
    const int dx = sgn(p.x + 1 - (e.x + e.w / 2)), dy = sgn(p.y - 2 - (e.y - e.h / 2));
    if (std::abs(p.x + 1 - (e.x + e.w / 2)) > 3)
      e.x += dx;
    if (std::abs(p.y - 2 - (e.y - e.h / 2)) > 2)
      e.y += dy;
  }
  t.pulls.erase(std::remove_if(t.pulls.begin(), t.pulls.end(), [](const auto& pl) { return pl.second <= 0; }),
    t.pulls.end());
  // Easter eggs: the quiet junction box speaks; the porthole's alien waves.
  if (t.quietX >= 0 && !t.quietSpoke)
  {
    const bool near = std::abs(p.x + 1 - (t.quietX + 1)) <= 3 && std::abs(p.y - t.quietY) <= 6;
    t.quietT = near ? t.quietT + 1 : 0;
    if (t.quietT >= kQuietFrames)
    {
      t.quietSpoke = true;
      showMessage(kOldLines[std::size_t(mStats.frames) % (sizeof(kOldLines) / sizeof(kOldLines[0]))]);
      playSound(Sfx::Beep);
    }
  }
  if (t.portX >= 0)
  {
    if (t.wave > 0)
      --t.wave;
    else if (t.waves < 3 && std::abs(p.x + 1 - t.portX) <= 10 && std::abs(p.y - t.portY) <= 10)
    {
      t.wave = 24;
      ++t.waves;
    }
  }
  syncPlatformCollision();
}

// --- The Harpoon Line ------------------------------------------------------------------------

// Crouched, fire plants a harpoon at the runner's feet; true if it did.
bool World::plantHarpoon()
{
  const auto& p = mPlayer;
  if (p.stance != Stance::Crouched || p.state != PlayerState::OnGround)
    return false;
  addHarpoon(p.x + 1, p.y + 1, -1);
  playSound(Sfx::HarpoonPlant);
  return true;
}

void World::addHarpoon(int x, int y, int anchor)
{
  auto& t = mTrench;
  t.harpoons.push_back({x, y, anchor});
  if (t.harpoons.size() > 2)
    t.harpoons.erase(t.harpoons.begin()); // one line at a time: the oldest goes
  remakeLine();
  if (t.harpoons.size() == 2 && !t.line)
    t.harpoons.pop_back(); // too far or too steep: the second one drops
}

void World::remakeLine()
{
  auto& t = mTrench;
  t.line = false;
  if (t.harpoons.size() == 2)
  {
    const auto& a = t.harpoons[0];
    const auto& b = t.harpoons[1];
    if (std::abs(a.x - b.x) <= kLineReach && std::abs(a.x - b.x) >= 2 && std::abs(a.y - b.y) <= kLineSlope)
    {
      // Taut, and walked as level at the first harpoon's height (an anchor's
      // line at the height of the ground by it).
      t.line = true;
      t.lineX0 = std::min(a.x, b.x);
      t.lineX1 = std::max(a.x, b.x);
      int row = a.y;
      if (a.anchor >= 0)
        row = t.anchors[std::size_t(a.anchor)].row;
      else if (b.anchor >= 0)
        row = t.anchors[std::size_t(b.anchor)].row;
      t.lineY = row;
      playSound(Sfx::LineTaut);
    }
  }
  syncPlatformCollision();
}

void World::addTrenchPlatforms()
{
  const auto& t = mTrench;
  if (t.line)
    mMap.addPlatform({t.lineX0, t.lineY, t.lineX1 - t.lineX0 + 1, 1});
}

bool World::lineUnder() const
{
  const auto& t = mTrench;
  const auto& p = mPlayer;
  return t.line && p.y + 1 == t.lineY && p.x + 2 >= t.lineX0 && p.x <= t.lineX1;
}

// A harpoon in a wall sticks there.
void World::stickHarpoon(Projectile& pr)
{
  const int x = pr.dx > 0 ? pr.x + pr.w : pr.x - 1;
  addHarpoon(x, pr.y, -1);
  playSound(Sfx::HarpoonStick);
}

// A harpoon at an anchor ring sticks in it; true if it did.
bool World::shotAtTrench(Projectile& pr, const CellBox& hit)
{
  auto& t = mTrench;
  if (pr.kind != ShotKind::Proto || pr.proto != int(ProtoId::HarpoonLine))
    return false;
  for (std::size_t i = 0; i < t.anchors.size(); ++i)
    if (t.anchors[i].catchBox().intersects(hit))
    {
      addHarpoon(t.anchors[i].x + (pr.dx > 0 ? 0 : 1), t.anchors[i].y + 1, int(i));
      playSound(Sfx::HarpoonStick);
      burst(cellCenter(t.anchors[i].catchBox()), rgb(230, 240, 255), rgb(120, 180, 220), 6, 1.0f);
      return true;
    }
  return false;
}

// 0 as usual, 1 the shot is spent, 2 it passes through.
int World::shotAtTrenchEnemy(Projectile& pr, Enemy& e)
{
  auto& t = mTrench;
  if (e.kind == EnemyKind::CableEel)
  {
    for (const auto& b : t.eels)
      if (b.enemy >= 0 && &mEnemies[std::size_t(b.enemy)] == &e && b.state != 2 && b.state != 3)
        return 2; // in its box: out of reach
  }
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::HarpoonLine) && e.w <= 4 && e.h <= 4)
  {
    const int index = int(&e - mEnemies.data());
    t.pulls.push_back({index, kHarpoonPull});
  }
  return 0;
}

// --- Enemies ---------------------------------------------------------------------------------

void World::updateAnglerfish(Enemy& e, const EnemyDef& def)
{
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  if (e.cool > 0)
    --e.cool;
  if (e.variant == 1)
    return; // the old one in the cave: it only dangles its lure
  const int pcx = p.x + 1, ecx = e.x + e.w / 2;
  if (e.dive > 0)
  {
    // Lunging, then swimming back to its spot.
    --e.dive;
    if (e.dive >= kAnglerLunge / 2)
    {
      e.x += 2 * e.dir;
      if (std::abs(p.y - 1 - e.y) > 0)
        e.y += sgn(p.y - 1 - e.y);
    }
    if (e.dive == 0)
      e.cool = def.cooldown;
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      e.dive = kAnglerLunge;
      playSound(Sfx::AnglerLunge);
    }
    return;
  }
  // Back to its spot.
  if (e.timer % 2 == 0)
  {
    e.x += sgn(e.ox - e.x);
    e.y += sgn(e.oy - e.y);
  }
  if (alive && e.cool == 0 && std::abs(pcx - ecx) <= kAnglerReach + e.w / 2 && std::abs(p.y - e.y) <= 6)
  {
    e.dir = pcx < ecx ? -1 : 1;
    e.tell = def.tell;
  }
}

void World::updateBoxJelly(Enemy& e, const EnemyDef& def)
{
  (void)def;
  auto& t = mTrench;
  const int index = int(&e - mEnemies.data());
  for (auto& j : t.jellies)
  {
    if (j.enemy != index)
      continue;
    const int period = j.rest + j.up + j.hold + j.down;
    const int ph = (e.timer + j.phase) % period;
    int lift = 0;
    e.tell = 0;
    if (ph < j.rest)
    {
      lift = 0;
      if (ph >= j.rest - 8)
        e.tell = j.rest - ph; // the bell contracts before a rise
      if (ph == j.rest - 8 && isOnScreen(e.box(), 4))
        playSound(Sfx::JellyPulse); // sound only
    }
    else if (ph < j.rest + j.up)
      lift = j.rise * (ph - j.rest) / j.up;
    else if (ph < j.rest + j.up + j.hold)
      lift = j.rise;
    else
      lift = j.rise - j.rise * (ph - j.rest - j.up - j.hold) / j.down;
    e.y = j.bottom - lift;
    // Currents carry it off its column; out of them it drifts back.
    bool pushed = false;
    for (const auto& c : t.currents)
      if (c.dx != 0 && c.rect.contains(e.x + e.w / 2, e.y - e.h / 2))
      {
        pushed = true;
        if (e.timer % (c.every * 2) == 0 && std::abs(e.x + c.dx - j.homeX) <= 8)
          e.x += c.dx;
      }
    if (!pushed && e.timer % 4 == 0)
      e.x += sgn(j.homeX - e.x);
    return;
  }
}

void World::updateCableEel(Enemy& e, const EnemyDef& def)
{
  auto& t = mTrench;
  auto& p = mPlayer;
  const bool alive = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int index = int(&e - mEnemies.data());
  for (auto& b : t.eels)
  {
    if (b.enemy != index)
      continue;
    const int mx = b.bx * kCellsPerTile + 1 + b.fx, my = b.by * kCellsPerTile + 1 + b.fy;
    ++b.t;
    switch (b.state)
    {
      case 0: // in its box
        if (e.cool > 0)
          --e.cool;
        else if (alive && std::abs(p.x + 1 - mx) <= kEelWake && std::abs(p.y - 2 - my) <= kEelWake)
        {
          b.state = 1;
          b.t = 0;
          e.tell = def.tell;
          playSound(Sfx::EelSpark);
        }
        break;
      case 1: // the box sparks and hums
        e.tell = std::max(0, def.tell - b.t);
        if (b.t >= def.tell)
        {
          b.state = 2;
          b.t = 0;
          playSound(Sfx::EelSnap);
        }
        break;
      case 2: // out
        b.ext = std::min(kEelReach, kEelReach * b.t / kEelOut);
        if (b.t >= kEelOut)
        {
          b.state = 3;
          b.t = 0;
        }
        break;
      case 3: // back in
        b.ext = std::max(0, kEelReach - kEelReach * b.t / kEelBack);
        if (b.t >= kEelBack)
        {
          b.state = 0;
          b.t = 0;
          b.ext = 0;
          e.cool = def.cooldown;
        }
        break;
    }
    // Its body: from the box's mouth along its facing, as far as it is out.
    const int len = std::max(1, b.ext);
    const int bx = b.bx * kCellsPerTile, by = b.by * kCellsPerTile;
    if (b.fx != 0)
    {
      e.w = len;
      e.h = 3;
      e.x = b.fx > 0 ? bx + 2 : bx - len;
      e.y = by + 1;
    }
    else
    {
      e.w = 3;
      e.h = len;
      e.x = bx;
      e.y = b.fy > 0 ? by + 1 + len : by - 1;
    }
    e.hidden = b.ext == 0;
    if (alive && b.ext > 0 && e.box().intersects(p.hitBox()))
      hurtPlayer(1);
    return;
  }
}

// --- Bubble Up -------------------------------------------------------------------------------

// The runner rides a bubble up: half a cell a frame, a tap of jump sinks it
// two cells, left and right at half a cell a frame. Coral spikes pop it: back
// at the last ring passed, nothing lost.
void World::updateBubbleRide(int mvX, int mvY, const PlayerInput& in)
{
  (void)mvY;
  auto& t = mTrench;
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  p.state = PlayerState::Falling;
  p.frames = 0;
  if (t.pop > 0)
  {
    --t.pop;
    setVisual(PlayerVisual::Falling);
    return;
  }
  ++t.bubbleTick;
  if (mvX != 0)
  {
    if (mvX != p.facing)
      p.facing = mvX;
    if (t.bubbleTick % 2 == 0)
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX);
  }
  if (in.jump.triggered)
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), kBubbleSink);
  else if (t.bubbleTick % 2 == 0)
    mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1);
  setVisual(PlayerVisual::Jumping);
}

void World::updateBubbleWorld()
{
  auto& t = mTrench;
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting || t.pop > 0)
    return;
  // Rings passed: the next start.
  for (int r : t.rings)
    if (p.y < r && r < t.ringY)
    {
      t.ringY = r;
      t.ringX = p.x;
      playSound(Sfx::RingPass);
    }
  // Coral spikes pop the bubble.
  if (mMap.overlapsHazard(boxAt(p.x - 1, p.y + 1, Player::kWidth + 2, p.height() + 2)))
  {
    playSound(Sfx::BubblePop);
    burst({(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 2.0f) * kCellSize}, rgb(220, 245, 255), rgb(120, 200, 240),
      16, 1.6f);
    p.x = p.prevX = t.ringX;
    p.y = p.prevY = t.ringY;
    t.pop = 15;
  }
}

// --- Saving ----------------------------------------------------------------------------------

bool World::trenchCanSave() const
{
  const auto& t = mTrench;
  for (const auto& b : t.eels)
    if (b.state != 0)
      return false;
  return t.pulls.empty() && t.fade == 0 && t.pop == 0;
}

std::vector<int> World::trenchSave() const
{
  const auto& t = mTrench;
  std::vector<int> v{t.groundX, t.groundY, t.quietSpoke, t.waves, t.ringX, t.ringY, int(t.harpoons.size())};
  for (const auto& h : t.harpoons)
  {
    v.push_back(h.x);
    v.push_back(h.y);
    v.push_back(h.anchor);
  }
  return v;
}

bool World::validTrenchSave(const std::vector<int>& s) const
{
  if (s.size() < 7 || s[6] < 0 || s[6] > 2 || s.size() != std::size_t(7 + 3 * s[6]))
    return false;
  for (int i = 0; i < s[6]; ++i)
  {
    const int a = s[std::size_t(9 + 3 * i)];
    if (a < -1 || a >= int(mTrench.anchors.size()))
      return false;
  }
  return true;
}

void World::loadTrench(const std::vector<int>& s)
{
  auto& t = mTrench;
  t.groundX = s[0];
  t.groundY = s[1];
  t.quietSpoke = s[2] != 0;
  t.waves = s[3];
  t.ringX = s[4];
  t.ringY = s[5];
  t.harpoons.clear();
  for (int i = 0; i < s[6]; ++i)
    t.harpoons.push_back({s[std::size_t(7 + 3 * i)], s[std::size_t(8 + 3 * i)], s[std::size_t(9 + 3 * i)]});
  remakeLine();
}

} // namespace gr
