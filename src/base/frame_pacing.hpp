#pragma once

#include <algorithm>
#include <cmath>

namespace gr
{

// Frame pacing for the desktop window loop. The game ticks at a fixed 60 Hz
// and the picture shows nothing but the last tick, so a frame drawn before
// the next tick is due repeats the previous one. The loop therefore always
// sleeps until the next tick is due, vsync or not:
//   - Whether a present blocks cannot be told from the renderer's flags. A
//     vsynced present returns at once while the swap queue has room (Mesa
//     DRI3, NVIDIA, DXGI, Wayland, Android BufferQueue), and drivers and
//     settings ignore vsync outright (vblank_mode=0, sync-to-vblank off,
//     G-Sync), so relying on it would spin a core at 100%.
//   - Sleeping costs a vsynced loop nothing: the time owed counts the time
//     spent blocked in the present, so a present that waited for the vblank
//     only shortens the sleep that follows. The two waits overlap instead
//     of adding up, and the loop still draws one frame per tick.

// Milliseconds to sleep after a frame before the next one.
//   tickSeconds - the fixed tick length (1/60 s).
//   dueSeconds  - game time owed so far: the accumulator left after the
//                 frame's ticks plus the time since it was last advanced,
//                 which includes drawing and presenting the frame.
// Zero when a tick is already due, e.g. after a frame that overran. The
// wait is rounded up: waking a fraction of a millisecond late only delays
// the tick that much, and the excess goes back into the accumulator, while
// waking early would draw one more identical frame. It never exceeds one
// tick (rounded up), even for a negative or stale dueSeconds, and is zero
// for a NaN or a tick length that is not positive.
inline int framePauseMs(double tickSeconds, double dueSeconds)
{
  if (!(tickSeconds > 0.0))
    return 0;
  const double untilTick = std::min(tickSeconds - dueSeconds, tickSeconds);
  // The negated test also catches NaN.
  if (!(untilTick > 0.0))
    return 0;
  return int(std::ceil(untilTick * 1000.0));
}

} // namespace gr
