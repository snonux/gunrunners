#pragma once

#include <algorithm>
#include <cmath>

namespace gr
{

// Frame pacing for the desktop window loop. The game ticks at a fixed 60 Hz
// and the picture shows nothing but the last tick, so a frame drawn before
// the next tick is due repeats the previous one. A vsynced present paces the
// loop by itself; without vsync (a renderer created without it, or a driver
// or compositor that ignores it) the loop would spin at 100% of a core
// drawing identical frames, so it sleeps until the next tick instead.

// The shortest frame a working vsync can produce: no display refreshes
// faster than 500 Hz. A frame that returns quicker than this was not held
// back by the present, whatever the renderer's flags claim (vblank_mode=0,
// a driver's sync-to-vblank switched off, a minimized Wayland window).
constexpr double kMinVsyncFrameSeconds = 1.0 / 500.0;

// Milliseconds to sleep after a frame before the next one.
//   vsync        - the renderer reports SDL_RENDERER_PRESENTVSYNC.
//   frameSeconds - how long the frame (events, ticks, picture, present)
//                  took, not counting an earlier sleep.
//   tickSeconds  - the fixed tick length (1/60 s).
//   dueSeconds   - game time owed so far: the accumulator left after the
//                  frame's ticks plus the time since it was last advanced.
// Zero when vsync really paces the loop (sleeping there could miss a
// vblank and halve the frame rate) or when a tick is already due, e.g.
// after a frame that overran. The wait is rounded up: waking a fraction of
// a millisecond late only delays the tick that much, and the excess goes
// back into the accumulator, while waking early would draw one more
// identical frame. It never exceeds one tick, even for a negative or
// stale dueSeconds.
inline int framePauseMs(bool vsync, double frameSeconds, double tickSeconds, double dueSeconds)
{
  const bool vsyncPaced = vsync && frameSeconds >= kMinVsyncFrameSeconds;
  if (vsyncPaced || !(tickSeconds > 0.0))
    return 0;
  const double untilTick = std::min(tickSeconds - dueSeconds, tickSeconds);
  // The negated test also catches NaN.
  if (!(untilTick > 0.0))
    return 0;
  return int(std::ceil(untilTick * 1000.0));
}

} // namespace gr
