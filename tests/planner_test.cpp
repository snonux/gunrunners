// The bot's planner (task ne3): plans come out the same every time, also a
// big one. Expanding a search node pushes its children onto the node list;
// the planner used to keep reading the node through a reference into a
// vector whose reallocation (past its kNodeBudget * 4 reserve) had freed it.
// A plain build then reads stale memory that usually still holds the old
// values, so the plans looked fine and only ASan caught the bug reliably.
// The node list is a deque now, whose push_back never moves existing nodes,
// which rules that out structurally. The test drives the planner on a level
// until one plan makes more than kBigPlan nodes, twice from the start, and
// compares the inputs (determinism, through the big plan). Under
// -fsanitize=address it also guards the memory safety directly. It also
// checks that a runner out of play gets a single idle input without any
// search, on a planner that has searched before.

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

// A big plan: more nodes than four children per expanded node (the size the
// node vector's reserve used to hold, past which the old bug read freed
// memory). Derived from the planner's budget so it follows any retuning.
constexpr int kBigPlan = Planner::kNodeBudget * 4;

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
  int bigPlanAt = -1; // the frame the first plan over kBigPlan nodes was made on
  int bigPlanNodes = 0;
  int plans = 0;
};

// Plays the level with a fresh planner until a plan makes more than
// kBigPlan nodes and its inputs have been played out (or maxFrames pass).
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
      if (r.bigPlanAt < 0 && planner.lastPlanNodes() > kBigPlan)
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
  std::printf("     %d plans; first big plan at frame %d (%d nodes, over %d)\n", a.plans, a.bigPlanAt, a.bigPlanNodes,
    kBigPlan);
  check(a.bigPlanAt >= 0, "a big plan completes");
  const Run b = play(level, theme, art, maxFrames);
  check(b.bigPlanAt == a.bigPlanAt && b.bigPlanNodes == a.bigPlanNodes, "the big plan comes at the same frame, same size");
  bool same = a.inputs.size() == b.inputs.size();
  for (std::size_t i = 0; same && i < a.inputs.size(); ++i)
    same = sameInput(a.inputs[i], b.inputs[i]);
  check(same, "the same inputs both times, through the big plan");
}

// Plays the level until a plan searches (makes nodes), then drains that
// plan's queue without moving the world on. False: no plan searched within
// maxFrames.
bool searchOnce(World& world, Planner& planner, int maxFrames)
{
  Input prev;
  for (int f = 0; f < maxFrames && world.state() == WorldState::Playing; ++f)
  {
    const bool planning = planner.queued() == 0;
    const Input in = planner.next(world);
    if (planning && planner.lastPlanNodes() > 0)
    {
      while (planner.queued() > 0) // next() only plans on an empty queue
        planner.next(world);
      return true;
    }
    world.update(toPlayer(in, prev));
    prev = in;
  }
  return false;
}

// A runner beaming out through the exit (the Exit cheat) gets one idle
// input and no search. The planner searched once before, so a node count
// left over from that plan would show.
void checkNoSearch(const std::shared_ptr<const Level>& level, const Theme& theme, const Art& art, int maxFrames)
{
  World world(level, 0, theme, art);
  Planner planner;
  check(searchOnce(world, planner, maxFrames) && planner.lastPlanNodes() > 0 && planner.queued() == 0,
    "a plan searches first, and its queue drains");
  world.cheat(Cheat::Exit);
  check(world.state() != WorldState::Playing || world.player().state == PlayerState::Teleporting,
    "the Exit cheat takes the runner out of play");
  const Input in = planner.next(world);
  check(sameInput(in, Input{}), "a runner out of play gets an idle input");
  check(planner.lastPlanNodes() == 0, "and no search (the earlier plan's nodes are gone)");
  check(planner.queued() == 0, "and nothing more queued");
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
    checkNoSearch(level, theme, art, maxFrames);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "planner: all ok" : "planner: FAILURES");
  return gFailures == 0 ? 0 : 1;
}
