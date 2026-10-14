// Navigation check: walks every level the way a player can, with the real
// game logic, and lists what the runner can't get to. A breadth-first search
// over the places a runner comes to rest (standing, on a ladder, hanging,
// swimming, driving), trying short input moves from each: steps, jumps with
// and without steering (also steering late or letting go early), up and down
// (ladders, doors, monitors), waits for lifts, a few shots ahead (breakable
// walls) and USE (vehicles). The search runs with God Mode, the access card
// and the level's prototype, so only the geometry and the level's own
// mechanics decide; the bot plays without them.
//
//   navcheck [--character N,M,...] [--map DIR] [--max-nodes N] LEVEL...
//
// With several runners a place counts as reached if any of them gets
// there (runners can be swapped mid-level). The default is Rocco (the
// lowest jump) and Nova (the highest).
//
// Prints, per level: the exit, the bonus entrance and every item the search
// never touched (with its block). --map writes DIR/<level>.ppm: walls grey,
// one-way and ladders blue, where the runner went green, missed items red.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace gr;

namespace
{

constexpr int kSettle = 90; // frames a move may take to come to rest
constexpr std::size_t kMaxQueue = 10000; // worlds waiting to be expanded

struct Pad
{
  int dx = 0;
  bool up = false, down = false, jump = false, fire = false, use = false;
};

struct Move
{
  std::vector<Pad> pre; // played first
  Pad hold;             // then held until the runner rests
  int pump = 0;         // on a vine: frames of pumping it, then a launch at the end of a swing
};

bool resting(const World& w)
{
  const auto& p = w.player();
  if (w.gallery().hop > 0)
    return false; // Level 39: mid monitor hop
  if (p.vehicle >= 0)
    return true;
  // Rides that never land on their own (a bubble, a bungee, the cannon's
  // flight, free fall): every move is a place.
  if (w.trench().bubble || w.storm().bungee || w.fleet().cannonball || w.level().rules == "freefall")
    return true;
  switch (p.state)
  {
    case PlayerState::OnGround:
    case PlayerState::Ladder:
    case PlayerState::Pipe:
    case PlayerState::Swing:
    case PlayerState::Cling:
    case PlayerState::Swim:
      return true;
    default:
      return false;
  }
}

// Where the runner is, plus what the way on depends on: the platforms near
// it (a pulley gondola's height is history, not time), hatches, doors and
// breakable walls.
std::uint64_t keyOf(const World& w)
{
  const auto& p = w.player();
  std::uint64_t h = 1469598103934665603ull;
  auto mix = [&h](std::int64_t v) {
    h ^= std::uint64_t(v);
    h *= 1099511628211ull;
  };
  mix(p.x);
  mix(p.y);
  mix(int(p.state));
  mix(p.vehicle);
  mix(w.tower().rot);
  // Turbo jumps higher but shrugs off pulls (Level 15's bonus vent).
  mix(p.turbo > 0);
  // Both Sides: the bottom runner, on the same input, gets stopped by
  // other walls.
  if (w.level().rules == "split_mirror")
  {
    mix(w.twin().x);
    mix(w.twin().y);
  }
  for (const auto& pl : w.platforms())
    if (std::abs(pl.x + pl.w / 2 - p.x) < 16 + pl.w / 2 && std::abs(pl.y - p.y) < 16)
    {
      mix(pl.x);
      mix(pl.y);
    }
  // The walls around the runner (struck words, opened doors, broken glass).
  const auto& m = w.map();
  std::uint64_t bits = 0;
  int n = 0;
  for (int y = p.y - 12; y <= p.y + 4; y += 2)
    for (int x = p.x - 12; x <= p.x + 14; x += 2)
    {
      bits = (bits << 1) | (m.solid(x, y) ? 1u : 0u);
      if (++n == 60)
      {
        mix(std::int64_t(bits));
        bits = 0;
        n = 0;
      }
    }
  mix(std::int64_t(bits));
  // Switches that send the way elsewhere: levers, mirrors, tube valves.
  for (const auto& l : w.levers())
    mix(l.state);
  for (const auto& mr : w.mirrors())
    mix(mr.to);
  for (const auto& t : w.gulletTubes())
    mix(t.set);
  for (const auto& g : w.grav().gates)
    mix(g.open);
  for (const auto& z : w.grav().zones)
    mix(int(z.dir));
  for (const auto& ht : w.hatches())
    mix(ht.open);
  for (const auto& d : w.doors())
    mix(d.solid);
  for (const auto& b : w.breakables())
    mix(b.broken);
  return h;
}

std::vector<Move> movesFor(const World& w)
{
  std::vector<Move> m;
  const auto& p = w.player();
  auto pad = [](int dx, bool jump = false) {
    Pad q;
    q.dx = dx;
    q.jump = jump;
    return q;
  };
  for (int d : {-1, 1})
  {
    m.push_back({{pad(d)}, {}});
    m.push_back({{pad(d)}, pad(d)});
    m.push_back({{pad(d), pad(d)}, {}});
    // Jumps: steer all the way, steer then let go, go up first then steer.
    m.push_back({{pad(d, true)}, pad(d, true)});
    m.push_back({{pad(d, true), pad(d, true)}, pad(0, true)});
    m.push_back({{pad(d, true), pad(d, true), pad(d, true), pad(d, true)}, pad(0, true)});
    m.push_back({{pad(0, true), pad(0, true)}, pad(d, true)});
    m.push_back({{pad(0, true), pad(0, true), pad(0, true), pad(0, true)}, pad(d, true)});
    m.push_back({{pad(d, true)}, pad(d)});                       // a short hop
    m.push_back({{pad(d, true), pad(d, true)}, pad(-d, true)}); // back onto a ledge
    // A few shots ahead, then on.
    Pad f = pad(d);
    f.fire = true;
    Pad g = pad(0);
    g.fire = true;
    // (then a wait: a struck word or a cracked wall takes a moment to go).
    std::vector<Pad> shots = {f, {}, g, {}, g, {}, g, {}, g, {}, g, {}};
    shots.resize(shots.size() + 24);
    m.push_back({shots, {}});
    // Down and fire in the air: the Recoil Cannon's second jump, a
    // jetpack.
    for (int k : {2, 4, 6})
    {
      Pad air = pad(d, true);
      Pad kick = air;
      kick.down = true;
      kick.fire = true;
      std::vector<Pad> pre(static_cast<std::size_t>(k), air);
      for (int i = 0; i < 10; ++i)
        pre.push_back(i % 2 ? air : kick);
      m.push_back({pre, pad(d)});
    }
    // Crouched shots (low targets).
    Pad cf = pad(d);
    cf.down = true;
    cf.fire = true;
    Pad cg = pad(0);
    cg.down = true;
    cg.fire = true;
    Pad cd = pad(0);
    cd.down = true;
    std::vector<Pad> low = {cf, cd, cg, cd, cg, cd, cg, cd};
    low.resize(low.size() + 24);
    m.push_back({low, {}});
    // Charge and hold weapons (the Sunstone Lance opens a sun door held on
    // it for 15 frames, which then shuts again).
    std::vector<Pad> held(20, g);
    held.resize(held.size() + 24);
    m.push_back({held, {}});
    std::vector<Pad> heldOn(20, g);
    heldOn.resize(heldOn.size() + 12, pad(d));
    m.push_back({heldOn, {}});
    // A Breach Charge: throw it, let it stick, hold fire to set it off.
    if (w.level().weapon == "breach_charge")
    {
      std::vector<Pad> charge = {f};
      charge.resize(9);
      charge.resize(charge.size() + 12, g);
      charge.resize(charge.size() + 24);
      m.push_back({charge, {}});
    }
    // Steering with up held (free fall, swimming, jetpacks).
    Pad steerUp = pad(d);
    steerUp.up = true;
    m.push_back({std::vector<Pad>(3, steerUp), {}});
    m.push_back({std::vector<Pad>(8, steerUp), {}});
    m.push_back({{steerUp}, steerUp});
    // Crawling.
    Pad crawl = pad(d);
    crawl.down = true;
    m.push_back({std::vector<Pad>(2, crawl), {}});
    m.push_back({std::vector<Pad>(8, crawl), {}});
    // Subwoofer pads launch on the beat: jumps after a short wait.
    if (!w.pads().empty())
      for (int wait = 1; wait < 8; ++wait)
      {
        std::vector<Pad> pre(static_cast<std::size_t>(wait), Pad{});
        pre.push_back(pad(d, true));
        m.push_back({pre, pad(d, true)});
        pre.back() = pad(0, true);
        m.push_back({pre, pad(0, true)});
      }
  }
  m.push_back({{pad(0, true)}, pad(0, true)});
  // Repeated jump presses: swim strokes (Level 30), air jumps.
  for (int d : {-1, 0, 1})
    for (int every : {3, 4, 5})
    {
      std::vector<Pad> taps;
      for (int i = 0; i < 4; ++i)
      {
        taps.push_back(pad(d, true));
        for (int k = 1; k < every; ++k)
          taps.push_back(pad(d));
      }
      m.push_back({taps, pad(d)});
    }
  if (p.vine >= 0)
    for (int k : {0, 26, 52, 78, 104, 156})
    {
      Move pump;
      pump.pump = k + 1;
      m.push_back(pump);
    }
  Pad up, down, use;
  up.up = true;
  down.down = true;
  use.use = true;
  m.push_back({{up}, {}});
  m.push_back({{up, up, up, up}, {}});
  // Two quick presses (Level 19's closet switch wants two hits in 15 frames).
  for (int k : {11, 13})
  {
    std::vector<Pad> twice = {up};
    twice.resize(std::size_t(k));
    twice.push_back(up);
    twice.resize(twice.size() + 16);
    m.push_back({twice, {}});
  }
  m.push_back({{down}, {}});
  m.push_back({{down, down, down, down}, {}});
  Pad downJump = down;
  downJump.jump = true;
  m.push_back({{downJump}, {}});
  m.push_back({{use}, {}});
  // Shots straight up (looking up and firing).
  {
    Pad uf = up;
    uf.fire = true;
    std::vector<Pad> shots = {uf, up, uf, up, uf, up, uf, up};
    shots.resize(shots.size() + 24);
    m.push_back({shots, {}});
  }
  m.push_back({std::vector<Pad>(8), {}});
  m.push_back({std::vector<Pad>(30), {}});
  (void)p;
  return m;
}

Pad pad(int dx, bool jump = false)
{
  Pad q;
  q.dx = dx;
  q.jump = jump;
  return q;
}

PlayerInput toInput(const Pad& q, const Pad& prev)
{
  PlayerInput in;
  in.left = q.dx < 0;
  in.right = q.dx > 0;
  in.up = q.up;
  in.down = q.down;
  in.jump = {q.jump, q.jump && !prev.jump};
  in.fire = {q.fire, q.fire && !prev.fire};
  in.use = {q.use, q.use && !prev.use};
  return in;
}

struct Result
{
  std::vector<unsigned char> cov; // cells the runner's box covered
  int w = 0, h = 0;
  int nodes = 0;
  int botFrames = 0;
  bool exitReached = false, bonusReached = false, capped = false;
};

void coverBox(Result& r, const World& w, const CellBox& b)
{
  // Level 35 turns its map round the runner: back to the level's own x.
  const int rot = w.tower().rot;
  for (int y = b.y; y < b.y + b.h; ++y)
    for (int cx = b.x; cx < b.x + b.w; ++cx)
    {
      const int x = rot ? ((cx - rot) % r.w + r.w) % r.w : cx;
      if (x >= 0 && y >= 0 && x < r.w && y < r.h)
        r.cov[std::size_t(y * r.w + x)] = 1;
    }
}

void cover(Result& r, const World& w)
{
  coverBox(r, w, w.player().box());
  // Both Sides: the bottom runner picks up its half's gems.
  if (w.level().rules == "split_mirror")
    coverBox(r, w, w.twin().box());
}

// A box in the level's own x (Level 35's tower starts turned round the
// runner, so its items do too).
CellBox unturned(const World& w, CellBox b)
{
  const int rot = w.tower().rot, wc = w.map().width();
  if (rot)
    b.x = ((b.x - rot) % wc + wc) % wc;
  return b;
}

bool covered(const Result& r, const CellBox& b, int grow = 0)
{
  for (int y = b.y - grow; y < b.y + b.h + grow; ++y)
    for (int x = b.x - grow; x < b.x + b.w + grow; ++x)
      if (x >= 0 && y >= 0 && x < r.w && y < r.h && r.cov[std::size_t(y * r.w + x)])
        return true;
  return false;
}

// The bot plays the level the way it is meant to be played (every twist
// has its own rules in the bot and planner); every place it comes to rest
// on the way is a root for the search, so side rooms behind a lever, a
// monitor hop or a sun door get searched from where the route opens them.
void botRoots(const World& start, bool bonus, int maxFrames, std::deque<std::unique_ptr<World>>& roots, Result& r)
{
  World w(start);
  Bot bot;
  bot.setTakeBonus(bonus);
  Input prev;
  int last = -1000;
  for (int f = 0; f < maxFrames && w.state() == WorldState::Playing; ++f)
  {
    const Input in = bot.play(w);
    PlayerInput p;
    p.left = in.left;
    p.right = in.right;
    p.up = in.up;
    p.down = in.down;
    p.jump = {in.jump, in.jump && !prev.jump};
    p.fire = {in.fire, in.fire && !prev.fire};
    p.use = {in.use, in.use && !prev.use};
    prev = in;
    w.update(p);
    cover(r, w);
    if (std::getenv("NAV_DEBUG") && f % 500 == 0)
      std::fprintf(stderr, "  bot f%d at %d,%d state %d in %d%d%d%d%d%d\n", f, w.player().x, w.player().y,
                   int(w.player().state), in.left, in.right, in.up, in.down, in.jump, in.fire);
    if (w.bonusRequested())
    {
      r.bonusReached = true;
      break;
    }
    if (resting(w) && f - last >= 10)
    {
      roots.push_back(w.cloneForSim());
      last = f;
    }
  }
  if (w.state() == WorldState::Exiting || w.state() == WorldState::Done)
    r.exitReached = true;
  if (std::getenv("NAV_DEBUG"))
    std::fprintf(stderr, "bot run (bonus %d): frame %d, state %d, at %d,%d, bonus %d, deaths %d\n", int(bonus),
                 w.stats().frames, int(w.state()), w.player().x, w.player().y, int(w.bonusRequested()),
                 w.stats().deaths);
}

Result explore(const World& start, int maxNodes, bool withBot)
{
  Result r;
  r.w = start.map().width();
  r.h = start.map().height();
  r.cov.assign(std::size_t(r.w * r.h), 0);
  std::unordered_set<std::uint64_t> seen;
  std::deque<std::unique_ptr<World>> queue, roots;
  // The start as it is (Free Fall opens in the air) and settled.
  roots.push_back(start.cloneForSim());
  auto first = start.cloneForSim();
  for (int f = 0; f < kSettle && !resting(*first); ++f)
    first->update({});
  roots.push_back(std::move(first));
  if (withBot)
  {
    botRoots(start, false, 15 * 60 * 15, roots, r);
    botRoots(start, true, 15 * 60 * 15, roots, r);
  }
  for (auto& root : roots)
  {
    // The search shoots with the level's prototype: some walls only break
    // to it (the bot plays with what it picks up).
    root->cheat(Cheat::Ammo);
    if (!root->godMode())
      root->cheat(Cheat::God);
    root->cheat(Cheat::Card);
    cover(r, *root);
    if (seen.insert(keyOf(*root)).second)
      queue.push_back(std::move(root));
  }
  while (!queue.empty())
  {
    std::unique_ptr<World> node = std::move(queue.front());
    queue.pop_front();
    if (++r.nodes > maxNodes)
    {
      r.capped = true;
      break;
    }
    for (const Move& mv : movesFor(*node))
    {
      auto w = node->cloneForSim();
      // Keep the prototype topped up: a search path may have thrown
      // every charge before it got to the wall that needs one.
      if (w->player().ammo < 4)
        w->cheat(Cheat::Ammo);
      Pad prev;
      bool dead = false, left = false;
      auto step = [&](const Pad& q) {
        w->update(toInput(q, prev));
        prev = q;
        cover(r, *w);
        if (w->bonusRequested())
        {
          r.bonusReached = true;
          w->clearBonusRequest();
        }
        if (w->state() != WorldState::Playing)
        {
          if (w->state() == WorldState::Exiting)
            r.exitReached = true;
          left = true;
        }
        if (w->player().state == PlayerState::Dying)
          dead = true;
      };
      for (const Pad& q : mv.pre)
        if (!dead && !left)
          step(q);
      if (mv.pump > 0)
      {
        // Push along each swing, then let go near the end of one.
        auto swing = [&]() -> const Vine* {
          const int v = w->player().vine;
          return v >= 0 && w->player().state == PlayerState::Swing ? &w->vines()[std::size_t(v)] : nullptr;
        };
        for (int f = 0; f < mv.pump && !dead && !left && swing(); ++f)
          step(pad(swing()->swingDir()));
        for (int f = 0; f < 14 && !dead && !left && swing(); ++f)
        {
          const Vine& v = *swing();
          if (v.t % (v.period / 2) == v.period / 2 - 2)
            break;
          step(pad(v.swingDir()));
        }
        if (!dead && !left && swing())
          step(pad(swing()->swingDir(), true));
      }
      for (int f = 0; f < kSettle && !dead && !left && !resting(*w); ++f)
        step(mv.hold);
      if (dead || left)
        continue;
      if (seen.insert(keyOf(*w)).second)
      {
        if (std::getenv("NAV_NODES"))
          std::fprintf(stderr, "node %d,%d state %d\n", w->player().x, w->player().y, int(w->player().state));
        // A world is a few hundred kB: past this the queue would eat the
        // machine's memory (Level 26's top-down bonus fans out wide).
        if (queue.size() >= kMaxQueue)
          r.capped = true;
        else
          queue.push_back(std::move(w));
      }
    }
  }
  return r;
}

const char* kindName(ItemKind k)
{
  switch (k)
  {
    case ItemKind::Health: return "health";
    case ItemKind::Merch: return "merch";
    case ItemKind::Laser: return "laser";
    case ItemKind::Rocket: return "rocket";
    case ItemKind::Flame: return "flame";
    case ItemKind::RapidFire: return "rapid fire";
    case ItemKind::Key: return "key";
    case ItemKind::Gem: return "gem";
    case ItemKind::LetterG: return "letter G";
    case ItemKind::LetterU: return "letter U";
    case ItemKind::LetterN: return "letter N";
    case ItemKind::Turbo: return "turbo";
    case ItemKind::Virus: return "virus";
    case ItemKind::Proto: return "prototype";
    case ItemKind::Duck: return "duck";
  }
  return "?";
}

void writeMap(const std::string& path, const World& w, const Result& r, const std::vector<CellBox>& missed)
{
  const int sc = 2;
  const auto& m = w.map();
  std::vector<unsigned char> px(std::size_t(r.w * sc * r.h * sc * 3), 0);
  auto put = [&](int cx, int cy, int R, int G, int B) {
    for (int y = 0; y < sc; ++y)
      for (int x = 0; x < sc; ++x)
      {
        const std::size_t i = std::size_t(((cy * sc + y) * r.w * sc + cx * sc + x) * 3);
        px[i] = (unsigned char)R;
        px[i + 1] = (unsigned char)G;
        px[i + 2] = (unsigned char)B;
      }
  };
  for (int y = 0; y < r.h; ++y)
    for (int x = 0; x < r.w; ++x)
    {
      const bool c = r.cov[std::size_t(y * r.w + x)];
      if (m.solid(x, y))
        put(x, y, 110, 110, 110);
      else if (m.ladder(x, y))
        put(x, y, 60, 140, 255);
      else if (m.solidTop(x, y))
        put(x, y, 40, 90, 200);
      else if (m.hazard(x, y))
        put(x, y, 200, 120, 0);
      else if (c)
        put(x, y, 30, 120, 40);
      else
        put(x, y, 12, 12, 20);
    }
  for (const auto& b : missed)
    for (int y = b.y; y < b.y + b.h; ++y)
      for (int x = b.x; x < b.x + b.w; ++x)
        if (x >= 0 && y >= 0 && x < r.w && y < r.h)
          put(x, y, 255, 40, 40);
  const auto& lv = w.level();
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x)
      put(lv.exitTx * 2 + x, lv.exitTy * 2 + y, 255, 255, 0);
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f)
    return;
  std::fprintf(f, "P6\n%d %d\n255\n", r.w * sc, r.h * sc);
  std::fwrite(px.data(), 1, px.size(), f);
  std::fclose(f);
}

} // namespace

int main(int argc, char** argv)
{
  std::vector<int> characters = {1, 2}; // Rocco (the lowest jump) and Nova (the highest)
  int maxNodes = 300000;
  bool withBot = true;
  std::string mapDir;
  std::vector<std::string> files;
  for (int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    if (a == "--character" && i + 1 < argc)
    {
      characters.clear();
      for (const char* c = argv[++i]; *c; ++c)
        if (*c >= '0' && *c <= '9')
          characters.push_back(*c - '0');
    }
    else if (a == "--map" && i + 1 < argc)
      mapDir = argv[++i];
    else if (a == "--no-bot")
      withBot = false;
    else if (a == "--max-nodes" && i + 1 < argc)
      maxNodes = std::atoi(argv[++i]);
    else
      files.push_back(a);
  }
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  int problems = 0;
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    for (const auto& file : files)
    {
      const auto t0 = std::chrono::steady_clock::now();
      auto level = std::make_shared<const Level>(Level::loadFile(file));
      World w(level, characters.front(), theme, art);
      Result r = explore(w, maxNodes, withBot);
      for (std::size_t c = 1; c < characters.size(); ++c)
      {
        World other(level, characters[c], theme, art);
        const Result more = explore(other, maxNodes, withBot);
        for (std::size_t i = 0; i < r.cov.size(); ++i)
          r.cov[i] |= more.cov[i];
        r.nodes += more.nodes;
        r.capped = r.capped || more.capped;
        r.exitReached = r.exitReached || more.exitReached;
        r.bonusReached = r.bonusReached || more.bonusReached;
      }
      const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      std::string base = file.substr(file.find_last_of('/') + 1);
      base = base.substr(0, base.find('.'));
      std::vector<CellBox> missed;
      std::string lines;
      char buf[256];
      const auto& lv = *level;
      const CellBox exitBox{lv.exitTx * 2, lv.exitTy * 2, 2, 2};
      const bool exitOk = !lv.hasExit || r.exitReached || covered(r, exitBox);
      if (!exitOk)
      {
        std::snprintf(buf, sizeof buf, "  exit at block %d,%d not reached\n", lv.exitTx, lv.exitTy);
        lines += buf;
        missed.push_back(exitBox);
      }
      for (const auto& s : lv.spawns)
        if (s.kind == 'B')
        {
          const CellBox b{s.tx * 2, s.ty * 2, 2, 2};
          if (!r.bonusReached && !covered(r, b))
          {
            std::snprintf(buf, sizeof buf, "  bonus entrance at block %d,%d not reached\n", s.tx, s.ty);
            lines += buf;
            missed.push_back(b);
          }
        }
      for (const auto& it : w.items())
      {
        const CellBox b = unturned(w, it.box());
        if (it.kind == ItemKind::Virus || covered(r, b))
          continue;
        std::snprintf(buf, sizeof buf, "  %s at block %d,%d\n", kindName(it.kind), b.x / 2, b.y / 2);
        lines += buf;
        missed.push_back(b);
      }
      // $ caches: the post-credits scene wants every one of them.
      for (const auto& pr : w.props())
      {
        const CellBox b = unturned(w, pr.box());
        if (pr.kind != PropKind::GemCache || covered(r, b))
          continue;
        std::snprintf(buf, sizeof buf, "  $ cache at block %d,%d\n", b.x / 2, b.y / 2);
        lines += buf;
        missed.push_back(b);
      }
      for (const auto& bx : w.boxes())
      {
        const CellBox b = unturned(w, bx.box());
        if (covered(r, b, 6))
          continue;
        std::snprintf(buf, sizeof buf, "  %s box at block %d,%d\n", kindName(bx.content), b.x / 2, b.y / 2);
        lines += buf;
        missed.push_back(b);
      }
      std::printf("%s: %d places, %.1fs%s%s\n%s", base.c_str(), r.nodes, secs, r.capped ? " (capped)" : "",
                  lines.empty() ? ", all reached" : "", lines.c_str());
      std::fflush(stdout);
      if (!lines.empty())
        ++problems;
      if (!mapDir.empty())
        writeMap(mapDir + "/" + base + ".ppm", w, r, missed);
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  return problems ? 1 : 0;
}
