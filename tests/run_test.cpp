// Running: a short tap of left or right, then the same way pressed again and
// held, runs that way at 2 cells a frame (twice the walk) until the way is
// let go. The gesture is checked tick by tick at 60 Hz, then every runner
// runs, jumps and stops in a flat room: a running jump goes as high as a
// walking one, only further.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/run_gesture.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const char* what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    ++gFailures;
}

// Feeds `ticks` ticks of `dir`; returns whether the last one ran.
bool feed(RunGesture& g, int dir, int ticks)
{
  bool r = false;
  for (int i = 0; i < ticks; ++i)
    r = g.update(dir);
  return r;
}

void gesture()
{
  {
    RunGesture g;
    feed(g, 1, 6);
    feed(g, 0, 6);
    check(feed(g, 1, 1) && feed(g, 1, 120), "tap right, then right held: running");
    check(!feed(g, 0, 1), "letting go stops the run");
    check(!feed(g, 1, 30), "pressing again after a run walks (the run was no tap)");
  }
  {
    RunGesture g;
    feed(g, -1, 4);
    feed(g, 0, 3);
    check(feed(g, -1, 60), "tap left, then left held: running");
    check(!feed(g, 1, 10), "turning around walks");
  }
  {
    RunGesture g;
    feed(g, 1, RunGesture::kTapTicks + 5);
    feed(g, 0, 4);
    check(!feed(g, 1, 30), "a long press first is a walk, not a tap");
  }
  {
    RunGesture g;
    feed(g, 1, 5);
    feed(g, 0, RunGesture::kGapTicks + 5);
    check(!feed(g, 1, 30), "a second press that comes too late walks");
  }
  {
    RunGesture g;
    feed(g, -1, 5);
    feed(g, 0, 5);
    check(!feed(g, 1, 30), "tap left, then right: walks");
  }
  {
    RunGesture g;
    feed(g, 1, 1);
    feed(g, 0, 1);
    check(feed(g, 1, 10), "a very quick double tap runs");
    g.reset();
    check(!feed(g, 1, 10), "reset forgets the run");
  }
}

// A flat room 150 blocks wide, the runner at block (3, 21), feet on cell row 43.
std::string room()
{
  const int w = 150, h = 24;
  std::vector<std::string> map(std::size_t(h), std::string(std::size_t(w), '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][std::size_t(w - 2)] =
      map[std::size_t(y)][std::size_t(w - 1)] = '#';
  for (int x = 0; x < w; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  map[21][3] = 'P';
  std::string s = "name=RUN TEST\nepisode=1\ntheme=neon_rooftops\nweapon=pulse_pistol\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n";
  return s;
}

void step(World& w, bool right, bool run, bool jump = false)
{
  PlayerInput p;
  p.right = right;
  p.run = run;
  p.jump.pressed = p.jump.triggered = jump;
  w.update(p);
}

void idle(World& w, int frames)
{
  for (int i = 0; i < frames; ++i)
    w.update(PlayerInput{});
}

// Holds right (running or not) and jumps: how high and how far it went
// before landing.
void jumpArc(World& w, bool run, int& rise, int& dist)
{
  const int x0 = w.player().x, y0 = w.player().y;
  int top = y0;
  step(w, true, run, true);
  for (int i = 0; i < 60 && w.player().state != PlayerState::OnGround; ++i)
  {
    step(w, true, run, true);
    top = std::min(top, w.player().y);
  }
  rise = y0 - top;
  dist = w.player().x - x0;
}

} // namespace

int main()
{
  gesture();

  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const auto level = std::make_shared<const Level>(Level::parse(room()));
    for (int character = 0; character < 6; ++character)
    {
      std::printf("runner %d\n", character);
      World walk(level, character, theme, art);
      World run(level, character, theme, art);
      idle(walk, 10);
      idle(run, 10);
      // Face right first (turning costs a frame), like the gesture's tap does.
      step(walk, true, false);
      step(run, true, false);
      const int xw = walk.player().x, xr = run.player().x;
      for (int i = 0; i < 20; ++i)
      {
        step(walk, true, false);
        step(run, true, true);
      }
      check(walk.player().x - xw == 20, "walking: a cell a frame");
      check(run.player().x - xr == 40, "running: two cells a frame");

      int walkRise = 0, walkDist = 0, runRise = 0, runDist = 0;
      jumpArc(walk, false, walkRise, walkDist);
      jumpArc(run, true, runRise, runDist);
      check(runRise == walkRise && walkRise > 0, "a running jump goes as high as a walking one");
      check(runDist > walkDist + walkDist / 2, "and further");

      // Let go of run: walks again.
      const int xs = run.player().x;
      for (int i = 0; i < 10; ++i)
        step(run, true, false);
      check(run.player().x - xs == 10, "without run held, the runner walks");

      // A run can't start in mid-air: a walking jump stays a walking jump.
      World air(level, character, theme, art);
      idle(air, 10);
      step(air, true, false);
      step(air, true, false, true);
      step(air, true, false, true);
      const int xa = air.player().x;
      for (int i = 0; i < 4; ++i)
        step(air, true, true, true);
      check(air.player().state != PlayerState::OnGround && air.player().x - xa == 4,
            "running doesn't start in mid-air");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "passed");
  return gFailures ? 1 : 0;
}
