// The bot's planner (task ne3): a plan whose search outgrows the node
// vector's reserve must still come out the same every time. Expanding a node
// pushes its children onto that vector; the planner used to keep reading the
// node through a reference the reallocation had freed (only an ASan build
// sees that for sure, a plain build just plans nondeterministically). So the
// test drives the planner on a level until one plan makes more nodes than the
// reserve, twice from the start, and compares the inputs. Run it under
// -fsanitize=address as well. Also checks that a runner out of play gets a
// single idle input without any search.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/planner.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
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

// The node vector's reserve in Planner::plan (kBudget * 4).
constexpr int kReserve = 2600 * 4;

// The same edges the planner's own simulation uses.
PlayerInput toPlayer(const Input& in, const Input& prev)
{
  PlayerInput p;
  p.left = in.left;
  p.right = in.right;
  p.up = in.up;
  p.down = in.down;
  p.jump = {in.jump, in.jump && !prev.jump};
  p.fire = {in.fire, in.fire && !prev.fire};
  return p;
}

bool sameInput(const Input& a, const Input& b)
{
  return a.left == b.left && a.right == b.right && a.up == b.up && a.down == b.down && a.jump == b.jump &&
    a.fire == b.fire;
}

struct Run
{
  std::vector<Input> inputs;
  int bigPlanAt = -1; // the frame the first plan over the reserve was made on
  int bigPlanNodes = 0;
  int plans = 0;
};

// Plays the level with a fresh planner until a plan makes more nodes than
// the reserve and its inputs have been played out (or maxFrames pass).
Run play(const std::shared_ptr<const Level>& level, const Theme& theme, const Art& art, int maxFrames)
{
  Run r;
  World world(level, 0, theme, art);
  Planner planner;
  Input prev;
  for (int f = 0; f < maxFrames && world.state() == WorldState::Playing; ++f)
  {
    const bool planning = planner.queued() == 0;
    const Input in = planner.next(world);
    if (planning)
    {
      ++r.plans;
      if (r.bigPlanAt < 0 && planner.lastPlanNodes() > kReserve)
      {
        r.bigPlanAt = f;
        r.bigPlanNodes = planner.lastPlanNodes();
      }
    }
    r.inputs.push_back(in);
    world.update(toPlayer(in, prev));
    prev = in;
    if (r.bigPlanAt >= 0 && planner.queued() == 0)
      break;
  }
  return r;
}

void checkBigPlan(const std::shared_ptr<const Level>& level, const Theme& theme, const Art& art, int maxFrames)
{
  const Run a = play(level, theme, art, maxFrames);
  std::printf("     %d plans; first over the reserve at frame %d (%d nodes)\n", a.plans, a.bigPlanAt, a.bigPlanNodes);
  check(a.bigPlanAt >= 0, "a plan outgrows the node reserve");
  const Run b = play(level, theme, art, maxFrames);
  check(b.bigPlanAt == a.bigPlanAt && b.bigPlanNodes == a.bigPlanNodes, "the big plan comes at the same frame, same size");
  bool same = a.inputs.size() == b.inputs.size();
  for (std::size_t i = 0; same && i < a.inputs.size(); ++i)
    same = sameInput(a.inputs[i], b.inputs[i]);
  check(same, "the same inputs both times, through the big plan");
}

// A runner beaming out through the exit (the Exit cheat) gets one idle
// input and no search.
void checkNoSearch(const std::shared_ptr<const Level>& level, const Theme& theme, const Art& art)
{
  World world(level, 0, theme, art);
  world.cheat(Cheat::Exit);
  check(world.state() != WorldState::Playing || world.player().state == PlayerState::Teleporting,
    "the Exit cheat takes the runner out of play");
  Planner planner;
  const Input in = planner.next(world);
  check(sameInput(in, Input{}), "a runner out of play gets an idle input");
  check(planner.lastPlanNodes() == 0 && planner.queued() == 0, "and no search");
}

} // namespace

int main(int argc, char** argv)
{
  const std::string levelPath = argc > 1 ? argv[1] : "levels/49_hive_mother.txt";
  const int maxFrames = argc > 2 ? std::atoi(argv[2]) : 3000;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    const auto level = std::make_shared<const Level>(Level::loadFile(levelPath));
    checkBigPlan(level, theme, art, maxFrames);
    checkNoSearch(level, theme, art);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "planner: all ok" : "planner: FAILURES");
  return gFailures == 0 ? 0 : 1;
}
