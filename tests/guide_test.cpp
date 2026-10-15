// The guide arrow (ASSISTS: GUIDE ARROW), in small test rooms and on every
// campaign level: off until switched on and in bonus levels; points the way
// along a route (round a ceiling to a ladder, not straight up into it) to
// the nearest gem, box or bot still missing, then to the exit; a runner who
// only follows the arrow collects what it points at. On the real levels it
// finds something to point at from the start, quickly.

#include "assets/art.hpp"
#include "data/campaign.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

// A walled room 64 blocks wide and 16 high, floor on row 14, the runner at
// block `px` of row 13, the exit near the right wall.
std::string room(const std::string& header, const std::function<void(int, std::string&)>& edit = {}, int px = 2)
{
  const int W = 64, H = 16;
  std::string s = "name=GUIDE TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n" + header + "[map]\n";
  for (int y = 0; y < H; ++y)
  {
    std::string row(std::size_t(W), '.');
    if (y == 0 || y >= 14)
      row.assign(std::size_t(W), '#');
    row[0] = row[std::size_t(W - 1)] = '#';
    if (y == 13)
    {
      row[std::size_t(px)] = 'P';
      row[std::size_t(W - 4)] = 'X';
    }
    if (edit)
      edit(y, row);
    s += row + "\n";
  }
  s += "[entities]\n";
  return s;
}

std::shared_ptr<const Level> load(const std::string& text)
{
  return std::make_shared<const Level>(Level::parse(text));
}

void guideOn(World& w)
{
  World::PlayOptions o = w.playOptions();
  o.guide = true;
  w.setPlayOptions(o);
}

const char* kindName(World::GuideKind k)
{
  switch (k)
  {
    case World::GuideKind::Item:
      return "item";
    case World::GuideKind::Enemy:
      return "bot";
    case World::GuideKind::Exit:
      return "exit";
    default:
      return "none";
  }
}

std::string describe(const World::Guide& g)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s dx %d dy %d at %d,%d%s%s left %d", kindName(g.kind), g.dx, g.dy, g.tx, g.ty,
    g.routed ? " routed" : "", g.walkable ? " walkable" : "", g.left);
  return buf;
}

// Plays by the arrow alone: left or right as it says, up (climb, or jump)
// when it points up. Returns the frames until `done`, -1 if never.
int followArrow(World& w, const std::function<bool()>& done, int frames, bool shoot = false)
{
  bool jumpWas = false;
  for (int f = 0; f < frames; ++f)
  {
    if (done())
      return f;
    const World::Guide& g = w.guide();
    PlayerInput in;
    // In reach of something to shoot: turn to it (a tap), then stand and fire.
    const bool turn = !g.fire || (g.dx > 0) != (w.player().facing > 0);
    in.left = g.dx < 0 && turn;
    in.right = g.dx > 0 && turn;
    in.up = g.dy < 0;
    in.down = g.dy > 0; // down, crouch to shoot low, crawl
    // Up at a ladder climbs it; anywhere else it is a jump. Sideways off a
    // ladder is a jump too.
    const CellBox b = w.player().box();
    bool ladder = w.player().state == PlayerState::Ladder;
    for (int x = b.left(); x <= b.right() && !ladder; ++x)
      ladder = w.map().ladder(x, b.top());
    const bool onLadder = w.player().state == PlayerState::Ladder;
    // Jump is held for the whole jump (letting go cuts it short), and let go
    // for a frame on landing to jump again.
    const bool want = (g.dy < 0 && !ladder) || (onLadder && g.dx != 0);
    const bool landed = w.player().state == PlayerState::OnGround && jumpWas;
    const bool jump = (want || w.player().state == PlayerState::Jumping) && !landed;
    in.jump = {jump, jump && !jumpWas};
    const bool fire = shoot && f % 2 == 0;
    in.fire = {fire, fire};
    jumpWas = jump;
    if (std::getenv("GR_GUIDE_TRACE"))
      std::printf("     f %d runner %d,%d state %d: %s\n", f, w.player().x, w.player().y, int(w.player().state), describe(g).c_str());
    w.update(in);
  }
  return done() ? frames : -1;
}

} // namespace

int main()
{
  const std::string levels = std::string(GR_DATA_DIR) + "/levels/";
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    check(art.guideArrow.w() > 0, "the arrow is baked");

    // A gem down the corridor to the right.
    const auto gemRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
        row[20] = 'g';
    }));
    {
      World w(gemRoom, 0, theme, art);
      check(w.guide().kind == World::GuideKind::None, "off until switched on: " + describe(w.guide()));
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == 1 && g.dy == 0 && g.walkable,
        "a gem to the right: points right (" + describe(g) + ")");
      check(g.left == 1, "one thing left");
      const int f = followArrow(w, [&] { return w.stats().gems == 1; }, 300);
      check(f >= 0, "following the arrow takes the gem (frames " + std::to_string(f) + ")");
      w.update(PlayerInput{});
      check(w.guide().kind == World::GuideKind::Exit && w.guide().dx == 1, "then it points at the exit: " + describe(w.guide()));
      const int out = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 600);
      if (out < 0)
        std::printf("     runner at %d,%d state %d: %s\n", w.player().x, w.player().y, int(w.player().state), describe(w.guide()).c_str());
      check(out >= 0, "and following it reaches the exit (frames " + std::to_string(out) + ")");
      check(w.guide().kind == World::GuideKind::None, "no arrow while leaving");
    }

    // The gem sits on a shelf over the runner's head; the only way up is a
    // ladder behind them. The arrow points back to the ladder, not up.
    const auto shelfRoom = load(room(
      "",
      [](int y, std::string& row) {
        if (y == 9)
          for (int x = 6; x < 63; ++x)
            row[std::size_t(x)] = '#';
        if (y >= 8 && y <= 13)
          row[3] = 'H';
        if (y == 8)
          row[14] = 'g';
      },
      10));
    {
      World w(shelfRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == -1 && g.walkable,
        "a gem on the shelf overhead: points back to the ladder (" + describe(g) + ")");
      const int f = followArrow(w, [&] { return w.stats().gems == 1; }, 900);
      if (f < 0)
        std::printf("     runner at %d,%d state %d: %s\n", w.player().x, w.player().y, int(w.player().state), describe(w.guide()).c_str());
      check(f >= 0, "following the arrow climbs up and takes it (frames " + std::to_string(f) + ")");
    }

    // A bot down the corridor; a box of merchandise; nothing else counts.
    const auto botRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
      {
        row[30] = 'w';
        row[8] = 'h'; // a health box: not part of 100%
      }
    }));
    {
      World w(botRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Enemy && g.dx == 1 && g.left == 1, "a bot to the right: points at it (" + describe(g) + ")");
    }
    const auto merchRoom = load(room("", [](int y, std::string& row) {
      if (y == 13)
        row[12] = 'm';
    }, 30));
    {
      World w(merchRoom, 0, theme, art);
      guideOn(w);
      const World::Guide& g = w.guide();
      check(g.kind == World::GuideKind::Item && g.dx == -1, "a box of merchandise behind: points left (" + describe(g) + ")");
    }

    // Nothing to collect: straight to the exit. A bonus level: no arrow.
    {
      World w(load(room("")), 0, theme, art);
      guideOn(w);
      check(w.guide().kind == World::GuideKind::Exit && w.guide().dx == 1, "an empty room: the exit (" + describe(w.guide()) + ")");
    }
    {
      World w(load(room("rules=airjump\ntimer=60\ngoal=exit\n", [](int y, std::string& row) {
        if (y == 13)
          row[20] = 'g';
      })),
        0, theme, art);
      guideOn(w);
      check(w.guide().kind == World::GuideKind::None, "a bonus level: no arrow (" + describe(w.guide()) + ")");
    }

    // Every campaign level and extra: something to point at from the start.
    std::vector<std::string> files;
    for (int n = 1; n <= kAllLevels; ++n)
      files.push_back(levelFile(GR_DATA_DIR, n));
    for (const auto& e : std::filesystem::directory_iterator(levels))
      if (e.path().filename().string().rfind("extra_", 0) == 0)
        files.push_back(e.path().string());
    int walkable = 0;
    double worst = 0.0;
    std::string worstLevel;
    for (const auto& path : files)
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(path));
      World w(level, 0, theme, art);
      guideOn(w);
      const auto t0 = std::chrono::steady_clock::now();
      const World::Guide g = w.guide();
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      if (ms > worst)
      {
        worst = ms;
        worstLevel = level->slug;
      }
      walkable += g.walkable;
      std::printf("     %-28s %s (%.1f ms)\n", level->slug.c_str(), describe(g).c_str(), ms);
      check(g.kind != World::GuideKind::None && (g.dx != 0 || g.dy != 0), level->slug + ": the arrow points somewhere");
    }
    // Level 1 played by the arrow alone (shooting all the time, no damage).
    {
      const auto level = std::make_shared<const Level>(Level::loadFile(levelFile(GR_DATA_DIR, 1)));
      World w(level, 0, theme, art);
      guideOn(w);
      w.cheat(Cheat::God);
      const int left0 = w.guide().left;
      const int f = followArrow(w, [&] { return w.state() != WorldState::Playing; }, 3000, true);
      const auto& st = w.stats();
      std::printf("     level 1 by the arrow: %d frames, gems %d/%d, merch %d/%d, bots %d/%d, letters %s, left %d (%s)\n", f,
        st.gems, st.gemsTotal, st.merch, st.merchTotal, st.kills, st.enemiesTotal, st.letters.c_str(), w.guide().left,
        describe(w.guide()).c_str());
      // A crude player (no timing, no looking ahead) still gets a good way
      // round: the crates crouched, the crawlspace, up the ladders.
      check(left0 - w.guide().left >= 15, "level 1: following the arrow alone collects and clears a good deal");
    }
    std::printf("     %d of %d levels start with a walkable route; slowest search %.1f ms (%s)\n", walkable,
      int(files.size()), worst, worstLevel.c_str());
    check(walkable * 4 >= int(files.size()) * 3, "most levels start with a walkable route");
    check(worst < 250.0, "every search is quick");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all guide checks passed" : "guide checks FAILED");
  return gFailures == 0 ? 0 : 1;
}
