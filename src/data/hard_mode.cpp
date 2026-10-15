#include "data/hard_mode.hpp"

#include "data/enemies.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gr
{

namespace
{

using K = EnemyKind;

// Kinds that fight on their own where they stand (or swim): no rail, wire,
// valve, lane, script or boss to belong to. A twin of one of these is
// simply one more of them.
bool standsAlone(K k)
{
  switch (k)
  {
    case K::Walker:
    case K::Bouncer:
    case K::Shield:
    case K::Guardian:
    case K::Monk:
    case K::Crab:
    case K::Skitter:
    case K::Gloop:
    case K::ThornHog:
    case K::SpearRunner:
    case K::Stalker:
    case K::Imp:
    case K::HauntedArmor:
    case K::EggTimer:
    case K::JumpingSpider:
    case K::BombardierBeetle:
    case K::RubberGoon:
    case K::GrinningBomb:
    case K::GhostImage:
    case K::ScorpionMech:
    case K::Stagehand:
    case K::Mimic:
    case K::Mannequin:
    case K::SketchGuard:
    case K::WarmupComic:
    case K::CableWrangler:
    case K::BoomOp:
    case K::Glob:
    case K::ShardGolem:
    case K::Blinker:
    case K::Raver:
    case K::Turret:
    case K::Sniper:
    case K::Spitpod:
    case K::Howler:
    case K::CleaverBot:
    case K::AudiencePlant:
    case K::Rigger:
    case K::Mole:
    case K::Biker:
    case K::Puck:
    case K::SleeperPod:
    case K::Barnacle:
    case K::FlipWalker:
    case K::MaserTruck:
    case K::Crow:
    case K::Anglerfish:
    case K::Foghorn:
    case K::MastClimber:
    case K::CocoonPod:
    case K::Viper:
      return true;
    default:
      return false;
  }
}

// Kinds that hover or swim: their twin goes in the same stretch of open
// space (and the same water) instead of on a floor.
bool flies(K k)
{
  switch (k)
  {
    case K::Flyer:
    case K::Wisp:
    case K::VoidRay:
    case K::PrismBat:
    case K::Wraith:
    case K::EvaRam:
    case K::Probe:
    case K::PowderPuffer:
    case K::Disco:
    case K::Warden:
    case K::RailDrone:
    case K::Gator:
    case K::Fish:
    case K::Jelly:
    case K::SeaMine:
      return true;
    default:
      return false;
  }
}

// Entity keys a twin can be made from; anything else ties the enemy to
// something in the level (a rail, a valve, a path...), so it gets no twin.
bool plainKey(const std::string& key)
{
  static const std::set<std::string> kPlain = {"dir", "carrier", "look", "hat", "patrol", "sentry", "side"};
  return kPlain.count(key) != 0;
}

// What a twin keeps: not the Virus (carrier), an easter egg (hat) or a
// patrol span (blocks of the original's floor).
bool copiedKey(const std::string& key) { return key == "dir" || key == "look" || key == "sentry" || key == "side"; }

// Rect entities anyone may stand in: looks and lighting, nothing that acts.
bool harmlessRect(const std::string& kind)
{
  static const std::set<std::string> kHarmless = {
    "deco", "dark", "billboard", "narrate", "rain", "manordeco", "sector", "unlit", "shadow", "cover"};
  return kHarmless.count(kind) != 0;
}

// Placed entities anyone may stand next to (the bot's route notes, looks).
bool harmlessSpot(const std::string& kind)
{
  static const std::set<std::string> kHarmless = {
    "deco", "step", "narrate", "cue", "cuelist", "botroute", "route", "light", "lamp", "sign", "note", "crowd", "duck"};
  return kHarmless.count(kind) != 0;
}

bool standable(Tile t) { return t == Tile::Solid || t == Tile::Platform || t == Tile::Grate; }
bool walkable(Tile t) { return t == Tile::Empty || t == Tile::Ladder || t == Tile::Pipe; }

bool debug() { return std::getenv("GR_HARD_DEBUG") != nullptr; }

struct Spot
{
  int x, y;
};

struct Rect
{
  int x0, y0, x1, y1;
  bool ambient = false;
  bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

// An enemy's blocks: x .. x + w - 1, top .. y (it stands on the bottom row
// of block y, w and h are in cells).
struct Body
{
  int w, top, h;
  explicit Body(const EnemyDef& d, int y)
    : w((d.w + 1) / 2)
    , top((2 * y + 2 - d.h) >> 1)
    , h(y - ((2 * y + 2 - d.h) >> 1) + 1)
  {
  }
};

class Placer
{
public:
  explicit Placer(const Level& lv)
    : mLv(lv)
  {
    for (const auto& s : lv.spawns)
    {
      if (s.kind == 'c')
        mCheckpoints.push_back({s.tx, s.ty});
      else if (s.kind == 'B')
        mDoors.push_back({s.tx, s.ty});
      else if (s.kind == 'g')
        mGems.push_back({s.tx, s.ty});
    }
    for (const auto& e : lv.entities)
    {
      int x0, y0, x1, y1;
      const bool enemy = enemyIndex(e.kind) >= 0;
      if (e.kind == "respawn" && e.hasPos)
        mCheckpoints.push_back({e.x, e.y});
      if (e.rect("rect", x0, y0, x1, y1))
      {
        // A zone over a quarter of the level or more (Hangar Bay's vents)
        // is the level itself, not a set piece in it.
        const bool ambient = 4 * (x1 - x0 + 1) * (y1 - y0 + 1) >= lv.width * lv.height;
        if (!harmlessRect(e.kind))
          mRects.push_back({x0, y0, x1, y1, ambient});
      }
      else if (e.hasPos && !enemy && !harmlessSpot(e.kind))
      {
        // Doors and hatches have a size; everything else, a little room.
        const int w = std::max(1, e.num("w", 1)), h = std::max(1, e.num("h", 1));
        mSpots.push_back({e.x - 2, e.y - 3, e.x + w + 1, e.y + h + 1});
      }
      // Platforms, cables and other things that move along a path.
      for (const auto& p : e.path("path"))
        mSpots.push_back({p.first - 2, p.second - 3, p.first + 2, p.second + 2});
      if (enemy && e.hasPos)
        mEnemies.push_back({e.x, e.y});
    }
  }

  // A twin for this enemy, or false where none fits.
  bool twin(const EntityDef& e, const EnemyDef& d, Spot& out)
  {
    const Body b(d, e.y);
    const bool grounded = groundedAny(e.x, e.y, b.w);
    const bool ceiling = ceilingAll(e.x, b.top, b.w);
    if (!grounded && !flies(d.kind))
    {
      if (debug())
        std::fprintf(stderr, "  %s %d,%d: not on the ground\n", e.kind.c_str(), e.x, e.y);
      return false; // a walker on a moving platform, a ledge the map doesn't show
    }
    // The set pieces it stands in (its sludge, its dark room, its gravity
    // chamber): its twin stands in the same ones and in no others.
    std::vector<int> home;
    for (std::size_t i = 0; i < mRects.size(); ++i)
      if (mRects[i].contains(e.x, e.y))
        home.push_back(int(i));
    // Further into the level first: the start is where the runner comes from.
    const int away = e.x >= mLv.startTx ? 1 : -1;
    int why[4] = {0, 0, 0, 0};
    for (int dist = 5; dist <= 16; ++dist)
      for (int side : {away, -away})
      {
        const int nx = e.x + side * dist;
        if (!open(nx, e.y, b, grounded))
        {
          ++why[0];
          continue;
        }
        if (!inside(nx, e.y, b, home))
        {
          ++why[1];
          continue;
        }
        if (!clear(nx, e.y, 7))
        {
          ++why[2];
          continue;
        }
        if ((grounded ? !walkway(e.x, nx, e.y, b) : !airway(e.x, nx, b)) || ceiling != ceilingAll(nx, b.top, b.w))
        {
          ++why[3];
          continue;
        }
        out = {nx, e.y};
        mEnemies.push_back(out);
        return true;
      }
    if (debug())
      std::fprintf(stderr, "  %s %d,%d: tiles %d, set pieces %d, start/exit/enemies %d, path %d\n", e.kind.c_str(), e.x,
        e.y, why[0], why[1], why[2], why[3]);
    return false;
  }

  // A floor for a new enemy of this size where the level has none for a
  // good stretch: open, with room to patrol and for a runner to walk, on
  // the way the gems lead and in no set piece at all. A flyer hovers three
  // blocks over such a floor. Not `strict`: seven blocks of floor will do,
  // gems or not, a little closer to the other enemies.
  bool guest(const EnemyDef& d, bool strict, Spot& out)
  {
    const int run = strict ? 4 : 3, gap = strict ? 9 : 7;
    const int lift = flies(d.kind) ? 3 : 0;
    int best = -1;
    int why[5] = {0, 0, 0, 0, 0};
    for (int y = 2; y < mLv.height - 2; ++y)
      for (int x = 1; x < mLv.width - 1; ++x)
      {
        const Body b(d, y);
        const Body air(d, y - lift);
        if (!open(x, y, b, true) || !groundedAll(x, y, b.w))
          continue;
        if (!inside(x, y, b, {}))
        {
          ++why[0];
          continue;
        }
        if (lift > 0 && (!open(x, y - lift, air, false) || !inside(x, y - lift, air, {})))
          continue;
        if (strict && !mGems.empty() && std::none_of(mGems.begin(), mGems.end(), [&](const Spot& g) { return near(g, x, y, 12, 5); }))
        {
          ++why[1];
          continue;
        }
        // Nine blocks of floor with a runner's headroom (3 blocks) over it.
        bool room = true;
        for (int c = x - run; room && c < x + b.w + run; ++c)
        {
          room = standable(mLv.at(c, y + 1));
          for (int r = y - 2; room && r <= y; ++r)
            room = mLv.at(c, r) == Tile::Empty;
        }
        if (!room || !clear(x, y, gap) || !clear(x, y - lift, gap))
        {
          ++why[room ? 3 : 2];
          continue;
        }
        int nearest = 1 << 20;
        for (const auto& o : mEnemies)
          nearest = std::min(nearest, std::abs(o.x - x) + 2 * std::abs(o.y - y));
        if (nearest > best)
        {
          best = nearest;
          out = {x, y - lift};
        }
      }
    if (best < 0 && debug())
      std::fprintf(stderr, "  guest %s: set pieces %d, no gems %d, no room %d, start/exit/enemies %d\n", d.key, why[0],
        why[1], why[2], why[3]);
    if (best < 0)
      return false;
    mEnemies.push_back(out);
    return true;
  }

private:
  bool groundedAny(int x, int y, int w) const
  {
    for (int c = x; c < x + w; ++c)
      if (standable(mLv.at(c, y + 1)))
        return true;
    return false;
  }
  bool groundedAll(int x, int y, int w) const
  {
    for (int c = x; c < x + w; ++c)
      if (!standable(mLv.at(c, y + 1)))
        return false;
    return true;
  }
  bool ceilingAll(int x, int top, int w) const
  {
    for (int c = x; c < x + w; ++c)
      if (!mLv.isSolid(c, top - 1))
        return false;
    return true;
  }
  static bool near(const Spot& a, int x, int y, int dx, int dy) { return std::abs(a.x - x) < dx && std::abs(a.y - y) < dy; }

  // The enemy's own blocks are open (a one-way platform above its feet is
  // fine: it walks under it with its head through) and inside the map; on a
  // floor, the whole width of it stands on something.
  bool open(int x, int y, const Body& b, bool grounded) const
  {
    if (x < 1 || x + b.w > mLv.width - 1 || b.top < 0)
      return false;
    for (int r = b.top; r <= y; ++r)
      for (int c = x; c < x + b.w; ++c)
      {
        const Tile t = mLv.at(c, r);
        if (t != Tile::Empty && !(t == Tile::Platform && r < y))
          return false;
      }
    return !grounded || groundedAll(x, y, b.w);
  }
  // Inside every set piece in `home`, and clear (by a block) of all others.
  bool inside(int x, int y, const Body& b, const std::vector<int>& home) const
  {
    for (std::size_t i = 0; i < mRects.size(); ++i)
    {
      const Rect& r = mRects[i];
      const bool mine = std::find(home.begin(), home.end(), int(i)) != home.end();
      if (r.ambient && !mine)
        continue;
      if (mine)
      {
        if (!r.contains(x, y) || !r.contains(x + b.w - 1, b.top))
          return false;
      }
      else if (x + b.w - 1 >= r.x0 - 1 && x <= r.x1 + 1 && y >= r.y0 - 1 && b.top <= r.y1 + 1)
        return false;
    }
    for (const Rect& r : mSpots)
      if (x + b.w - 1 >= r.x0 && x <= r.x1 && y >= r.y0 && b.top <= r.y1)
        return false;
    return true;
  }
  // Away from the start, the checkpoints, the exit, the bonus door and
  // (by `gap` blocks) the other enemies.
  bool clear(int x, int y, int gap) const
  {
    if (near({mLv.startTx, mLv.startTy}, x, y, 16, 10))
      return false;
    if (mLv.hasExit && near({mLv.exitTx, mLv.exitTy}, x, y, 6, 6))
      return false;
    for (const auto& c : mCheckpoints)
      if (near(c, x, y, 10, 8))
        return false;
    for (const auto& b : mDoors)
      if (near(b, x, y, 6, 6))
        return false;
    // Never more than two to a screen's width of the same floor.
    int crowd = 0;
    for (const auto& o : mEnemies)
    {
      if (near(o, x, y, gap, 4))
        return false;
      crowd += near(o, x, y, 12, 4);
    }
    return crowd <= 1;
  }
  // The same floor all the way from the enemy to its twin, no wall between:
  // the twin walks the stretch the level gave its original.
  bool walkway(int x, int nx, int y, const Body& b) const
  {
    for (int c = std::min(x, nx); c < std::max(x, nx) + b.w; ++c)
    {
      if (!standable(mLv.at(c, y + 1)))
        return false;
      for (int r = std::max(b.top, y - 1); r <= y; ++r)
        if (!walkable(mLv.at(c, r)))
          return false;
    }
    return true;
  }
  // Open space all the way across.
  bool airway(int x, int nx, const Body& b) const
  {
    for (int c = std::min(x, nx); c < std::max(x, nx) + b.w; ++c)
      for (int r = b.top; r < b.top + b.h; ++r)
        if (mLv.isSolid(c, r))
          return false;
    return true;
  }

  const Level& mLv;
  std::vector<Rect> mRects, mSpots;
  std::vector<Spot> mCheckpoints, mDoors, mGems, mEnemies;
};

// A kind that makes a good newcomer on any floor (or in the air over it):
// it patrols, chases or hovers, and needs nothing of the level.
bool guestKind(K k)
{
  switch (k)
  {
    case K::Walker:
    case K::Flyer:
    case K::Shield:
    case K::Guardian:
    case K::Monk:
    case K::Skitter:
    case K::Gloop:
    case K::ThornHog:
    case K::SpearRunner:
    case K::Stalker:
    case K::Imp:
    case K::HauntedArmor:
    case K::EggTimer:
    case K::JumpingSpider:
    case K::BombardierBeetle:
    case K::RubberGoon:
    case K::GrinningBomb:
    case K::Stagehand:
    case K::Mannequin:
    case K::SketchGuard:
    case K::WarmupComic:
    case K::CableWrangler:
    case K::Glob:
    case K::ShardGolem:
    case K::Blinker:
    case K::Wisp:
    case K::VoidRay:
    case K::PrismBat:
    case K::PowderPuffer:
    case K::Warden:
    case K::MaserTruck:
    case K::MastClimber:
      return true;
    default:
      return false;
  }
}

// Newcomers for a level whose own enemies all belong to its set pieces:
// someone from the same episode.
std::string episodeGuest(const Level& level)
{
  const int number = std::atoi(level.slug.c_str());
  switch (level.episode)
  {
    case 1:
      return "chrome_cop";
    case 2:
      return "guardian";
    case 3:
      return "glob";
    case 4:
      return number == 26 ? "maser_truck" : "rubber_goon";
    case 5:
      return number == 34 ? "flyer" : "walker";
    case 6:
      return "comic";
    case 7:
      return "skitter";
    default:
      return "walker";
  }
}

bool twinnable(const EntityDef& e, const EnemyDef& d)
{
  if (!e.hasPos || e.has("rect") || (d.flags & (kEnemyHarmless | kEnemyNoTally)) != 0)
    return false;
  if (!standsAlone(d.kind) && !flies(d.kind))
    return false;
  return std::all_of(e.keys.begin(), e.keys.end(), [](const auto& kv) { return plainKey(kv.first); });
}

} // namespace

int addHardModeEnemies(Level& level)
{
  level.hard = true;
  if (!level.rules.empty() || level.timer > 0 || level.entities.empty())
    return 0; // bonus levels keep their own rules; the training stage stays a sandbox
  Placer placer(level);
  std::vector<EntityDef> extras;
  int enemies = 0;
  std::map<std::string, int> own; // the level's own kinds that could have a twin, for guests
  for (const auto& e : level.entities)
  {
    const int def = enemyIndex(e.kind);
    if (def < 0)
      continue;
    const EnemyDef& d = enemyDef(def);
    if ((d.flags & (kEnemyHarmless | kEnemyNoTally)) == 0)
      ++enemies;
    if (!twinnable(e, d))
      continue;
    if (guestKind(d.kind))
      ++own[e.kind];
    Spot s;
    if (!placer.twin(e, d, s))
      continue;
    EntityDef t;
    t.kind = e.kind;
    t.x = s.x;
    t.y = s.y;
    t.hasPos = true;
    t.line = e.line;
    for (const auto& kv : e.keys)
      if (copiedKey(kv.first))
        t.keys[kv.first] = kv.second;
    extras.push_back(std::move(t));
  }

  // Where twins are few (the level's enemies belong to its set pieces), new
  // ones on open floors along the gems: the level's own commonest patroller
  // or flyer, else one from its episode. Up to about half again the
  // level's enemies.
  std::string guest = episodeGuest(level);
  int most = 0;
  for (const auto& [kind, n] : own)
    if (n > most)
    {
      most = n;
      guest = kind;
    }
  const int target = std::max(3, (enemies + 1) / 2);
  const EnemyDef& gd = enemyDef(enemyIndex(guest));
  for (int n = int(extras.size()); n < target; ++n)
  {
    Spot s;
    if (!placer.guest(gd, true, s) && !placer.guest(gd, false, s))
      break;
    EntityDef t;
    t.kind = guest;
    t.x = s.x;
    t.y = s.y;
    t.hasPos = true;
    extras.push_back(std::move(t));
  }

  level.hardExtras = int(extras.size());
  for (auto& t : extras)
    level.entities.push_back(std::move(t));
  return level.hardExtras;
}

} // namespace gr
