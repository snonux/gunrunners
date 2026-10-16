// Checks how long the desktop window loop sleeps between frames: not at all
// while vsync paces it, until the next 60 Hz tick when nothing else does,
// and never a negative or runaway wait.

#include "base/frame_pacing.hpp"

#include <cmath>
#include <cstdio>
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
// A frame a vsynced 60 Hz present holds back, and one nothing holds back.
constexpr double kVsyncFrame = 1.0 / 60.0;
constexpr double kFastFrame = 0.0005;

void vsyncCases()
{
  check(framePauseMs(true, kVsyncFrame, kTick, 0.0) == 0, "vsync: no sleep right after a tick");
  check(framePauseMs(true, kVsyncFrame, kTick, 0.008) == 0, "vsync: no sleep halfway to a tick");
  check(framePauseMs(true, 1.0 / 144.0, kTick, 0.0) == 0, "vsync at 144 Hz: no sleep");
  check(framePauseMs(true, kMinVsyncFrameSeconds, kTick, 0.0) == 0, "vsync: a 500 Hz frame still counts as paced");
  // The flag is set but the present returned at once: vsync is not working.
  check(framePauseMs(true, kFastFrame, kTick, 0.0) == 17, "ignored vsync: sleeps until the next tick");
  check(framePauseMs(true, 0.0, kTick, 0.0) == 17, "ignored vsync, zero-length frame: sleeps");
}

void noVsyncCases()
{
  check(framePauseMs(false, kFastFrame, kTick, 0.0) == 17, "no vsync, just ticked: sleeps a whole tick");
  check(framePauseMs(false, kVsyncFrame, kTick, 0.0) == 17, "no vsync, slow frame: the frame length alone does not count");
  check(framePauseMs(false, kFastFrame, kTick, 0.010) == 7, "no vsync, 10 ms owed: sleeps the rest, rounded up");
  check(framePauseMs(false, kFastFrame, kTick, kTick - 1e-6) == 1, "no vsync, a microsecond to go: sleeps 1 ms, not 0");
  // Every wait lies between zero and one tick, rounded up.
  bool bounded = true;
  for (int i = -20; i <= 40; ++i)
  {
    const int ms = framePauseMs(false, kFastFrame, kTick, i * 0.001);
    bounded = bounded && ms >= 0 && ms <= 17;
  }
  check(bounded, "no vsync: every wait is within 0..17 ms");
}

void edgeCases()
{
  check(framePauseMs(false, kFastFrame, kTick, kTick) == 0, "a tick due right now: no sleep");
  check(framePauseMs(false, 0.05, kTick, 0.05) == 0, "frame overran by several ticks: no (negative) sleep");
  check(framePauseMs(false, kFastFrame, kTick, 0.25) == 0, "accumulator at its cap: no sleep");
  check(framePauseMs(false, kFastFrame, kTick, -0.01) == 17, "negative owed time: capped at one tick");
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  check(framePauseMs(false, kFastFrame, kTick, -inf) == 17, "minus infinity owed: capped at one tick");
  check(framePauseMs(false, kFastFrame, kTick, inf) == 0, "infinity owed: no sleep");
  check(framePauseMs(false, kFastFrame, kTick, nan) == 0, "NaN owed: no sleep");
  check(framePauseMs(false, kFastFrame, 0.0, 0.0) == 0, "zero tick length: no sleep");
  check(framePauseMs(false, kFastFrame, -kTick, 0.0) == 0, "negative tick length: no sleep");
  check(framePauseMs(false, kFastFrame, nan, 0.0) == 0, "NaN tick length: no sleep");
}

} // namespace

int main()
{
  vsyncCases();
  noVsyncCases();
  edgeCases();
  std::printf("%s\n", gFailures == 0 ? "all frame pacing tests passed" : "SOME FRAME PACING TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
