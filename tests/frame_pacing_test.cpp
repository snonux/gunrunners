// Checks how long the desktop window loop sleeps between frames: until the
// next 60 Hz tick, never a negative or runaway wait, and, on a simulated
// clock that runs the loop frame by frame, about 60 frames a second without
// spinning whether or not a vsynced present blocks.

#include "base/frame_pacing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <initializer_list>
#include <limits>

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

constexpr double kTick = 1.0 / 60.0;
// The longest sleep: one tick, rounded up to whole milliseconds.
constexpr int kMaxPauseMs = 17;

void pauseCases()
{
  check(framePauseMs(kTick, 0.0) == 17, "just ticked: sleeps a whole tick, rounded up");
  check(framePauseMs(kTick, 0.010) == 7, "10 ms owed: sleeps the rest, rounded up");
  check(framePauseMs(kTick, kTick - 1e-6) == 1, "a microsecond to go: sleeps 1 ms, not 0");
  // Every wait lies between zero and one tick, rounded up.
  bool bounded = true;
  for (int i = -20; i <= 40; ++i)
  {
    const int ms = framePauseMs(kTick, i * 0.001);
    bounded = bounded && ms >= 0 && ms <= kMaxPauseMs;
  }
  check(bounded, "every wait is within 0..17 ms");
}

void edgeCases()
{
  check(framePauseMs(kTick, kTick) == 0, "a tick due right now: no sleep");
  check(framePauseMs(kTick, 0.05) == 0, "frame overran by several ticks: no (negative) sleep");
  check(framePauseMs(kTick, 0.25) == 0, "accumulator at its cap: no sleep");
  check(framePauseMs(kTick, -0.01) == 17, "negative owed time: capped at one tick");
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  check(framePauseMs(kTick, -inf) == 17, "minus infinity owed: capped at one tick");
  check(framePauseMs(kTick, inf) == 0, "infinity owed: no sleep");
  check(framePauseMs(kTick, nan) == 0, "NaN owed: no sleep");
  check(framePauseMs(0.0, 0.0) == 0, "zero tick length: no sleep");
  check(framePauseMs(-kTick, 0.0) == 0, "negative tick length: no sleep");
  check(framePauseMs(nan, 0.0) == 0, "NaN tick length: no sleep");
}

// A display behind the present. blocks=false is a present that returns at
// once: no vsync, or vsync claimed but ignored. Otherwise up to `depth`
// presented frames may wait for their vblank before a present blocks
// (depth 0: every present waits for its own frame to be shown).
struct Display
{
  bool blocks;
  int depth;
  double refresh;
};

// Advances the clock t through a present and returns the new time.
double present(const Display& d, std::deque<double>& pending, double t)
{
  if (!d.blocks)
    return t;
  while (!pending.empty() && pending.front() <= t)
    pending.pop_front();
  double vblank = (std::floor(t / d.refresh) + 1.0) * d.refresh;
  if (!pending.empty())
    vblank = std::max(vblank, pending.back() + d.refresh);
  pending.push_back(vblank);
  while (int(pending.size()) > d.depth)
  {
    t = std::max(t, pending.front());
    pending.pop_front();
  }
  return t;
}

struct LoopStats
{
  double seconds = 0.0;
  long frames = 0;
  long ticks = 0;
  long unslept = 0; // frames followed by no sleep at all
  int maxPauseMs = 0;
  int maxTicksPerFrame = 0;
  double fps() const { return frames / seconds; }
  double tps() const { return ticks / seconds; }
};

// How late a sleep wakes: 0 to 0.8 ms, from a fixed pseudo-random sequence.
double wakeLateness(std::uint32_t& seed)
{
  seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
  return double((seed >> 16) % 800u) * 1e-6;
}

// Runs the desktop loop for 10 simulated seconds: the accumulator steps of
// WindowedRun::frame, `work(frame)` seconds of events, ticks and drawing,
// the present, then the sleep of runFrames, which wakes a little late.
template <typename Work>
LoopStats simulate(const Display& display, Work work)
{
  LoopStats s;
  std::deque<double> pending;
  std::uint32_t seed = 12345;
  double t = 0.0, last = 0.0, accumulator = 0.0;
  while (t < 10.0)
  {
    accumulator = std::min(accumulator + (t - last), 0.25);
    last = t;
    int ticks = 0;
    for (; accumulator >= kTick; accumulator -= kTick)
      ++ticks;
    t = present(display, pending, t + work(s.frames));
    const int pause = framePauseMs(kTick, accumulator + (t - last));
    t += pause / 1000.0 + wakeLateness(seed);
    s.ticks += ticks;
    s.maxTicksPerFrame = std::max(s.maxTicksPerFrame, ticks);
    s.unslept += pause == 0;
    s.maxPauseMs = std::max(s.maxPauseMs, pause);
    ++s.frames;
  }
  s.seconds = t;
  return s;
}

bool near60(double rate) { return rate > 58.5 && rate < 61.0; }

// Fixed-length frames that end in a present at the given display.
LoopStats steady(const Display& display, double frameSeconds)
{
  return simulate(display, [frameSeconds](long) { return frameSeconds; });
}

void unpacedLoopCases()
{
  // Vsync claimed but ignored, or none: the present never holds back.
  const Display none{false, 0, 0.0};
  const LoopStats fast3 = steady(none, 0.003);
  check(near60(fast3.fps()) && near60(fast3.tps()), "ignored vsync, 3 ms frames: about 60 frames and ticks a second");
  check(fast3.unslept == 0, "ignored vsync, 3 ms frames: sleeps after every frame, never spins");
  const LoopStats fast19 = steady(none, 0.0019);
  check(near60(fast19.fps()) && fast19.unslept == 0, "ignored vsync, 1.9 ms frames: 60 a second, always sleeps");
  const LoopStats instant = steady(none, 0.0);
  check(near60(instant.fps()) && instant.unslept == 0, "zero-length frames: 60 a second, always sleeps");
  check(std::max({fast3.maxPauseMs, fast19.maxPauseMs, instant.maxPauseMs}) <= kMaxPauseMs, "unpaced: no sleep longer than one tick");
}

void vsyncLoopCases()
{
  // A 60 Hz vsync swap queue of depth 0 to 3, a 59.94 Hz display and a
  // 144 Hz one: the sleep never costs a frame, and it never sleeps longer
  // than one tick.
  const Display displays[] = {
    {true, 0, 1.0 / 60.0}, {true, 2, 1.0 / 60.0}, {true, 3, 1.0 / 60.0},
    {true, 0, 1.0 / 59.94}, {true, 3, 1.0 / 144.0},
  };
  bool rate = true, bounded = true;
  for (const Display& d : displays)
  {
    const LoopStats s = steady(d, 0.002);
    rate = rate && near60(s.fps()) && near60(s.tps());
    bounded = bounded && s.maxPauseMs <= kMaxPauseMs && s.maxTicksPerFrame <= 2;
  }
  check(rate, "vsync swap queues: about 60 frames and ticks a second");
  check(bounded, "vsync swap queues: no sleep over one tick, at most 2 ticks in a frame");
}

void overrunLoopCases()
{
  const Display none{false, 0, 0.0};
  // Every frame takes 40 ms: no sleeping, and the ticks catch up.
  const LoopStats slow = steady(none, 0.040);
  check(slow.unslept == slow.frames && near60(slow.tps()), "40 ms frames: never sleeps, still 60 ticks a second");
  // Every 30th frame takes 50 ms: the frames after it catch up.
  const LoopStats hitches = simulate(none, [](long f) { return f % 30 == 29 ? 0.050 : 0.002; });
  check(near60(hitches.tps()) && hitches.maxPauseMs <= kMaxPauseMs, "50 ms hitches: 60 ticks a second, no long sleep");
  // A one second stall: the accumulator caps at 0.25 s (at most 15 ticks
  // at once) and the other 0.75 s of game time is dropped.
  const LoopStats stall = simulate(none, [](long f) { return f == 100 ? 1.0 : 0.002; });
  const double expected = (stall.seconds - 0.75) * 60.0;
  check(stall.maxTicksPerFrame >= 14 && stall.maxTicksPerFrame <= 15, "1 s stall: the catch-up stops at the 0.25 s cap");
  check(std::fabs(stall.ticks - expected) < 3.0, "1 s stall: 60 ticks a second apart from the dropped 0.75 s");
  check(stall.maxPauseMs <= kMaxPauseMs, "1 s stall: no sleep over one tick afterwards");
}

} // namespace

int main()
{
  pauseCases();
  edgeCases();
  unpacedLoopCases();
  vsyncLoopCases();
  overrunLoopCases();
  std::printf("%s\n", gFailures == 0 ? "all frame pacing tests passed" : "SOME FRAME PACING TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
