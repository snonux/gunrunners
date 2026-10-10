// Level 9, Hall of Traps (SPEC 09): Your Traps Too. Every trap fires from a
// pressure plate, and plates fire for any weight: the runner, an enemy, a
// beetle. Rolling stones run along their grooves, blades sweep out of wall
// slits and spike strips come up, and they hurt whatever is in the way.
// Floors crack under you and fall, and three stone keys open the door to
// the final hall, where the spikes and blades run on a 60-frame cycle.
// Stone Guardians shrug off shots to the face, Dart Faces drop darts on
// their own rhythm and Scarab Tides flow along the floor toward you. The
// prototype is the Snare Bolas: it topples a walker, rolls it along and
// leaves it tangled (onto a plate, if you aim well). Also the Trapmaster
// bonus (rules=trapmaster), where you work the traps yourself.

#include "game/world.hpp"

#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(10, 8, 20);
constexpr float kCellPx = float(kCellSize) * kPixelScale;
constexpr int kStoneTell = 15;    // frames of rumble before a stone rolls
constexpr int kStoneBack = 120;   // frames before a stone is back in its recess
constexpr int kStoneSpeed = 2;    // cells a frame
constexpr int kKnockback = 4;     // cells a stone throws the runner
constexpr int kGlowReach = 12;    // cells: trap paths glow when the runner is this close
constexpr int kTangleFrames = 45; // frames a Bolas victim lies tangled
constexpr int kTangleRoll = 6;    // cells it rolls first
constexpr int kKeyDoorSink = 20;  // frames the key door takes to sink
constexpr int kDrumEgg = 600;     // frames the final hall plays drums
constexpr int kDrumRhythm[4] = {0, 8, 15, 23};
constexpr int kTrapmasterRearm = 45;
constexpr int kWaves = 6, kPerWave = 5, kWaveGap = 150;

int sgn(int v) { return (v > 0) - (v < 0); }

int trapDamage(TrapKind k) { return k == TrapKind::Stone ? 12 : (k == TrapKind::Blade ? 6 : 4); }

// "66..80" -> 66, 80.
bool parseRange(const std::string& s, int& a, int& b)
{
  const auto dots = s.find("..");
  if (dots == std::string::npos)
    return false;
  a = std::atoi(s.substr(0, dots).c_str());
  b = std::atoi(s.substr(dots + 2).c_str());
  if (a > b)
    std::swap(a, b);
  return true;
}

bool walkerLike(EnemyKind k)
{
  switch (k)
  {
    case EnemyKind::Walker:
    case EnemyKind::Guardian:
    case EnemyKind::Bouncer:
    case EnemyKind::Cutter:
    case EnemyKind::Trooper:
    case EnemyKind::Shield:
    case EnemyKind::Keeper:
    case EnemyKind::Looter:
    case EnemyKind::Hunter:
      return true;
    default:
      return false;
  }
}

} // namespace

int World::stoneKeysHeld() const
{
  int n = 0;
  for (const auto& k : mStoneKeys)
    n += k.taken;
  return n;
}

// --- Level entities ----------------------------------------------------------------

bool World::setupTempleEntity(const EntityDef& e)
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  const bool hasRect = e.rect("rect", x0, y0, x1, y1);
  auto plateIndex = [&](const std::string& id) {
    for (std::size_t i = 0; i < mPlates.size(); ++i)
      if (mPlates[i].id == id)
        return int(i);
    if (!id.empty())
      std::fprintf(stderr, "level line %d: no plate %s (plates go before what they fire)\n", e.line, id.c_str());
    return -1;
  };
  if (e.kind == "plate" && e.hasPos)
  {
    Plate p;
    p.id = e.id;
    p.x = e.x * kCellsPerTile;
    p.y = e.y * kCellsPerTile;
    p.w = std::max(1, e.num("w", 1)) * kCellsPerTile;
    mPlates.push_back(p);
    return true;
  }
  if (e.kind == "trap")
  {
    Trap t;
    t.id = e.id;
    const std::string kind = e.str("kind", "stone");
    t.kind = kind == "blade" ? TrapKind::Blade : (kind == "spikes" ? TrapKind::Spikes : TrapKind::Stone);
    t.plate = plateIndex(e.str("plate"));
    t.cycle = e.num("cycle", 0);
    t.phase = e.num("phase", 0);
    t.startX = e.has("start") ? e.num("start") * kCellsPerTile : -1;
    t.startAt = t.startX >= 0 ? -1 : 0;
    if (t.kind == TrapKind::Stone)
    {
      int g0 = 0, g1 = 0;
      if (!parseRange(e.str("groove"), g0, g1))
        return true;
      t.x0 = g0 * kCellsPerTile;
      t.x1 = g1 * kCellsPerTile + 1;
      t.row = e.num("row", 0) * kCellsPerTile + 1;
      t.dir = e.str("dir", "l") == "r" ? 1 : -1;
      t.tell = kStoneTell;
      t.rearm = e.num("rearm", kStoneBack);
      t.sx = t.dir < 0 ? t.x1 - 3 : t.x0;
    }
    else
    {
      if (hasRect)
        t.box = {x0 * kCellsPerTile, y0 * kCellsPerTile, (x1 - x0 + 1) * kCellsPerTile, (y1 - y0 + 1) * kCellsPerTile};
      else if (t.kind == TrapKind::Blade && e.hasPos)
      {
        // A slit in the wall at x: sweeps x-1..x+1 over the three rows above `row`.
        const int row = e.num("row", e.y);
        t.box = {(e.x - 1) * kCellsPerTile, (row - 3) * kCellsPerTile, 3 * kCellsPerTile, 3 * kCellsPerTile};
      }
      else
        return true;
      t.tell = e.num("tell", t.kind == TrapKind::Spikes ? 12 : 10);
      t.active = e.num("active", t.kind == TrapKind::Spikes ? 16 : 6);
      t.rearm = e.num("rearm", t.kind == TrapKind::Spikes ? 12 : 30);
    }
    if (mTrapmaster)
      t.rearm = std::min(t.rearm, kTrapmasterRearm);
    mTraps.push_back(t);
    return true;
  }
  if (e.kind == "collapse" && hasRect)
  {
    for (int ty = y0; ty <= y1; ++ty)
      for (int tx = x0; tx <= x1; ++tx)
        if (mMap.block(tx, ty) == Tile::Solid)
          mCollapse.push_back({tx, ty, 0, 0});
    return true;
  }
  if (e.kind == "stonekey" && e.hasPos)
  {
    mStoneKeys.push_back({e.id, e.x * kCellsPerTile, e.y * kCellsPerTile, false});
    return true;
  }
  if (e.kind == "keydoor" && e.hasPos)
  {
    KeyDoor d;
    d.tx = e.x;
    d.ty = e.y;
    d.h = std::max(1, e.num("h", 3));
    d.keys = std::max(1, e.num("keys", 3));
    for (int ty = d.ty; ty < d.ty + d.h; ++ty)
    {
      mMap.setBlock(d.tx, ty, Tile::Solid);
      if (d.tx >= 0 && ty >= 0 && d.tx < mLevel->width && ty < mLevel->height)
        mLayerMask[std::size_t(ty * mLevel->width + d.tx)] = 1; // drawn as a carved slab
    }
    mKeyDoors.push_back(d);
    return true;
  }
  if ((e.kind == "secret" && hasRect) || e.kind == "wake")
  {
    SecretDoor d;
    d.plate = plateIndex(e.str("plate"));
    d.presses = std::max(1, e.num("presses", 1));
    d.bonus = e.kind == "wake";
    if (!d.bonus)
    {
      d.x0 = x0;
      d.y0 = y0;
      d.x1 = x1;
      d.y1 = y1;
      for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
          mMap.setBlock(tx, ty, Tile::Solid);
    }
    mSecretDoors.push_back(d);
    return true;
  }
  if (e.kind == "drums")
  {
    // The final hall's entry plates, in the order the rhythm steps on them.
    mDrumPlates.clear();
    const std::string list = e.str("plates");
    std::size_t at = 0;
    while (at <= list.size())
    {
      const auto comma = list.find(',', at);
      const std::string id = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
      mDrumPlates.push_back(plateIndex(id));
      if (comma == std::string::npos)
        break;
      at = comma + 1;
    }
    return true;
  }
  if (e.kind == "idol" && e.hasPos)
  {
    mIdolX = e.x * kCellsPerTile;
    Prop pr;
    pr.kind = PropKind::Idol;
    pr.x = e.x * kCellsPerTile - 1;
    pr.y = e.y * kCellsPerTile - 4;
    pr.w = 4;
    pr.h = 6;
    mProps.push_back(pr);
    return true;
  }
  return false;
}

void World::setupTempleEnemy(Enemy& en, const EntityDef& e)
{
  switch (en.kind)
  {
    case EnemyKind::Guardian:
    {
      en.attach = e.num("sentry", 0) != 0 ? 1 : 0;
      int a = 0, b = 0;
      if (parseRange(e.str("patrol"), a, b))
      {
        en.railX0 = a * kCellsPerTile;
        en.railX1 = (b + 1) * kCellsPerTile - en.w;
      }
      else
        en.railX0 = en.railX1 = -1;
      if (e.str("dir") == "l")
        en.dir = -1;
      break;
    }
    case EnemyKind::DartFace:
      en.aimX = e.num("phase", 0);
      en.attach = e.num("silent", 0) != 0 ? 1 : 0;
      en.y = en.prevY = e.y * kCellsPerTile + 1; // hangs from the ceiling above
      break;
    case EnemyKind::Scarabs:
    {
      const int count = std::clamp(e.num("count", 8), 1, 30);
      en.hp = count;
      en.w = std::max(2, count + count / 3);
      en.h = 1;
      en.y = en.prevY = e.y * kCellsPerTile + 1;
      break;
    }
    case EnemyKind::Hunter:
      en.dir = 1;
      break;
    default:
      break;
  }
}

// --- Update ----------------------------------------------------------------------------

void World::updateTemple(const PlayerInput& input)
{
  if (mPlates.empty() && mTraps.empty() && mCollapse.empty() && mStoneKeys.empty() && mKeyDoors.empty())
    return;
  updatePlates();
  updateTraps();
  updateCollapse();
  updateKeys();
  if (mDrumEgg > 0 && --mDrumEgg == 0)
    showMessage("THE TRAPS ARE BACK TO WORK");
  // The bonus patch sleeps until its plate has been pressed enough.
  for (const auto& d : mSecretDoors)
    if (d.bonus)
      for (auto& pr : mProps)
        if (pr.kind == PropKind::BonusDoor)
          pr.dormant = !d.open;
  // The skeleton's note.
  const CellBox pb = mPlayer.box();
  for (auto& pr : mProps)
  {
    if (pr.kind != PropKind::Skeleton)
      continue;
    const CellBox near{pr.x - 3, pr.y - 4, pr.w + 6, pr.h + 6};
    const bool by = near.intersects(pb) && mPlayer.state == PlayerState::OnGround;
    if (by && input.up && pr.hold == 0)
    {
      pr.hold = 1;
      showMessage(characterIndex() == 1 ? "A NOTE: \"SHOULD HAVE PICKED NOVA.\""
                                        : "A NOTE: \"SHOULD HAVE PICKED ROCCO.\"");
    }
    if (!by)
      pr.hold = 0;
  }
}

void World::updatePlates()
{
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  for (std::size_t i = 0; i < mPlates.size(); ++i)
  {
    Plate& pl = mPlates[i];
    const CellBox top{pl.x, pl.y - 1, pl.w, 1};
    bool down = false;
    if (!mTrapmaster && p.state != PlayerState::Dying && p.state != PlayerState::Teleporting &&
        pb.bottom() == pl.y - 1 && top.intersects(pb) && mMap.onSolidGround(pb))
      down = true;
    for (const auto& e : mEnemies)
    {
      if (down || mTrapmaster)
        break; // in Trapmaster only the cursor works the plates
      if (!e.alive || e.kind == EnemyKind::DartFace || e.kind == EnemyKind::Flyer)
        continue;
      const CellBox b = e.box();
      down = b.bottom() == pl.y - 1 && b.intersects(top);
    }
    if (down && !pl.down)
    {
      ++pl.presses;
      pl.pressedAt = mStats.frames;
      playSound(Sfx::Click);
      for (auto& t : mTraps)
        if (t.plate == int(i) && t.state == 0)
          fireTrap(t);
      for (auto& d : mSecretDoors)
        if (d.plate == int(i) && !d.open && pl.presses >= d.presses)
        {
          // Opens with no sound and nothing to see from here.
          d.open = true;
          if (!d.bonus)
            for (int ty = d.y0; ty <= d.y1; ++ty)
              for (int tx = d.x0; tx <= d.x1; ++tx)
                mMap.setBlock(tx, ty, Tile::Empty);
        }
      // The drum egg: the entry plates in the rhythm of the theme's first bar.
      for (std::size_t k = 0; k < mDrumPlates.size(); ++k)
      {
        if (mDrumPlates[k] != int(i))
          continue;
        if (k == 0)
          mDrumTaps.assign(1, mStats.frames);
        else if (mDrumTaps.size() == k)
          mDrumTaps.push_back(mStats.frames);
        else
          mDrumTaps.clear();
        if (mDrumTaps.size() == 4 && mDrumPlates.size() == 4)
        {
          bool beat = true;
          for (int j = 0; j < 4; ++j)
            beat = beat && std::abs(mDrumTaps[std::size_t(j)] - mDrumTaps[0] - kDrumRhythm[j]) <= 3;
          mDrumTaps.clear();
          if (beat)
          {
            mDrumEgg = kDrumEgg;
            playSound(Sfx::Drum);
            showMessage("THE HALL KEEPS THE BEAT");
            addScore(4200, cellCenter(pb));
          }
        }
        break;
      }
    }
    pl.down = down;
  }
}

void World::fireTrap(Trap& t)
{
  t.state = 1;
  t.t = 0;
  t.hit.clear();
  t.hitRunner = false;
  if (t.kind == TrapKind::Stone)
  {
    t.sx = t.dir < 0 ? t.x1 - 3 : t.x0;
    playSound(Sfx::Rumble);
  }
}

void World::updateTraps()
{
  for (auto& t : mTraps)
  {
    if (t.plate < 0 && t.cycle > 0)
    {
      // On a cycle: from the frame it started (when the runner passed its
      // start column, or the level's start).
      if (t.startAt < 0)
      {
        if (mPlayer.x < t.startX)
        {
          t.state = 0;
          continue;
        }
        t.startAt = mStats.frames;
      }
      const int local = (((mStats.frames - t.startAt - t.phase) % t.cycle) + t.cycle) % t.cycle;
      const int state = local < t.tell ? 1 : (local < t.tell + t.active ? 2 : 0);
      if (state == 2 && t.state != 2)
      {
        t.hit.clear();
        t.hitRunner = false;
        if (mDrumEgg > 0 && t.startX >= 0)
          playSound(Sfx::Drum);
        else if (isOnScreen(t.box, 4))
          playSound(t.kind == TrapKind::Blade ? Sfx::Slice : Sfx::Clunk);
      }
      t.state = state;
      t.t = local;
      if (t.state == 2 && !(mDrumEgg > 0 && t.startX >= 0))
        trapHits(t);
      continue;
    }
    switch (t.state)
    {
      case 0:
        break;
      case 1: // telegraph
        if (++t.t >= t.tell)
        {
          t.state = 2;
          t.t = 0;
          if (t.kind == TrapKind::Blade)
            playSound(Sfx::Slice);
          else if (t.kind == TrapKind::Spikes)
            playSound(Sfx::Clunk);
        }
        break;
      case 2: // firing
        if (t.kind == TrapKind::Stone)
        {
          for (int s = 0; s < kStoneSpeed && t.state == 2; ++s)
          {
            const bool end = t.dir < 0 ? t.sx <= t.x0 : t.sx + 3 >= t.x1;
            if (end)
            {
              // Into the catch slot at the end of its groove.
              t.state = 3;
              t.t = 0;
              const Vec2 c = cellCenter(t.stoneBox());
              burst(c, rgb(200, 170, 120), rgb(120, 96, 70), 16, 1.8f, false);
              playSound(Sfx::Land);
              if (isOnScreen(t.stoneBox(), 0))
                mCamera.shake(6, 1.2f);
              break;
            }
            t.sx += t.dir;
            trapHits(t);
          }
          break;
        }
        trapHits(t);
        if (++t.t >= t.active)
        {
          t.state = 3;
          t.t = 0;
        }
        break;
      default: // re-arming
        if (++t.t >= t.rearm)
        {
          t.state = 0;
          t.t = 0;
          if (t.kind == TrapKind::Stone)
            t.sx = t.dir < 0 ? t.x1 - 3 : t.x0; // back in its recess
        }
        break;
    }
  }
}

void World::trapHits(Trap& t)
{
  const CellBox reach = t.reach();
  auto& p = mPlayer;
  if (!mTrapmaster && !t.hitRunner && reach.intersects(p.hitBox()) && p.state != PlayerState::Dying &&
      p.state != PlayerState::Teleporting)
  {
    t.hitRunner = true;
    const int hp = p.hp;
    hurtPlayer(t.kind == TrapKind::Stone ? 2 : 1);
    if (t.kind == TrapKind::Stone && p.hp < hp && p.state != PlayerState::Dying)
    {
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), t.dir * kKnockback);
      if (p.state == PlayerState::OnGround && !mMap.onSolidGround(p.box()))
        startFalling();
    }
  }
  for (auto& e : mEnemies)
  {
    if (!e.alive || !e.box().intersects(reach) || e.kind == EnemyKind::DartFace)
      continue;
    if (std::find(t.hit.begin(), t.hit.end(), e.id) != t.hit.end())
      continue;
    t.hit.push_back(e.id);
    e.active = true;
    if (e.kind == EnemyKind::Scarabs && t.kind == TrapKind::Stone)
      killBeetles(e, e.hp); // a stone takes every beetle it passes
    else
      damageEnemy(e, trapDamage(t.kind));
  }
}

void World::updateCollapse()
{
  const auto& p = mPlayer;
  const CellBox pb = p.box();
  for (auto& c : mCollapse)
  {
    const CellBox tile{c.tx * kCellsPerTile, c.ty * kCellsPerTile, kCellsPerTile, kCellsPerTile};
    switch (c.state)
    {
      case 0:
        if (pb.bottom() + 1 == tile.y && pb.right() >= tile.x && pb.left() <= tile.right() &&
            p.state == PlayerState::OnGround)
        {
          c.state = 1;
          c.t = 0;
          playSound(Sfx::Creak);
        }
        break;
      case 1:
        if (++c.t >= 12)
        {
          c.state = 2;
          c.t = 0;
          mMap.setBlock(c.tx, c.ty, Tile::Empty);
          burst(cellCenter(tile), rgb(210, 180, 120), rgb(140, 110, 70), 8, 1.4f, false);
          playSound(Sfx::BoxBreak);
        }
        break;
      default:
        if (++c.t >= 90)
        {
          // Back, once nothing is in the way.
          bool clear = !tile.intersects(pb);
          for (const auto& e : mEnemies)
            clear = clear && !(e.alive && e.box().intersects(tile));
          if (clear)
          {
            c.state = 0;
            c.t = 0;
            mMap.setBlock(c.tx, c.ty, Tile::Solid);
          }
        }
        break;
    }
  }
}

void World::updateKeys()
{
  const CellBox pb = mPlayer.box();
  for (auto& k : mStoneKeys)
  {
    if (k.taken || !CellBox{k.x, k.y, 2, 2}.intersects(pb))
      continue;
    k.taken = true;
    const Vec2 c{(float(k.x) + 1.0f) * kCellSize, (float(k.y) + 1.0f) * kCellSize};
    burst(c, rgb(255, 220, 120), rgb(200, 150, 80), 18, 2.0f);
    flashAt(c, 90.0f, rgb(255, 210, 120), 14);
    playSound(Sfx::Key);
    addScore(1000, c);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "STONE KEY %d OF %d", stoneKeysHeld(), int(mStoneKeys.size()));
    showMessage(buf);
  }
  for (auto& d : mKeyDoors)
  {
    if (d.open)
      continue;
    if (d.nag > 0)
      --d.nag;
    if (d.sink >= 0)
    {
      if (++d.sink >= kKeyDoorSink)
      {
        d.open = true;
        for (int ty = d.ty; ty < d.ty + d.h; ++ty)
          mMap.setBlock(d.tx, ty, Tile::Empty);
      }
      continue;
    }
    const CellBox near{d.tx * kCellsPerTile - 3, d.ty * kCellsPerTile, kCellsPerTile + 6, d.h * kCellsPerTile};
    if (!near.intersects(pb))
      continue;
    if (stoneKeysHeld() >= d.keys)
    {
      d.sink = 0;
      playSound(Sfx::Rumble);
      mCamera.shake(12, 1.0f);
      showMessage("THE DOOR SINKS INTO THE FLOOR");
    }
    else if (d.nag == 0)
    {
      d.nag = 90;
      char buf[64];
      std::snprintf(buf, sizeof(buf), "THREE GLYPH SLOTS: %d STONE KEY%s SO FAR", stoneKeysHeld(),
        stoneKeysHeld() == 1 ? "" : "S");
      showMessage(buf);
    }
  }
}

void World::resetTemple()
{
  for (auto& t : mTraps)
  {
    if (t.plate < 0)
      continue;
    t.state = 0;
    t.t = 0;
    if (t.kind == TrapKind::Stone)
      t.sx = t.dir < 0 ? t.x1 - 3 : t.x0;
  }
  for (auto& c : mCollapse)
  {
    c.state = 0;
    c.t = 0;
    mMap.setBlock(c.tx, c.ty, Tile::Solid);
  }
}

// --- Enemies ---------------------------------------------------------------------------

void World::updateGuardian(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const auto& p = mPlayer;
  const CellBox pb = p.box(), b = e.box();
  const bool vulnerable = p.state != PlayerState::Dying && p.state != PlayerState::Teleporting;
  const int dx = (pb.x + 1) - (b.x + b.w / 2);
  const bool sameFloor = std::abs(pb.bottom() - b.bottom()) <= 2;
  if (e.dive > 0)
  {
    // The swing: a club's reach in front of it.
    --e.dive;
    const CellBox club{e.dir > 0 ? b.right() + 1 : b.left() - 4, b.y + 1, 4, b.h - 1};
    if (e.dive == 2 && vulnerable && club.intersects(p.hitBox()))
      hurtPlayer(1);
    return;
  }
  if (e.tell > 0)
  {
    if (--e.tell == 0)
    {
      e.dive = 4;
      playSound(Sfx::Slice);
    }
    return;
  }
  if (e.aimX > 0)
    --e.aimX;
  // Turns to face a runner on its floor.
  if (sameFloor && std::abs(dx) <= def.range && vulnerable && dx != 0)
    e.dir = sgn(dx);
  const int gap = e.dir > 0 ? pb.left() - b.right() - 1 : b.left() - pb.right() - 1;
  if (sameFloor && vulnerable && sgn(dx) == e.dir && gap <= 4 && gap >= -2 && e.aimX == 0)
  {
    e.tell = def.tell; // raises the club
    e.aimX = def.cooldown;
    return;
  }
  if (e.attach == 1 || e.timer % std::max(1, def.stepEvery) != 0)
    return; // a sentry stays put
  const int aheadX = e.dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = e.dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  const bool ledge = !mMap.solidTop(aheadX, b.bottom() + 1);
  const bool post = e.railX0 >= 0 && ((e.dir < 0 && e.x <= e.railX0) || (e.dir > 0 && e.x >= e.railX1));
  if (wall || ledge || post)
  {
    if (!(sameFloor && std::abs(dx) <= def.range))
      e.dir = -e.dir;
  }
  else
    e.x += e.dir;
}

void World::updateDartFace(Enemy& e, const EnemyDef& def)
{
  if (e.attach == 1)
    return; // the one that never fires (it holds the camera)
  const int cd = std::max(1, def.cooldown);
  const int t = ((mStats.frames - e.aimX) % cd + cd) % cd;
  e.tell = t >= cd - def.tell ? cd - t : 0;
  if (t == 0 && isOnScreen(e.box(), 4))
  {
    spawnProjectile(ShotKind::Enemy, e.x, e.y + 1, 0, 1);
    playSound(Sfx::Dart);
  }
}

void World::updateScarabs(Enemy& e, const EnemyDef& def)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 1);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return;
  }
  const CellBox pb = mPlayer.box(), b = e.box();
  if (e.timer % 30 == 0 && isOnScreen(b, 0))
    playSound(Sfx::Skitter);
  if (e.timer % std::max(1, def.stepEvery) != 0)
    return;
  // Flows toward the runner along its floor; stops at walls and edges.
  const int target = pb.x + 1 - b.w / 2;
  const int dir = sgn(target - e.x);
  if (dir == 0 || std::abs(pb.bottom() - b.bottom()) > 24)
    return;
  e.dir = dir;
  const int aheadX = dir > 0 ? b.right() + 1 : b.left() - 1;
  const bool wall = dir > 0 ? mMap.touchingRightWall(b) : mMap.touchingLeftWall(b);
  if (!wall && mMap.solidTop(aheadX, b.bottom() + 1))
    e.x += dir;
}

void World::killBeetles(Enemy& e, int n)
{
  n = std::min(n, e.hp);
  if (n <= 0 || !e.alive)
    return;
  e.hp -= n;
  e.flash = 6;
  const Vec2 c = cellCenter(e.box());
  addScore(enemyDef(e.def).score * n, c);
  burst(c, e.carrier ? rgb(130, 255, 80) : rgb(60, 140, 120), rgb(20, 30, 30), 4 + n * 2, 1.4f, false);
  playSound(Sfx::Hit);
  if (e.hp <= 0)
  {
    e.alive = false;
    ++mStats.kills;
    playSound(Sfx::SmallExplosion);
    return;
  }
  // The carpet shrinks toward its middle.
  const int w = std::max(2, e.hp + e.hp / 3);
  e.x += (e.w - w) / 2;
  e.w = w;
}

void World::tangle(Enemy& e, int dir)
{
  e.tangle = kTangleFrames;
  e.tangleRoll = (dir == 0 ? 1 : dir) * kTangleRoll;
  e.tell = 0;
  e.dive = 0;
  e.flash = 4;
  playSound(Sfx::Land);
  burst(cellCenter(e.box()), rgb(170, 130, 80), rgb(110, 80, 50), 8, 1.2f, false);
}

bool World::tangled(Enemy& e)
{
  if (!mMap.onSolidGround(e.box()))
  {
    mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
    if (e.y > mMap.height() + 4)
      e.alive = false;
    return true;
  }
  if (e.tangleRoll != 0)
  {
    // Rolls a cell a frame, the way the cords pulled it.
    const int dir = sgn(e.tangleRoll);
    if (mMap.moveHorizontally(e.x, e.y, e.w, e.h, dir) != MoveResult::Completed)
      e.tangleRoll = 0;
    else
      e.tangleRoll -= dir;
    return true;
  }
  --e.tangle;
  return true;
}

bool World::shotAtTemple(Projectile& pr, Enemy& e)
{
  const int dir = pr.precise ? (pr.vx < 0.0f ? -1 : 1) : pr.dx;
  if (pr.kind == ShotKind::Proto && pr.proto == int(ProtoId::SnareBolas))
  {
    if (e.kind == EnemyKind::Scarabs)
      killBeetles(e, 3);
    else
    {
      if (walkerLike(e.kind) && e.tangle == 0)
        tangle(e, dir);
      damageEnemy(e, pr.damage);
    }
    return true;
  }
  if (e.kind == EnemyKind::Scarabs)
  {
    killBeetles(e, 3); // a shot takes up to three in a line
    return !pr.pierce;
  }
  if (e.kind == EnemyKind::Guardian && e.tangle == 0 && mPlayer.turbo == 0 && pr.dy <= 0 && dir == -e.dir)
  {
    // Sparks off its face: hit it from behind, or with a trap.
    e.flash = 3;
    burst(cellCenter(pr.box()), rgb(255, 240, 200), rgb(200, 180, 140), 6, 1.4f);
    playSound(Sfx::Land);
    return true;
  }
  return false;
}

// --- Trapmaster -----------------------------------------------------------------------

void World::updateTrapmaster(const PlayerInput& input)
{
  auto& p = mPlayer;
  p.hidden = true;
  p.mercy = 2;
  // Up and down move the glyph cursor along the plates, fire works the trap.
  const int n = int(mPlates.size());
  if (n > 0)
  {
    const bool up = input.up, down = input.down;
    if (up && !mCursor.up)
      mCursor.at = (mCursor.at + n - 1) % n;
    if (down && !mCursor.down)
      mCursor.at = (mCursor.at + 1) % n;
    mCursor.up = up;
    mCursor.down = down;
    if (up || down)
    {
      // The camera follows the cursor to a plate out of view.
      const Plate& pl = mPlates[std::size_t(mCursor.at)];
      const int view = int(kViewCellsW);
      if (pl.x < mCursor.pan + 8 || pl.x > mCursor.pan + view - 8)
        mCursor.pan = pl.x - view / 2;
    }
    if (input.fire.triggered)
    {
      bool fired = false;
      for (auto& t : mTraps)
        if (t.plate == mCursor.at && t.state == 0)
        {
          fireTrap(t);
          fired = true;
        }
      if (fired)
      {
        ++mPlates[std::size_t(mCursor.at)].presses;
        playSound(Sfx::Click);
      }
    }
  }
  // Left and right pan the camera over the gallery.
  const int mvX = (input.right ? 1 : 0) - (input.left ? 1 : 0);
  const int maxPan = std::max(0, mMap.width() - int(kViewCellsW));
  mCursor.pan = std::clamp(mCursor.pan + mvX * 2, 0, maxPan);
  // Keep the camera on the player box: park it where the camera should look,
  p.x = p.prevX = mCursor.pan + int(kViewCellsW) / 2;
  // and on the cursor's floor.
  p.y = p.prevY = n > 0 ? mPlates[std::size_t(mCursor.at)].y - 1 : 2 + Player::kHeight;
  // The hunters: six waves of five, in on the left, alternately on the two floors.
  if (mWave < kWaves && mStats.frames >= mWaveNext)
  {
    const int def = enemyIndex("treasure_hunter");
    const bool upper = mWave % 2 == 0;
    int floorY = -1;
    for (const auto& pl : mPlates)
      floorY = std::max(floorY, pl.y);
    for (int i = 0; i < kPerWave && def >= 0; ++i)
    {
      // Ground rows: the upper floor is ground 10, the lower ground 22.
      const int gy = (upper ? 10 : 22) * kCellsPerTile - 1;
      spawnEnemy(def, 2 - i * 8, gy);
      Enemy& h = mEnemies.back();
      h.dir = 1;
      h.active = true;
      h.timer = i;
    }
    ++mWave;
    mWaveNext = mStats.frames + kWaveGap;
  }
  for (auto& e : mEnemies)
  {
    if (!e.alive || e.kind != EnemyKind::Hunter || e.tangle > 0)
      continue;
    if (!mMap.onSolidGround(e.box()) && e.x >= 0)
    {
      mMap.moveVertically(e.x, e.y, e.w, e.h, 2);
      continue;
    }
    if (e.timer % 2 == 0)
    {
      if (e.x < 0 || mMap.moveHorizontally(e.x, e.y, e.w, e.h, 1) != MoveResult::Completed)
        e.x += e.x < 0 ? 1 : 0;
    }
    // At the idol: one coin, and off they go (nothing is lost).
    if (mIdolX >= 0 && e.x + e.w >= mIdolX && e.y > mMap.height() / 2)
    {
      e.alive = false;
      burst(cellCenter(e.box()), rgb(255, 220, 80), rgb(255, 255, 200), 6, 1.2f);
      playSound(Sfx::Gem);
    }
  }
}

// --- Drawing --------------------------------------------------------------------------

void World::drawTempleProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame,
  bool foreground) const
{
  if (foreground)
    return;
  switch (pr.kind)
  {
    case PropKind::Skeleton:
    {
      // Slumped against the wall, hat and all.
      const Color bone = rgb(236, 226, 196);
      const float bx = x + w * 0.5f, by = y + h;
      r.fillRect(bx - 22.0f, by - 10.0f, 44.0f, 8.0f, bone); // legs
      r.fillRect(bx - 8.0f, by - 44.0f, 16.0f, 36.0f, bone);
      for (int k = 0; k < 3; ++k)
        r.fillRect(bx - 16.0f, by - 40.0f + float(k) * 9.0f, 32.0f, 4.0f, bone); // ribs
      r.fillRect(bx - 12.0f, by - 66.0f, 24.0f, 22.0f, bone);
      r.fillRect(bx - 7.0f, by - 59.0f, 5.0f, 5.0f, kInk);
      r.fillRect(bx + 2.0f, by - 59.0f, 5.0f, 5.0f, kInk);
      r.fillRect(bx - 22.0f, by - 70.0f, 44.0f, 6.0f, rgb(150, 110, 60)); // the explorer's hat
      r.fillRect(bx - 12.0f, by - 82.0f, 24.0f, 14.0f, rgb(150, 110, 60));
      r.fillRect(bx + 18.0f, by - 16.0f, 16.0f, 12.0f, rgb(250, 246, 230)); // the note
      if (pr.hold == 0 && (frame / 20) % 2 == 0)
        drawGlow(r, mArt, bx + 26.0f, by - 10.0f, 18.0f, rgb(255, 240, 200), 0.5f);
      break;
    }
    case PropKind::Glyph:
      drawGlow(r, mArt, x + w * 0.5f, y + h * 0.5f, 48.0f, rgb(255, 60, 40), 0.25f);
      r.drawText(pr.text, x + w * 0.5f, y + h * 0.5f - 18.0f, {32.0f, rgb(255, 90, 60), kInk, true}, Align::Center);
      break;
    case PropKind::Torch:
    {
      const float cx = x + w * 0.5f;
      r.fillRect(cx - 4.0f, y + h * 0.4f, 8.0f, h * 0.6f, rgb(90, 64, 40));
      const float flick = 0.8f + 0.2f * std::sin(float(frame) * 0.9f + x * 0.01f);
      drawGlow(r, mArt, cx, y + h * 0.3f, 90.0f, rgb(255, 160, 60), 0.45f * flick);
      r.fillRect(cx - 7.0f, y + h * 0.15f, 14.0f, 20.0f * flick, rgb(255, 190, 70));
      r.fillRect(cx - 4.0f, y + h * 0.22f, 8.0f, 12.0f * flick, rgb(255, 245, 190));
      break;
    }
    case PropKind::Idol:
    {
      // A squat golden idol on a plinth.
      const float cx = x + w * 0.5f;
      drawGlow(r, mArt, cx, y + h * 0.4f, 90.0f, rgb(255, 210, 80), 0.35f);
      r.fillRect(cx - 30.0f, y + h - 20.0f, 60.0f, 20.0f, rgb(150, 120, 80));
      r.fillRect(cx - 18.0f, y + h * 0.35f, 36.0f, h * 0.65f - 20.0f, rgb(230, 180, 60));
      r.fillRect(cx - 22.0f, y + h * 0.12f, 44.0f, h * 0.26f, rgb(240, 196, 70));
      r.fillRect(cx - 12.0f, y + h * 0.2f, 7.0f, 7.0f, rgb(220, 30, 40));
      r.fillRect(cx + 5.0f, y + h * 0.2f, 7.0f, 7.0f, rgb(220, 30, 40));
      break;
    }
    default:
      break;
  }
}

void World::drawTempleBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)alpha;
  if (mPlates.empty() && mTraps.empty() && mCollapse.empty() && mStoneKeys.empty() && mKeyDoors.empty())
    return;
  const float viewW = float(kScreenW), viewH = float(kScreenH);
  auto onView = [&](float x, float y, float w, float h) {
    return x + w > -32.0f && x < viewW + 32.0f && y + h > -32.0f && y < viewH + 32.0f;
  };
  const CellBox pb = mPlayer.box();
  auto near = [&](const CellBox& b) {
    if (mTrapmaster)
      return true;
    const CellBox n{b.x - kGlowReach, b.y - kGlowReach, b.w + kGlowReach * 2, b.h + kGlowReach * 2};
    return n.intersects(pb);
  };
  const Color red = rgb(255, 50, 30);

  // Trap paths, glowing faint red when the runner is close.
  for (const auto& t : mTraps)
  {
    if (t.kind == TrapKind::Stone)
    {
      const float gx = float(t.x0) * kCellPx - camX, gw = float(t.x1 - t.x0 + 1) * kCellPx;
      const float gy = float(t.row + 1) * kCellPx - camY;
      if (!onView(gx, gy - 70.0f, gw, 80.0f))
        continue;
      r.fillRect(gx, gy - 4.0f, gw, 4.0f, rgba(40, 26, 18, 160)); // the groove
      if (near({t.x0, t.row - 3, t.x1 - t.x0 + 1, 4}))
        r.fillRect(gx, gy - 4.0f, gw, 4.0f, withAlpha(red, 60 + 30 * ((frame / 8) % 2)), Blend::Add);
      // The recess at its head.
      const float hx = float(t.dir < 0 ? t.x1 - 3 : t.x0) * kCellPx - camX;
      r.fillRect(hx - 4.0f, gy - 4.0f * kCellPx - 4.0f, 4.0f * kCellPx + 8.0f, 4.0f * kCellPx, rgba(20, 12, 8, 120));
      if (t.state == 3)
        continue; // in its catch slot
      float sx = float(t.sx) * kCellPx - camX;
      float sy = float(t.row - 3) * kCellPx - camY;
      if (t.state == 1)
      {
        sx += float((frame % 3) - 1) * 3.0f; // the rumble
        for (int k = 0; k < 3; ++k)
          r.fillRect(hx + float((frame * 7 + k * 23) % 60), gy - 70.0f + float((frame * 3 + k * 17) % 50), 4.0f, 4.0f,
            rgba(220, 200, 160, 160)); // dust
      }
      const float d = 4.0f * kCellPx;
      r.fillRect(sx + 4.0f, sy + 4.0f, d - 8.0f, d - 8.0f, rgb(150, 132, 104));
      r.fillRect(sx, sy + 12.0f, d, d - 24.0f, rgb(150, 132, 104));
      r.fillRect(sx + 12.0f, sy, d - 24.0f, d, rgb(150, 132, 104));
      r.fillRect(sx + 10.0f, sy + 8.0f, d - 30.0f, 8.0f, rgb(186, 170, 140));
      // A glyph that turns as it rolls.
      const float a = float(t.sx) * 0.35f;
      const float cx = sx + d * 0.5f, cy = sy + d * 0.5f;
      r.drawLine(cx - std::cos(a) * 16.0f, cy - std::sin(a) * 16.0f, cx + std::cos(a) * 16.0f, cy + std::sin(a) * 16.0f,
        5.0f, rgb(110, 40, 30));
      r.drawLine(cx - std::sin(a) * 10.0f, cy + std::cos(a) * 10.0f, cx + std::sin(a) * 10.0f, cy - std::cos(a) * 10.0f,
        5.0f, rgb(110, 40, 30));
      continue;
    }
    const float bx = float(t.box.x) * kCellPx - camX, by = float(t.box.y) * kCellPx - camY;
    const float bw = float(t.box.w) * kCellPx, bh = float(t.box.h) * kCellPx;
    if (!onView(bx, by, bw, bh))
      continue;
    const bool drums = mDrumEgg > 0 && t.startX >= 0;
    if (t.kind == TrapKind::Blade)
    {
      // The slit in the wall at its middle, and the blade's sweep.
      const float sx = bx + bw * 0.5f;
      r.fillRect(sx - 4.0f, by, 8.0f, bh, rgba(20, 10, 8, 180));
      if (near(t.box))
        r.fillRect(sx - 4.0f, by, 8.0f, bh, withAlpha(red, 50), Blend::Add);
      if (t.state == 1)
        r.fillRect(sx - 6.0f, by, 12.0f, bh, withAlpha(drums ? rgb(255, 200, 60) : red, 120 + 100 * ((frame / 2) % 2)),
          Blend::Add);
      if (t.state == 2 && !drums)
      {
        const float u = float(t.t) / float(std::max(1, t.active));
        const float a = -1.2f + 2.4f * u;
        const float px = sx, py = by + bh * 0.5f;
        const float len = bw * 0.55f;
        const float ex = px + std::sin(a) * len, ey = py - std::cos(a) * len * 0.3f;
        r.drawLine(px, py, ex, ey, 14.0f, rgb(210, 216, 226));
        r.drawLine(px, py, ex, ey, 4.0f, rgb(255, 255, 255));
        r.drawLine(px, py - 10.0f, px - std::sin(a) * len, py + 10.0f, 10.0f, rgba(210, 216, 226, 120));
      }
      if (t.state == 2 && drums)
        drawGlow(r, mArt, sx, by + bh * 0.5f, 60.0f, rgb(255, 200, 60), 0.6f);
      continue;
    }
    // Spikes: a strip along the floor; tips peek, then up.
    if (near(t.box))
      r.fillRect(bx, by + bh - 6.0f, bw, 6.0f, withAlpha(red, 50), Blend::Add);
    const float rise = t.state == 1 ? 0.25f : (t.state == 2 ? (drums ? 0.4f : 1.0f) : 0.0f);
    if (rise <= 0.0f)
      continue;
    const float tipH = bh * rise;
    for (float sx = bx; sx < bx + bw - 2.0f; sx += 16.0f)
    {
      const float top = by + bh - tipH;
      r.drawLine(sx + 2.0f, by + bh, sx + 8.0f, top, 5.0f, rgb(200, 204, 214));
      r.drawLine(sx + 14.0f, by + bh, sx + 8.0f, top, 5.0f, rgb(150, 154, 166));
    }
  }

  // Plates: red glyphs set into the floor, sunk while pressed.
  for (const auto& pl : mPlates)
  {
    const float x = float(pl.x) * kCellPx - camX, y = float(pl.y) * kCellPx - camY;
    const float w = float(pl.w) * kCellPx;
    if (!onView(x, y - 10.0f, w, 20.0f))
      continue;
    const float sink = pl.down ? 3.0f : 0.0f;
    r.fillRect(x + 2.0f, y - 3.0f + sink, w - 4.0f, 6.0f, rgb(120, 92, 70));
    r.fillRect(x + w * 0.5f - 7.0f, y - 2.0f + sink, 14.0f, 3.0f, rgb(230, 50, 40));
    r.fillRect(x + w * 0.5f - 2.0f, y - 6.0f + sink, 4.0f, 8.0f, rgb(230, 50, 40));
    if (mTrapmaster && int(&pl - mPlates.data()) == mCursor.at)
    {
      drawGlow(r, mArt, x + w * 0.5f, y - 20.0f, 60.0f, rgb(255, 210, 80), 0.7f + 0.2f * float((frame / 6) % 2));
      r.drawText("V", x + w * 0.5f, y - 60.0f, {30.0f, rgb(255, 230, 120), kInk, true}, Align::Center);
    }
  }

  // Cracked floor tiles.
  for (const auto& c : mCollapse)
  {
    if (c.state != 1)
      continue;
    const float x = float(c.tx * kCellsPerTile) * kCellPx - camX, y = float(c.ty * kCellsPerTile) * kCellPx - camY;
    const float s = 2.0f * kCellPx;
    const float shake = float((frame % 2) * 2 - 1) * 1.5f;
    r.drawLine(x + 6.0f + shake, y + 2.0f, x + s * 0.5f, y + s * 0.6f, 3.0f, rgb(40, 26, 18));
    r.drawLine(x + s * 0.5f, y + s * 0.6f, x + s - 4.0f + shake, y + 4.0f, 3.0f, rgb(40, 26, 18));
    r.drawLine(x + s * 0.5f, y + s * 0.6f, x + s * 0.4f, y + s, 3.0f, rgb(40, 26, 18));
  }

  // The key door: a carved slab with three glyph slots, sinking.
  for (const auto& d : mKeyDoors)
  {
    if (d.open)
      continue;
    const float x = float(d.tx * kCellsPerTile) * kCellPx - camX, y = float(d.ty * kCellsPerTile) * kCellPx - camY;
    const float w = 2.0f * kCellPx, h = float(d.h * kCellsPerTile) * kCellPx;
    if (!onView(x, y, w, h))
      continue;
    const float sunk = d.sink < 0 ? 0.0f : h * float(d.sink) / float(kKeyDoorSink);
    r.fillRect(x, y + sunk, w, h - sunk, rgb(132, 108, 80));
    r.fillRect(x + 4.0f, y + sunk + 4.0f, w - 8.0f, 6.0f, rgb(176, 150, 112));
    for (int k = 0; k < d.keys; ++k)
    {
      const float ky = y + sunk + 22.0f + float(k) * (h - 40.0f) / float(std::max(1, d.keys));
      if (ky > y + h - 10.0f)
        continue;
      const bool lit = k < stoneKeysHeld();
      r.fillRect(x + 8.0f, ky, w - 16.0f, 16.0f, lit ? rgb(255, 200, 80) : rgb(70, 52, 38));
    }
  }

  // Stone keys: tablets hovering over their spots.
  for (const auto& k : mStoneKeys)
  {
    if (k.taken)
      continue;
    const float bob = std::sin(float(frame) * 0.12f + float(k.x)) * 4.0f;
    const float x = float(k.x) * kCellPx - camX, y = float(k.y) * kCellPx - camY + bob;
    if (!onView(x, y, 32.0f, 32.0f))
      continue;
    drawGlow(r, mArt, x + 16.0f, y + 16.0f, 44.0f, rgb(255, 210, 120), 0.5f);
    r.fillRect(x + 4.0f, y + 2.0f, 24.0f, 28.0f, rgb(186, 160, 116));
    r.fillRect(x + 8.0f, y + 8.0f, 16.0f, 4.0f, rgb(220, 60, 40));
    r.fillRect(x + 14.0f, y + 8.0f, 4.0f, 16.0f, rgb(220, 60, 40));
  }
}

void World::drawTempleFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)r;
  (void)camX;
  (void)camY;
  (void)frame;
  (void)alpha;
}

void World::drawTempleHud(Renderer& r, float x, float top) const
{
  // The stone keys, in the inventory's second slot.
  if (mStoneKeys.empty())
    return;
  for (std::size_t i = 0; i < mStoneKeys.size() && i < 3; ++i)
  {
    const float kx = x + 4.0f + float(i) * 13.0f;
    const bool have = mStoneKeys[i].taken;
    r.fillRect(kx, top + 26.0f, 11.0f, 26.0f, have ? rgb(206, 176, 120) : rgba(255, 255, 255, 30));
    if (have)
      r.fillRect(kx + 3.0f, top + 32.0f, 5.0f, 5.0f, rgb(220, 60, 40));
  }
}

} // namespace gr
